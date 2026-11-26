#ifndef D3D12_ENGINE_COMSCOPE_H
#define D3D12_ENGINE_COMSCOPE_H

// Starts COM on the current thread for as long as the object lives. Image loading needs it, so each thread
// keeps one for the whole run.

namespace core {
    class ComScope {
    public:
        ComScope();
        ~ComScope();

        ComScope(const ComScope &) = delete;
        ComScope &operator=(const ComScope &) = delete;
    };
} // namespace core

#endif // D3D12_ENGINE_COMSCOPE_H
