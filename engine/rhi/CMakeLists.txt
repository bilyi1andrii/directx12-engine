# The rhi library, a thin layer over D3D12 and DXGI, and the CPU-only GPU-timing library it uses.

add_library(engine_gputimes STATIC
        GpuTimes.cpp
        GpuTimes.h)

engine_target_defaults(engine_gputimes)
target_include_directories(engine_gputimes PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_gputimes PUBLIC
        engine_core)

add_library(engine_rhi STATIC
        AgilitySdk.h
        Allocator.cpp
        Allocator.h
        BindlessHeap.cpp
        BindlessHeap.h
        Buffer.cpp
        Buffer.h
        CommandContext.cpp
        CommandContext.h
        CommandQueue.cpp
        CommandQueue.h
        DepthFormat.h
        DescriptorAllocator.cpp
        DescriptorAllocator.h
        Device.cpp
        Device.h
        FrameContext.cpp
        FrameContext.h
        GpuCopyable.h
        GpuProfiler.cpp
        GpuProfiler.h
        PipelineCache.cpp
        PipelineCache.h
        RhiCommon.h
        SwapChain.cpp
        SwapChain.h
        Texture.cpp
        Texture.h
        UploadContext.cpp
        UploadContext.h
        UploadRing.cpp
        UploadRing.h
        ValidationStats.h)

engine_target_defaults(engine_rhi)

target_include_directories(engine_rhi PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_rhi PUBLIC
        engine_core
        engine_gputimes
        Microsoft::DirectX-Headers
        Microsoft::DirectX-Guids
        GPUOpen::D3D12MemoryAllocator
        d3d12 dxgi dxguid)