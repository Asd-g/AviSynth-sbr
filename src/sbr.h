#pragma once

#include <memory>
#include <string>

#include "avisynth.h"

template <typename T>
class sbr : public GenericVideoFilter
{
    int process[3];
    int pb_pitch;
    std::unique_ptr<T[]> buffer;
    bool v8;

    void(*sbr_)(void* dstp, void* tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;

public:
    sbr(PClip child, int y, int u, int v, int opt, std::string name, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

    int __stdcall SetCacheHints(int cachehints, int frame_range) override
    {
        return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
    }
};

template <int name>
void sbr_sse2_8(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template <int c, int h, uint32_t u, int name>
void sbr_sse2_16(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;

template <int name>
void sbr_avx2_8(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template <int c, int h, uint32_t u, int name>
void sbr_avx2_16(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;

template <int name>
void sbr_avx512_8(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template <int c, int h, uint32_t u, int name>
void sbr_avx512_16(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
