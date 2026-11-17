#include "qml/qmlsystemmonitorproxy.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "moc_qmlsystemmonitorproxy.cpp"

#ifdef Q_OS_LINUX
#include <QDir>
#include <QFile>
#endif

#ifdef Q_OS_MACOS
#include <mach/mach.h>
#endif

namespace mixxx {
namespace qml {

namespace {

double clampPercent(double value) {
    return std::clamp(value, 0.0, 100.0);
}

double roundPercent(double value) {
    return std::round(clampPercent(value) * 10.0) / 10.0;
}

} // namespace

QmlSystemMonitorProxy::QmlSystemMonitorProxy(QObject* pParent)
        : QObject(pParent) {
    // Prime the CPU baseline so the first visible sample is meaningful.
    readCpuUsage();
    initGpu();

    m_timer.setInterval(1000);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &QmlSystemMonitorProxy::sample);
    m_timer.start();
}

QmlSystemMonitorProxy::~QmlSystemMonitorProxy() {
    m_timer.stop();
    closeGpu();
}

void QmlSystemMonitorProxy::sample() {
    if (readCpuUsage()) {
        const double usage = roundPercent(m_cpuUsage);
        if (usage != m_cpuUsage) {
            m_cpuUsage = usage;
            emit cpuUsageChanged();
        }
    }
    const bool wasAvailable = m_gpuAvailable;
    const double previousGpuUsage = m_gpuUsage;
    if (readGpuUsage()) {
        m_gpuUsage = roundPercent(m_gpuUsage);
    }
    if (wasAvailable != m_gpuAvailable || previousGpuUsage != m_gpuUsage) {
        emit gpuUsageChanged();
    }
}

bool QmlSystemMonitorProxy::readCpuUsage() {
#ifdef Q_OS_WIN
    FILETIME idleTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        return false;
    }
    const auto toUint64 = [](const FILETIME& time) {
        ULARGE_INTEGER value;
        value.LowPart = time.dwLowDateTime;
        value.HighPart = time.dwHighDateTime;
        return static_cast<quint64>(value.QuadPart);
    };
    const quint64 idle = toUint64(idleTime);
    const quint64 kernel = toUint64(kernelTime);
    const quint64 user = toUint64(userTime);
    if (!m_haveCpuSample) {
        m_prevIdle = idle;
        m_prevKernel = kernel;
        m_prevUser = user;
        m_haveCpuSample = true;
        return false;
    }
    const quint64 idleDelta = idle - m_prevIdle;
    const quint64 totalDelta = (kernel - m_prevKernel) + (user - m_prevUser);
    m_prevIdle = idle;
    m_prevKernel = kernel;
    m_prevUser = user;
    if (totalDelta == 0) {
        return false;
    }
    m_cpuUsage = 100.0 * (1.0 - static_cast<double>(idleDelta) / totalDelta);
    return true;
#elif defined(Q_OS_LINUX)
    QFile statFile(QStringLiteral("/proc/stat"));
    if (!statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QByteArray line = statFile.readLine();
    const QList<QByteArray> fields = line.simplified().split(' ');
    if (fields.size() < 5 || fields.first() != "cpu") {
        return false;
    }
    quint64 total = 0;
    for (int i = 1; i < fields.size(); ++i) {
        bool ok = false;
        const quint64 value = fields.at(i).toULongLong(&ok);
        if (!ok) {
            break;
        }
        total += value;
    }
    bool okIdle = false;
    bool okIowait = false;
    const quint64 idle = fields.at(3).toULongLong(&okIdle);
    const quint64 iowait = fields.at(4).toULongLong(&okIowait);
    if (!okIdle || !okIowait || total == 0) {
        return false;
    }
    const quint64 idleTotal = idle + iowait;
    if (!m_haveCpuSample) {
        m_prevIdle = idleTotal;
        m_prevTotal = total;
        m_haveCpuSample = true;
        return false;
    }
    const quint64 idleDelta = idleTotal - m_prevIdle;
    const quint64 totalDelta = total - m_prevTotal;
    m_prevIdle = idleTotal;
    m_prevTotal = total;
    if (totalDelta == 0) {
        return false;
    }
    m_cpuUsage = 100.0 * (1.0 - static_cast<double>(idleDelta) / totalDelta);
    return true;
#elif defined(Q_OS_MACOS)
    host_cpu_load_info_data_t info;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(),
                HOST_CPU_LOAD_INFO,
                reinterpret_cast<host_info_t>(&info),
                &count) != KERN_SUCCESS) {
        return false;
    }
    const quint64 user = info.cpu_ticks[CPU_STATE_USER];
    const quint64 system = info.cpu_ticks[CPU_STATE_SYSTEM];
    const quint64 idle = info.cpu_ticks[CPU_STATE_IDLE];
    const quint64 nice = info.cpu_ticks[CPU_STATE_NICE];
    const quint64 total = user + system + idle + nice;
    if (!m_haveCpuSample) {
        m_prevIdle = idle;
        m_prevTotal = total;
        m_haveCpuSample = true;
        return false;
    }
    const quint64 idleDelta = idle - m_prevIdle;
    const quint64 totalDelta = total - m_prevTotal;
    m_prevIdle = idle;
    m_prevTotal = total;
    if (totalDelta == 0) {
        return false;
    }
    m_cpuUsage = 100.0 * (1.0 - static_cast<double>(idleDelta) / totalDelta);
    return true;
#else
    return false;
#endif
}

void QmlSystemMonitorProxy::initGpu() {
#ifdef Q_OS_WIN
    if (PdhOpenQueryW(nullptr, 0, &m_gpuQuery) != ERROR_SUCCESS) {
        m_gpuQuery = nullptr;
        return;
    }
    // The counter name is localized on some systems; prefer the English form.
    PDH_STATUS status = PdhAddEnglishCounterW(m_gpuQuery,
            L"\\GPU Engine(*)\\Utilization Percentage",
            0,
            &m_gpuCounter);
    if (status != ERROR_SUCCESS) {
        status = PdhAddCounterW(m_gpuQuery,
                L"\\GPU Engine(*)\\Utilization Percentage",
                0,
                &m_gpuCounter);
    }
    if (status != ERROR_SUCCESS) {
        closeGpu();
        return;
    }
    // The first collection only primes the counter.
    PdhCollectQueryData(m_gpuQuery);
#endif
}

void QmlSystemMonitorProxy::closeGpu() {
#ifdef Q_OS_WIN
    m_gpuCounter = nullptr;
    if (m_gpuQuery) {
        PdhCloseQuery(m_gpuQuery);
        m_gpuQuery = nullptr;
    }
#endif
}

bool QmlSystemMonitorProxy::readGpuUsage() {
#ifdef Q_OS_WIN
    if (!m_gpuCounter || !m_gpuQuery) {
        m_gpuAvailable = false;
        return false;
    }
    if (PdhCollectQueryData(m_gpuQuery) != ERROR_SUCCESS) {
        m_gpuAvailable = false;
        return false;
    }
    DWORD bufferSize = 0;
    DWORD itemCount = 0;
    PDH_STATUS status = PdhGetFormattedCounterArrayW(m_gpuCounter,
            PDH_FMT_DOUBLE,
            &bufferSize,
            &itemCount,
            nullptr);
    if (status != PDH_MORE_DATA || bufferSize == 0) {
        m_gpuAvailable = false;
        return false;
    }
    // The counter array needs aligned storage; vector ensures that.
    std::vector<PDH_FMT_COUNTERVALUE_ITEM_W> items(
            bufferSize / sizeof(PDH_FMT_COUNTERVALUE_ITEM_W) + 1);
    DWORD alignedSize = static_cast<DWORD>(
            items.size() * sizeof(PDH_FMT_COUNTERVALUE_ITEM_W));
    status = PdhGetFormattedCounterArrayW(m_gpuCounter,
            PDH_FMT_DOUBLE,
            &alignedSize,
            &itemCount,
            items.data());
    if (status != ERROR_SUCCESS) {
        m_gpuAvailable = false;
        return false;
    }
    double busiest = 0.0;
    bool valid = false;
    for (DWORD i = 0; i < itemCount; ++i) {
        if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
                items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) {
            continue;
        }
        busiest = (std::max)(busiest, items[i].FmtValue.doubleValue);
        valid = true;
    }
    m_gpuAvailable = valid;
    if (valid) {
        m_gpuUsage = busiest;
    }
    return valid;
#elif defined(Q_OS_LINUX)
    if (!m_gpuPathChecked) {
        m_gpuPathChecked = true;
        const QDir drm(QStringLiteral("/sys/class/drm"));
        const QStringList cards = drm.entryList({QStringLiteral("card[0-9]*")},
                QDir::Dirs | QDir::NoDotAndDotDot,
                QDir::Name);
        for (const QString& card : cards) {
            const QString path = drm.filePath(
                    card + QStringLiteral("/device/gpu_busy_percent"));
            if (QFile::exists(path)) {
                m_gpuBusyPath = path;
                break;
            }
        }
    }
    if (m_gpuBusyPath.isEmpty()) {
        m_gpuAvailable = false;
        return false;
    }
    QFile busyFile(m_gpuBusyPath);
    if (!busyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_gpuAvailable = false;
        return false;
    }
    bool ok = false;
    const double usage = QString::fromLatin1(busyFile.readAll()).trimmed().toDouble(&ok);
    if (!ok) {
        m_gpuAvailable = false;
        return false;
    }
    m_gpuAvailable = true;
    m_gpuUsage = usage;
    return true;
#else
    m_gpuAvailable = false;
    return false;
#endif
}

} // namespace qml
} // namespace mixxx
