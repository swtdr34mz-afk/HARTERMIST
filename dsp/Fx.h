/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
/* The four effect pages, in series as on the Artemis: DIST, MOD, DELAY, REV, all stereo. Each takes its algorithm
 * and its five sliders (0..1, the last one MIX: at 0 the page is out of the way). A new algorithm crossfades in;
 * every slider glides. The algorithms (the names and the sliders' roles as the Artemis manual gives them):
 *
 *   DIST   Exponential, Parabolic, Sine Clip, S-Curve, Soft Clip, Hard Clip, Tri Clip, Tri Fold, Single Fold, Multi
 *          Fold, Sine Bend, Sine Fold, Sine Shred, Bin Shred, Sym Warp: BITS, RATE, GAIN, LEVEL, MIX (the shaping at
 *          2x, then the bit depth and the sample rate brought down)
 *   MOD    Chorus, Ensemble, Tape Chorus, BBD Chorus, Flanger, BBD Flanger, Thru-Zero Flanger, Phaser, Barber-Pole
 *          Phaser: DPTH, SPD, FEED, WDTH, MIX; Double Notch: DPTH, SPD, NTCH, WDTH, MIX; Pitch Shifter: L, R, FEED,
 *          DAMP, MIX; BBD Resonator: TIME, OFST, FEED, SPRD, MIX
 *   DELAY  Stereo: L, R, FEED, DAMP, MIX; Ping-Pong: TIME, PAN, FEED, DAMP, MIX; BBD Delay: TIME, FEED, SPD, DPTH,
 *          MIX; BBD Playground: TIME, OFST, FEED, SPRD, MIX; Random Repeater: SIZE, REP, PROB, EG, MIX (the times
 *          notes of the clock with DELAYS BPM SYNC)
 *   REV    Small, Large: PRE, SIZE, FEED, DAMP, MIX; Huge: SIZE, FEED, SPD, DPTH, MIX; Cloud: TUNE, SIZE, FEED,
 *          GRAIN, MIX; Shimmer: TUNE, SIZE, FEED, DAMP, MIX */
#pragma once
#include "Dsp.h"
#include "Params.h"
#include <vector>

namespace hm
{
/* a delay line, a power of two long */
struct Line {
    std::vector<float> b;
    int mask = 0, w = 0;
    void init(int n)
    {
        int s = 1;
        while (s < n + 4)
            s <<= 1;
        b.assign((size_t)s, 0.0f);
        mask = s - 1;
        w = 0;
    }
    void clear()
    {
        std::fill(b.begin(), b.end(), 0.0f);
        w = 0;
    }
    inline void push(float x)
    {
        b[(size_t)w] = x;
        w = (w + 1) & mask;
    }
    /* d samples ago (1: the last one pushed) */
    inline float rd(float d) const
    {
        float p = (float)w - d;
        int i0 = (int)std::floor(p);
        float f = p - (float)i0;
        float a = b[(size_t)(i0 & mask)], c = b[(size_t)((i0 + 1) & mask)];
        return a + (c - a) * f;
    }
    int size() const { return mask + 1; }
};

/* a one-pole that is a low-pass under 0 and a high-pass over it (DAMP: -1..1) */
struct Damp {
    float s = 0.0f;
    inline float run(float x, float damp, float isr)
    {
        if (damp < -0.01f) {
            float fc = 20000.0f * std::exp2(damp * 5.3f);
            s += (x - s) * (1.0f - std::exp(-2.0f * PI * minf(fc, 0.45f / isr) * isr));
            return s;
        }
        if (damp > 0.01f) {
            float fc = 20.0f * std::exp2(damp * 6.6f);
            s += (x - s) * (1.0f - std::exp(-2.0f * PI * fc * isr));
            return x - s;
        }
        s = x;
        return x;
    }
};

/* what each page has in common: its algorithm crossfaded, its sliders gliding, MIX */
class FxPageBase
{
public:
    virtual ~FxPageBase() = default;
    void prepare(float sampleRate)
    {
        sr = sampleRate;
        isr = 1.0f / sr;
        alloc();
        clearAll();
    }
    void set(int type_, const float *v, float bpm_, bool sync_)
    {
        want = type_;
        for (int i = 0; i < 5; i++)
            tP[i] = clampf(v[i], 0.0f, 1.0f);
        bpm = bpm_ < 20.0f ? 120.0f : bpm_;
        sync = sync_;
        if (!init) {
            type = want;
            for (int i = 0; i < 5; i++)
                sP[i] = tP[i];
            init = true;
        }
    }
    void clearAll()
    {
        clearState();
        xf = 1.0f;
    }
    void process(float *L, float *R, int n);
    bool idle() const { return sP[4] < 1e-4f && tP[4] <= 0.0f; }

protected:
    float sr = 44100.0f, isr = 1.0f / 44100.0f, bpm = 120.0f;
    bool sync = false, init = false;
    int want = 0, type = 0;
    float tP[5] = {}, sP[5] = {}, xf = 1.0f;
    uint32_t seed = 0x2468ACE1u;
    virtual void alloc() = 0;
    virtual void clearState() = 0;
    /* one sample: the dry in, the wet out */
    virtual void run(float xl, float xr, float &yl, float &yr) = 0;
    virtual void endBlock() {}
    /* a time slider synced to the clock: the division's seconds (the slider's top the longest) */
    float syncedSec(float x) const
    {
        int i = (int)std::lround(x * (float)(NDIV - 1));
        i = i < 0 ? 0 : i >= NDIV ? NDIV - 1 : i;
        return DIV_BEATS[NDIV - 1 - i] * 60.0f / bpm;
    }
};

class Dist : public FxPageBase
{
    HalfBand hb[2];
    float hold[2] = {}, holdN = 0.0f, dcx[2] = {}, dcy[2] = {};
    void alloc() override {}
    void clearState() override;
    void run(float xl, float xr, float &yl, float &yr) override;
    float shape(float x) const;
};

class Mod : public FxPageBase
{
    Line ln[2], ln2[2];
    float ph = 0.0f, ph2 = 0.0f, fb[2] = {}, lp[2] = {}, lp2[2] = {}, wob[2] = {}, wobT[2] = {};
    int wobN = 0;
    float ap[2][4][6] = {}, apFb[2][4] = {}, bph = 0.0f;   /* the phasers' stages (barber-pole: four of six) */
    float nz[2][4] = {};                                     /* the notches' states */
    float psPh[2] = {};                                      /* the pitch shifter's windows */
    Damp dmp[2];
    void alloc() override;
    void clearState() override;
    void run(float xl, float xr, float &yl, float &yr) override;
    float phaser(int c, int k, float x, float fc, float fbk);
};

class Delay : public FxPageBase
{
    Line ln[2], rec;
    float tS[2] = {0.3f, 0.3f}, lp[2] = {}, ph = 0.0f;
    Damp dmp[2];
    /* the random repeater */
    std::vector<float> piece;
    int rpPos = 0, rpLen = 0, rpLeft = 0, rpWait = 0;
    bool rpOn = false;
    void alloc() override;
    void clearState() override;
    void run(float xl, float xr, float &yl, float &yr) override;
    float timeSec(float x, float maxMs, float minMs) const;
};

class Reverb : public FxPageBase
{
    static constexpr int NL = 8;
    Line fdn[NL], diff[6], pre, gr[2], shp[2];
    float lpS[NL] = {}, mph[NL] = {}, hpIn = 0.0f, shOut[2] = {};
    Damp dmp[NL];
    struct Grain {
        float pos = 0.0f, inc = 1.0f, len = 1.0f, age = 0.0f;
        bool on = false;
    } grains[2][8];
    float psPh[2] = {};
    void alloc() override;
    void clearState() override;
    void run(float xl, float xr, float &yl, float &yr) override;
    void fdnRun(float in, float size, float t60, float damp, float modRate, float modDepth, float &yl, float &yr);
};

/* the four in series */
struct FxChain {
    Dist dist;
    Mod mod;
    Delay delay;
    Reverb rev;
    void prepare(float sr)
    {
        dist.prepare(sr);
        mod.prepare(sr);
        delay.prepare(sr);
        rev.prepare(sr);
    }
    void clear()
    {
        dist.clearAll();
        mod.clearAll();
        delay.clearAll();
        rev.clearAll();
    }
    void process(float *L, float *R, int n)
    {
        dist.process(L, R, n);
        mod.process(L, R, n);
        delay.process(L, R, n);
        rev.process(L, R, n);
    }
};
} // namespace hm
