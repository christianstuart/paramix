#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFontInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <functional>
#include <memory>
#include <ostream>
#include <string>

#include "coreservices.h"
#include "preferences/configobject.h"
#include "qml/qmlapplication.h"
#include "skin/qml/qmlskin.h"
#include "test/mixxxtest.h"
#include "util/cmdlineargs.h"
#include "util/versionstore.h"

namespace {

struct QmlSkin {
    const char* name;
    bool useDefaultSkin;
};

void PrintTo(const QmlSkin& skin, std::ostream* stream) {
    *stream << skin.name;
}

class RejectFileDialogs final : public QObject {
  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Show) {
            if (auto* dialog = qobject_cast<QFileDialog*>(watched)) {
                QTimer::singleShot(0, dialog, &QDialog::reject);
            }
        }
        return false;
    }
};

class SettingsPathGuard final {
  public:
    explicit SettingsPathGuard(CmdlineArgs& args)
            : m_args(args),
              m_previousArgs(args) {
    }

    ~SettingsPathGuard() {
        m_args = m_previousArgs;
    }

  private:
    CmdlineArgs& m_args;
    const CmdlineArgs m_previousArgs;
};

class QmlResponsiveBoundsTest : public MixxxTest {};

class QmlStartupSmokeTest : public MixxxTest,
                            public testing::WithParamInterface<QmlSkin> {
  protected:
    static CmdlineArgs makeCmdlineArgs(const QString& settingsPath) {
        CmdlineArgs args;
        args.setSettingsPath(settingsPath);
        return args;
    }

    static void writeProfile(const QString& settingsPath, const QmlSkin& skin) {
        auto settings = UserSettingsPointer(new UserSettings(settingsPath + "mixxx.cfg"));
        settings->setValue(ConfigKey("[Config]", "Version"), VersionStore::FUTURE_UNSTABLE);
        if (!skin.useDefaultSkin) {
            settings->setValue(
                    ConfigKey("[Config]", "ResizableSkin"),
                    QString::fromLatin1(skin.name));
        }
        settings->save();

        QFile soundConfig(settingsPath + "soundconfig.xml");
        ASSERT_TRUE(soundConfig.open(QIODevice::WriteOnly | QIODevice::Truncate));
        ASSERT_GT(soundConfig.write("<SoundManagerConfig api=\"None\"/>\n"), 0);
    }
};

TEST_P(QmlStartupSmokeTest, Starts) {
    const auto& skin = GetParam();
    QTemporaryDir profile;
    ASSERT_TRUE(profile.isValid());
    const QString settingsPath = profile.path() + QLatin1Char('/');
    writeProfile(settingsPath, skin);

    SettingsPathGuard settingsPathGuard(CmdlineArgs::Instance());
    CmdlineArgs::Instance().setSettingsPath(settingsPath);
    const auto args = makeCmdlineArgs(settingsPath);
    auto coreServices = std::make_shared<mixxx::CoreServices>(args, application());

    QString mainQmlFilePath;
    if (!skin.useDefaultSkin) {
        ASSERT_QSTRING_EQ(
                QString::fromLatin1(skin.name),
                coreServices->getSettings()->getValueString(
                        ConfigKey("[Config]", "ResizableSkin")));
        const auto qmlSkin = mixxx::skin::qml::QmlSkin::fromDirectory(
                QDir(QStringLiteral(RESOURCE_FOLDER "/skins/LateNightQML")));
        ASSERT_TRUE(qmlSkin);
        ASSERT_EQ(mixxx::skin::SkinType::QML, qmlSkin->type());
        mainQmlFilePath = qmlSkin->mainQmlFilePath();
    }

    RejectFileDialogs rejectFileDialogs;
    application()->installEventFilter(&rejectFileDialogs);
    bool startupReady = false;
    {
        mixxx::qml::QmlApplication qmlApplication(application(), coreServices, mainQmlFilePath);
        startupReady = qmlApplication.isReady();
    }
    application()->removeEventFilter(&rejectFileDialogs);

    EXPECT_TRUE(startupReady)
            << "Mixxx failed to start the QML skin '" << skin.name << "'"
            << "\nQML entry point: "
            << (mainQmlFilePath.isEmpty() ? "<default>" : qPrintable(mainQmlFilePath))
            << "\nSettings path: " << qPrintable(settingsPath);
}

INSTANTIATE_TEST_SUITE_P(
        QmlSkins,
        QmlStartupSmokeTest,
        testing::Values(
                QmlSkin{"LateNightQML", false},
                QmlSkin{"Default", true}),
        [](const testing::TestParamInfo<QmlSkin>& info) {
            return std::string(info.param.name);
        });

/// The Theme singleton is not in the item tree; evaluate it in the context of
/// the skin's root item, which is where its import is in scope.
QObject* resolveTheme(QQuickItem* item) {
    QQmlContext* pContext = qmlContext(item);
    if (!pContext) {
        return nullptr;
    }
    QQmlExpression expression(pContext, item, QStringLiteral("Theme"));
    return expression.evaluate().value<QObject*>();
}

QObject* resolveMixxxConfig(QQuickItem* item) {
    QQmlContext* pContext = qmlContext(item);
    if (!pContext) {
        return nullptr;
    }
    QQmlExpression expression(pContext, item, QStringLiteral("Mixxx.Config"));
    return expression.evaluate().value<QObject*>();
}

QQuickItem* findItemByClass(QQuickItem* root, const char* classNamePrefix) {
    for (QQuickItem* pChild : root->childItems()) {
        if (QString::fromLatin1(pChild->metaObject()->className())
                        .startsWith(QLatin1String(classNamePrefix))) {
            return pChild;
        }
        if (QQuickItem* pFound = findItemByClass(pChild, classNamePrefix)) {
            return pFound;
        }
    }
    return nullptr;
}

/// A fingerprint of every visible item's mapped geometry. Comparing successive
/// fingerprints tells us the layout has stopped changing, which is what makes
/// the bounds check below deterministic.
/// A fingerprint of the band geometry. Only the top two levels are compared:
/// waveform playheads, VU segments and other animated children change shape all
/// the time without meaning that the layout is still reflowing.
QString layoutSignature(const QQuickItem* root) {
    QStringList parts;
    std::function<void(const QQuickItem*, int)> walk = [&](const QQuickItem* item, int depth) {
        if (!item->isVisible() || depth > 2) {
            return;
        }
        if (item->width() > 0 && item->height() > 0) {
            const QRectF mapped = item->mapRectToItem(root,
                    QRectF(0, 0, item->width(), item->height()));
            parts.append(QStringLiteral("%1:%2,%3,%4,%5")
                            .arg(reinterpret_cast<quintptr>(item))
                            .arg(qRound(mapped.x()))
                            .arg(qRound(mapped.y()))
                            .arg(qRound(mapped.width()))
                            .arg(qRound(mapped.height())));
        }
        for (QQuickItem* pChild : item->childItems()) {
            walk(pChild, depth + 1);
        }
    };
    walk(root, 0);
    return parts.join(QLatin1Char(';'));
}

/// Number of items in the tree, used to wait until asynchronous components
/// (Loaders, async image providers) have finished creating their children
/// before geometry is judged.
int countItems(const QQuickItem* root) {
    int count = 1;
    for (QQuickItem* pChild : root->childItems()) {
        count += countItems(pChild);
    }
    return count;
}

/// All items with the given objectName, in tree order.
QList<QQuickItem*> findItemsByObjectName(QQuickItem* root, const QString& name) {
    QList<QQuickItem*> found;
    std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
        if (item->objectName() == name) {
            found.append(item);
        }
        for (QQuickItem* pChild : item->childItems()) {
            walk(pChild);
        }
    };
    walk(root);
    return found;
}

/// Collects visible items whose mapped bounds fall outside the window, which is
/// what a squeezed layout looks like: a band or a control painting past the
/// edge instead of shrinking. Items under a clipping ancestor are skipped
/// because their overflow is already contained.
QStringList findOverflowingItems(const QQuickItem* root) {
    QStringList offenders;

    std::function<void(const QQuickItem*, bool)> walk =
            [&](const QQuickItem* item, bool underClip) {
                if (!item->isVisible() || item->width() <= 0 || item->height() <= 0) {
                    return;
                }
                if (!underClip) {
                    const QRectF mapped = item->mapRectToItem(root,
                            QRectF(0, 0, item->width(), item->height()));
                    constexpr qreal kTolerance = 1.0;
                    if (mapped.left() < -kTolerance || mapped.top() < -kTolerance ||
                            mapped.right() > root->width() + kTolerance ||
                            mapped.bottom() > root->height() + kTolerance) {
                        offenders.append(QStringLiteral("%1 '%2' at %3,%4 %5x%6")
                                        .arg(QString::fromLatin1(item->metaObject()->className()),
                                                item->objectName(),
                                                QString::number(qRound(mapped.x())),
                                                QString::number(qRound(mapped.y())),
                                                QString::number(qRound(mapped.width())),
                                                QString::number(qRound(mapped.height()))));
                    }
                }
                const bool clips = underClip || item->clip();
                for (QQuickItem* pChild : item->childItems()) {
                    walk(pChild, clips);
                }
            };

    walk(root, false);
    return offenders;
}

/// Opt-in layout audit: with PARAMIX_QML_DUMP_DIR set, writes every visible
/// item's mapped geometry (and text metrics) per window size,
/// for tools/qml_layout_audit.py. The bounds check above only sees items that
/// leave the window; sibling overlap and text overflow need the full tree.
void dumpLayout(QQuickItem* root, const QSize& size) {
    const QString dir = qEnvironmentVariable("PARAMIX_QML_DUMP_DIR");
    if (dir.isEmpty()) {
        return;
    }
    QDir().mkpath(dir);
    const QString base = QStringLiteral("%1/%2x%3").arg(dir).arg(size.width()).arg(size.height());

    QJsonArray items;
    std::function<void(QQuickItem*, int, qreal)> walk =
            [&](QQuickItem* item, int parent, qreal opacity) {
                if (!item->isVisible()) {
                    return;
                }
                const QRectF r = item->mapRectToItem(root,
                        QRectF(0, 0, item->width(), item->height()));
                const qreal effectiveOpacity = opacity * item->opacity();
                QJsonObject o{
                        {"p", parent},
                        {"cls", QString::fromLatin1(item->metaObject()->className())},
                        {"name", item->objectName()},
                        {"x", r.x()},
                        {"y", r.y()},
                        {"w", r.width()},
                        {"h", r.height()},
                        {"iw", item->implicitWidth()},
                        {"ih", item->implicitHeight()},
                        {"clip", item->clip()},
                        {"op", effectiveOpacity}};
                const QVariant text = item->property("text");
                if (text.typeId() == QMetaType::QString && !text.toString().isEmpty() &&
                        item->property("contentWidth").isValid()) {
                    o["text"] = text.toString();
                    o["cw"] = item->property("contentWidth").toDouble();
                    o["ch"] = item->property("contentHeight").toDouble();
                    o["trunc"] = item->property("truncated").toBool();
                    o["elide"] = item->property("elide").toInt();
                    const QFont font = item->property("font").value<QFont>();
                    o["px"] = font.pixelSize();
                    o["family"] = font.family();
                    o["resolved"] = QFontInfo(font).family();
                }
                const int index = items.size();
                items.append(o);
                for (QQuickItem* pChild : item->childItems()) {
                    walk(pChild, index, effectiveOpacity);
                }
            };
    walk(root, -1, 1.0);

    QFile json(base + QStringLiteral(".json"));
    if (json.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        json.write(QJsonDocument(items).toJson(QJsonDocument::Compact));
    }
}

/// Window sizes to measure. PARAMIX_QML_SIZES ("1366x720,2560x1369") replaces
/// the asserted defaults for an audit run.
QList<QSize> boundsTestSizes() {
    QList<QSize> sizes;
    const QStringList specs = qEnvironmentVariable("PARAMIX_QML_SIZES")
                                      .split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& spec : specs) {
        const QStringList wh = spec.split(QLatin1Char('x'));
        if (wh.size() == 2 && wh[0].toInt() > 0 && wh[1].toInt() > 0) {
            sizes.append(QSize(wh[0].toInt(), wh[1].toInt()));
        }
    }
    if (sizes.isEmpty()) {
        sizes = {QSize(2560, 1369),
                QSize(1792, 1008),
                QSize(1440, 852),
                QSize(1280, 800),
                QSize(1280, 720)};
    }
    return sizes;
}

/// The spacing grid and the fixed layout metrics follow `uiScale`, which is the
/// lever a scaled-down display needs: text stays on `fontScale`.
TEST(QmlThemeUiScaleTest, ScalingShrinksTheSpacingGrid) {
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(RESOURCE_FOLDER "/qml"));
    // singletonInstance(uri, typeName) only exists from Qt 6.5; reach the
    // singleton through a component instead so this builds on Qt 6.4.
    QQmlComponent component(&engine);
    component.setData(
            "import QtQml\nimport Theme\nQtObject { property var theme: Theme }\n",
            QUrl());
    const std::unique_ptr<QObject> pHolder(component.create());
    ASSERT_NE(nullptr, pHolder) << qPrintable(component.errorString());
    QObject* pTheme = pHolder->property("theme").value<QObject*>();
    ASSERT_NE(nullptr, pTheme);

    pTheme->setProperty("uiScale", 1.0);
    EXPECT_EQ(16, pTheme->property("space4").toInt());
    EXPECT_EQ(44, pTheme->property("toolbarHeight").toInt());
    EXPECT_EQ(22, pTheme->property("fxControlHeight").toInt());
    EXPECT_EQ(30, pTheme->property("topBarEffectsHeight").toInt());
    EXPECT_EQ(74, pTheme->property("topBarHeight").toInt());

    pTheme->setProperty("uiScale", 0.75);
    EXPECT_EQ(12, pTheme->property("space4").toInt());
    EXPECT_EQ(qRound(44 * 0.75), pTheme->property("toolbarHeight").toInt());
    EXPECT_EQ(qRound(22 * 0.75), pTheme->property("fxControlHeight").toInt());
    EXPECT_EQ(qRound(22 * 0.75) + 2 * qRound(4 * 0.75),
            pTheme->property("topBarEffectsHeight").toInt());
    EXPECT_EQ(qRound(44 * 0.75) + qRound(22 * 0.75) + 2 * qRound(4 * 0.75),
            pTheme->property("topBarHeight").toInt());
}

/// The skin is designed for 1792x1008 and has to stay inside a scaled-down
/// window. This catches a band or control that paints past the edge instead of
/// shrinking, at the sizes the compact-layout work targets.
TEST_F(QmlResponsiveBoundsTest, BandsStayInsideTheWindow) {
    QTemporaryDir profile;
    ASSERT_TRUE(profile.isValid());
    const QString settingsPath = profile.path() + QLatin1Char('/');
    {
        auto settings = UserSettingsPointer(new UserSettings(settingsPath + "mixxx.cfg"));
        settings->setValue(ConfigKey("[Config]", "Version"), VersionStore::FUTURE_UNSTABLE);
        // Audit runs can measure the opt-in jog wheel layout as well.
        if (qEnvironmentVariableIsSet("PARAMIX_QML_JOG_WHEELS")) {
            settings->setValue(ConfigKey("[Paramix]", "ShowJogWheels"), 1);
        }
        // The preview bar is a stacked band row; keep it visible so the bounds
        // check actually measures it.
        settings->setValue(ConfigKey("[Skin]", "show_preview_decks"), 1);
        settings->save();

        QFile soundConfig(settingsPath + "soundconfig.xml");
        ASSERT_TRUE(soundConfig.open(QIODevice::WriteOnly | QIODevice::Truncate));
        ASSERT_GT(soundConfig.write("<SoundManagerConfig api=\"None\"/>\n"), 0);
    }
    // Opt-in audit with a populated library. The database must already exist
    // and its track paths must be readable on this machine.
    const QString profileDb = qEnvironmentVariable("PARAMIX_QML_PROFILE_DB");
    if (!profileDb.isEmpty()) {
        ASSERT_TRUE(QFile::copy(profileDb, settingsPath + "mixxxdb.sqlite"));
    }

    SettingsPathGuard settingsPathGuard(CmdlineArgs::Instance());
    CmdlineArgs::Instance().setSettingsPath(settingsPath);

    CmdlineArgs args;
    args.setSettingsPath(settingsPath);
    auto coreServices = std::make_shared<mixxx::CoreServices>(args, application());

    RejectFileDialogs rejectFileDialogs;
    application()->installEventFilter(&rejectFileDialogs);

    mixxx::qml::QmlApplication qmlApplication(application(), coreServices, QString());
    ASSERT_TRUE(qmlApplication.isReady());

    QQuickWindow* pWindow = nullptr;
    for (QWindow* pTopLevel : QGuiApplication::topLevelWindows()) {
        auto* pQuickWindow = qobject_cast<QQuickWindow*>(pTopLevel);
        if (pQuickWindow && pQuickWindow->isVisible()) {
            pWindow = pQuickWindow;
            break;
        }
    }
    ASSERT_NE(nullptr, pWindow) << "No visible QQuickWindow to measure";

    // The ApplicationWindow's width/height are bound to the screen, so resizing
    // the native window does not reflow the skin. Measure the skin item itself
    // instead: that is what the layout is computed against. The Theme tokens
    // are bound to the window rather than to this item, so they have to be
    // driven too or the measurement takes the design-size branches.
    QQuickItem* pMainWindow = nullptr;
    {
        QElapsedTimer wait;
        wait.start();
        while (!pMainWindow && wait.elapsed() < 4000) {
            QCoreApplication::processEvents();
            QThread::msleep(10);
            pMainWindow = findItemByClass(pWindow->contentItem(), "MainWindow");
        }
    }
    ASSERT_NE(nullptr, pMainWindow) << "Did not find the MainWindow item";

    QObject* pTheme = resolveTheme(pMainWindow);
    ASSERT_NE(nullptr, pTheme) << "Theme singleton not found in the skin context";

    // Wait until the Loaders have created their content: a shrink while items
    // are still being created measures half-built bands.
    {
        QElapsedTimer creation;
        creation.start();
        int previousCount = -1;
        int stableCreations = 0;
        // Half a second without a new item: the library and the second deck
        // are created asynchronously and arrive in bursts on a busy machine.
        while (creation.elapsed() < 15000) {
            QCoreApplication::processEvents();
            QThread::msleep(20);
            const int count = countItems(pMainWindow);
            if (count == previousCount) {
                if (++stableCreations >= 25) {
                    break;
                }
            } else {
                stableCreations = 0;
                previousCount = count;
            }
        }
    }

    // The item count can level off during a lull while the band Loaders are
    // still constructing, which makes the first measurement see a half-built
    // tree and then report the preview bar as missing. The preview bar is a
    // known leaf of the finished mixer band, so wait for it explicitly before
    // the first check. It stays instantiated while hidden, so this does not
    // depend on the bar being visible.
    {
        QElapsedTimer previewCreation;
        previewCreation.start();
        while (!findItemByClass(pMainWindow, "PreviewDeckBar") &&
                previewCreation.elapsed() < 10000) {
            QCoreApplication::processEvents();
            QThread::msleep(10);
        }
    }

    // 1024x768 is deliberately not asserted: below roughly 1200 px wide the
    // layout does not converge (the geometry never stops changing, see
    // docs/qml-responsive-layout.md), so an assertion there reports the
    // oscillation rather than a stable overflow. Fix the instability first.
    const auto checkSize = [&](const QSize& size) {
        // The skin sits in a Loader that main.qml sizes (and scales) from the
        // window, and a Loader imposes its own size on its item. That binding
        // re-evaluates when the window's content area changes (the menu bar
        // arriving, for one), so the size is re-applied until it sticks.
        const auto applySize = [&] {
            if (pMainWindow->size() == QSizeF(size)) {
                return false;
            }
            if (QQuickItem* pLoader = pMainWindow->parentItem()) {
                pLoader->setSize(QSizeF(size));
            }
            pMainWindow->setSize(QSizeF(size));
            return true;
        };
        applySize();
        pTheme->setProperty("windowWidth", size.width());
        pTheme->setProperty("windowHeight", size.height());

        // Let the layouts settle. The skin reparents and resizes over several
        // event loops, so wait until the geometry stops changing instead of
        // guessing a delay.
        QElapsedTimer settle;
        settle.start();
        QString previousSignature;
        int stableRounds = 0;
        while (settle.elapsed() < 8000) {
            QCoreApplication::processEvents();
            QThread::msleep(10);
            // The item count is part of the signature so that a band still
            // being created does not read as settled.
            const QString signature = applySize()
                    ? QString()
                    : QString::number(countItems(pMainWindow)) + QLatin1Char(';') +
                            layoutSignature(pMainWindow);
            // A tree that has not had its first layout pass yet is all 0x0 and
            // would otherwise count as settled, so it has to have bands first.
            if (signature == previousSignature && signature.count(QLatin1Char(';')) >= 3) {
                if (++stableRounds >= 10) {
                    break;
                }
            } else {
                stableRounds = 0;
                previousSignature = signature;
            }
        }
        EXPECT_GE(stableRounds, 10)
                << "Layout did not converge at " << size.width() << "x"
                << size.height() << " (geometry kept changing for 8 s)";

        EXPECT_EQ(QSizeF(size), pMainWindow->size())
                << "The skin was resized behind the test's back";

        dumpLayout(pMainWindow, size);

        const QStringList offenders = findOverflowingItems(pMainWindow);
        EXPECT_TRUE(offenders.isEmpty())
                << "Items paint outside a " << size.width() << "x" << size.height()
                << " window:\n"
                << offenders.join(QLatin1Char('\n')).toStdString();

        // The preview bar has to be reserved by the band, not laid out below
        // the window. This is the regression that four decks at 720 or a
        // disabled library used to expose.
        // A configuration toggle can rebuild the band after this check starts,
        // and the Loader recreates its item on the next event loop. Give it a
        // moment so a transient rebuild is not reported as a missing bar.
        QQuickItem* pPreviewBar = nullptr;
        QElapsedTimer previewWait;
        previewWait.start();
        while (!pPreviewBar && previewWait.elapsed() < 3000) {
            pPreviewBar = findItemByClass(pMainWindow, "PreviewDeckBar");
            if (pPreviewBar) {
                break;
            }
            QCoreApplication::processEvents();
            QThread::msleep(10);
        }
        EXPECT_NE(nullptr, pPreviewBar)
                << "Preview bar missing at " << size.width() << "x"
                << size.height();
        if (pPreviewBar) {
            EXPECT_TRUE(pPreviewBar->isVisible());
            const QRectF barRect = pPreviewBar->mapRectToItem(pMainWindow,
                    QRectF(0, 0, pPreviewBar->width(), pPreviewBar->height()));
            EXPECT_GE(barRect.top(), -1.0);
            EXPECT_LE(barRect.bottom(), pMainWindow->height() + 1.0);
        }

        // A single sample catches items mid-transition (a pane fading in, a
        // fader being laid out). Sample a few times and judge the largest
        // height seen, so a transient squeeze during the resize is not a
        // failure.
        qreal tallestFx = 0;
        qreal tallestPane = 0;
        for (int sample = 0; sample < 10; ++sample) {
            QCoreApplication::processEvents();
            QThread::msleep(100);
            for (QQuickItem* pFx :
                    findItemsByObjectName(pMainWindow, QStringLiteral("deckFxAssign"))) {
                if (pFx->isVisible()) {
                    tallestFx = std::max(tallestFx, pFx->height());
                }
            }
            for (QQuickItem* pPane :
                    findItemsByObjectName(pMainWindow, QStringLiteral("deckPane"))) {
                if (pPane->isVisible()) {
                    tallestPane = std::max(tallestPane, pPane->height());
                }
            }
        }

        // The deck's FX-assign column sits under the jog wheel in the same row
        // and used to get 0 px because the spinny took the whole row. A deck
        // that hides the column entirely (narrow windows) has no such items.
        if (tallestFx > 0) {
            EXPECT_GE(tallestFx, 30)
                    << "FX-assign keys squeezed at " << size.width() << "x"
                    << size.height() << ": " << tallestFx << "px";
        }
        if (tallestPane > 0) {
            EXPECT_GE(tallestPane, 110)
                    << "Deck pane squeezed at " << size.width() << "x" << size.height()
                    << ": " << tallestPane << "px";
        }
    };

    for (const QSize& size : boundsTestSizes()) {
        checkSize(size);
    }

    // The two layouts the preview bar used to fall out of: four decks at the
    // minimum height, and the library switched off. Cover them separately and
    // together at the shortest asserted height.
    QObject* pMixxxConfig = resolveMixxxConfig(pMainWindow);
    ASSERT_NE(nullptr, pMixxxConfig) << "Mixxx.Config singleton not found";

    pMainWindow->setProperty("fourDecksRequested", true);
    checkSize(QSize(1280, 720));

    pMainWindow->setProperty("fourDecksRequested", false);
    pMixxxConfig->setProperty("paramixShowLibrary", false);
    checkSize(QSize(1280, 720));

    pMainWindow->setProperty("fourDecksRequested", true);
    checkSize(QSize(1280, 720));

    pMixxxConfig->setProperty("paramixShowLibrary", true);
    pMainWindow->setProperty("fourDecksRequested", false);

    application()->removeEventFilter(&rejectFileDialogs);
}

} // namespace
