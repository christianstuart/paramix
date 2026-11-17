#pragma once

#include <QElapsedTimer>
#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>

#ifdef Q_OS_WIN
#include <pdh.h>
#include <pdhmsg.h>
#include <windows.h>
#endif

namespace mixxx {
namespace qml {

/// System-wide CPU and GPU utilization for the toolbar.
///
/// Samples once per second on the main thread. The GPU value is the busiest
/// engine, matching Task Manager, and is reported unavailable on platforms
/// or machines without a usable counter.
class QmlSystemMonitorProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
    Q_PROPERTY(double gpuUsage READ gpuUsage NOTIFY gpuUsageChanged)
    Q_PROPERTY(bool gpuAvailable READ gpuAvailable NOTIFY gpuUsageChanged)
    QML_NAMED_ELEMENT(SystemMonitor)
    QML_SINGLETON
  public:
    explicit QmlSystemMonitorProxy(QObject* pParent = nullptr);
    ~QmlSystemMonitorProxy() override;

    static QmlSystemMonitorProxy* create(
            QQmlEngine* pQmlEngine,
            [[maybe_unused]] QJSEngine* pJsEngine) {
        return new QmlSystemMonitorProxy(pQmlEngine);
    }

    double cpuUsage() const {
        return m_cpuUsage;
    }
    double gpuUsage() const {
        return m_gpuUsage;
    }
    bool gpuAvailable() const {
        return m_gpuAvailable;
    }

  signals:
    void cpuUsageChanged();
    void gpuUsageChanged();

  private:
    void sample();
    void initGpu();
    void closeGpu();
    bool readCpuUsage();
    bool readGpuUsage();

    QTimer m_timer;
    double m_cpuUsage = 0.0;
    double m_gpuUsage = 0.0;
    bool m_gpuAvailable = false;

    bool m_haveCpuSample = false;
    quint64 m_prevIdle = 0;
    quint64 m_prevTotal = 0;
#ifdef Q_OS_WIN
    quint64 m_prevKernel = 0;
    quint64 m_prevUser = 0;
    PDH_HQUERY m_gpuQuery = nullptr;
    PDH_HCOUNTER m_gpuCounter = nullptr;
#endif
#ifdef Q_OS_LINUX
    QString m_gpuBusyPath;
    bool m_gpuPathChecked = false;
#endif
};

} // namespace qml
} // namespace mixxx
