#pragma once

#include <QObject>
#include <QString>

namespace mixxx {
namespace qml {

class QmlConfigProxyBase : public QObject {
    Q_OBJECT
  public:
    explicit QmlConfigProxyBase(QObject* pParent = nullptr)
            : QObject(pParent) {
    }
    virtual ~QmlConfigProxyBase() override = default;

    virtual void setConfigScheme(const QString& scheme) = 0;

    /// Emit the Paramix NOTIFY signals after Preferences wrote [Paramix] keys,
    /// so the running QML skin re-reads them without a restart.
    static void notifyParamixSettings() {
        if (s_pInstance) {
            s_pInstance->notifyParamixSettingsChanged();
        }
    }

    static inline QmlConfigProxyBase* s_pInstance = nullptr;

  signals:
    void configSchemeChanged();

  private:
    virtual void notifyParamixSettingsChanged() {
    }
};

} // namespace qml
} // namespace mixxx
