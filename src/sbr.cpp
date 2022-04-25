#include "sbr.h"

template <typename T, int c>
static void vertical_blur_c(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const T* srcp{ reinterpret_cast<const T*>(srcp_) };
    T* __restrict dstp{ reinterpret_cast<T*>(dstp_) };

    for (int y{ 0 }; y < height; ++y)
    {
        const T* srcpp{ (y == 0) ? srcp + src_pitch : srcp - src_pitch };
        const T* srcpn{ (y == height - 1) ? srcp - src_pitch : srcp + src_pitch };

        for (int x{ 0 }; x < width; ++x)
            dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + c) >> 2;

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

template <typename T>
static void blur_c(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const T* srcp{ reinterpret_cast<const T*>(srcp_) };
    T* __restrict dstp{ reinterpret_cast<T*>(dstp_) };

    for (int y{ 0 }; y < height; ++y)
    {
        const T* srcpp{ (y == 0) ? srcp + src_pitch : srcp - src_pitch };
        const T* srcpn{ (y == height - 1) ? srcp - src_pitch : srcp + src_pitch };

        dstp[0] = srcp[0];

        for (int x{ 1 }; x < width - 1; x += 1)
            dstp[x] = (srcpp[x - 1] + srcpp[x + 1] + srcpn[x - 1] + srcpn[x + 1] + ((srcpp[x] + srcp[x - 1] + srcp[x + 1] + srcpn[x]) << 1) + (srcp[x] << 2) + 8) >> 4;

        dstp[width - 1] = srcp[width - 1];

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

template <typename T, int p, int h>
static void mt_makediff_c(void* __restrict dstp_, const void* c1p_, const void* c2p_, int dst_pitch, int c1_pitch, int c2_pitch, int width, int height) noexcept
{
    const T* c1p{ reinterpret_cast<const T*>(c1p_) };
    const T* c2p{ reinterpret_cast<const T*>(c2p_) };
    T* __restrict dstp{ reinterpret_cast<T*>(dstp_) };

    for (int y{ 0 }; y < height; ++y)
    {
        for (int x{ 0 }; x < width; ++x)
            dstp[x] = std::max(std::min(c1p[x] - c2p[x] + h, p), 0);

        dstp += dst_pitch;
        c1p += c1_pitch;
        c2p += c2_pitch;
    }
}

template <typename T, int c, int p, int h, int name>
static void sbr_c(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept
{
    if constexpr (name == 0)
    {
        vertical_blur_c<T, c>(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
        mt_makediff_c<T, p, h>(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
        vertical_blur_c<T, c>(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.vblur()
    }
    else
    {
        blur_c<T>(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
        mt_makediff_c<T, p, h>(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
        blur_c<T>(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.blur()
    }

    const T* srcp{ reinterpret_cast<const T*>(srcp_) };
    T* __restrict tempp{ reinterpret_cast<T*>(tempp_) };
    T* __restrict dstp{ reinterpret_cast<T*>(dstp_) };

    for (int y{ 0 }; y < height; ++y)
    {
        for (int x{ 0 }; x < width; ++x)
        {
            int t{ dstp[x] - tempp[x] };
            int t2{ dstp[x] - h };
            if (t * t2 < 0)
                dstp[x] = srcp[x];
            else
            {
                if (std::abs(t) < std::abs(t2))
                    dstp[x] = srcp[x] - t;
                else
                    dstp[x] = srcp[x] - dstp[x] + h;
            }
        }

        dstp += dst_pitch;
        srcp += src_pitch;
        tempp += temp_pitch;
    }
}

template <typename T>
sbr<T>::sbr(PClip child, int y, int u, int v, int opt, std::string name, IScriptEnvironment* env)
    : GenericVideoFilter(child), process{ 1, 1, 1 }, v8(true)
{
    if (!vi.IsPlanar())
        env->ThrowError("%s: only planar input is supported!", name.c_str());
    if (vi.IsRGB())
        env->ThrowError("%s: only YUV input is supported!", name.c_str());
    if (opt < -1 || opt > 3)
        env->ThrowError("%s: opt must be between -1..3.", name.c_str());

    const bool avx512{ !!(env->GetCPUFlags() & CPUF_AVX512F) };
    const bool avx2{ !!(env->GetCPUFlags() & CPUF_AVX2) };
    const bool sse2{ !!(env->GetCPUFlags() & CPUF_SSE2) };

    if (!avx512 && opt == 3)
        env->ThrowError("%s: opt=3 requires AVX512F.", name.c_str());
    if (!avx2 && opt == 2)
        env->ThrowError("%s: opt=2 requires AVX2.", name.c_str());
    if (!sse2 && opt == 1)
        env->ThrowError("%s: opt=1 requires SSE2.", name.c_str());

    const int planecount{ std::min(vi.NumComponents(), 3) };
    const int planes[3]{ y, u, v };

    for (int i{ 0 }; i < planecount; ++i)
    {
        switch (planes[i])
        {
            case 3: process[i] = 3; break;
            case 2: process[i] = 2; break;
            case 1: process[i] = 1; break;
            default: env->ThrowError("%s: y/u/v must be between 1..3.", name.c_str());
        }
    }

    if ((avx512 && opt < 0) || opt == 3)
    {
        pb_pitch = (vi.width + 63) & ~63;

        if (sizeof(T) == 1)
            sbr_ = (name == "sbrV") ? sbr_avx512_8<0> : sbr_avx512_8<1>;
        else
        {
            switch (vi.BitsPerComponent())
            {
                case 10: sbr_ = (name == "sbrV") ? sbr_avx512_16<3, 512, 0x200200, 0> : sbr_avx512_16<3, 512, 0x200200, 1>; break;
                case 12: sbr_ = (name == "sbrV") ? sbr_avx512_16<4, 2048, 0x800800, 0 > : sbr_avx512_16<4, 2048, 0x800800, 1>; break;
                case 14: sbr_ = (name == "sbrV") ? sbr_avx512_16<16, 8192, 0x20002000, 0> : sbr_avx512_16<16, 8192, 0x20002000, 1>; break;
                default: sbr_ = (name == "sbrV") ? sbr_avx512_16<64, 32768, 0x80008000, 0> : sbr_avx512_16<64, 32768, 0x80008000, 1>; break;
            }
        }
    }
    else if ((avx2 && opt < 0) || opt == 2)
    {
        pb_pitch = (vi.width + 31) & ~31;

        if (sizeof(T) == 1)
            sbr_ = (name == "sbrV") ? sbr_avx2_8<0> : sbr_avx2_8<1>;
        else
        {
            switch (vi.BitsPerComponent())
            {
                case 10: sbr_ = (name == "sbrV") ? sbr_avx2_16<3, 512, 0x200200, 0> : sbr_avx2_16<3, 512, 0x200200, 1>; break;
                case 12: sbr_ = (name == "sbrV") ? sbr_avx2_16<4, 2048, 0x800800, 0> : sbr_avx2_16<4, 2048, 0x800800, 1>; break;
                case 14: sbr_ = (name == "sbrV") ? sbr_avx2_16<16, 8192, 0x20002000, 0> : sbr_avx2_16<16, 8192, 0x20002000, 1>; break;
                default: sbr_ = (name == "sbrV") ? sbr_avx2_16<64, 32768, 0x80008000, 0> : sbr_avx2_16<64, 32768, 0x80008000, 1>; break;
            }
        }
    }
    else if ((sse2 && opt < 0) || opt == 1)
    {
        pb_pitch = (vi.width + 15) & ~15;

        if (sizeof(T) == 1)
            sbr_ = (name == "sbrV") ? sbr_sse2_8<0> : sbr_sse2_8<1>;
        else
        {
            switch (vi.BitsPerComponent())
            {
                case 10: sbr_ = (name == "sbrV") ? sbr_sse2_16<3, 512, 0x200200, 0> : sbr_sse2_16<3, 512, 0x200200, 1>; break;
                case 12: sbr_ = (name == "sbrV") ? sbr_sse2_16<4, 2048, 0x800800, 0> : sbr_sse2_16<4, 2048, 0x800800, 1>; break;
                case 14: sbr_ = (name == "sbrV") ? sbr_sse2_16<16, 8192, 0x20002000, 0> : sbr_sse2_16<16, 8192, 0x20002000, 1>; break;
                default: sbr_ = (name == "sbrV") ? sbr_sse2_16<64, 32768, 0x80008000, 0> : sbr_sse2_16<64, 32768, 0x80008000, 1>; break;
            }
        }
    }
    else
    {
        pb_pitch = (vi.width + 15) & ~15;

        if (sizeof(T) == 1)
            sbr_ = (name == "sbrV") ? sbr_c<T, 2, 255, 128, 0> : sbr_c<T, 8, 255, 128, 1>;
        else
        {
            switch (vi.BitsPerComponent())
            {
                case 10: sbr_ = (name == "sbrV") ? sbr_c<T, 3, 1023, 512, 0> : sbr_c<T, 3, 1023, 512, 1>; break;
                case 12: sbr_ = (name == "sbrV") ? sbr_c<T, 4, 4095, 2048, 0> : sbr_c<T, 4, 4095, 2048, 1>; break;
                case 14: sbr_ = (name == "sbrV") ? sbr_c<T, 16, 16383, 8192, 0> : sbr_c<T, 16, 16383, 8192, 1>; break;
                default: sbr_ = (name == "sbrV") ? sbr_c<T, 64, 65535, 32768, 0> : sbr_c<T, 64, 65535, 32768, 1>; break;
            }
        }
    }

    buffer = std::make_unique<T[]>(vi.height * pb_pitch * 2 * sizeof(T));

    try { env->CheckVersion(8); }
    catch (const AvisynthError&) { v8 = false; }
}

template <typename T>
PVideoFrame __stdcall sbr<T>::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame src{ child->GetFrame(n, env) };
    PVideoFrame dst{ (v8) ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi) };

    const int planes[3]{ PLANAR_Y, PLANAR_U, PLANAR_V };

    for (int pid{ 0 }; pid < 3; ++pid)
    {
        const int height{ src->GetHeight(planes[pid]) };
        const uint8_t* srcp{ src->GetReadPtr(planes[pid]) };
        uint8_t* dstp{ dst->GetWritePtr(planes[pid]) };

        if (process[pid] == 2)
            env->BitBlt(dstp, dst->GetPitch(planes[pid]), srcp, src->GetPitch(planes[pid]), src->GetRowSize(planes[pid]), height);
        else
        {
            const size_t src_pitch{ src->GetPitch(planes[pid]) / sizeof(T) };
            const size_t dst_pitch{ dst->GetPitch(planes[pid]) / sizeof(T) };
            const size_t width{ src->GetRowSize(planes[pid]) / sizeof(T) };

            sbr_(dstp, buffer.get(), srcp, dst_pitch, pb_pitch, src_pitch, width, height);
        }
    }

    return dst;
}

AVSValue __cdecl Create_sbrV(AVSValue args, void*, IScriptEnvironment* env)
{
    enum { CLIP, Y, U, V, OPT };
    PClip clip = args[CLIP].AsClip();

    switch (clip->GetVideoInfo().ComponentSize())
    {
        case 1: return new sbr<uint8_t>(clip, args[Y].AsInt(3), args[U].AsInt(2), args[V].AsInt(2), args[OPT].AsInt(-1), "sbrV", env);
        case 2: return new sbr<uint16_t>(clip, args[Y].AsInt(3), args[U].AsInt(2), args[V].AsInt(2), args[OPT].AsInt(-1), "sbrV", env);
        default: env->ThrowError("sbrV: only 8..16-bit input is supported!");
    }
}

AVSValue __cdecl Create_sbr(AVSValue args, void*, IScriptEnvironment* env)
{
    enum { CLIP, Y, U, V, OPT };
    PClip clip = args[CLIP].AsClip();

    switch (clip->GetVideoInfo().ComponentSize())
    {
        case 1: return new sbr<uint8_t>(clip, args[Y].AsInt(3), args[U].AsInt(2), args[V].AsInt(2), args[OPT].AsInt(-1), "sbr", env);
        case 2: return new sbr<uint16_t>(clip, args[Y].AsInt(3), args[U].AsInt(2), args[V].AsInt(2), args[OPT].AsInt(-1), "sbr", env);
        default: env->ThrowError("sbrV: only 8..16-bit input is supported!");
    }
}

const AVS_Linkage* AVS_linkage = nullptr;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("sbrV", "c[y]i[u]i[v]i[opt]i", Create_sbrV, 0);
    env->AddFunction("sbr", "c[y]i[u]i[v]i[opt]i", Create_sbr, 0);
    return "sbrVS?";
}
