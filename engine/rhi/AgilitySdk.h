#ifndef D3D12_ENGINE_AGILITYSDK_H
#define D3D12_ENGINE_AGILITYSDK_H

#include "rhi/RhiCommon.h"

// Agility SDK: these two exports make d3d12.dll load the newer runtime from <exe dir>/D3D12/
// instead of the one built into Windows. They must be defined in the .exe itself (not in a DLL
// or static library), so every app expands this macro once, at global scope in its main.cpp.
#define ENGINE_EXPORT_AGILITY_SDK()                                              \
    extern "C" {                                                                 \
    __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION; \
    __declspec(dllexport) extern const char *D3D12SDKPath = ".\\D3D12\\";        \
    }

#endif //D3D12_ENGINE_AGILITYSDK_H
