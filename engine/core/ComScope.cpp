#include "core/ComScope.h"

#include <Windows.h>
#include <combaseapi.h>

#include "core/ThrowIfFailed.h"

namespace core {
    ComScope::ComScope() {
        ThrowIfFailed(::CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    }

    ComScope::~ComScope() {
        ::CoUninitialize();
    }
} // namespace core
