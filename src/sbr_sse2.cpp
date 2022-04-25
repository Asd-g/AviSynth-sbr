#include "sbr.h"
#include "VCL2/vectorclass.h"

static void vertical_blur_sse2_8(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint8_t* srcp{ reinterpret_cast<const uint8_t*>(srcp_) };
    uint8_t* __restrict dstp{ reinterpret_cast<uint8_t*>(dstp_) };

    const auto two{ Vec8us(2) };

    for (int y{ 0 }; y < height; ++y)
    {
        const uint8_t* srcpp{ (y == 0) ? srcp + src_pitch : srcp - src_pitch };
        const uint8_t* srcpn{ (y == height - 1) ? srcp - src_pitch : srcp + src_pitch };

        for (int x{ 0 }; x < width; x += 16)
        {
            const auto p{ Vec16uc().load(srcpp + x) };
            const auto c{ Vec16uc().load(srcp + x) };
            const auto n{ Vec16uc().load(srcpn + x) };

            const auto p_lo{ extend_low(p) };
            const auto p_hi{ extend_high(p) };
            const auto c_lo{ extend_low(c) };
            const auto c_hi{ extend_high(c) };
            const auto n_lo{ extend_low(n) };
            const auto n_hi{ extend_high(n) };

            auto acc_lo{ c_lo + p_lo };
            auto acc_hi{ c_hi + p_hi };

            acc_lo = acc_lo + c_lo;
            acc_hi = acc_hi + c_hi;

            acc_lo = acc_lo + n_lo;
            acc_hi = acc_hi + n_hi;

            acc_lo = acc_lo + two;
            acc_hi = acc_hi + two;

            acc_lo = acc_lo >> 2;
            acc_hi = acc_hi >> 2;

            compress_saturated(acc_lo, acc_hi).store(dstp + x);
        }

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

static void blur_sse2_8(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint8_t* srcp{ reinterpret_cast<const uint8_t*>(srcp_) };
    uint8_t* __restrict dstp{ reinterpret_cast<uint8_t*>(dstp_) };

    for (int y{ 0 }; y < height; ++y)
    {
        const uint8_t* srcpp{ (y == 0) ? srcp + src_pitch : srcp - src_pitch };
        const uint8_t* srcpn{ (y == height - 1) ? srcp - src_pitch : srcp + src_pitch };

        dstp[0] = srcp[0];

        for (int x{ 1 }; x < width - 1; x += 16)
        {
            const auto a1{ Vec16uc().load(srcpp + x - 1) };
            const auto a2{ Vec16uc().load(srcpp + x) };
            const auto a3{ Vec16uc().load(srcpp + x + 1) };
            const auto a4{ Vec16uc().load(srcp + x - 1) };
            const auto a5{ Vec16uc().load(srcp + x) };
            const auto a6{ Vec16uc().load(srcp + x + 1) };
            const auto a7{ Vec16uc().load(srcpn + x - 1) };
            const auto a8{ Vec16uc().load(srcpn + x) };
            const auto a9{ Vec16uc().load(srcpn + x + 1) };

            const auto a1_lo{ extend_low(a1) };
            const auto a2_lo{ extend_low(a2) };
            const auto a3_lo{ extend_low(a3) };
            const auto a4_lo{ extend_low(a4) };
            const auto a5_lo{ extend_low(a5) };
            const auto a6_lo{ extend_low(a6) };
            const auto a7_lo{ extend_low(a7) };
            const auto a8_lo{ extend_low(a8) };
            const auto a9_lo{ extend_low(a9) };

            const auto result_lo{ (a1_lo + a3_lo + a7_lo + a9_lo + ((a2_lo + a4_lo + a6_lo + a8_lo) << 1) + (a5_lo << 2) + Vec8us(8)) >> 4 };
            //
            const auto a1_hi{ extend_high(a1) };
            const auto a2_hi{ extend_high(a2) };
            const auto a3_hi{ extend_high(a3) };
            const auto a4_hi{ extend_high(a4) };
            const auto a5_hi{ extend_high(a5) };
            const auto a6_hi{ extend_high(a6) };
            const auto a7_hi{ extend_high(a7) };
            const auto a8_hi{ extend_high(a8) };
            const auto a9_hi{ extend_high(a9) };

            const auto result_hi{ (a1_hi + a3_hi + a7_hi + a9_hi + ((a2_hi + a4_hi + a6_hi + a8_hi) << 1) + (a5_hi << 2) + Vec8us(8)) >> 4 };
            //
            compress_saturated(result_lo, result_hi).store(dstp + x);
        }

        dstp[width - 1] = srcp[width - 1];

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

static void mt_makediff_sse2_8(void* __restrict dstp_, const void* c1p_, const void* c2p_, int dst_pitch, int c1_pitch, int c2_pitch, int width, int height) noexcept
{
    const uint8_t* c1p{ reinterpret_cast<const uint8_t*>(c1p_) };
    const uint8_t* c2p{ reinterpret_cast<const uint8_t*>(c2p_) };
    uint8_t* __restrict dstp{ reinterpret_cast<uint8_t*>(dstp_) };

    const auto v128{ Vec16uc(0x80808080) };

    for (int y{ 0 }; y < height; ++y)
    {
        for (int x{ 0 }; x < width; x += 16)
        {
            auto c1{ Vec16uc().load(c1p + x) };
            auto c2{ Vec16uc().load(c2p + x) };

            c1 = c1 - v128;
            c2 = c2 - v128;

            auto diff{ c1 - c2 };
            diff = diff + v128;
            diff.store(dstp + x);
        }

        dstp += dst_pitch;
        c1p += c1_pitch;
        c2p += c2_pitch;
    }
}

template <int name>
void sbr_sse2_8(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept
{
    if constexpr (name == 0)
    {
        vertical_blur_sse2_8(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
        mt_makediff_sse2_8(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
        vertical_blur_sse2_8(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.vblur()
    }
    else
    {
        blur_sse2_8(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
        mt_makediff_sse2_8(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
        blur_sse2_8(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.blur()
    }

    const uint8_t* srcp{ reinterpret_cast<const uint8_t*>(srcp_) };
    uint8_t* __restrict tempp{ reinterpret_cast<uint8_t*>(tempp_) };
    uint8_t* __restrict dstp{ reinterpret_cast<uint8_t*>(dstp_) };

    const Vec8us zero{ zero_si128() };
    const auto v128{ Vec8us(128) };

    for (int y{ 0 }; y < height; ++y)
    {
        for (int x{ 0 }; x < width; x += 8)
        {
            auto dst{ extend_low(Vec16uc().loadl(dstp + x)) };
            auto temp{ extend_low(Vec16uc().loadl(tempp + x)) };
            auto src{ extend_low(Vec16uc().loadl(srcp + x)) };

            auto t{ dst - temp };
            auto t2{ dst - v128 };

            auto nochange_mask{ Vec8s(t * t2) < zero };

            auto t_mask{ abs(t) < abs(t2) };
            auto desired{ src - t };
            auto otherwise{ (src - dst) + v128 };
            auto result{ select(nochange_mask, src, select(t_mask, desired, otherwise)) };

            compress_saturated(result, zero).storel(dstp + x);
        }

        dstp += dst_pitch;
        srcp += src_pitch;
        tempp += temp_pitch;
    }
}

template void sbr_sse2_8<0>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void sbr_sse2_8<1>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;

template <int c_>
static void vertical_blur_sse2_16(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint16_t* srcp{ reinterpret_cast<const uint16_t*>(srcp_) };
    uint16_t* __restrict dstp{ reinterpret_cast<uint16_t*>(dstp_) };

    const auto two{ Vec4ui(c_) };

    for (int y{ 0 }; y < height; ++y)
    {
        const uint16_t* srcpp{ (y == 0) ? srcp + src_pitch : srcp - src_pitch };
        const uint16_t* srcpn{ (y == height - 1) ? srcp - src_pitch : srcp + src_pitch };

        for (int x{ 0 }; x < width; x += 8)
        {
            auto p{ Vec8us().load(srcpp + x) };
            auto c{ Vec8us().load(srcp + x) };
            auto n{ Vec8us().load(srcpn + x) };

            auto p_lo{ extend_low(p) };
            auto p_hi{ extend_high(p) };
            auto c_lo{ extend_low(c) };
            auto c_hi{ extend_high(c) };
            auto n_lo{ extend_low(n) };
            auto n_hi{ extend_high(n) };

            auto acc_lo{ c_lo + p_lo };
            auto acc_hi{ c_hi + p_hi };

            acc_lo = acc_lo + c_lo;
            acc_hi = acc_hi + c_hi;

            acc_lo = acc_lo + n_lo;
            acc_hi = acc_hi + n_hi;

            acc_lo = acc_lo + two;
            acc_hi = acc_hi + two;

            acc_lo = acc_lo >> 2;
            acc_hi = acc_hi >> 2;

            compress_saturated(acc_lo, acc_hi).store(dstp + x);
        }

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

static void blur_sse2_16(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint16_t* srcp{ reinterpret_cast<const uint16_t*>(srcp_) };
    uint16_t* __restrict dstp{ reinterpret_cast<uint16_t*>(dstp_) };

    for (int y{ 0 }; y < height; ++y)
    {
        const uint16_t* srcpp{ (y == 0) ? srcp + src_pitch : srcp - src_pitch };
        const uint16_t* srcpn{ (y == height - 1) ? srcp - src_pitch : srcp + src_pitch };

        dstp[0] = srcp[0];

        for (int x{ 1 }; x < width - 1; x += 8)
        {
            const auto a1{ Vec8us().load(srcpp + x - 1) };
            const auto a2{ Vec8us().load(srcpp + x) };
            const auto a3{ Vec8us().load(srcpp + x + 1) };
            const auto a4{ Vec8us().load(srcp + x - 1) };
            const auto a5{ Vec8us().load(srcp + x) };
            const auto a6{ Vec8us().load(srcp + x + 1) };
            const auto a7{ Vec8us().load(srcpn + x - 1) };
            const auto a8{ Vec8us().load(srcpn + x) };
            const auto a9{ Vec8us().load(srcpn + x + 1) };

            const auto a1_lo{ extend_low(a1) };
            const auto a2_lo{ extend_low(a2) };
            const auto a3_lo{ extend_low(a3) };
            const auto a4_lo{ extend_low(a4) };
            const auto a5_lo{ extend_low(a5) };
            const auto a6_lo{ extend_low(a6) };
            const auto a7_lo{ extend_low(a7) };
            const auto a8_lo{ extend_low(a8) };
            const auto a9_lo{ extend_low(a9) };

            const auto result_lo{ (a1_lo + a3_lo + a7_lo + a9_lo + ((a2_lo + a4_lo + a6_lo + a8_lo) << 1) + (a5_lo << 2) + Vec4ui(8)) >> 4 };
            //
            const auto a1_hi{ extend_high(a1) };
            const auto a2_hi{ extend_high(a2) };
            const auto a3_hi{ extend_high(a3) };
            const auto a4_hi{ extend_high(a4) };
            const auto a5_hi{ extend_high(a5) };
            const auto a6_hi{ extend_high(a6) };
            const auto a7_hi{ extend_high(a7) };
            const auto a8_hi{ extend_high(a8) };
            const auto a9_hi{ extend_high(a9) };

            const auto result_hi{ (a1_hi + a3_hi + a7_hi + a9_hi + ((a2_hi + a4_hi + a6_hi + a8_hi) << 1) + (a5_hi << 2) + Vec4ui(8)) >> 4 };
            //
            compress_saturated(result_lo, result_hi).store(dstp + x);
        }

        dstp[width - 1] = srcp[width - 1];

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

template <uint32_t u>
static void mt_makediff_sse2_16(void* __restrict dstp_, const void* c1p_, const void* c2p_, int dst_pitch, int c1_pitch, int c2_pitch, int width, int height) noexcept
{
    const uint16_t* c1p{ reinterpret_cast<const uint16_t*>(c1p_) };
    const uint16_t* c2p{ reinterpret_cast<const uint16_t*>(c2p_) };
    uint16_t* __restrict dstp{ reinterpret_cast<uint16_t*>(dstp_) };

    const auto v128{ Vec8us(u) };

    for (int y{ 0 }; y < height; ++y)
    {
        for (int x{ 0 }; x < width; x += 8)
        {
            auto c1{ Vec8us().load(c1p + x) };
            auto c2{ Vec8us().load(c2p + x) };

            c1 = c1 - v128;
            c2 = c2 - v128;

            auto diff{ c1 - c2 };
            diff = diff + v128;
            diff.store(dstp + x);
        }

        dstp += dst_pitch;
        c1p += c1_pitch;
        c2p += c2_pitch;
    }
}

template <int c, int h, uint32_t u, int name>
void sbr_sse2_16(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept
{
    if constexpr (name == 0)
    {
        vertical_blur_sse2_16<c>(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
        mt_makediff_sse2_16<u>(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
        vertical_blur_sse2_16<c>(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.vblur()
    }
    else
    {
        blur_sse2_16(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
        mt_makediff_sse2_16<u>(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
        blur_sse2_16(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.blur()
    }

    const uint16_t* srcp{ reinterpret_cast<const uint16_t*>(srcp_) };
    uint16_t* __restrict tempp{ reinterpret_cast<uint16_t*>(tempp_) };
    uint16_t* __restrict dstp{ reinterpret_cast<uint16_t*>(dstp_) };

    const Vec4ui zero{ zero_si128() };
    const auto v128{ Vec4ui(h) };

    for (int y{ 0 }; y < height; ++y)
    {
        for (int x{ 0 }; x < width; x += 4)
        {
            auto dst{ extend_low(Vec8us().load(dstp + x)) };
            auto temp{ extend_low(Vec8us().load(tempp + x)) };
            auto src{ extend_low(Vec8us().load(srcp + x)) };

            auto t{ dst - temp };
            auto t2{ dst - v128 };

            auto nochange_mask{ Vec4i(t * t2) < zero };

            auto t_mask{ abs(t) < abs(t2) };
            auto desired{ src - t };
            auto otherwise{ (src - dst) + v128 };
            auto result{ select(nochange_mask, src, select(t_mask, desired, otherwise)) };

            compress_saturated(result, zero).storel(dstp + x);
        }

        dstp += dst_pitch;
        srcp += src_pitch;
        tempp += temp_pitch;
    }
}

template void sbr_sse2_16<3, 512, 0x200200, 0>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void sbr_sse2_16<4, 2048, 0x800800, 0>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void sbr_sse2_16<16, 8192, 0x20002000, 0>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void sbr_sse2_16<64, 32768, 0x80008000, 0>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;

template void sbr_sse2_16<3, 512, 0x200200, 1>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void sbr_sse2_16<4, 2048, 0x800800, 1>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void sbr_sse2_16<16, 8192, 0x20002000, 1>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void sbr_sse2_16<64, 32768, 0x80008000, 1>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
