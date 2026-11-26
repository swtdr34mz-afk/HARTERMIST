/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
/* HARTERMIST's building blocks: small helpers (from TREEGONE's), the polyphase half-band for the oversampling, and the
 * Artemis oscillator: one wave that morphs continuously from sawtooth through square to triangle, with a pulse width
 * that narrows the whole wave into the front of its cycle (to silence at the end, as the Artemis' PW does), its
 * corners smoothed by polynomial BLEPs and BLAMPs. Those are a function of where the phase is, not of which way it
 * moves: so the oscillator stays clean under through-zero FM, its phase running backwards. */
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace hm
{
constexpr float PI = 3.14159265358979f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
inline float minf(float a, float b) { return a < b ? a : b; }
inline float maxf(float a, float b) { return a > b ? a : b; }
/* 2^x, fast (a polynomial: 2e-7 off), for what runs per sample */
inline float fexp2(float x)
{
    if (x < -126.0f)
        return 0.0f;
    if (x > 127.0f)
        x = 127.0f;
    float fl = std::floor(x), f = x - fl;
    float p = 1.534580891e-04f;
    p = p * f + 1.339993158e-03f;
    p = p * f + 9.618488972e-03f;
    p = p * f + 5.550328776e-02f;
    p = p * f + 2.402264689e-01f;
    p = p * f + 6.931472057e-01f;
    p = p * f + 1.0f;
    int32_t e = ((int32_t)fl + 127) << 23;          /* (2^fl from its exponent bits: no ldexp) */
    float m;
    std::memcpy(&m, &e, 4);
    return p * m;
}
/* a gentle tanh (Pade, flat at +-3) */
inline float softT(float x)
{
    if (x > 3.0f)
        return 1.0f;
    if (x < -3.0f)
        return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
/* a transconductor's curve: L tanh(x / L) */
inline float ota(float x)
{
    const float L = 1.25f;
    return L * softT(x * (1.0f / L));
}
/* the filter's input stage: a soft clip without a division (flat at +-1.5, where it reaches 1) */
inline float sat(float x)
{
    x = clampf(x, -1.5f, 1.5f);
    return x - 0.148148f * x * x * x;
}
inline float hzOf(float midi) { return 440.0f * std::exp2((midi - 69.0f) * (1.0f / 12.0f)); }
inline float flushDn(float x) { return std::fabs(x) < 1e-15f ? 0.0f : x; }
inline float tanW(float w)                          /* tan(w) for the prewarp, w < ~1.2 */
{
    float w2 = w * w;
    return w * (1.0f + w2 * (0.33333f + w2 * (0.13333f + w2 * 0.05397f)));
}
inline uint32_t rndU(uint32_t &s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}
inline float rndF(uint32_t &s) { return (float)(int32_t)rndU(s) * (1.0f / 2147483648.0f); }   /* -1..1 */
inline float rnd01(uint32_t &s) { return (float)(rndU(s) >> 8) * (1.0f / 16777216.0f); }       /* 0..1 */

/* ----------------------------------------------------------- the half-band --- */
/* a polyphase IIR half-band (two paths of four first-order allpasses; de Soras' elliptic design: -91 dB) */
static const float HB_A0[4] = {0.0472898531f, 0.3385179585f, 0.6552730688f, 0.8745126132f};
static const float HB_A1[4] = {0.1727806110f, 0.5069776792f, 0.7764899192f, 0.9589457184f};
inline float hbAp4(float x, float *t, const float *a)
{
    float y0 = a[0] * (x - t[1]) + t[0], y1 = a[1] * (y0 - t[2]) + t[1], y2 = a[2] * (y1 - t[3]) + t[2],
          y3 = a[3] * (y2 - t[4]) + t[3];
    t[0] = x;
    t[1] = y0;
    t[2] = y1;
    t[3] = y2;
    t[4] = y3;
    return y3;
}
struct HalfBand {
    float u0[5] = {}, u1[5] = {}, d0[5] = {}, d1[5] = {}, dp = 0.0f;
    inline void up(float x, float &a, float &b)
    {
        a = hbAp4(x, u0, HB_A0);
        b = hbAp4(x, u1, HB_A1);
    }
    inline float down(float a, float b)
    {
        float y = 0.5f * (hbAp4(a, d0, HB_A0) + hbAp4(dp, d1, HB_A1));
        dp = b;
        return y;
    }
    void clear() { *this = HalfBand(); }
    void flush()
    {
        for (int i = 0; i < 5; i++) {
            u0[i] = flushDn(u0[i]);
            u1[i] = flushDn(u1[i]);
            d0[i] = flushDn(d0[i]);
            d1[i] = flushDn(d1[i]);
        }
        dp = flushDn(dp);
    }
};

/* ----------------------------------------------------------- the polynomial BLEP / BLAMP --- */
/* the residuals of a unit step and of a unit change of slope (per sample), tau samples after it (-1..1) */
inline float blepR(float tau) { return tau < 0.0f ? 0.5f * (tau + 1.0f) * (tau + 1.0f) : -0.5f * (1.0f - tau) * (1.0f - tau); }
inline float blampR(float tau)
{
    float u = tau < 0.0f ? tau + 1.0f : 1.0f - tau;
    return u * u * u * (1.0f / 6.0f);
}
/* the distance (samples, -1..1) from a corner at phase e to the phase ph, adt the increment's size; 2 if far */
inline float tauOf(float ph, float e, float adt)
{
    float t = ph - e;                                 /* (both 0..1) */
    if (t < 0.0f)
        t += 1.0f;
    if (t < adt)
        return t / adt;
    if (t > 1.0f - adt)
        return (t - 1.0f) / adt;
    return 2.0f;
}

/* ----------------------------------------------------------- the Artemis wave --- */
/* m: 0 sawtooth, 0.5 square, 1 triangle (between them both, crossfaded); d: the duty (1: the whole cycle; toward 0
 * the wave squeezed into the start of its cycle, the rest silent). Its pieces: the saw falls 1..-1 (so that its
 * fundamental and the square's add, not cancel, on the way between them), the square +1 then -1, the triangle -1 up to +1 at the middle and down: all over 0..d of the phase */
struct Shape {
    float ws = 1.0f, wq = 0.0f, wt = 0.0f, d = 1.0f;
    void set(float m, float duty)
    {
        m = clampf(m, 0.0f, 1.0f);
        if (m < 0.5f) {
            ws = 1.0f - 2.0f * m;
            wq = 2.0f * m;
            wt = 0.0f;
        } else {
            ws = 0.0f;
            wq = 2.0f - 2.0f * m;
            wt = 2.0f * m - 1.0f;
        }
        d = clampf(duty, 0.004f, 1.0f);
    }
    /* the naive value at a phase (0..1) */
    inline float at(float ph) const
    {
        if (ph >= d)
            return 0.0f;
        float u = ph / d;
        float y = ws * (1.0f - 2.0f * u) + wq * (u < 0.5f ? 1.0f : -1.0f);
        if (wt > 0.0f)
            y += wt * (u < 0.5f ? 4.0f * u - 1.0f : 3.0f - 4.0f * u);
        return y;
    }
    /* the value with its corners smoothed, adt the phase's increment (its size: either direction) */
    inline float tick(float ph, float adt) const
    {
        float y = at(ph);
        if (adt <= 0.0f || adt >= 0.5f)
            return y;
        const bool full = d >= 0.999f;
        /* the steps: at 0 (silence, or the end of the cycle, to the start), at d/2 (the square's drop), at d (to
         * silence) */
        float v0 = ws + wq - wt;                          /* the value at the start */
        float vEnd = -ws - wq - wt;                       /* just before d */
        float t;
        if ((t = tauOf(ph, 0.0f, adt)) < 1.5f)
            y += (v0 - (full ? vEnd : 0.0f)) * blepR(t);
        if (!full && (t = tauOf(ph, d, adt)) < 1.5f)
            y += (0.0f - vEnd) * blepR(t);
        if (wq > 0.0f && (t = tauOf(ph, 0.5f * d, adt)) < 1.5f)
            y += -2.0f * wq * blepR(t);
        /* the corners: the saw's slope (-2/d) and the triangle's (+-4/d) start and end with the wave */
        float sS = -ws * 2.0f / d, sT = wt * 4.0f / d;
        if (sS != 0.0f || sT != 0.0f) {
            float inSlope = sS + sT, outSlope = sS - sT;   /* at the start, at the end */
            if ((t = tauOf(ph, 0.0f, adt)) < 1.5f)
                y += (inSlope - (full ? outSlope : 0.0f)) * adt * blampR(t);
            if (!full && (t = tauOf(ph, d, adt)) < 1.5f)
                y += (0.0f - outSlope) * adt * blampR(t);
            if (wt > 0.0f && (t = tauOf(ph, 0.5f * d, adt)) < 1.5f)
                y += (-2.0f * sT) * adt * blampR(t);
        }
        return y;
    }
};
} // namespace hm
