#include "rhi/Device.h"

#include <cstddef>
#include <format>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "core/Utf8.h"

namespace rhi {
    namespace {
        using core::LogError;
        using core::LogInfo;
        using core::LogWarning;
        using core::ThrowIfFailed;
        using core::ToUtf8;

#if defined(_DEBUG)
        constexpr bool kDebugBuild = true;
#else
        constexpr bool kDebugBuild = false;
#endif

        // Bindless needs resource binding tier 3 and shader model 6.6
        constexpr D3D_FEATURE_LEVEL kMinFeatureLevel = D3D_FEATURE_LEVEL_12_0;
        constexpr D3D_SHADER_MODEL kMinShaderModel = D3D_SHADER_MODEL_6_6;

        // Debug names in DRED output may be missing
        std::string NameOrUnnamed(const wchar_t *name) {
            return name != nullptr ? ToUtf8(name) : std::string("<unnamed>");
        }

        // D3D_FEATURE_LEVEL_12_1 is 0xc100, D3D_SHADER_MODEL_6_6 is 0x66
        std::string FeatureLevelName(D3D_FEATURE_LEVEL level) {
            return std::format("{}_{}", (level >> 12) & 0xF, (level >> 8) & 0xF);
        }

        std::string ShaderModelName(D3D_SHADER_MODEL model) {
            return std::format("{}.{}", (model >> 4) & 0xF, model & 0xF);
        }

        // The debug layer validates every D3D12 call and reports misuse (wrong resource states,
        // missing barriers, objects destroyed while in use). GPU-based validation additionally
        // patches shaders to check what they access while running; slow, so Debug builds only.
        // Both must be enabled before the device is created.
        void EnableDebugLayer() {
            ComPtr<ID3D12Debug1> debug;
            ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
            debug->EnableDebugLayer();
            debug->SetEnableGPUBasedValidation(TRUE);
        }

        // DRED (Device Removed Extended Data) records which operations each command list has
        // finished on the GPU ("breadcrumbs") and the address of a GPU page fault, so a
        // device-removed error can be traced to the command that caused it.
        // Must also be enabled before the device is created.
        void EnableDred() {
            ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dred;
            ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&dred)));
            dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }

        // Calls handle(message) for each message from `producer` stored in the DXGI info queue
        template <typename Handler>
        void ForEachStoredMessage(IDXGIInfoQueue &infoQueue, DXGI_DEBUG_ID producer, Handler &&handle) {
            const UINT64 count = infoQueue.GetNumStoredMessages(producer);
            std::vector<std::byte> buffer;
            for (UINT64 i = 0; i < count; ++i) {
                SIZE_T size = 0;
                ThrowIfFailed(infoQueue.GetMessage(producer, i, nullptr, &size));
                buffer.resize(size);
                auto *message = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE *>(buffer.data());
                ThrowIfFailed(infoQueue.GetMessage(producer, i, message, &size));
                handle(*message);
            }
        }

        // DescriptionByteLength counts the terminating null
        std::string_view Description(const DXGI_INFO_QUEUE_MESSAGE &message) {
            return {message.pDescription, message.DescriptionByteLength - 1};
        }

        // Logs every DXGI/D3D12 object that is still alive and returns how many there are.
        // Called after the device is released, so anything listed here is a leak.
        uint32_t ReportLiveObjects(IDXGIInfoQueue &infoQueue) {
            ComPtr<IDXGIDebug1> dxgiDebug;
            ThrowIfFailed(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)));

            // The report goes into the DXGI info queue; copy it from there to the log
            infoQueue.ClearStoredMessages(DXGI_DEBUG_ALL);
            ThrowIfFailed(dxgiDebug->ReportLiveObjects(
                DXGI_DEBUG_ALL,
                static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL)));

            uint32_t count = 0;
            ForEachStoredMessage(infoQueue, DXGI_DEBUG_ALL, [&count](const DXGI_INFO_QUEUE_MESSAGE &message) {
                LogWarning("{}", Description(message));
                ++count;
            });
            return count;
        }
    } // namespace

    Device::Device(ValidationStats &stats) : m_stats(stats) {
        if constexpr (kDebugBuild) {
            EnableDebugLayer();
            EnableDred();
        }
        CreateFactory();
        SelectAdapter();
        CreateDevice();
        CheckFeatureSupport();
        if constexpr (kDebugBuild) {
            SetUpInfoQueue();
            SetUpDxgiInfoQueue();
            // A run is only checked if both message sources work; the destructor clears this again
            // if the live-object report fails
            m_stats.active = m_messageCallbackCookie != 0 && m_dxgiInfoQueue != nullptr;
        }
    }

    Device::~Device() {
        if (m_messageCallbackCookie != 0) {
            // The callback receives `this`, so it must not outlive this object
            ComPtr<ID3D12InfoQueue1> infoQueue;
            if (SUCCEEDED(m_device.As(&infoQueue))) {
                infoQueue->UnregisterMessageCallback(m_messageCallbackCookie);
            }
        }

        m_device.Reset();
        m_adapter.Reset();
        m_factory.Reset();

        if constexpr (kDebugBuild) {
            try {
                // The last DXGI messages, including those from releasing the swap chain and the factory
                DrainDxgiMessages();
                if (m_dxgiInfoQueue) {
                    m_stats.liveObjectCount = ReportLiveObjects(*m_dxgiInfoQueue.Get());
                } else {
                    LogWarning("DXGI debug interface not available, cannot report live objects");
                    m_stats.active = false;
                }
            } catch (const std::exception &e) {
                LogError("Live-object report failed: {}", e.what());
                m_stats.active = false;
            }
        }
    }

    void Device::CreateFactory() {
        // The DXGI factory enumerates adapters and creates swap chains.
        // The debug flag makes DXGI validate its calls too.
        const UINT flags = kDebugBuild ? DXGI_CREATE_FACTORY_DEBUG : 0;
        ThrowIfFailed(::CreateDXGIFactory2(flags, IID_PPV_ARGS(&m_factory)));

        // Tearing (presenting without waiting for vblank) is needed for vsync off with a
        // flip-model swap chain. A failed check means it is not supported.
        BOOL allowTearing = FALSE;
        if (FAILED(m_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing,
                                                  sizeof(allowTearing)))) {
            allowTearing = FALSE;
        }
        m_tearingSupported = allowTearing != FALSE;
    }

    void Device::SelectAdapter() {
        // Pick the hardware adapter with the most dedicated VRAM that can run D3D12.
        // On a laptop with integrated + discrete GPUs this is the discrete one.
        SIZE_T maxDedicatedVideoMemory = 0;
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            ThrowIfFailed(adapter->GetDesc1(&desc));

            const bool isSoftware = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0; // WARP
            // With a null output pointer, D3D12CreateDevice only checks whether a device could be created
            const bool supportsD3D12 =
                SUCCEEDED(D3D12CreateDevice(adapter.Get(), kMinFeatureLevel, __uuidof(ID3D12Device), nullptr));

            if (!isSoftware && supportsD3D12 && desc.DedicatedVideoMemory > maxDedicatedVideoMemory) {
                maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
                ThrowIfFailed(adapter.As(&m_adapter));
                m_adapterName = ToUtf8(desc.Description);
            }
        }

        if (!m_adapter) {
            throw std::runtime_error(
                std::format("No GPU supports Direct3D 12 at feature level {}", FeatureLevelName(kMinFeatureLevel)));
        }

        // The user-mode driver version, as shown in Device Manager (e.g. 32.0.16.1742)
        m_driverVersion = "unknown";
        LARGE_INTEGER umdVersion{};
        if (SUCCEEDED(m_adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umdVersion))) {
            const auto version = static_cast<uint64_t>(umdVersion.QuadPart);
            m_driverVersion = std::format("{}.{}.{}.{}", version >> 48, (version >> 32) & 0xFFFF,
                                          (version >> 16) & 0xFFFF, version & 0xFFFF);
        }

        LogInfo("GPU: {} ({} MB VRAM, driver {})", m_adapterName, maxDedicatedVideoMemory / (1024 * 1024),
                m_driverVersion);
    }

    void Device::CreateDevice() {
        ThrowIfFailed(D3D12CreateDevice(m_adapter.Get(), kMinFeatureLevel, IID_PPV_ARGS(&m_device)));
        ThrowIfFailed(m_device->SetName(L"Device"));
    }

    void Device::CheckFeatureSupport() const {
        static constexpr D3D_FEATURE_LEVEL kLevels[] = {
            D3D_FEATURE_LEVEL_12_2,
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
        };
        D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels{
            .NumFeatureLevels = static_cast<UINT>(std::size(kLevels)),
            .pFeatureLevelsRequested = kLevels,
        };
        ThrowIfFailed(
            m_device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels)));

        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)));

        // In: the highest shader model the caller knows about. Out: the highest one the GPU supports
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{.HighestShaderModel = D3D_HIGHEST_SHADER_MODEL};
        ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)));

        LogInfo("Feature level {}, shader model {}, resource binding tier {}",
                FeatureLevelName(featureLevels.MaxSupportedFeatureLevel),
                ShaderModelName(shaderModel.HighestShaderModel), static_cast<int>(options.ResourceBindingTier));

        // Bindless: every descriptor in the heap may be accessed from a shader through ResourceDescriptorHeap[i]
        if (options.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_3) {
            throw std::runtime_error(
                std::format("{} does not support resource binding tier 3 (needed for bindless)", m_adapterName));
        }
        if (shaderModel.HighestShaderModel < kMinShaderModel) {
            throw std::runtime_error(std::format("{} supports shader model {}, the engine needs {}", m_adapterName,
                                                 ShaderModelName(shaderModel.HighestShaderModel),
                                                 ShaderModelName(kMinShaderModel)));
        }
    }

    void Device::SetUpInfoQueue() {
        // The info queue collects the debug layer's messages. Version 1 adds a callback,
        // which lets the engine log and count them.
        ComPtr<ID3D12InfoQueue1> infoQueue;
        if (FAILED(m_device.As(&infoQueue))) {
            LogWarning("ID3D12InfoQueue1 not available; debug-layer messages only go to the debugger output");
            return;
        }

        // Stop in the debugger on the message that reports a bug. Only when a debugger is attached:
        // without one, the break is an unhandled breakpoint exception that kills the process.
        if (::IsDebuggerPresent()) {
            ThrowIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
            ThrowIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
        }

        ThrowIfFailed(infoQueue->RegisterMessageCallback(&Device::OnDebugMessage, D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                                                         this, &m_messageCallbackCookie));
        // Each message now appears twice in the debugger output (D3D12's own line and the log's).
    }

    void Device::SetUpDxgiInfoQueue() {
        // DXGI's debug layer (DXGI_CREATE_FACTORY_DEBUG) stores its messages in a separate queue that
        // has no callback, so DrainDxgiMessages() reads it once per frame
        if (FAILED(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&m_dxgiInfoQueue)))) {
            LogWarning("DXGI debug interface not available; DXGI messages are not logged or counted");
            return;
        }

        // Same policy as for D3D12: stop in the debugger on errors, only when one is attached
        if (::IsDebuggerPresent()) {
            ThrowIfFailed(m_dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_DXGI,
                                                              DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE));
            ThrowIfFailed(
                m_dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_DXGI, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE));
        }
    }

    void Device::DrainDxgiMessages() {
        if (!m_dxgiInfoQueue) {
            return;
        }
        // Only DXGI's own messages. D3D12's messages are in this queue too (producer DXGI_DEBUG_D3D12), but they
        // already went through OnDebugMessage and would be counted twice
        ForEachStoredMessage(*m_dxgiInfoQueue.Get(), DXGI_DEBUG_DXGI, [this](const DXGI_INFO_QUEUE_MESSAGE &message) {
            switch (message.Severity) {
            case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION:
            case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR:
                ++m_stats.errorCount;
                LogError("DXGI #{}: {}", message.ID, Description(message));
                break;
            case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING:
                LogWarning("DXGI #{}: {}", message.ID, Description(message));
                break;
            default:
                LogInfo("DXGI #{}: {}", message.ID, Description(message));
                break;
            }
        });
        m_dxgiInfoQueue->ClearStoredMessages(DXGI_DEBUG_DXGI);
    }

    void __stdcall Device::OnDebugMessage(D3D12_MESSAGE_CATEGORY /*category*/, D3D12_MESSAGE_SEVERITY severity,
                                          D3D12_MESSAGE_ID id, LPCSTR description, void *context) {
        // Called from inside D3D12 calls: exceptions must not propagate into the runtime
        try {
            switch (severity) {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
            case D3D12_MESSAGE_SEVERITY_ERROR:
                ++static_cast<Device *>(context)->m_stats.errorCount;
                LogError("D3D12 #{}: {}", static_cast<int>(id), description);
                break;
            case D3D12_MESSAGE_SEVERITY_WARNING:
                LogWarning("D3D12 #{}: {}", static_cast<int>(id), description);
                break;
            default:
                LogInfo("D3D12 #{}: {}", static_cast<int>(id), description);
                break;
            }
        } catch (...) {
            // Nothing sensible to do if logging itself fails
        }
    }

    void Device::LogDeviceRemovedInfo() const {
        if (!m_device) {
            return;
        }
        const HRESULT reason = m_device->GetDeviceRemovedReason();
        if (reason == S_OK) {
            return;
        }
        LogError("Device removed, reason 0x{:08X}: {}", static_cast<unsigned>(reason),
                 std::system_category().message(reason));

        // DRED data exists only if DRED was enabled before the device was created (Debug builds)
        ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
        if (FAILED(m_device.As(&dred))) {
            return;
        }

        // One node per command list that was executing. pLastBreadcrumbValue is how many of its
        // operations the GPU finished; the first unfinished one is the likely culprit.
        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs{};
        if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs))) {
            for (const D3D12_AUTO_BREADCRUMB_NODE1 *node = breadcrumbs.pHeadAutoBreadcrumbNode; node != nullptr;
                 node = node->pNext) {
                const UINT finished = node->pLastBreadcrumbValue != nullptr ? *node->pLastBreadcrumbValue : 0;
                if (finished >= node->BreadcrumbCount) {
                    continue;
                }
                // The number is a D3D12_AUTO_BREADCRUMB_OP value (see d3d12.h)
                LogError("  Command list '{}' on queue '{}': {} of {} operations finished, next: op {}",
                         NameOrUnnamed(node->pCommandListDebugNameW), NameOrUnnamed(node->pCommandQueueDebugNameW),
                         finished, node->BreadcrumbCount, static_cast<int>(node->pCommandHistory[finished]));
            }
        }

        D3D12_DRED_PAGE_FAULT_OUTPUT1 pageFault{};
        if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&pageFault)) && pageFault.PageFaultVA != 0) {
            LogError("  GPU page fault at address 0x{:016X}", pageFault.PageFaultVA);
            for (const D3D12_DRED_ALLOCATION_NODE1 *node = pageFault.pHeadExistingAllocationNode; node != nullptr;
                 node = node->pNext) {
                LogError("    allocation at that address: '{}'", NameOrUnnamed(node->ObjectNameW));
            }
            for (const D3D12_DRED_ALLOCATION_NODE1 *node = pageFault.pHeadRecentFreedAllocationNode; node != nullptr;
                 node = node->pNext) {
                LogError("    recently freed allocation at that address: '{}'", NameOrUnnamed(node->ObjectNameW));
            }
        }
    }
} // namespace rhi
