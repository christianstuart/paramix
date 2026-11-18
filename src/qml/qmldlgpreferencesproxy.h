#pragma once

#include <QDialog>
#include <QObject>
#include <QQmlEngine>
#include <functional>
#include <memory>

namespace mixxx {
namespace qml {

class QmlDlgPreferencesProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(PreferencesDialog)
    QML_SINGLETON
  public:
    /// The dialog is created on first use. Constructing every preferences page
    /// at startup costs seconds on a cold profile and is not needed until the
    /// user opens the dialog.
    using DialogFactory = std::function<std::shared_ptr<QDialog>()>;

    explicit QmlDlgPreferencesProxy(
            DialogFactory dialogFactory,
            QObject* parent = nullptr);

    Q_INVOKABLE void show();

    static QmlDlgPreferencesProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static inline std::unique_ptr<QmlDlgPreferencesProxy> s_pInstance;

  private:
    std::shared_ptr<QDialog> ensureDialog();

    static inline QJSEngine* s_pJsEngine = nullptr;
    DialogFactory m_dialogFactory;
    std::shared_ptr<QDialog> m_pDlgPreferences;
};

} // namespace qml
} // namespace mixxx
