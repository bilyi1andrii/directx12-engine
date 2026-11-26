#include "rhi/GpuProfiler.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <format>
#include <span>
#include <stdexcept>

#include <directx/d3dx12.h>

#include "core/ThrowIfFailed.h"
#include "rhi/CommandContext.h"

namespace rhi {
    using core::ThrowIfFailed;

    namespace {
        constexpr uint32_t kQueryCount = kFramesInFlight * timestamp::kPerFrame;
    } // namespace

    GpuProfiler::GpuProfiler(NativeDevice *device, Allocator &allocator, uint64_t ticksPerSecond)
        : m_ticksPerSecond(ticksPerSecond) {
        const D3D12_QUERY_HEAP_DESC heapDesc{
            .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP, .Count = kQueryCount, .NodeMask = 0};
        ThrowIfFailed(device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_queryHeap)));
        ThrowIfFailed(m_queryHeap->SetName(L"GPU timestamps"));

        const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(kQueryCount * sizeof(uint64_t));
        m_readback = allocator.CreateResource(D3D12_HEAP_TYPE_READBACK, bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                              nullptr, L"GPU timestamps readback");
    }

    void GpuProfiler::BeginFrame(CommandContext &commands, uint64_t frame) {
        m_slot = static_cast<uint32_t>(frame % kFramesInFlight);
        SlotRecord &record = m_slots[m_slot];
        if (record.resolved) {
            m_latest = ReadSlot(m_slot, record);
        }

        record = {.frame = frame};
        commands.WriteTimestamp(m_queryHeap.Get(), FirstQuery(m_slot) + timestamp::kFrameBegin);
    }

    uint32_t GpuProfiler::BeginScope(CommandContext &commands, uint32_t scopeId) {
        SlotRecord &record = m_slots[m_slot];
        if (record.scopeCount == kMaxGpuScopes) {
            throw std::length_error(std::format("More than {} GPU scopes in frame {}", kMaxGpuScopes, record.frame));
        }

        const uint32_t scope = record.scopeCount++;
        record.scopeIds[scope] = scopeId;
        commands.WriteTimestamp(m_queryHeap.Get(), FirstQuery(m_slot) + timestamp::ScopeBegin(scope));
        return scope;
    }

    void GpuProfiler::EndScope(CommandContext &commands, uint32_t scopeIndex) {
        commands.WriteTimestamp(m_queryHeap.Get(), FirstQuery(m_slot) + timestamp::ScopeEnd(scopeIndex));
    }

    void GpuProfiler::EndFrame(CommandContext &commands) {
        SlotRecord &record = m_slots[m_slot];

        const uint32_t first = FirstQuery(m_slot);
        commands.WriteTimestamp(m_queryHeap.Get(), first + timestamp::kFrameEnd);
        commands.ResolveTimestamps(m_queryHeap.Get(), first, timestamp::Count(record.scopeCount),
                                   m_readback->GetResource(), uint64_t{first} * sizeof(uint64_t));
        record.resolved = true;
    }

    std::vector<GpuFrameTimes> GpuProfiler::ReadRemaining() {
        std::vector<GpuFrameTimes> remaining;
        for (uint32_t slot = 0; slot < kFramesInFlight; ++slot) {
            if (SlotRecord &record = m_slots[slot]; record.resolved) {
                remaining.push_back(ReadSlot(slot, record));
                record.resolved = false;
            }
        }
        std::ranges::sort(remaining, {}, &GpuFrameTimes::frame);
        if (!remaining.empty()) {
            m_latest = remaining.back();
        }
        return remaining;
    }

    GpuFrameTimes GpuProfiler::ReadSlot(uint32_t slot, const SlotRecord &record) const {
        const uint32_t count = timestamp::Count(record.scopeCount);
        const size_t begin = size_t{FirstQuery(slot)} * sizeof(uint64_t);
        const size_t size = size_t{count} * sizeof(uint64_t);

        ID3D12Resource *buffer = m_readback->GetResource();
        const D3D12_RANGE readRange{.Begin = begin, .End = begin + size};
        void *mapped = nullptr;
        ThrowIfFailed(buffer->Map(0, &readRange, &mapped));
        std::array<uint64_t, timestamp::kPerFrame> ticks{};
        std::memcpy(ticks.data(), static_cast<const std::byte *>(mapped) + begin, size);
        const D3D12_RANGE noWrite{.Begin = 0, .End = 0};
        buffer->Unmap(0, &noWrite);

        std::array<std::string_view, kMaxGpuScopes> names{};
        for (uint32_t scope = 0; scope < record.scopeCount; ++scope) {
            names[scope] = m_names.Name(record.scopeIds[scope]);
        }
        return MakeGpuFrameTimes(record.frame, std::span(ticks).first(count), std::span(names).first(record.scopeCount),
                                 m_ticksPerSecond);
    }
} // namespace rhi
