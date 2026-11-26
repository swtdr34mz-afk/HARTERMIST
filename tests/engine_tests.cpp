/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
/* The engine's tests: every program at three rates (sound, no NaN, a sane level, silence after), the oscillators in
 * tune, the filter singing in tune at full tracking (and never with NO OSC), every effect algorithm at its extremes,
 * the five play modes, the arpeggiator, the sequencer (recorded and played), MPE's per-note bend, the matrix, any
 * buffer size. Exit code: the failures. */
#include "Engine.h"
#include "Presets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace hm;

static int fails = 0;
#define CHECK(c, ...)                                                                                                  \
    do {                                                                                                               \
        if (!(c)) {                                                                                                    \
            fails++;                                                                                                   \
            std::printf("FAIL: " __VA_ARGS__);                                                                          \
            std::printf("\n");                                                                                         \
        }                                                                                                              \
    } while (0)

struct Out {
    std::vector<float> L, R;
    float peak(size_t a, size_t b) const
    {
        float p = 0.0f;
        for (size_t i = a; i < b && i < L.size(); i++)
            p = std::max(p, std::max(std::fabs(L[i]), std::fabs(R[i])));
        return p;
    }
    float rms(size_t a, size_t b) const
    {
        double s = 0.0;
        size_t n = 0;
        for (size_t i = a; i < b && i < L.size(); i++, n++)
            s += 0.5 * ((double)L[i] * L[i] + (double)R[i] * R[i]);
        return n ? (float)std::sqrt(s / (double)n) : 0.0f;
    }
    bool finite() const
    {
        for (size_t i = 0; i < L.size(); i++)
            if (!std::isfinite(L[i]) || !std::isfinite(R[i]))
                return false;
        return true;
    }
};

static void render(Engine &e, Out &o, int n, int block = 256)
{
    size_t at = o.L.size();
    o.L.resize(at + (size_t)n);
    o.R.resize(at + (size_t)n);
    for (int i = 0; i < n; i += block) {
        int k = std::min(block, n - i);
        e.render(o.L.data() + at + i, o.R.data() + at + i, k);
    }
}

static float freqOf(const std::vector<float> &x, size_t a, size_t b, float sr)
{
    long first = -1, last = -1, n = 0;
    for (size_t i = a + 1; i < b; i++)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
            if (first < 0)
                first = (long)i;
            last = (long)i;
            n++;
        }
    return n > 1 ? (float)(n - 1) * sr / (float)(last - first) : 0.0f;
}

static void testPrograms()
{
    for (float sr : {44100.0f, 48000.0f, 96000.0f})
        for (int i = 0; i < numFactory(); i++) {
            Program p = factory(i);
            Engine e;
            e.prepare(sr);
            e.setProgram(p);
            if (p.p[P_SEQ_ON])
                e.play();
            Out o;
            e.noteOn(1, 48, 0.8f);
            e.noteOn(1, 55, 0.8f);
            e.noteOn(1, 60, 0.8f);
            render(e, o, (int)(sr * 2.0f));
            e.noteOff(1, 48);
            e.noteOff(1, 55);
            e.noteOff(1, 60);
            if (p.p[P_SEQ_ON])
                e.stopTwice();
            render(e, o, (int)(sr * (sr == 48000.0f ? 12.0f : 0.5f)), 333);
            size_t s2 = (size_t)(sr * 2.0f);
            float pk = o.peak(0, s2), r = o.rms(0, s2), tail = o.peak(o.L.size() - (size_t)(sr * 0.5f), o.L.size());
            CHECK(o.finite(), "%s at %.0f: not finite", p.name, sr);
            CHECK(pk > 0.05f && pk < 1.3f, "%s at %.0f: peak %.3f", p.name, sr, pk);
            CHECK(r > 0.01f, "%s at %.0f: rms %.4f", p.name, sr, r);
            if (sr == 48000.0f)
                CHECK(tail < 0.02f, "%s: still %.4f after 12 s (a note stuck?)", p.name, tail);
            if (sr == 48000.0f)
                std::printf("  %-22s peak %.2f rms %.3f\n", p.name, pk, r);
        }
}

static void testTuning()
{
    Program p;
    programInit(p);
    p.p[P_INSTAB] = 0;
    for (int note : {36, 60, 84}) {
        Engine e;
        e.prepare(48000.0);
        e.setProgram(p);
        e.noteOn(1, note, 0.9f);
        Out o;
        render(e, o, 48000);
        float f = freqOf(o.L, 24000, 48000, 48000.0f), want = 440.0f * std::exp2((float)(note - 69) / 12.0f);
        CHECK(std::fabs(f / want - 1.0f) < 0.006f, "osc at %d: %.2f Hz, want %.2f", note, f, want);
    }
    /* the filter singing, VCOs silenced (PW at its top), tracking full: an octave a twelve */
    p.p[P_PW] = 1000;
    p.p[P_RES] = 1000;
    p.p[P_TRACK] = TR_FULL;
    p.p[P_CUT] = 500;
    for (int lp12 = 0; lp12 < 2; lp12++) {
        p.p[P_LP12] = (int16_t)lp12;
        float f0 = 0.0f;
        for (int note : {48, 60, 72}) {
            Engine e;
            e.prepare(48000.0);
            e.setProgram(p);
            e.noteOn(1, note, 0.9f);
            Out o;
            render(e, o, 48000);
            float f = freqOf(o.L, 24000, 48000, 48000.0f);
            CHECK(o.peak(24000, 48000) > 0.03f, "filter (%s) at %d: not singing", lp12 ? "12" : "24", note);
            if (f0 > 0.0f)
                CHECK(std::fabs(f / f0 - 2.0f) < 0.08f, "filter tracking (%s): %.1f then %.1f", lp12 ? "12" : "24", f0, f);
            f0 = f;
        }
    }
    p.p[P_DRIVE] = DR_NOOSC;
    {
        Engine e;
        e.prepare(48000.0);
        e.setProgram(p);
        e.noteOn(1, 60, 0.9f);
        Out o;
        render(e, o, 48000);
        CHECK(o.peak(24000, 48000) < 0.01f, "NO OSC: the filter sings (%.4f)", o.peak(24000, 48000));
    }
}

static void testEffects()
{
    const int TY[4] = {P_DIST_TYPE, P_MOD_TYPE, P_DLY_TYPE, P_REV_TYPE}, N[4] = {NDIST, NMOD, NDELAY, NREV};
    for (int pg = 0; pg < 4; pg++)
        for (int t = 0; t < N[pg]; t++)
            for (int var = 0; var < 3; var++) {
                Program p;
                programInit(p);
                p.p[TY[pg]] = (int16_t)t;
                for (int s = 0; s < 4; s++)
                    p.p[TY[pg] + 1 + s] = (int16_t)(var * 500);
                p.p[TY[pg] + 5] = 1000;
                Engine e;
                e.prepare(48000.0);
                e.setProgram(p);
                Out o;
                e.noteOn(1, 48, 1.0f);
                e.noteOn(1, 55, 1.0f);
                render(e, o, 48000);
                e.noteOff(1, 48);
                e.noteOff(1, 55);
                render(e, o, 96000);
                CHECK(o.finite(), "effect %d/%d (%d): not finite", pg, t, var);
                CHECK(o.peak(0, o.L.size()) < 1.6f, "effect %d/%d (%d): peak %.2f", pg, t, var, o.peak(0, o.L.size()));
            }
}

static void testPlayModes()
{
    for (int m = 0; m < NPLAYMODE; m++) {
        Program p;
        programInit(p);
        p.p[P_PLAY_MODE] = (int16_t)m;
        Engine e;
        e.prepare(48000.0);
        e.setProgram(p);
        Out o;
        e.noteOn(1, 60, 0.8f);
        e.noteOn(1, 64, 0.8f);
        render(e, o, 24000);
        int want = m == PM_POLY ? 2 : m == PM_TRI ? 4 : m == PM_DUO ? 6 : m == PM_UNISON ? 6 : 1;
        CHECK(e.voicesSounding() == want, "play mode %s: %d voices, want %d", PLAYMODE_NAME[m], e.voicesSounding(), want);
        e.noteOff(1, 60);
        e.noteOff(1, 64);
        render(e, o, 48000);
        CHECK(e.voicesSounding() == 0, "play mode %s: still sounding", PLAYMODE_NAME[m]);
    }
}

static void testEightVoices()
{
    /* 8 voices: eight notes at once in POLY; UNISON stacks all eight; TRI four pairs */
    Program p;
    programInit(p);
    for (int m : {PM_POLY, PM_UNISON, PM_TRI, PM_DUO}) {
        p.p[P_PLAY_MODE] = (int16_t)m;
        Engine e;
        e.prepare(48000.0);
        e.setVoices(8);
        e.setProgram(p);
        Out o;
        for (int k = 0; k < 8; k++)
            e.noteOn(1, 48 + 3 * k, 0.8f);
        render(e, o, 12000);
        CHECK(e.voicesSounding() == 8, "8 voices, %s: %d sounding", PLAYMODE_NAME[m], e.voicesSounding());
        CHECK(o.finite() && o.peak(0, 12000) < 1.3f, "8 voices, %s: level %.2f", PLAYMODE_NAME[m], o.peak(0, 12000));
    }
}

static void testArpSeq()
{
    {
        Program p;
        programInit(p);
        p.p[P_ARP_ON] = 1;
        p.p[P_ARP_DIV] = 11;                     /* 1/16 */
        p.p[P_AE_S] = 0;
        p.p[P_AE_D] = 300;
        Engine e;
        e.prepare(48000.0);
        e.setProgram(p);
        e.noteOn(1, 60, 0.8f);
        e.noteOn(1, 64, 0.8f);
        e.noteOn(1, 67, 0.8f);
        Out o;
        int onsets = 0;
        float prev = 0.0f;
        for (int b = 0; b < 400; b++) {          /* 2 s at 120: 16 steps */
            render(e, o, 240);
            float l = e.voiceLevel(0) + e.voiceLevel(1) + e.voiceLevel(2) + e.voiceLevel(3) + e.voiceLevel(4) + e.voiceLevel(5);
            if (l > prev + 0.3f)
                onsets++;
            prev = l;
        }
        CHECK(onsets >= 12 && onsets <= 20, "arp: %d notes in 2 s, want 16", onsets);
        CHECK(o.finite(), "arp: not finite");
    }
    {
        /* the sequencer: three steps recorded (a note, a rest, a chord), then played by the transport */
        Program p;
        programInit(p);
        p.p[P_SEQ_ON] = 1;
        p.p[P_SEQ_STEPS] = 4;
        Engine e;
        e.prepare(48000.0);
        e.setProgram(p);
        e.rec(true);
        e.noteOn(1, 60, 0.8f);
        e.noteOff(1, 60);
        e.seqRest();
        e.noteOn(1, 64, 0.8f);
        e.noteOn(1, 67, 0.8f);
        e.noteOff(1, 64);
        e.noteOff(1, 67);
        CHECK(e.recStep() == 3, "seq: recording at step %d, want 3", e.recStep());
        e.rec(false);
        const Program &q = e.program();
        CHECK(q.seq.note[0][0] == 62 && q.seq.note[1][0] == PN_REST && q.seq.note[2][1] == 69, "seq: steps not as recorded");
        Out o;
        render(e, o, 4800);
        CHECK(o.peak(0, 4800) < 1e-3f, "seq: plays with the transport stopped");
        e.play();
        render(e, o, 96000);
        CHECK(o.peak(4800, 100800) > 0.05f, "seq: silent while playing");
        e.stopTwice();
        render(e, o, 96000);
        CHECK(o.peak(o.L.size() - 4800, o.L.size()) < 1e-3f, "seq: still sounding after STOP");
    }
}

static void testMpeAndMatrix()
{
    Program p;
    programInit(p);
    p.p[P_INSTAB] = 0;
    Engine e;
    e.prepare(48000.0);
    e.setMpe(true, 48);
    e.setProgram(p);
    e.noteOn(2, 60, 0.8f);
    e.bend(2, 12.0f / 48.0f);                    /* this note an octave up */
    Out o;
    render(e, o, 48000);
    float f = freqOf(o.L, 24000, 48000, 48000.0f);
    CHECK(std::fabs(f / 523.25f - 1.0f) < 0.01f, "MPE bend: %.1f Hz, want 523.3", f);

    /* the matrix: the mod wheel opening the filter */
    Program q;
    programInit(q);
    q.p[P_CUT] = 200;
    Engine a, b;
    a.prepare(48000.0);
    b.prepare(48000.0);
    a.setProgram(q);
    q.mod[MS_MW][P_CUT] = 80;
    b.setProgram(q);
    b.modWheel(1.0f);
    a.noteOn(1, 48, 0.8f);
    b.noteOn(1, 48, 0.8f);
    Out oa, ob;
    render(a, oa, 24000);
    render(b, ob, 24000);
    CHECK(ob.rms(12000, 24000) > oa.rms(12000, 24000) * 2.0f, "matrix: the wheel did not open the filter (%.4f / %.4f)",
          ob.rms(12000, 24000), oa.rms(12000, 24000));
}

static void testCc74()
{
    /* CC74 on the cutoff through the matrix: at 64 nothing moves (the knob's sound exactly), under 64 it closes, over
     * it opens; CC121's 64 again */
    Program p;
    programInit(p);
    p.p[P_CUT] = 450;
    p.p[P_INSTAB] = 0;
    auto run = [&](bool route, float cc) {
        Program q = p;
        if (route)
            q.mod[MS_CC74][P_CUT] = 40;
        Engine e;
        e.prepare(48000.0);
        e.setProgram(q);
        if (cc >= 0.0f)
            e.cc74(1, cc);
        e.noteOn(1, 48, 0.8f);
        Out o;
        render(e, o, 24000);
        return o.rms(12000, 24000);
    };
    float plain = run(false, -1.0f), mid = run(true, 64.0f / 127.0f), lo = run(true, 0.0f), hi = run(true, 1.0f),
          none = run(true, -1.0f);
    CHECK(std::fabs(mid / plain - 1.0f) < 0.02f, "CC74 at 64 moves the cutoff (%.4f / %.4f)", mid, plain);
    CHECK(std::fabs(none / plain - 1.0f) < 0.02f, "CC74 unsent moves the cutoff (%.4f / %.4f)", none, plain);
    CHECK(lo < plain * 0.8f && hi > plain * 1.2f, "CC74 does not move the cutoff both ways (%.4f %.4f %.4f)", lo, plain, hi);
}

static void testBlocks()
{
    /* any buffer size: the same sound */
    Program p = factory(0);
    Out a, b;
    for (int pass = 0; pass < 2; pass++) {
        Engine e;
        e.prepare(48000.0);
        e.setProgram(p);
        e.noteOn(1, 57, 0.7f);
        render(e, pass ? b : a, 24000, pass ? 1 : 512);
    }
    float d = 0.0f;
    for (size_t i = 0; i < a.L.size(); i++)
        d = std::max(d, std::fabs(a.L[i] - b.L[i]));
    CHECK(d < 1e-5f, "block sizes differ by %.6f", d);
}

int main()
{
    std::printf("programs (%d):\n", numFactory());
    testPrograms();
    testTuning();
    testEffects();
    testPlayModes();
    testEightVoices();
    testArpSeq();
    testMpeAndMatrix();
    testCc74();
    testBlocks();
    std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails;
}
