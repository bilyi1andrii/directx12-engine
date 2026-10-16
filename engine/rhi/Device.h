#ifndef D3D12_ENGINE_DEVICE_H
#define D3D12_ENGINE_DEVICE_H

#include <string>

#include "rhi/RhiCommon.h"

namespace engine::rhi {
    // Owns the DXGI factory, the adapter (GPU) and the D3D12 device.
    // In Debug builds it also enables the debug layer, GPU-based validation and DRED,
    // and routes debug-layer messages to the log.
    // Not copyable or movable: the debug-message callback keeps `this`.

    class Device {
    public:
        Device();
        // Releases the device, then reports live (leaked) DXGI/D3D12 objects in Debug builds
        ~Device();

        Device(const Device &) = delete;
        Device &operator=(const Device &) = delete;
        Device(Device &&) = delete;
        Device &operator=(Device &&) = delete;

        [[nodiscard]] NativeDevice *Native() const noexcept { return m_device.Get(); }
        [[nodiscard]] IDXGIFactory6 *Factory() const noexcept { return m_factory.Get(); }
        [[nodiscard]] bool TearingSupported() const noexcept { return m_tearingSupported; }

        // If the device was removed (GPU hang, driver crash, invalid command), logs the reason
        // and, in Debug builds, the DRED and page-fault info. Does nothing otherwise.
        void LogDeviceRemovedInfo() const;

    private:
        void CreateFactory();
        void SelectAdapter();
        void CreateDevice();
        void CheckFeatureSupport() const;
        void SetUpInfoQueue();

        static void __stdcall OnDebugMessage(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity,
                                             D3D12_MESSAGE_ID id, LPCSTR description, void *context);

        ComPtr<IDXGIFactory6> m_factory;
        ComPtr<IDXGIAdapter4> m_adapter;
        ComPtr<NativeDevice> m_device;
        std::string m_adapterName;
        bool m_tearingSupported = false;
        DWORD m_messageCallbackCookie = 0; // 0: no callback registered
    };
} // namespace engine::rhi

#endif //D3D12_ENGINE_DEVICE_H
