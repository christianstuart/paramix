#include <gtest/gtest.h>

#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <utility>

#include "qml/qmllibrarytracklistcolumn.h"
#include "qml/qmlsidebarmodelproxy.h"

namespace mixxx::qml {
namespace {

void initColumn(QmlLibraryTrackListColumn* column, const QString& key) {
    column->setProperty("key", key);
    column->setProperty("label", key);
}

QStringList layoutKeys(const QVariantList& layout) {
    QStringList keys;
    for (const QVariant& item : std::as_const(layout)) {
        keys.append(item.toMap().value(QStringLiteral("key")).toString());
    }
    return keys;
}

TEST(QmlColumnLayoutTest, MoveAndHideApplyToTheSharedLayout) {
    QmlSidebarModelProxy sidebar;
    QmlLibraryTrackListColumn title;
    QmlLibraryTrackListColumn artist;
    QmlLibraryTrackListColumn genre;
    initColumn(&title, QStringLiteral("title"));
    initColumn(&artist, QStringLiteral("artist"));
    initColumn(&genre, QStringLiteral("genre"));
    sidebar.setColumns({&title, &artist, &genre});

    ASSERT_EQ(layoutKeys(sidebar.columnLayout()),
            QStringList({QStringLiteral("title"),
                    QStringLiteral("artist"),
                    QStringLiteral("genre")}));

    sidebar.moveColumn(2, 0);
    EXPECT_EQ(layoutKeys(sidebar.columnLayout()),
            QStringList({QStringLiteral("genre"),
                    QStringLiteral("title"),
                    QStringLiteral("artist")}));

    sidebar.setColumnHidden(1, true);
    const QVariantList layout = sidebar.columnLayout();
    ASSERT_EQ(layout.size(), 3);
    EXPECT_TRUE(layout.at(1).toMap().value(QStringLiteral("hidden")).toBool());
    EXPECT_FALSE(layout.at(0).toMap().value(QStringLiteral("hidden")).toBool());
}

TEST(QmlColumnLayoutTest, ApplyLayoutKeepsUnknownAndMissingColumns) {
    QmlSidebarModelProxy sidebar;
    QmlLibraryTrackListColumn title;
    QmlLibraryTrackListColumn artist;
    QmlLibraryTrackListColumn genre;
    initColumn(&title, QStringLiteral("title"));
    initColumn(&artist, QStringLiteral("artist"));
    initColumn(&genre, QStringLiteral("genre"));
    sidebar.setColumns({&title, &artist, &genre});

    // A saved layout from another version: one unknown key, one known column
    // missing. Missing columns must be appended instead of dropped.
    const QVariantList saved{
            QVariantMap{{QStringLiteral("key"), QStringLiteral("not-a-column")}},
            QVariantMap{{QStringLiteral("key"), QStringLiteral("genre")},
                    {QStringLiteral("hidden"), true}},
            QVariantMap{{QStringLiteral("key"), QStringLiteral("title")}},
    };
    sidebar.applyColumnLayout(saved);

    EXPECT_EQ(layoutKeys(sidebar.columnLayout()),
            QStringList({QStringLiteral("genre"),
                    QStringLiteral("title"),
                    QStringLiteral("artist")}));
    const QVariantList layout = sidebar.columnLayout();
    EXPECT_TRUE(layout.at(0).toMap().value(QStringLiteral("hidden")).toBool());
    EXPECT_FALSE(layout.at(1).toMap().value(QStringLiteral("hidden")).toBool());
}

} // namespace
} // namespace mixxx::qml
