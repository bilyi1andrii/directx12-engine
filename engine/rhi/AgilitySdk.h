#ifndef D3D12_ENGINE_AGILITYSDK_H
#define D3D12_ENGINE_AGILITYSDK_H

// Makes the app load the newer D3D12 runtime shipped next to the program instead of the one built into Windows.
// Each app expands the macro once, at global scope in its main.cpp.

#include "rhi/RhiCommon.h"

#define ENGINE_EXPORT_AGILITY_SDK()                                              \
    extern "C" {                                                                 \
    __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION; \
    __declspec(dllexport) extern const char *D3D12SDKPath = ".\\D3D12\\";        \
    }

#endif //D3D12_ENGINE_AGILITYSDK_H
