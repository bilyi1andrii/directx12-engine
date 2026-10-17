// Compiles the global root signature on its own into rs_global.cso (DXC -T rootsig_1_1
// -E GLOBAL_ROOT_SIGNATURE, see shaders/CMakeLists.txt). The renderer creates the
// ID3D12RootSignature from that file. The definition itself is in common/root_signature.hlsli.

#include "common/root_signature.hlsli"
