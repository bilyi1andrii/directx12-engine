#ifndef D3D12_ENGINE_DEVICE_H
#define D3D12_ENGINE_DEVICE_H

#include <string>

#include <dxgidebug.h> // IDXGIInfoQueue

#include "rhi/RhiCommon.h"
#include "rhi/ValidationStats.h"

namespace engine::rhi {
    // Owns the DXGI factory, the adapter (GPU) and the D3D12 device.
    // In Debug builds it also enables the debug layer, GPU-based validation and DRED,
    // routes D3D12 and DXGI debug messages to the log and counts errors and leaks in `stats`.
    // Not copyable or movable: the debug-message callback keeps `this`.

    class Device {
    public:
        // `stats` must outlive the Device (the destructor writes the live-object count)
        explicit Device(ValidationStats &stats);
        // Releases the device, then reports live (leaked) DXGI/D3D12 objects in Debug builds
        ~Device();

        Device(const Device &) = delete;
        Device &operator=(const Device &) = delete;
        Device(Device &&) = delete;
        Device &operator=(Device &&) = delete;

        [[nodiscard]] NativeDevice *Native() const noexcept { return m_device.Get(); }
        [[nodiscard]] IDXGIFactory6 *Factory() const noexcept { return m_factory.Get(); }
        [[nodiscard]] IDXGIAdapter4 *Adapter() const noexcept { return m_adapter.Get(); }
        [[nodiscard]] bool TearingSupported() const noexcept { return m_tearingSupported; }
        // The adapter's description (e.g. "NVIDIA GeForce RTX 3050 Laptop GPU") and its user-mode driver version as
        // Device Manager shows it (e.g. "32.0.16.1742", or "unknown"), for logs and benchmark results
        [[nodiscard]] const std::string &AdapterName() const noexcept { return m_adapterName; }
        [[nodiscard]] const std::string &DriverVersion() const noexcept { return m_driverVersion; }

        // If the device was removed (GPU hang, driver crash, invalid command), logs the reason
        // and, in Debug builds, the DRED and page-fault info. Does nothing otherwise.
        void LogDeviceRemovedInfo() const;

        // Moves the messages DXGI stored since the last call (e.g. from Present or ResizeBuffers) to the
        // log and counts its errors. D3D12's messages arrive through the callback instead.
        // Call once per frame; does nothing in Release builds.
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
        ComPtr<IDXGIInfoQueue> m_dxgiInfoQueue; // Debug builds only
        ComPtr<IDXGIFactory6> m_factory;
        ComPtr<IDXGIAdapter4> m_adapter;
        ComPtr<NativeDevice> m_device;
        std::string m_adapterName;
        std::string m_driverVersion;
        bool m_tearingSupported = false;
        DWORD m_messageCallbackCookie = 0; // 0: no callback registered
    };
} // namespace engine::rhi

#endif //D3D12_ENGINE_DEVICE_H
