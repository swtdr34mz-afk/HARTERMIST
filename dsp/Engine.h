/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
/* HARTERMIST's engine: an Artemis, voice by voice as its manual describes it.
 *
 *   voices     6; PLAY MODE POLY (6), TRI (3 notes of 2 voices), DUO (2 of 3), UNISON (1 of 6), MONO (1 of 1); the
 *              stacked voices detuned by DTN; LEGATO, GLIDE ON / AUTO (polyphonic glide)
 *   per voice  two VCOs, one WAVE morphing both from sawtooth through square to triangle, PW narrowing both (to silence),
 *              VCO 2 +-2 octaves in semitones with DTN raising it, hard SYNC of VCO 2 to VCO 1, through-zero linear
 *              FM of VCO 1 by VCO 2 (or its depth from the filter envelope: ENV), the sub (an octave under VCO 1) or
 *              white noise (SUB / NOISE), the VCO 1 / VCO 2 crossfader; the low-pass (24 dB of four OTA stages, or
 *              12 dB), its RES singing at the top (DRIVE's NO OSC: never), DRIVE before it, TRACK zero / half / full,
 *              ENV, FFM (VCO 2 or noise at audio rate to the cutoff); the OTA VCA; two ADSR (filter and FM, VCA, or
 *              one shared); two LFOs (sine, triangle, saw, ramp, square, random, Env+, Env-; free, key, BPM, BPM+key),
 *              LFO 1 to the VCOs' pitch (1, 2 or both) and the cutoff with its FADE IN, LFO 2 to LFO 1's rate (XMOD),
 *              the WAVE and VCO 1's pulse width; INSTABILITY (each voice its own small differences)
 *   after      the voices' sum (odd voices left, even right: SPREAD), the 12 dB stereo high-pass (CUT, RES), then the
 *              four effect pages, PRESET LEVEL
 *   matrix     LFO 1, LFO 2, the VCA EG, the filter EG, velocity, the mod wheel, aftertouch (poly or channel), key
 *              track and CC74, each to any knob or slider (-100..+100); per voice, and for the global ones (the high
 *              pass, SPREAD, the effects' sliders) their monophonic versions
 *   playing    the clock (PLAY / STOP, or the host's), the arpeggiator (UP, DOWN, ORD, INC, EXC, RND; DIV, PROB,
 *              SWNG, GATE; REC holds), the poly sequencer (64 steps of six notes, ties and rests; STEPS, DIV, PROB,
 *              SWNG, GATE), MPE
 *
 * The voices run at 2x or 4x the rate (the oscillators, the filter), summed, then brought down. The engine works in
 * chunks of 16 samples: notes land on the next chunk, the matrix is worked out per chunk, what moves at audio rate
 * moves per sample. All calls from the audio thread. */
#pragma once
#include "Dsp.h"
#include "Fx.h"
#include "Params.h"

namespace hm
{
constexpr int NV = 6, CH = 16, NKEYS = 32, MAXOS = 4;

class Engine
{
public:
    Engine();
    void prepare(double sampleRate);
    void setProgram(const Program &p);
    const Program &program() const { return prog; }
    void setParam(int p, int v);
    void setMod(int src, int dst, int amt);
    void setQuality(int q) { quality = q ? 1 : 0; }
    void setMpe(bool on, int pbRange) { mpe = on; mpePb = (float)pbRange; }

    /* MIDI (channels 1..16) */
    void noteOn(int ch, int note, float vel);
    void noteOff(int ch, int note);
    void polyPressure(int ch, int note, float v);
    void chanPressure(int ch, float v);
    void bend(int ch, float v);                  /* -1..1 */
    void cc74(int ch, float v);
    void modWheel(float v) { mwV = clampf(v, 0.0f, 1.0f); }
    void sustain(bool on);
    void allOff();
    void panic();
    void host(bool playing, double ppq, double bpm, bool sync);

    /* the transport (PLAY, STOP; STOP twice: back to the first step, all notes off) */
    void play();
    void stop();
    void stopTwice();
    bool running() const { return clockRunning(); }
    float bpmShown() const { return bpmNow; }
    bool beat() const { return pos - std::floor(pos) < 0.25; }

    /* REC: the arpeggiator's hold, or the sequencer's recording */
    void rec(bool on);
    bool recOn() const { return recording || arpHold; }
    void seqRest();                              /* (the encoder pressed, no keys held) */
    void seqTie();                               /* (with keys held) */
    void seqScroll(int d);
    int seqStepShown() const;                    /* the step playing, or being recorded (-1: none) */
    int recStep() const { return rStep; }

    void render(float *L, float *R, int n);

    int voicesSounding() const;
    float voiceLevel(int v) const { return v >= 0 && v < NV ? V[v].ae : 0.0f; }
    float lfoShown(int i) const { return i ? gL2 : gL1; }

private:
    struct Voice {
        bool on = false, gate = false, sus = false;
        int key = -1, note = -1, ch = 0, slot = -1;
        float vel = 0.0f, det = 0.0f;            /* its velocity, its place in a stack's detune (-1..1) */
        float p = 60.0f, t = 60.0f, gr = 0.0f;   /* pitch now, where it glides, how fast (semitones a second) */
        float ph1 = 0.0f, ph2 = 0.0f, phs = 0.0f, o2 = 0.0f, syncCorr = 0.0f;
        float fe = 0.0f, ae = 0.0f;
        uint8_t fst = 0, ast = 0;
        float l1ph = 0.0f, l2ph = 0.0f, l1sh = 0.0f, l2sh = 0.0f, l1v = 0.0f, l2v = 0.0f, fade = 0.0f;
        bool l1done = false, l2done = false;
        float f[4] = {}, ffb = 0.0f;             /* the filter's stages (12 dB: the first two) */
        float at = 0.0f, bend = 0.0f, cc74 = 0.5f;
        float vr[6] = {};                        /* INSTABILITY: this voice's own differences (-1..1) */
        float dr = 0.0f, dt = 0.0f;
        int drn = 0;
        uint32_t seed = 1, age = 0;
        float kill = 1.0f;                       /* taken: faded out first */
        bool pend = false;
        int pkey = -1, pnote = -1, pch = 0;
        float pvel = 0.0f, pdet = 0.0f;
        /* the last chunk's values (ramped from) */
        float s1 = 0, s2 = 0, sCut = 0, sMorph = 0, sDuty = 1, sDuty1 = 1, sG1 = 0, sG2 = 0, sSub = 0, sNoise = 0, sFm = 0,
              sFfm = 0, sRes = 0, sEnv = 0, sAmp = 0;
        bool sOk = false;
    };
    enum { ES_IDLE, ES_ATT, ES_DEC, ES_REL };

    Program prog;
    Voice V[NV];
    FxChain fx;
    int quality = 1;
    float sr = 44100.0f;
    bool mpe = false;
    float mpePb = 48.0f;
    uint32_t ageN = 0, seed = 0x9E3779B9u;
    float bendG = 0, mwV = 0, atG = 0, cc74G = 0.5f, ctl[3] = {};   /* (smoothed: bend, wheel, pressure) */
    bool susOn = false;
    /* the voices' sum at the high rate, the stages down */
    float bufL[CH * MAXOS] = {}, bufR[CH * MAXOS] = {};
    HalfBand dnL[2], dnR[2];
    float outL[CH] = {}, outR[CH] = {};
    int gpos = CH;
    float dcL = 0, dcR = 0, dcxL = 0, dcxR = 0, dcK = 0.999f;
    float hp[2][2] = {};
    /* the monophonic modulation sources (the global destinations) */
    float gL1ph = 0, gL2ph = 0, gL1 = 0, gL2 = 0, gL1sh = 0, gL2sh = 0;
    float lastVel = 0.8f, lastKey = 60.0f;
    int lastVoice = 0;
    /* the matrix: the routings set (source, destination, amount), per voice and global */
    struct Route {
        int8_t src;
        int16_t dst;
        float amt;
    };
    Route vRoutes[NMODSRC * NPARAM], gRoutes[NMODSRC * NPARAM];
    int nvR = 0, ngR = 0;
    void rebuildRoutes();

    /* keys held, in order */
    int8_t keys[NKEYS];
    float keyVel[NKEYS];
    int8_t keyCh[NKEYS];
    int nKeys = 0;
    float lastPitch = 60.0f;
    bool anyHeld = false;
    /* the clock */
    double pos = 0.0, fpos = 0.0;               /* the transport's place (beats), and a clock always running */
    float gSpread = 0.0f;
    float chBend[17] = {}, chAt[17] = {}, chCc74[17] = {};   /* MPE: each channel's last expression */
    float bpmNow = 120.0f;
    bool hostPlaying = false, hostSync = true, internalRun = false;
    double hostPpq = 0.0, hostBpm = 120.0;
    bool clockRunning() const { return (hostSync && hostPlaying) || internalRun; }
    struct Stepper {
        long idx = -1;
        double offAt = -1.0;                     /* (beats: when the step's gate closes) */
    } arpS, seqS;
    /* the arp */
    int8_t arpNotes[NKEYS];
    float arpVel[NKEYS];
    int arpN = 0, arpI = -1, arpDir = 1, arpPlaying = -1;
    bool arpHold = false;
    /* the sequencer */
    bool recording = false;
    int rStep = 0, rHeld = 0, pStep = -1;
    bool rFresh = true;
    uint8_t sounding[PSEQ_NOTES] = {};

    int stackSize() const;
    bool monoMode() const { return prog.p[P_PLAY_MODE] >= PM_UNISON; }
    void key(int ch, int note, float vel, bool down);
    void playNote(int key, int ch, int note, float vel, bool legato);
    void stopNote(int key);
    void startVoice(Voice &v, int key, int ch, int note, float vel, float det, bool retrig);
    void releaseVoice(Voice &v);
    int findSlot();
    void monoUpdate(bool newKey);
    void chunk();
    void clockChunk();
    bool stepEdge(Stepper &s, int div, int swing, int gate, bool &gateOff);
    void arpStep();
    void arpRebuild();
    void seqStep();
    void voiceChunk(Voice &v, int idx, int os);
};
} // namespace hm
