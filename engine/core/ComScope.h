#ifndef D3D12_ENGINE_COMSCOPE_H
#define D3D12_ENGINE_COMSCOPE_H

namespace core {
    // Starts COM on this thread while it exists. Loading images (WIC, used by DirectXTex) needs it.
    //
    // Create one per thread for the whole run, never one per load: DirectXTex keeps a COM object for the
    // whole process, and it would break once COM is shut down.
    class ComScope {
    public:
        // Throws HResultError on failure, e.g. if this thread already chose another COM mode
        ComScope();
        ~ComScope();

        ComScope(const ComScope &) = delete;
        ComScope &operator=(const ComScope &) = delete;
    };
} // namespace core

#endif // D3D12_ENGINE_COMSCOPE_H
