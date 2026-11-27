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

        constexpr D3D_FEATURE_LEVEL kMinFeatureLevel = D3D_FEATURE_LEVEL_12_0;
        constexpr D3D_SHADER_MODEL kMinShaderModel = D3D_SHADER_MODEL_6_6;

        std::string NameOrUnnamed(const wchar_t *name) {
            return name != nullptr ? ToUtf8(name) : std::string("<unnamed>");
        }

        std::string FeatureLevelName(D3D_FEATURE_LEVEL level) {
            return std::format("{}_{}", (level >> 12) & 0xF, (level >> 8) & 0xF);
        }

        std::string ShaderModelName(D3D_SHADER_MODEL model) {
            return std::format("{}.{}", (model >> 4) & 0xF, model & 0xF);
        }

        void EnableDebugLayer() {
            ComPtr<ID3D12Debug1> debug;
            ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
            debug->EnableDebugLayer();
            debug->SetEnableGPUBasedValidation(TRUE);
        }

        void EnableDred() {
            ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dred;
            ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&dred)));
            dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }

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

        std::string_view Description(const DXGI_INFO_QUEUE_MESSAGE &message) {
            return {message.pDescription, message.DescriptionByteLength - 1};
        }

        uint32_t ReportLiveObjects(IDXGIInfoQueue &infoQueue) {
            ComPtr<IDXGIDebug1> dxgiDebug;
            ThrowIfFailed(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)));

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
            m_stats.active = true;
        }
    }

    Device::~Device() {
        if constexpr (kDebugBuild) {
            ComPtr<ID3D12InfoQueue1> infoQueue;
            m_device.As(&infoQueue);
            infoQueue->UnregisterMessageCallback(m_messageCallbackCookie);
        }

        m_device.Reset();
        m_adapter.Reset();
        m_factory.Reset();

        if constexpr (kDebugBuild) {
            try {
                DrainDxgiMessages();
                m_stats.liveObjectCount = ReportLiveObjects(*m_dxgiInfoQueue.Get());
            } catch (const std::exception &e) {
                LogError("Live-object report failed: {}", e.what());
                m_stats.active = false;
            }
        }
    }

    void Device::CreateFactory() {
        const UINT flags = kDebugBuild ? DXGI_CREATE_FACTORY_DEBUG : 0;
        ThrowIfFailed(::CreateDXGIFactory2(flags, IID_PPV_ARGS(&m_factory)));

        BOOL allowTearing = FALSE;
        m_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        m_tearingSupported = allowTearing != FALSE;
    }

    void Device::SelectAdapter() {
        SIZE_T maxDedicatedVideoMemory = 0;
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            ThrowIfFailed(adapter->GetDesc1(&desc));

            const bool isSoftware = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
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

        LARGE_INTEGER umdVersion{};
        ThrowIfFailed(m_adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umdVersion));
        const auto version = static_cast<uint64_t>(umdVersion.QuadPart);
        m_driverVersion = std::format("{}.{}.{}.{}", version >> 48, (version >> 32) & 0xFFFF, (version >> 16) & 0xFFFF,
                                      version & 0xFFFF);

        LogInfo("GPU: {} ({} MB VRAM, driver {})", m_adapterName, maxDedicatedVideoMemory / (1024 * 1024),
                m_driverVersion);
    }

    void Device::CreateDevice() {
        ThrowIfFailed(D3D12CreateDevice(m_adapter.Get(), kMinFeatureLevel, IID_PPV_ARGS(&m_device)));
        ThrowIfFailed(m_device->SetName(L"Device"));
    }

    void Device::CheckFeatureSupport() const {
        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)));

        D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{.HighestShaderModel = D3D_HIGHEST_SHADER_MODEL};
        ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)));

        LogInfo("Shader model {}, resource binding tier {}", ShaderModelName(shaderModel.HighestShaderModel),
                static_cast<int>(options.ResourceBindingTier));

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
        ComPtr<ID3D12InfoQueue1> infoQueue;
        ThrowIfFailed(m_device.As(&infoQueue));

        if (::IsDebuggerPresent()) {
            ThrowIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
            ThrowIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
        }

        ThrowIfFailed(infoQueue->RegisterMessageCallback(&Device::OnDebugMessage, D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                                                         this, &m_messageCallbackCookie));
    }

    void Device::SetUpDxgiInfoQueue() {
        ThrowIfFailed(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&m_dxgiInfoQueue)));

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
            // called by D3D12 runtime, nothing should escape
        }
    }

    void Device::LogDeviceRemovedInfo() const {
        const HRESULT reason = m_device->GetDeviceRemovedReason();
        if (reason == S_OK) {
            return;
        }
        LogError("Device removed, reason 0x{:08X}: {}", static_cast<unsigned>(reason),
                 std::system_category().message(reason));

        ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
        if (FAILED(m_device.As(&dred))) {
            return;
        }

        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs{};
        if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs))) {
            for (const D3D12_AUTO_BREADCRUMB_NODE1 *node = breadcrumbs.pHeadAutoBreadcrumbNode; node != nullptr;
                 node = node->pNext) {
                const UINT finished = node->pLastBreadcrumbValue != nullptr ? *node->pLastBreadcrumbValue : 0;
                if (finished >= node->BreadcrumbCount) {
                    continue;
                }
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
