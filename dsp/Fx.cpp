/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
#include "Fx.h"
#include <algorithm>

namespace hm
{
/* ============================================================================== the page === */
void FxPageBase::process(float *L, float *R, int n)
{
    const float kp = 1.0f - std::exp(-1.0f / (0.025f * sr));
    const float kx = 1.0f / (0.012f * sr);
    const bool echo = dynamic_cast<Delay *>(this) || dynamic_cast<Reverb *>(this);
    if (idle()) {
        for (int i = 0; i < 5; i++)
            sP[i] = tP[i];
        type = want;
        xf = 1.0f;
        return;
    }
    if (sP[4] < 1e-4f && tP[4] > 0.0f)                     /* back from out of the way: from silence */
        clearState();
    for (int i = 0; i < n; i++) {
        if (want != type) {
            xf -= kx;
            if (xf <= 0.0f) {
                xf = 0.0f;
                type = want;
                clearState();
            }
        } else if (xf < 1.0f)
            xf = minf(1.0f, xf + kx);
        for (int k = 0; k < 5; k++)
            sP[k] += (tP[k] - sP[k]) * kp;
        float x0 = L[i], x1 = R[i], wl = 0.0f, wr = 0.0f;
        run(x0, x1, wl, wr);
        float m = sP[4] * xf;
        float gd, gw;
        if (echo) {                                         /* (the dry full until the middle, as a send) */
            gd = minf(1.0f, 2.0f * (1.0f - m));
            gw = minf(1.0f, 2.0f * m);
        } else {
            gd = 1.0f - m;
            gw = m;
        }
        L[i] = x0 * gd + wl * gw;
        R[i] = x1 * gd + wr * gw;
    }
    endBlock();
}

/* ============================================================================== DIST === */
void Dist::clearState()
{
    hb[0].clear();
    hb[1].clear();
    hold[0] = hold[1] = 0.0f;
    holdN = 0.0f;
    dcx[0] = dcx[1] = dcy[0] = dcy[1] = 0.0f;
}

float Dist::shape(float x) const
{
    const float a = std::fabs(x), sg = x < 0.0f ? -1.0f : 1.0f;
    switch (type) {
    case DT_EXP: return sg * (1.0f - std::exp(-a));
    case DT_PARABOLIC: {
        float c = minf(a, 2.0f);
        return sg * (c - c * c * 0.25f);
    }
    case DT_SINECLIP: return a < PI * 0.5f ? std::sin(x) : sg;
    case DT_SCURVE: {
        float c = clampf(x, -1.0f, 1.0f);
        return 1.5f * c - 0.5f * c * c * c;
    }
    case DT_SOFTCLIP: return std::tanh(x);
    case DT_HARDCLIP: return clampf(x, -1.0f, 1.0f);
    case DT_TRICLIP: return sg * (a < 1.0f ? a : a < 2.0f ? 1.0f - 0.5f * (a - 1.0f) : 0.5f);
    case DT_TRIFOLD: {
        float p = x * 0.25f + 0.25f;
        p -= std::floor(p);
        return 1.0f - 4.0f * std::fabs(p - 0.5f);
    }
    case DT_SINGLEFOLD: return a <= 1.0f ? x : a <= 3.0f ? sg * (2.0f - a) : -sg;
    case DT_MULTIFOLD: {
        auto f1 = [](float v) {
            float b = std::fabs(v), s = v < 0.0f ? -1.0f : 1.0f;
            return b <= 1.0f ? v : b <= 3.0f ? s * (2.0f - b) : -s;
        };
        return softT(1.2f * f1(1.8f * f1(x)));
    }
    case DT_SINEBEND: return std::sin(x * (1.0f + 0.5f * a));
    case DT_SINEFOLD: return std::sin(x * PI * 0.5f);
    case DT_SINESHRED: return 0.75f * std::sin(x * PI * 0.5f) + 0.35f * std::sin(x * PI * 4.5f) * minf(1.0f, a);
    case DT_BINSHRED: {
        int i = (int)(clampf(x * 0.5f, -1.0f, 1.0f) * 32767.0f);
        int16_t s = (int16_t)(i ^ (i << 1) ^ (i >> 3));
        return (float)s * (1.0f / 32768.0f);
    }
    case DT_SYMWARP: return std::tanh(x + 0.6f * x * a * 0.5f + 0.25f * a);
    default: return x;
    }
}

void Dist::run(float xl, float xr, float &yl, float &yr)
{
    const float g = std::pow(10.0f, sP[2] * 2.0f);          /* GAIN: +0 .. +40 dB */
    const float lev = 2.0f * sP[3] * std::pow(10.0f, -sP[2] * 0.35f);   /* LEVEL (a little made up for the gain) */
    float in[2] = {xl, xr}, out[2];
    for (int c = 0; c < 2; c++) {
        float a, b;
        hb[c].up(in[c] * g, a, b);
        out[c] = hb[c].down(shape(a), shape(b));
    }
    /* RATE: the sample rate brought down (held); BITS: the depth */
    if (sP[1] > 0.002f) {
        float N = 1.0f + sP[1] * sP[1] * 63.0f;
        holdN += 1.0f;
        if (holdN >= N) {
            holdN -= N;
            hold[0] = out[0];
            hold[1] = out[1];
        }
        out[0] = hold[0];
        out[1] = hold[1];
    }
    if (sP[0] > 0.002f) {
        float q = std::exp2(15.0f - 14.0f * sP[0]);
        out[0] = std::round(out[0] * q) / q;
        out[1] = std::round(out[1] * q) / q;
    }
    /* the DC the uneven curves leave, out (5 Hz) */
    const float k = 1.0f - 2.0f * PI * 5.0f * isr;
    for (int c = 0; c < 2; c++) {
        float y = out[c] - dcx[c] + k * dcy[c];
        dcx[c] = out[c];
        dcy[c] = flushDn(y);
        out[c] = y * lev;
    }
    yl = out[0];
    yr = out[1];
}

/* ============================================================================== MOD === */
void Mod::alloc()
{
    for (int c = 0; c < 2; c++) {
        ln[c].init((int)(sr * 0.11f));
        ln2[c].init((int)(sr * 0.11f));
    }
}
void Mod::clearState()
{
    for (int c = 0; c < 2; c++) {
        ln[c].clear();
        ln2[c].clear();
        fb[c] = lp[c] = lp2[c] = wob[c] = wobT[c] = 0.0f;
        psPh[c] = 0.0f;
        dmp[c] = Damp();
    }
    std::memset(ap, 0, sizeof ap);
    std::memset(apFb, 0, sizeof apFb);
    std::memset(nz, 0, sizeof nz);
}

float Mod::phaser(int c, int k, float x, float fc, float fbk)
{
    float t = tanW(clampf(PI * fc * isr, 0.0f, 1.2f)), a = (t - 1.0f) / (t + 1.0f);
    float v = x + apFb[c][k] * fbk;
    for (int s = 0; s < 6; s++) {
        float y = a * v + ap[c][k][s];
        ap[c][k][s] = flushDn(v - a * y);
        v = y;
    }
    apFb[c][k] = softT(v);
    return v;
}

void Mod::run(float xl, float xr, float &yl, float &yr)
{
    const float dp = sP[0], spd = sP[1], fd = sP[2], wd = sP[3];
    const float rate = 0.02f + spd * spd * 10.0f;
    ph += rate * isr;
    if (ph >= 1.0f)
        ph -= 1.0f;
    const float x[2] = {xl, xr};
    float y[2] = {};
    auto sinp = [](float p) { return std::sin(2.0f * PI * p); };
    switch (type) {
    case MT_CHORUS:
    case MT_BBDCHORUS: {
        bool bbd = type == MT_BBDCHORUS;
        for (int c = 0; c < 2; c++) {
            float m = 0.5f + 0.5f * sinp(ph + (c ? wd * 0.5f : 0.0f));
            float d = (0.006f + dp * 0.012f * m) * sr;
            float v = ln[c].rd(d);
            if (bbd) {
                lp[c] += (v - lp[c]) * (1.0f - std::exp(-2.0f * PI * 5200.0f * isr));
                v = softT(lp[c] * 1.1f) + 3e-5f * rndF(seed);
            }
            ln[c].push(x[c] + v * fd * 0.75f);
            y[c] = v;
        }
        break;
    }
    case MT_ENSEMBLE: {
        ph2 += 5.8f * isr;
        if (ph2 >= 1.0f)
            ph2 -= 1.0f;
        for (int c = 0; c < 2; c++) {
            float s = 0.0f;
            for (int k = 0; k < 3; k++) {
                float o = (float)k / 3.0f + (c ? wd * 0.5f : 0.0f);
                float m = 0.5f + 0.5f * sinp(ph + o), m2 = sinp(ph2 + o);
                s += ln[c].rd((0.010f + dp * 0.006f * m + 0.0004f * m2) * sr);
            }
            s *= 0.42f;
            ln[c].push(x[c] + s * fd * 0.6f);
            y[c] = s;
        }
        break;
    }
    case MT_TAPE: {
        /* wow and flutter: a slow random wander and a quicker small one */
        if (--wobN <= 0) {
            wobT[0] = rndF(seed);
            wobT[1] = rndF(seed);
            wobN = (int)(sr * (0.08f + 0.25f * (1.0f - spd)));
        }
        for (int c = 0; c < 2; c++) {
            wob[c] += (wobT[c] - wob[c]) * (1.0f - std::exp(-2.0f * PI * (0.8f + 4.0f * spd) * isr));
            float m = 0.6f * sinp(ph + (c ? wd * 0.5f : 0.0f)) + 0.4f * (c ? wob[0] + (wob[1] - wob[0]) * wd : wob[0]);
            float d = (0.012f + dp * 0.010f * (0.5f + 0.5f * m)) * sr;
            float v = ln[c].rd(d);
            lp[c] += (v - lp[c]) * (1.0f - std::exp(-2.0f * PI * 9000.0f * isr));
            v = softT(lp[c] * 1.2f) / 1.15f;
            ln[c].push(x[c] + v * fd * 0.7f);
            y[c] = v;
        }
        break;
    }
    case MT_FLANGER:
    case MT_BBDFLANGER: {
        bool bbd = type == MT_BBDFLANGER;
        float f = (fd - 0.5f) * 2.0f * 0.95f;              /* positive or negative feedback */
        for (int c = 0; c < 2; c++) {
            float m = 0.5f + 0.5f * sinp(ph + (c ? wd * 0.5f : 0.0f));
            float d = (0.00025f + dp * 0.0065f * m) * sr;
            float v = ln[c].rd(maxf(d, 1.0f));
            if (bbd) {
                lp[c] += (v - lp[c]) * (1.0f - std::exp(-2.0f * PI * 6000.0f * isr));
                v = softT(lp[c]);
            }
            ln[c].push(softT(x[c] + v * f));
            y[c] = 0.5f * (x[c] + v);
        }
        break;
    }
    case MT_TZFLANGER: {
        /* two delays: one still at the middle, the other swinging through it */
        float f = (fd - 0.5f) * 2.0f * 0.9f;
        float c0 = 0.0035f * sr;
        for (int c = 0; c < 2; c++) {
            float m = sinp(ph + (c ? wd * 0.5f : 0.0f));
            float a = ln[c].rd(c0), b = ln2[c].rd(maxf(1.0f, c0 * (1.0f + dp * 0.95f * m)));
            float sgn = f < 0.0f ? -1.0f : 1.0f;
            ln[c].push(x[c]);
            ln2[c].push(softT(x[c] + fb[c] * std::fabs(f)));
            float o = 0.5f * (a + sgn * b);
            fb[c] = o;
            y[c] = o;
        }
        break;
    }
    case MT_PHASER: {
        for (int c = 0; c < 2; c++) {
            float m = 0.5f + 0.5f * sinp(ph + (c ? wd * 0.5f : 0.0f));
            float fc = 180.0f * std::exp2(dp * 6.0f * m + (1.0f - dp) * 2.0f);
            float v = phaser(c, 0, x[c], fc, fd * 0.92f);
            y[c] = 0.5f * (x[c] + v);
        }
        break;
    }
    case MT_BARBER: {
        /* four phasers sweeping up, a quarter apart, each faded in and out: it seems to rise for ever */
        bph += (0.01f + spd * spd * 0.6f) * isr;
        if (bph >= 1.0f)
            bph -= 1.0f;
        float range = 2.0f + dp * 6.0f;
        for (int c = 0; c < 2; c++) {
            float s = 0.0f, ws = 0.0f;
            for (int k = 0; k < 4; k++) {
                float p = bph + 0.25f * (float)k + (c ? wd * 0.125f : 0.0f);
                p -= std::floor(p);
                float w = std::sin(PI * p);
                w *= w;
                float fc = 60.0f * std::exp2(p * range);
                s += w * phaser(c, k, x[c], fc, fd * 0.85f);
                ws += w;
            }
            y[c] = 0.5f * (x[c] + s / maxf(ws, 0.5f));
        }
        break;
    }
    case MT_NOTCH: {
        /* two notches an octave apart, swept; NTCH their width */
        float q = 0.4f + (1.0f - fd) * 6.0f;
        for (int c = 0; c < 2; c++) {
            float m = 0.5f + 0.5f * sinp(ph + (c ? wd * 0.5f : 0.0f));
            float f0 = 120.0f * std::exp2(dp * 6.0f * m + (1.0f - dp) * 2.5f);
            float v = x[c];
            for (int k = 0; k < 2; k++) {
                float fc = minf(f0 * (k ? 2.0f : 1.0f), 0.4f * sr);
                float g = tanW(clampf(PI * fc * isr, 0.0f, 1.2f)), R = 1.0f / (2.0f * q);
                float hp = (v - (2.0f * R + g) * nz[c][2 * k] - nz[c][2 * k + 1]) / (1.0f + 2.0f * R * g + g * g);
                float bp = g * hp + nz[c][2 * k];
                nz[c][2 * k] = flushDn(g * hp + bp);
                float lpv = g * bp + nz[c][2 * k + 1];
                nz[c][2 * k + 1] = flushDn(g * bp + lpv);
                v = v - 2.0f * R * bp;
            }
            y[c] = v;
        }
        break;
    }
    case MT_PITCH: {
        /* two grains a channel, a window apart, each read at the pitch's speed */
        const float W = 0.06f * sr;
        float semis[2] = {(sP[0] - 0.5f) * 24.0f, (sP[1] - 0.5f) * 24.0f};
        for (int c = 0; c < 2; c++) {
            float r = std::exp2(semis[c] / 12.0f);
            psPh[c] += (1.0f - r) / W;
            psPh[c] -= std::floor(psPh[c]);
            float p1 = psPh[c], p2 = p1 + 0.5f;
            p2 -= std::floor(p2);
            float a = ln[c].rd(2.0f + p1 * W), b = ln[c].rd(2.0f + p2 * W);
            float w1 = std::sin(PI * p1), w2 = std::sin(PI * p2);
            float v = a * w1 * w1 + b * w2 * w2;
            float fbv = dmp[c].run(v, (sP[3] - 0.5f) * 2.0f, isr);
            ln[c].push(x[c] + softT(fbv * sP[2] * 0.92f));
            y[c] = v;
        }
        break;
    }
    case MT_RESONATOR: {
        /* a short dark delay, the channels' times apart (OFST), up to ringing on its own (limited) */
        float t = (0.0003f + 0.0247f * sP[0] * sP[0]) * sr;
        float off = (sP[1] - 0.5f) * 2.0f;
        float f = sP[2] * 1.08f;
        float v[2];
        for (int c = 0; c < 2; c++) {
            float d = maxf(1.0f, t * (1.0f + (c ? -0.45f : 0.45f) * off));
            float s = ln[c].rd(d);
            lp[c] += (s - lp[c]) * (1.0f - std::exp(-2.0f * PI * 7000.0f * isr));
            ln[c].push(softT(x[c] + lp[c] * f));
            v[c] = lp[c];
        }
        float sp = sP[3];
        y[0] = v[0] * (0.5f + 0.5f * sp) + v[1] * (0.5f - 0.5f * sp);
        y[1] = v[1] * (0.5f + 0.5f * sp) + v[0] * (0.5f - 0.5f * sp);
        break;
    }
    default: y[0] = x[0]; y[1] = x[1];
    }
    yl = y[0];
    yr = y[1];
}

/* ============================================================================== DELAY === */
void Delay::alloc()
{
    ln[0].init((int)(sr * 2.1f));
    ln[1].init((int)(sr * 2.1f));
    rec.init((int)(sr * 2.1f));
    piece.assign((size_t)(sr * 0.8f), 0.0f);
}
void Delay::clearState()
{
    ln[0].clear();
    ln[1].clear();
    rec.clear();
    lp[0] = lp[1] = 0.0f;
    dmp[0] = dmp[1] = Damp();
    rpOn = false;
    rpWait = (int)(0.3f * sr);                  /* (the first piece after something has passed) */
    rpLeft = 0;
}
float Delay::timeSec(float x, float maxMs, float minMs) const
{
    if (sync)
        return minf(syncedSec(x), 2.0f);
    return (minMs + (maxMs - minMs) * x * x) * 0.001f;
}

void Delay::run(float xl, float xr, float &yl, float &yr)
{
    const float kt = 1.0f - std::exp(-1.0f / (0.12f * sr));   /* (a time's change glides: a tape's bend) */
    auto glide = [&](int c, float t) {
        tS[c] += (t - tS[c]) * kt;
        return clampf(tS[c] * sr, 2.0f, (float)ln[c].size() - 8.0f);
    };
    switch (type) {
    case DL_STEREO: {
        float dl = glide(0, timeSec(sP[0], 2000.0f, 5.0f)), dr = glide(1, timeSec(sP[1], 2000.0f, 5.0f));
        float a = ln[0].rd(dl), b = ln[1].rd(dr), f = sP[2] * 1.02f, dm = (sP[3] - 0.5f) * 2.0f;
        ln[0].push(xl + softT(dmp[0].run(a, dm, isr) * f));
        ln[1].push(xr + softT(dmp[1].run(b, dm, isr) * f));
        yl = a;
        yr = b;
        break;
    }
    case DL_PINGPONG: {
        float d = glide(0, timeSec(sP[0], 2000.0f, 5.0f));
        bool rl = sP[1] >= 0.5f;
        float a = ln[0].rd(d), b = ln[1].rd(d), f = sP[2] * 1.02f, dm = (sP[3] - 0.5f) * 2.0f;
        float in = 0.5f * (xl + xr);
        /* the input into one side, each side's echo into the other */
        ln[0].push((rl ? 0.0f : in) + softT(dmp[1].run(b, dm, isr) * f));
        ln[1].push((rl ? in : 0.0f) + softT(dmp[0].run(a, dm, isr) * f));
        yl = a;
        yr = b;
        break;
    }
    case DL_BBD: {
        /* dark and darker with time, its clock wobbling (SPD, DPTH), saturating */
        float t = timeSec(sP[0], 1000.0f, 20.0f);
        ph += (0.05f + sP[2] * sP[2] * 8.0f) * isr;
        if (ph >= 1.0f)
            ph -= 1.0f;
        float base = glide(0, t);
        float fc = clampf(9000.0f - tS[0] * 6500.0f, 1800.0f, 9000.0f);
        float k = 1.0f - std::exp(-2.0f * PI * fc * isr);
        float y2[2];
        for (int c = 0; c < 2; c++) {
            float m = std::sin(2.0f * PI * (ph + (c ? 0.25f : 0.0f)));
            float d = clampf(base * (1.0f + 0.02f * sP[3] * m) * (c ? 1.012f : 1.0f), 2.0f, (float)ln[c].size() - 8.0f);
            float v = ln[c].rd(d);
            lp[c] += (v - lp[c]) * k;
            ln[c].push(softT(((c ? xr : xl) + lp[c] * sP[1] * 1.05f) * 0.9f) * 1.05f + 2e-6f * rndF(seed));
            y2[c] = lp[c];
        }
        yl = y2[0];
        yr = y2[1];
        break;
    }
    case DL_PLAYGROUND: {
        float t = timeSec(sP[0], 600.0f, 2.0f), off = (sP[1] - 0.5f) * 2.0f;
        float dl = glide(0, t * (1.0f + 0.5f * maxf(0.0f, off))), dr = glide(1, t * (1.0f + 0.5f * maxf(0.0f, -off)));
        float f = sP[2] * 1.25f;                        /* (up to ringing on its own: limited) */
        float v[2] = {ln[0].rd(dl), ln[1].rd(dr)};
        for (int c = 0; c < 2; c++) {
            lp[c] += (v[c] - lp[c]) * (1.0f - std::exp(-2.0f * PI * 6500.0f * isr));
            ln[c].push(softT(((c ? xr : xl) + lp[c] * f) * 0.85f) / 0.85f);
        }
        float sp = sP[3];
        yl = lp[0] * (0.5f + 0.5f * sp) + lp[1] * (0.5f - 0.5f * sp);
        yr = lp[1] * (0.5f + 0.5f * sp) + lp[0] * (0.5f - 0.5f * sp);
        break;
    }
    case DL_REPEATER: {
        /* small pieces of what just passed, each round its own size and number of repeats (PROB: whether a round
         * plays), each repeat faded in and out (EG) */
        rec.push(0.5f * (xl + xr));
        float out = 0.0f;
        if (!rpOn && --rpWait <= 0) {
            float size = (0.02f + 0.48f * sP[0] * sP[0]) * (0.5f + rnd01(seed));
            rpLen = std::max(64, std::min((int)piece.size(), (int)(size * sr)));
            rpLeft = std::max(1, (int)std::lround((1.0f + sP[1] * 15.0f) * (0.5f + rnd01(seed))));
            rpPos = 0;
            rpOn = rnd01(seed) < sP[2];
            if (rpOn)                                   /* the piece caught: what just passed */
                for (int k = 0; k < rpLen; k++)
                    piece[(size_t)k] = rec.rd((float)(rpLen - k));
            else
                rpWait = rpLen;                         /* (this round silent) */
        }
        if (rpOn) {
            float fr = (float)rpPos / (float)rpLen, eg = 0.01f + 0.49f * sP[3];
            float env = minf(1.0f, minf(fr / eg, (1.0f - fr) / eg));
            out = piece[(size_t)rpPos] * env;
            if (++rpPos >= rpLen) {
                rpPos = 0;
                if (--rpLeft <= 0) {
                    rpOn = false;
                    rpWait = 1;
                }
            }
        }
        yl = yr = out;
        break;
    }
    default: yl = xl; yr = xr;
    }
}

/* ============================================================================== REV === */
static const float RV_LEN[8] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116};
static const float RV_AP[6] = {142, 107, 379, 277, 211, 163};

void Reverb::alloc()
{
    for (int i = 0; i < NL; i++)
        fdn[i].init((int)(RV_LEN[i] * sr / 44100.0f * 3.2f) + 16);
    for (int i = 0; i < 6; i++)
        diff[i].init((int)(RV_AP[i] * sr / 44100.0f) + 4);
    pre.init((int)(sr * 0.26f));
    for (int c = 0; c < 2; c++) {
        gr[c].init((int)(sr * 4.2f));
        shp[c].init((int)(sr * 0.12f));
    }
}
void Reverb::clearState()
{
    for (auto &l : fdn)
        l.clear();
    for (auto &l : diff)
        l.clear();
    pre.clear();
    for (int c = 0; c < 2; c++) {
        gr[c].clear();
        shp[c].clear();
        shOut[c] = 0.0f;
        psPh[c] = 0.0f;
        for (auto &g : grains[c])
            g = Grain();
    }
    std::memset(lpS, 0, sizeof lpS);
    for (auto &d : dmp)
        d = Damp();
    hpIn = 0.0f;
}

/* eight lines mixed by a Householder matrix, each damped, gently moving */
void Reverb::fdnRun(float in, float size, float t60, float damp, float modRate, float modDepth, float &yl, float &yr)
{
    /* input diffusion */
    for (int i = 0; i < 6; i++) {
        Line &l = diff[i];
        float dly = RV_AP[i] * sr / 44100.0f;
        float z = l.rd(dly), y = -0.62f * in + z;
        l.push(in + 0.62f * y);
        in = y;
    }
    float o[NL], sum = 0.0f;
    for (int i = 0; i < NL; i++) {
        mph[i] += (modRate * (0.8f + 0.07f * (float)i)) * isr;
        if (mph[i] >= 1.0f)
            mph[i] -= 1.0f;
        float len = RV_LEN[i] * sr / 44100.0f * size;
        float d = clampf(len + modDepth * std::sin(2.0f * PI * mph[i]), 4.0f, (float)fdn[i].size() - 8.0f);
        float y = fdn[i].rd(d);
        float g = t60 > 60.0f ? 1.0f : std::pow(10.0f, -3.0f * (d / sr) / t60);
        o[i] = dmp[i].run(y, damp, isr) * g;
        sum += o[i];
    }
    sum *= 0.25f;
    for (int i = 0; i < NL; i++) {
        float v = o[i] - sum + ((i & 1) ? in : in * 0.9f);
        fdn[i].push(clampf(v, -4.0f, 4.0f));
    }
    yl = 0.5f * (o[0] + o[2] + o[4] + o[6]);
    yr = 0.5f * (o[1] + o[3] + o[5] + o[7]);
}

void Reverb::run(float xl, float xr, float &yl, float &yr)
{
    /* the input's lows out (140 Hz): no mud under the dry */
    float in0 = 0.3f * (xl + xr);
    hpIn += (in0 - hpIn) * (1.0f - std::exp(-2.0f * PI * 140.0f * isr));
    float in = in0 - hpIn;
    switch (type) {
    case RV_SMALL:
    case RV_LARGE: {
        bool big = type == RV_LARGE;
        pre.push(in);
        float pd = maxf(1.0f, sP[0] * sP[0] * 0.25f * sr);
        float x = pre.rd(pd);
        float size = big ? 0.7f + 1.3f * sP[1] : 0.25f + 0.65f * sP[1];
        float f = sP[2];
        float t60 = f >= 0.995f ? 100.0f : (big ? 0.5f + 14.0f * f * f : 0.2f + 3.8f * f * f);
        fdnRun(x, size, t60, (sP[3] - 0.5f) * 2.0f, 0.3f, big ? 6.0f : 3.0f, yl, yr);
        break;
    }
    case RV_HUGE: {
        float size = 1.4f + 1.7f * sP[0];
        float f = sP[1];
        float t60 = f >= 0.995f ? 100.0f : 1.5f + 38.0f * f * f;
        fdnRun(in, size, t60, -0.35f, 0.1f + sP[2] * sP[2] * 3.0f, 2.0f + sP[3] * 40.0f, yl, yr);
        break;
    }
    case RV_SHIMMER: {
        /* a large room whose tail goes round through a pitch shifter (TUNE) */
        float semis = std::round((sP[0] - 0.5f) * 48.0f);
        float r = std::exp2(semis / 12.0f);
        float size = 0.9f + 1.6f * sP[1];
        float f = sP[2];
        float t60 = f >= 0.995f ? 100.0f : 1.0f + 18.0f * f * f;
        float x = in + softT(0.22f * (shOut[0] + shOut[1]) * (0.3f + 0.7f * f));
        float a, b;
        fdnRun(x, size, t60, (sP[3] - 0.5f) * 2.0f, 0.25f, 5.0f, a, b);
        const float W = 0.08f * sr;
        float src[2] = {a, b};
        for (int c = 0; c < 2; c++) {
            shp[c].push(src[c]);
            psPh[c] += (1.0f - r) / W;
            psPh[c] -= std::floor(psPh[c]);
            float p1 = psPh[c], p2 = p1 + 0.5f;
            p2 -= std::floor(p2);
            float w1 = std::sin(PI * p1), w2 = std::sin(PI * p2);
            shOut[c] = 0.5f * (shp[c].rd(2.0f + p1 * W) * w1 * w1 + shp[c].rd(2.0f + p2 * W) * w2 * w2);
        }
        yl = a + 0.5f * shOut[0];
        yr = b + 0.5f * shOut[1];
        break;
    }
    case RV_CLOUD: {
        /* the input recorded into a buffer, played back by several looping heads at random places (GRAIN: how
         * many; under the middle backwards), each its own speed (TUNE) */
        float g = (sP[3] - 0.5f) * 2.0f;
        int heads = 1 + (int)std::lround(std::fabs(g) * 7.0f);
        bool rev = g < -0.02f;
        float gsize = (0.03f + 0.77f * sP[1] * sP[1]) * sr;
        float fbk = sP[2] * 0.85f;
        float o[2] = {};
        for (int c = 0; c < 2; c++) {
            for (int h = 0; h < 8; h++) {
                Grain &gn = grains[c][h];
                if (h >= heads) {
                    gn.on = false;
                    continue;
                }
                if (!gn.on) {
                    gn.on = true;
                    gn.len = gsize * (0.6f + 0.8f * rnd01(seed));
                    gn.age = rnd01(seed) * gn.len * 0.5f;
                    float back = gn.len * 1.1f + rnd01(seed) * minf(2.0f * sr, (float)gr[c].size() - gn.len * 3.0f - 8.0f);
                    gn.pos = back;
                    float det = (rnd01(seed) - 0.5f) * sP[0] * 0.5f;
                    gn.inc = (rev ? -1.0f : 1.0f) * (1.0f + det);
                }
                float fr = gn.age / gn.len;
                float w = std::sin(PI * fr);
                float d = clampf(gn.pos, 2.0f, (float)gr[c].size() - 4.0f);
                o[c] += gr[c].rd(d) * w * w;
                /* the head moves through the past at its speed (the past itself moving on a sample) */
                gn.pos += 1.0f - gn.inc;
                gn.age += 1.0f;
                if (gn.age >= gn.len)
                    gn.on = false;
            }
            o[c] *= 1.0f / std::sqrt((float)heads);
        }
        float xin[2] = {xl * 0.5f, xr * 0.5f};
        for (int c = 0; c < 2; c++)
            gr[c].push(clampf(xin[c] + o[c] * fbk, -3.0f, 3.0f));
        /* a little diffusion: a cloud, not a stutter */
        float s = 0.5f * (o[0] + o[1]), dl, dr;
        fdnRun(s * 0.6f, 0.6f, 0.8f + sP[2] * 2.0f, -0.2f, 0.2f, 2.0f, dl, dr);
        yl = o[0] + 0.35f * dl;
        yr = o[1] + 0.35f * dr;
        break;
    }
    default: yl = xl; yr = xr;
    }
}
} // namespace hm
