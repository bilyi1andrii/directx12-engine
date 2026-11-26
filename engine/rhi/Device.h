#ifndef D3D12_ENGINE_DEVICE_H
#define D3D12_ENGINE_DEVICE_H

// The GPU and the D3D12 device. In Debug it also turns on the debug layer and its GPU checks, logs every message
// they report, and counts errors and leaked objects.

#include <string>

#include <dxgidebug.h>

#include "rhi/RhiCommon.h"
#include "rhi/ValidationStats.h"

namespace rhi {
    class Device {
    public:
        explicit Device(ValidationStats &stats);
        ~Device();

        Device(const Device &) = delete;
        Device &operator=(const Device &) = delete;
        Device(Device &&) = delete;
        Device &operator=(Device &&) = delete;

        [[nodiscard]] NativeDevice *Native() const noexcept { return m_device.Get(); }
        [[nodiscard]] IDXGIFactory6 *Factory() const noexcept { return m_factory.Get(); }
        [[nodiscard]] IDXGIAdapter4 *Adapter() const noexcept { return m_adapter.Get(); }
        [[nodiscard]] bool TearingSupported() const noexcept { return m_tearingSupported; }
        [[nodiscard]] const std::string &AdapterName() const noexcept { return m_adapterName; }
        [[nodiscard]] const std::string &DriverVersion() const noexcept { return m_driverVersion; }

        void LogDeviceRemovedInfo() const;

        void DrainDxgiMessages();

    private:
        void CreateFactory();
        void SelectAdapter();
        void CreateDevice();
        void CheckFeatureSupport() const;
        void SetUpInfoQueue();
        void SetUpDxgiInfoQueue();

        static void __stdcall OnDebugMessage(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity,
                                             D3D12_MESSAGE_ID id, LPCSTR description, void *context);

        ValidationStats &m_stats;
        ComPtr<IDXGIInfoQueue> m_dxgiInfoQueue;
        ComPtr<IDXGIFactory6> m_factory;
        ComPtr<IDXGIAdapter4> m_adapter;
        ComPtr<NativeDevice> m_device;
        std::string m_adapterName;
        std::string m_driverVersion;
        bool m_tearingSupported = false;
        DWORD m_messageCallbackCookie = 0;
    };
} // namespace rhi

#endif //D3D12_ENGINE_DEVICE_H
