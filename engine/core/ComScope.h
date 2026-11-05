#ifndef D3D12_ENGINE_COMSCOPE_H
#define D3D12_ENGINE_COMSCOPE_H

namespace engine::core {
    // Starts the COM runtime on this thread for the object's lifetime. WIC (the Windows image decoder behind
    // DirectXTex) is created through it. D3D12 and DXGI do not need it: their objects are only reference-counted.
    //
    // Create one at the start of the thread and keep it until the last image is loaded. Do not create one per
    // load or per test: DirectXTex keeps its WIC factory for the whole process, and ending the last scope would
    // leave that pointer stale.
    //
    // Multithreaded mode (MTA): needs no message pump from COM and suits loader threads later.
    // Not copyable or movable: it must end on the thread that started it.
    class ComScope {
    public:
        // Throws HResultError on failure, e.g. if this thread already chose another COM mode
        ComScope();
        ~ComScope();

        ComScope(const ComScope &) = delete;
        ComScope &operator=(const ComScope &) = delete;
    };
} // namespace engine::core

#endif // D3D12_ENGINE_COMSCOPE_H
