#include "core/ComScope.h"

#include <Windows.h>
#include <combaseapi.h>

#include "core/ThrowIfFailed.h"

namespace engine::core {
    ComScope::ComScope() {
        // S_FALSE also needs its CoUninitialize. If this throws, the
        // destructor does not run, so a failed start is never ended.
        ThrowIfFailed(::CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    }

    ComScope::~ComScope() {
        ::CoUninitialize();
    }
} // namespace engine::core
