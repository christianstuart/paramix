#include "stems/stemgpudetect.h"

#include <QtGlobal>

#include <cstdint>
#include <cstring>

#include "stems/demucsonnxseparator.h"

#if defined(Q_OS_WIN)
#include <windows.h>

#include <dxgi.h>
#elif defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <sys/sysctl.h>
#elif defined(Q_OS_LINUX)
#include <QDir>
#include <QFile>
#endif

namespace mixxx::stems {
namespace {

/// A video adapter needs this much dedicated memory before it is treated as a
/// discrete GPU. Shared-memory iGPUs and AMD APUs stay below it, and they are
/// exactly the devices where a CPU-only run is often faster.
constexpr qulonglong kMinDedicatedVramBytes = 1ULL << 30; // 1 GiB

#if defined(Q_OS_WIN)
// {770AAE78-F26F-4DBA-A829-253C83D1B387}: IID_IDXGIFactory1. Declaring it here
// keeps dxgi.dll a runtime dependency instead of adding dxgi.lib to the link.
const GUID kDxgiFactory1Iid = {0x770aae78,
        0xf26f,
        0x4dba,
        {0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87}};

using CreateDxgiFactory1 = HRESULT(WINAPI*)(REFIID, void**);

bool detectDedicatedGpu() {
    const HMODULE pDxgi = LoadLibraryW(L"dxgi.dll");
    if (pDxgi == nullptr) {
        return false;
    }
    bool found = false;
    const auto createFactory = reinterpret_cast<CreateDxgiFactory1>(
            GetProcAddress(pDxgi, "CreateDXGIFactory1"));
    if (createFactory != nullptr) {
        IDXGIFactory1* pFactory = nullptr;
        if (SUCCEEDED(createFactory(kDxgiFactory1Iid,
                    reinterpret_cast<void**>(&pFactory))) &&
                pFactory != nullptr) {
            IDXGIAdapter1* pAdapter = nullptr;
            for (UINT index = 0; !found &&
                    pFactory->EnumAdapters1(index, &pAdapter) != DXGI_ERROR_NOT_FOUND;
                    ++index) {
                DXGI_ADAPTER_DESC1 description{};
                if (SUCCEEDED(pAdapter->GetDesc1(&description)) &&
                        (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                        description.DedicatedVideoMemory >= kMinDedicatedVramBytes &&
                        (description.VendorId == 0x10de || // NVIDIA
                                description.VendorId == 0x1002)) { // AMD
                    found = true;
                }
                pAdapter->Release();
            }
            pFactory->Release();
        }
    }
    FreeLibrary(pDxgi);
    return found;
}
#elif defined(Q_OS_MACOS)
bool isAppleSilicon() {
    std::int64_t value = 0;
    std::size_t size = sizeof(value);
    return sysctlbyname("hw.optional.arm64", &value, &size, nullptr, 0) == 0 &&
            value == 1;
}

bool detectDedicatedGpu() {
    // Apple Silicon has no discrete GPU, but its GPU is a real, fast
    // accelerator and the inference runtimes use it.
    if (isAppleSilicon()) {
        return true;
    }
    // Intel Macs: an AMD or NVIDIA accelerator means a discrete GPU. A lone
    // IntelAccelerator is integrated graphics, where the CPU is usually the
    // better choice for this model.
    io_iterator_t iterator = IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(
                MACH_PORT_NULL, IOServiceMatching("IOAccelerator"), &iterator) !=
            KERN_SUCCESS ||
            iterator == IO_OBJECT_NULL) {
        return false;
    }
    bool found = false;
    io_object_t service = IO_OBJECT_NULL;
    while (!found && (service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        const CFTypeRef pClass = IORegistryEntryCreateCFProperty(
                service, CFSTR("IOClass"), kCFAllocatorDefault, 0);
        if (pClass != nullptr && CFGetTypeID(pClass) == CFStringGetTypeID()) {
            char name[256] = {};
            if (CFStringGetCString(static_cast<CFStringRef>(pClass),
                        name,
                        sizeof(name),
                        kCFStringEncodingUTF8)) {
                // "AMDRadeonX6000_...", "NVDAGPU...", "IntelAccelerator",
                // "AGXAccelerator" (Apple Silicon, handled above).
                found = strstr(name, "Intel") == nullptr &&
                        strstr(name, "AGX") == nullptr &&
                        strstr(name, "Apple") == nullptr;
            }
        }
        if (pClass != nullptr) {
            CFRelease(pClass);
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);
    return found;
}
#elif defined(Q_OS_LINUX)
bool detectDedicatedGpu() {
    const QDir drm(QStringLiteral("/sys/class/drm"));
    const QStringList cards = drm.entryList(
            {QStringLiteral("card[0-9]*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& card : cards) {
        const QString devicePath = drm.filePath(card) + QStringLiteral("/device/");
        QFile vendorFile(devicePath + QStringLiteral("vendor"));
        if (!vendorFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString vendor =
                QString::fromLatin1(vendorFile.readAll()).trimmed().toLower();
        if (vendor == QStringLiteral("0x10de")) { // NVIDIA is always discrete.
            return true;
        }
        if (vendor == QStringLiteral("0x1002")) { // AMD, discrete or APU.
            QFile vramFile(devicePath + QStringLiteral("mem_info_vram_total"));
            if (vramFile.open(QIODevice::ReadOnly)) {
                bool ok = false;
                const qulonglong bytes =
                        QString::fromLatin1(vramFile.readAll()).trimmed().toULongLong(&ok);
                if (ok && bytes >= kMinDedicatedVramBytes) {
                    return true;
                }
            }
        }
    }
    return false;
}
#else
bool detectDedicatedGpu() {
    return false;
}
#endif

} // namespace

bool hasDedicatedGpu() {
    // Probe once: the adapters cannot change while the process runs, and the
    // platform queries are not free.
    static const bool kHasDedicatedGpu = detectDedicatedGpu();
    return kHasDedicatedGpu;
}

bool gpuAccelerationDefault() {
    return hasDedicatedGpu() && hasGpuInferenceProvider();
}

bool isGpuAccelerationEnabled(UserSettingsPointer pConfig) {
    if (!pConfig) {
        return gpuAccelerationDefault();
    }
    return pConfig->getValue(ConfigKey("[Stems]", kGpuAccelerationKey),
            gpuAccelerationDefault());
}

void setGpuAccelerationEnabled(UserSettingsPointer pConfig, bool enabled) {
    if (!pConfig) {
        return;
    }
    if (enabled == gpuAccelerationDefault()) {
        pConfig->remove(ConfigKey("[Stems]", kGpuAccelerationKey));
        return;
    }
    pConfig->setValue(ConfigKey("[Stems]", kGpuAccelerationKey), enabled);
}

std::string stemDeviceForGpuAcceleration(bool enabled) {
    return enabled ? "auto" : "cpu";
}

} // namespace mixxx::stems
