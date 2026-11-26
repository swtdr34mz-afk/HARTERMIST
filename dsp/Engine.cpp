/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
#include "Engine.h"
#include <algorithm>

namespace hm
{
namespace
{
enum { K_MONO = 4000, K_ARP = 5000, K_SEQ = 6000 };
constexpr float VOICE_GAIN = 0.62f;
/* each voice of a stack: its place in the detune (-1..1) */
const float DET2[2] = {-1.0f, 1.0f};
const float DET3[3] = {-1.0f, 0.0f, 1.0f};
const float DET4[4] = {-1.0f, 1.0f, -0.33f, 0.33f};
const float DET8[8] = {-1.0f, 1.0f, -0.6f, 0.6f, -0.2f, 0.2f, -0.4f, 0.4f};

inline float attSec(float x) { return 0.0005f * std::exp2(x * 14.29f); }   /* 0.5 ms .. 10 s */
inline float decSec(float x) { return 0.0012f * std::exp2(x * 14.0f); }    /* 1.2 ms .. 20 s */
inline float glideSec(float x) { return 0.002f * std::exp2(x * 10.0f); }  /* 2 ms .. 2 s an octave */
inline float semisOf(int p, float x) { return (float)PARAMS[p].lo + x * (float)(PARAMS[p].hi - PARAMS[p].lo); }

/* an LFO's wave at a phase; sh its random value this cycle */
inline float lfoWave(int w, float ph, float sh)
{
    switch (w) {
    case LW_SINE: return std::sin(2.0f * PI * ph);
    case LW_TRI: return ph < 0.25f ? 4.0f * ph : ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f;
    case LW_SAW: return 2.0f * ph - 1.0f;
    case LW_RAMP: return 1.0f - 2.0f * ph;
    case LW_SQR: return ph < 0.5f ? 1.0f : -1.0f;
    case LW_RND: return sh;
    default: return 0.0f;
    }
}
} // namespace

/* ------------------------------------------------------------------- setup --- */
Engine::Engine()
{
    programInit(prog);
    uint32_t s = 0xC0FFEE11u;
    for (int c = 0; c < 17; c++)
        chCc74[c] = 0.5f;
    for (int i = 0; i < NV; i++) {
        V[i].seed = 0x1234567u + 7919u * (uint32_t)i;
        for (auto &r : V[i].vr)
            r = rndF(s);                          /* (each voice's tolerances: the same every time) */
        V[i].ph1 = rnd01(s);
        V[i].ph2 = rnd01(s);
        V[i].l1ph = rnd01(s);
        V[i].l2ph = rnd01(s);
    }
    rebuildRoutes();
}

void Engine::prepare(double sampleRate)
{
    sr = (float)sampleRate;
    for (auto &v : V) {
        Voice keep = v;
        v = Voice();
        v.seed = keep.seed;
        std::memcpy(v.vr, keep.vr, sizeof v.vr);
        v.ph1 = keep.ph1;
        v.ph2 = keep.ph2;
        v.l1ph = keep.l1ph;
        v.l2ph = keep.l2ph;
    }
    fx.prepare(sr);
    for (int i = 0; i < 2; i++) {
        dnL[i].clear();
        dnR[i].clear();
    }
    nKeys = 0;
    arpN = 0;
    arpI = -1;
    arpPlaying = -1;
    anyHeld = false;
    for (auto &x : sounding)
        x = 0;
    dcK = 1.0f - 2.0f * PI * 6.0f / sr;
    dcL = dcR = dcxL = dcxR = 0.0f;
    std::memset(hp, 0, sizeof hp);
    for (float &c : ctl)
        c = 0.0f;
    gpos = CH;
}

void Engine::setProgram(const Program &p)
{
    int oldMode = prog.p[P_PLAY_MODE];
    prog = p;
    if (oldMode != prog.p[P_PLAY_MODE])
        allOff();
    if (!prog.p[P_SEQ_ON] && pStep >= 0) {
        for (int k = 0; k < PSEQ_NOTES; k++)
            if (sounding[k]) {
                stopNote(K_SEQ + k);
                sounding[k] = 0;
            }
        pStep = -1;
    }
    recording = false;
    rebuildRoutes();
}

void Engine::setParam(int p, int v)
{
    if (p < 0 || p >= NPARAM)
        return;
    int old = prog.p[p];
    prog.p[p] = (int16_t)std::min(std::max(v, PARAMS[p].lo), PARAMS[p].hi);
    if (old == prog.p[p])
        return;
    if (p == P_PLAY_MODE)
        allOff();
    if (p == P_ARP_ON && !prog.p[p]) {
        arpHold = false;
        arpN = 0;
        arpRebuild();
    }
    if (p == P_SEQ_ON && !prog.p[p]) {
        recording = false;
        for (int k = 0; k < PSEQ_NOTES; k++)
            if (sounding[k]) {
                stopNote(K_SEQ + k);
                sounding[k] = 0;
            }
        pStep = -1;
    }
}

void Engine::setVoices(int n)
{
    n = n >= 8 ? 8 : 6;
    if (n == nVoices)
        return;
    nVoices = n;
    allOff();
    for (int i = n; i < NV; i++) {
        V[i].on = false;
        V[i].pend = false;
        V[i].fst = V[i].ast = ES_IDLE;
        V[i].fe = V[i].ae = 0.0f;
        V[i].key = -1;
    }
}

void Engine::setMod(int src, int dst, int amt)
{
    if (src < 0 || src >= NMODSRC || dst < 0 || dst >= NPARAM)
        return;
    if (src == NMODSRC)
        return;
    prog.mod[src][dst] = (int8_t)std::min(100, std::max(-100, amt));
    rebuildRoutes();
}

void Engine::rebuildRoutes()
{
    nvR = ngR = 0;
    for (int s = 0; s < NMODSRC; s++)
        for (int d = 0; d < NPARAM; d++) {
            int a = prog.mod[s][d];
            if (!a || !PARAMS[d].mod)
                continue;
            Route r{(int8_t)s, (int16_t)d, (float)a / 100.0f};
            if (PARAMS[d].voice)
                vRoutes[nvR++] = r;
            else
                gRoutes[ngR++] = r;
        }
}

/* ------------------------------------------------------------------- MIDI --- */
static bool masterCh(int ch) { return ch == 1 || ch == 16; }

void Engine::noteOn(int ch, int note, float vel) { key(ch, note, vel, true); }
void Engine::noteOff(int ch, int note) { key(ch, note, 0.0f, false); }

void Engine::polyPressure(int ch, int note, float v)
{
    for (auto &x : V)
        if (x.on && x.note == note && (mpe || true) && (x.ch == ch || !mpe))
            x.at = clampf(v, 0.0f, 1.0f);
}
void Engine::chanPressure(int ch, float v)
{
    v = clampf(v, 0.0f, 1.0f);
    if (mpe && !masterCh(ch)) {
        chAt[ch & 15] = v;
        for (auto &x : V)
            if (x.on && x.ch == ch)
                x.at = v;
    } else
        atG = v;
}
void Engine::bend(int ch, float v)
{
    v = clampf(v, -1.0f, 1.0f);
    if (mpe && !masterCh(ch)) {
        chBend[ch & 15] = v;
        for (auto &x : V)
            if (x.on && x.ch == ch)
                x.bend = v;
    } else
        bendG = v;
}
void Engine::cc74(int ch, float v)
{
    v = clampf(v, 0.0f, 1.0f);
    if (mpe && !masterCh(ch)) {
        chCc74[ch & 15] = v;
        for (auto &x : V)
            if (x.on && x.ch == ch)
                x.cc74 = v;
    } else
        cc74G = v;
}

/* ------------------------------------------------------------------- notes --- */
int Engine::stackSize() const
{
    /* TRI: pairs; DUO: two halves; UNISON: all of them (with 6 voices as the Artemis: 3 x 2, 2 x 3, 6) */
    switch (prog.p[P_PLAY_MODE]) {
    case PM_TRI: return 2;
    case PM_DUO: return nVoices / 2;
    case PM_UNISON: return nVoices;
    default: return 1;
    }
}

void Engine::key(int ch, int note, float vel, bool down)
{
    const int16_t *P = prog.p;
    int at = -1;
    for (int i = 0; i < nKeys; i++)
        if (keys[i] == note && keyCh[i] == ch)
            at = i;
    const bool arp = P[P_ARP_ON] && !P[P_SEQ_ON];
    if (down) {
        if (at >= 0)
            return;
        bool wasHeld = nKeys > 0;
        if (nKeys < NKEYS) {
            keys[nKeys] = (int8_t)note;
            keyVel[nKeys] = vel;
            keyCh[nKeys++] = (int8_t)ch;
        }
        lastVel = vel;
        lastKey = (float)note;
        if (recording && P[P_SEQ_ON]) {          /* REC: into the step (and heard) */
            SeqData &s = prog.seq;
            if (rFresh) {
                for (int k = 0; k < PSEQ_NOTES; k++)
                    s.note[rStep][k] = PN_REST;
                rFresh = false;
            }
            for (int k = 0; k < PSEQ_NOTES; k++)
                if (s.note[rStep][k] == PN_REST || s.note[rStep][k] == PN_TIE) {
                    s.note[rStep][k] = (uint8_t)(note + 2);
                    s.vel[rStep][k] = (uint8_t)clampf(vel * 127.0f, 1.0f, 127.0f);
                    break;
                }
            rHeld++;
        }
        if (arp) {
            if (arpHold && !wasHeld)
                arpN = 0;                        /* (held: a new chord replaces it) */
            bool first = arpN == 0;
            arpRebuild();
            if (first && !clockRunning()) {      /* the clock stopped: the arpeggio starts at once */
                fpos = 0.0;
                arpS.idx = -1;
            }
            return;
        }
        anyHeld = wasHeld;
        if (monoMode())
            monoUpdate(true);
        else
            playNote(ch * 128 + note, ch, note, vel, false);
    } else {
        if (at < 0)
            return;
        for (int i = at; i < nKeys - 1; i++) {
            keys[i] = keys[i + 1];
            keyVel[i] = keyVel[i + 1];
            keyCh[i] = keyCh[i + 1];
        }
        nKeys--;
        if (recording && P[P_SEQ_ON] && rHeld > 0 && --rHeld == 0) {
            rStep = (rStep + 1) % std::max(1, (int)P[P_SEQ_STEPS]);
            rFresh = true;
        }
        if (arp) {
            if (!arpHold && !susOn)
                arpRebuild();
            return;
        }
        if (monoMode())
            monoUpdate(false);
        else
            stopNote(ch * 128 + note);
    }
}

void Engine::playNote(int k, int ch, int note, float vel, bool legato)
{
    const int st = stackSize();
    if (monoMode()) {
        /* the arp's and the sequencer's notes in UNISON / MONO: the one stack */
        for (int j = 0; j < st; j++) {
            Voice &v = V[j];
            bool keep = legato && v.on && v.gate;
            startVoice(v, k, ch, note, vel, st > 1 ? DET8[j] : 0.0f, !keep);
        }
        return;
    }
    int slot = findSlot();
    if (slot < 0)
        return;
    for (int j = 0; j < st; j++)
        startVoice(V[slot * st + j], k, ch, note, vel, st == 2 ? DET2[j] : st == 3 ? DET3[j] : st == 4 ? DET4[j] : 0.0f, true);
}

void Engine::stopNote(int k)
{
    for (auto &v : V) {
        if (v.pend && v.pkey == k) {
            v.pend = false;
            continue;
        }
        if (v.key == k && v.gate) {
            if (susOn)
                v.sus = true;
            else
                releaseVoice(v);
            v.gate = false;
        }
    }
}

int Engine::findSlot()
{
    /* a free one (the longest silent), else the oldest let go, else the oldest */
    const int st = stackSize(), n = nVoices / st;
    int best = -1;
    uint32_t bestAge = 0xFFFFFFFFu;
    for (int pass = 0; pass < 3 && best < 0; pass++) {
        bestAge = 0xFFFFFFFFu;
        for (int s = 0; s < n; s++) {
            bool free = true, held = false;
            uint32_t age = 0;
            for (int j = 0; j < st; j++) {
                const Voice &v = V[s * st + j];
                free &= !v.on && !v.pend;
                held |= v.gate || v.sus;
                age = std::max(age, v.age);
            }
            if ((pass == 0 && !free) || (pass == 1 && held))
                continue;
            if (age < bestAge) {
                best = s;
                bestAge = age;
            }
        }
    }
    return best;
}

void Engine::startVoice(Voice &v, int k, int ch, int note, float vel, float det, bool retrig)
{
    const int16_t *P = prog.p;
    if (v.on && v.key != k && retrig && v.ae > 0.001f && v.ast != ES_IDLE && !monoMode()) {
        /* taken while it sounds: out in 3 ms, then this note */
        v.pend = true;
        v.pnote = note;
        v.pkey = k;
        v.pch = ch;
        v.pvel = vel;
        v.pdet = det;
        if (v.kill >= 1.0f)
            v.kill = 0.9999f;
        v.gate = false;
        v.age = ++ageN;
        return;
    }
    bool fresh = !v.on;
    v.on = true;
    v.key = k;
    v.note = note;
    v.ch = ch;
    v.vel = vel;
    v.det = det;
    v.gate = true;
    v.sus = false;
    v.age = ++ageN;
    v.kill = 1.0f;
    v.pend = false;
    if (mpe && !masterCh(ch)) {
        v.bend = chBend[ch & 15];
        v.at = chAt[ch & 15];
        v.cc74 = chCc74[ch & 15];
    } else {
        v.bend = 0.0f;
        v.at = 0.0f;
        v.cc74 = cc74G;
    }
    /* glide: from the note before (polyphonic glide); AUTO: not the first note after all were let go */
    float gld = (float)P[P_GLD] / 1000.0f;
    bool auto_ = P[P_GLIDE_AUTO] != 0;
    bool glideOn = gld > 0.001f && !(auto_ && !anyHeld && retrig);
    float from = monoMode() && !fresh ? v.p : lastPitch;
    v.t = (float)note;
    if (glideOn) {
        v.p = from;
        v.gr = 12.0f / glideSec(gld);
    } else {
        v.p = (float)note;
        v.gr = 0.0f;
    }
    lastPitch = (float)note;
    lastVoice = (int)(&v - V);
    if (retrig || fresh) {
        v.fst = ES_ATT;
        v.ast = ES_ATT;
        int sy = P[P_LFO_SYNC];
        if (sy == LS_KEY || sy == LS_BPMKEY) {
            v.l1ph = 0.0f;
            v.l2ph = 0.0f;
        }
        if (P[P_L1_WAV] >= LW_ENVP) {
            v.l1ph = 0.0f;
            v.l1done = false;
        }
        if (P[P_L2_WAV] >= LW_ENVP) {
            v.l2ph = 0.0f;
            v.l2done = false;
        }
        v.fade = 0.0f;
    }
    if (fresh) {
        v.sOk = false;
        v.fe = v.ae = 0.0f;
        for (float &f : v.f)
            f = 0.0f;
        v.ffb = 0.0f;
        v.o2 = 0.0f;
        v.syncCorr = 0.0f;
        v.l1v = v.l2v = 0.0f;
    }
}

void Engine::releaseVoice(Voice &v)
{
    if (v.fst != ES_IDLE)
        v.fst = ES_REL;
    if (v.ast != ES_IDLE)
        v.ast = ES_REL;
    v.sus = false;
}

void Engine::monoUpdate(bool newKey)
{
    const int16_t *P = prog.p;
    const int st = stackSize();
    if (nKeys == 0) {
        stopNote(K_MONO);
        anyHeld = false;
        return;
    }
    int note = keys[nKeys - 1], ch = keyCh[nKeys - 1];
    float vel = keyVel[nKeys - 1];
    bool snd = V[0].on && V[0].key == K_MONO && V[0].gate;
    if (!newKey && snd && V[0].note == note)
        return;                                  /* (a key let go that was not the one sounding) */
    /* LEGATO: a key over a held one changes the pitch, the envelopes go on */
    bool retrig = !(P[P_LEGATO] && snd);
    anyHeld = snd;
    for (int j = 0; j < st; j++)
        startVoice(V[j], K_MONO, ch, note, vel, st > 1 ? DET8[j] : 0.0f, retrig);
}

void Engine::sustain(bool on)
{
    susOn = on;
    if (!on) {
        for (auto &v : V)
            if (v.sus) {
                releaseVoice(v);
                v.sus = false;
            }
        if (prog.p[P_ARP_ON] && !arpHold)
            arpRebuild();
    }
}

void Engine::allOff()
{
    nKeys = 0;
    arpN = 0;
    arpI = -1;
    anyHeld = false;
    rHeld = 0;
    if (arpPlaying >= 0)
        arpPlaying = -1;
    for (auto &s : sounding)
        s = 0;
    for (auto &v : V) {
        v.gate = v.sus = false;
        v.pend = false;
        releaseVoice(v);
    }
}
void Engine::panic()
{
    allOff();
    for (auto &v : V) {
        v.fst = v.ast = ES_IDLE;
        v.fe = v.ae = 0.0f;
        v.on = false;
        v.key = -1;
        v.kill = 1.0f;
    }
    fx.clear();
    std::memset(hp, 0, sizeof hp);
}

void Engine::host(bool hp_, double ppq, double bpm, bool sync)
{
    hostSync = sync;
    if (bpm > 10.0)
        hostBpm = bpm;
    if (sync && hp_ && !hostPlaying) {
        seqS.idx = -1;
        arpS.idx = -1;
    }
    if (sync && !hp_ && hostPlaying && !internalRun) {
        for (int k = 0; k < PSEQ_NOTES; k++)
            if (sounding[k]) {
                stopNote(K_SEQ + k);
                sounding[k] = 0;
            }
        pStep = -1;
    }
    hostPlaying = hp_;
    hostPpq = ppq;
    if (sync && hp_)
        pos = ppq;
}

/* ------------------------------------------------------------------- the transport --- */
void Engine::play()
{
    internalRun = !internalRun;                  /* (PLAY starts, or pauses) */
    if (internalRun)
        seqS.idx = -1;
}
void Engine::stop()
{
    internalRun = false;
    for (int k = 0; k < PSEQ_NOTES; k++)
        if (sounding[k]) {
            stopNote(K_SEQ + k);
            sounding[k] = 0;
        }
}
void Engine::stopTwice()
{
    stop();
    pos = 0.0;
    pStep = -1;
    seqS.idx = -1;
    allOff();
}

/* ------------------------------------------------------------------- REC --- */
void Engine::rec(bool on)
{
    const int16_t *P = prog.p;
    if (P[P_ARP_ON] && !P[P_SEQ_ON]) {
        arpHold = on;                            /* the arpeggiator: hold */
        if (!on)
            arpRebuild();
        return;
    }
    if (!P[P_SEQ_ON]) {
        recording = false;
        return;
    }
    recording = on;
    if (on) {
        rStep = pStep >= 0 && !clockRunning() ? pStep : 0;
        rHeld = 0;
        rFresh = true;
    }
}
void Engine::seqRest()
{
    if (!recording)
        return;
    for (int k = 0; k < PSEQ_NOTES; k++)
        prog.seq.note[rStep][k] = PN_REST;
    rStep = (rStep + 1) % std::max(1, (int)prog.p[P_SEQ_STEPS]);
    rFresh = true;
}
void Engine::seqTie()
{
    if (!recording)
        return;
    for (int k = 0; k < PSEQ_NOTES; k++)
        prog.seq.note[rStep][k] = k == 0 ? PN_TIE : PN_REST;
    rStep = (rStep + 1) % std::max(1, (int)prog.p[P_SEQ_STEPS]);
    rFresh = true;
}
void Engine::seqScroll(int d)
{
    int n = std::max(1, (int)prog.p[P_SEQ_STEPS]);
    rStep = ((rStep + d) % n + n) % n;
    rFresh = true;
}
int Engine::seqStepShown() const
{
    if (recording)
        return rStep;
    return prog.p[P_SEQ_ON] && clockRunning() ? pStep : -1;
}

/* ------------------------------------------------------------------- the clock --- */
/* a step's edge on the grid of a division, with swing (50..75: the first of each pair longer) and the gate */
bool Engine::stepEdge(Stepper &s, int div, int swing, int gate, bool &gateOff)
{
    const double at = clockRunning() ? pos : fpos;
    const double bps = DIV_BEATS[std::min(std::max(div, 0), NDIV - 1)];
    const double sw = (double)swing / 100.0, x = at / bps;
    long pair = (long)std::floor(x * 0.5);
    double f = x - (double)pair * 2.0;
    long idx;
    double start, len;
    if (f < 2.0 * sw) {
        idx = pair * 2;
        start = (double)pair * 2.0;
        len = 2.0 * sw;
    } else {
        idx = pair * 2 + 1;
        start = (double)pair * 2.0 + 2.0 * sw;
        len = 2.0 - 2.0 * sw;
    }
    gateOff = false;
    if (s.offAt >= 0.0 && at >= s.offAt) {
        gateOff = true;
        s.offAt = -1.0;
    }
    if (idx != s.idx) {
        s.idx = idx;
        s.offAt = (start + len * (double)gate / 100.0) * bps;
        if (gate >= 100)
            s.offAt = (start + len + 0.001) * bps;
        return true;
    }
    return false;
}

void Engine::clockChunk()
{
    const int16_t *P = prog.p;
    const float dt = (float)CH / sr;
    bpmNow = hostSync && hostPlaying ? (float)hostBpm : (float)P[P_BPM];
    double adv = (double)(dt * bpmNow / 60.0f);
    fpos += adv;
    if (clockRunning())
        pos += adv;
    bool off;
    if (P[P_ARP_ON] && !P[P_SEQ_ON]) {
        if (stepEdge(arpS, P[P_ARP_DIV], P[P_ARP_SWING], P[P_ARP_GATE], off)) {
            if (off && arpPlaying >= 0) {
                stopNote(K_ARP);
                arpPlaying = -1;
            }
            arpStep();
        } else if (off && arpPlaying >= 0) {
            stopNote(K_ARP);
            arpPlaying = -1;
        }
    }
    if (P[P_SEQ_ON] && clockRunning()) {
        if (stepEdge(seqS, P[P_SEQ_DIV], P[P_SEQ_SWING], P[P_SEQ_GATE], off))
            seqStep();
        else if (off) {
            /* the gate closes: the step's notes let go, unless the next step ties them on */
            int n = std::max(1, (int)P[P_SEQ_STEPS]);
            int nx = pStep >= 0 ? (pStep + 1) % n : 0;
            if (prog.seq.note[nx][0] != PN_TIE)
                for (int k = 0; k < PSEQ_NOTES; k++)
                    if (sounding[k]) {
                        stopNote(K_SEQ + k);
                        sounding[k] = 0;
                    }
        }
    }
}

void Engine::arpRebuild()
{
    arpN = 0;
    for (int i = 0; i < nKeys && arpN < NKEYS; i++) {
        arpNotes[arpN] = keys[i];
        arpVel[arpN++] = keyVel[i];
    }
    if (arpN == 0) {
        arpI = -1;
        if (arpPlaying >= 0) {
            stopNote(K_ARP);
            arpPlaying = -1;
        }
    }
}

void Engine::arpStep()
{
    const int16_t *P = prog.p;
    if (arpPlaying >= 0) {
        stopNote(K_ARP);
        arpPlaying = -1;
    }
    if (arpN == 0)
        return;
    int8_t order[NKEYS];
    float ov[NKEYS];
    int n = arpN;
    for (int i = 0; i < n; i++) {
        order[i] = arpNotes[i];
        ov[i] = arpVel[i];
    }
    int mode = P[P_ARP_MODE];
    if (mode != AM_ORD)
        for (int i = 1; i < n; i++)
            for (int j = i; j > 0 && order[j] < order[j - 1]; j--) {
                std::swap(order[j], order[j - 1]);
                std::swap(ov[j], ov[j - 1]);
            }
    switch (mode) {
    case AM_DOWN: arpI = arpI <= 0 || arpI >= n ? n - 1 : arpI - 1; break;
    case AM_INC:                                 /* up and down, the ends played twice */
    case AM_EXC: {                               /* up and down, the ends once */
        int cyc = mode == AM_INC ? 2 * n : std::max(1, 2 * n - 2);
        arpDir = (arpDir + 1) % std::max(1, cyc);
        int k = arpDir;
        if (mode == AM_INC)
            arpI = k < n ? k : 2 * n - 1 - k;
        else
            arpI = n == 1 ? 0 : (k < n ? k : 2 * n - 2 - k);
        break;
    }
    case AM_RND: arpI = (int)(rndU(seed) % (uint32_t)n); break;
    default: arpI = arpI + 1 >= n ? 0 : arpI + 1;
    }
    arpI = std::min(std::max(arpI, 0), n - 1);
    /* PROB: whether this step plays */
    if (rnd01(seed) * 100.0f >= (float)P[P_ARP_PROB])
        return;
    int note = order[arpI];
    arpPlaying = note;
    anyHeld = true;
    playNote(K_ARP, 0, note, ov[arpI], false);
}

void Engine::seqStep()
{
    const int16_t *P = prog.p;
    int n = std::max(1, (int)P[P_SEQ_STEPS]);
    /* the step from the song's position: the same place whatever program is playing */
    pStep = (int)(((seqS.idx % n) + n) % n);
    const uint8_t *st = prog.seq.note[pStep];
    if (st[0] == PN_TIE)
        return;
    for (int k = 0; k < PSEQ_NOTES; k++)
        if (sounding[k]) {
            stopNote(K_SEQ + k);
            sounding[k] = 0;
        }
    if (rnd01(seed) * 100.0f >= (float)P[P_SEQ_PROB])
        return;
    for (int k = 0; k < PSEQ_NOTES; k++)
        if (st[k] >= 2) {
            int note = std::min(127, std::max(0, st[k] - 2));
            anyHeld = true;
            playNote(K_SEQ + k, 0, note, prog.seq.vel[pStep][k] / 127.0f, false);
            sounding[k] = st[k];
        }
}

/* --------------------------------------------------------------- the voice --- */
void Engine::voiceChunk(Voice &v, int idx, int os)
{
    const int16_t *P = prog.p;
    const float dt = (float)CH / sr, fos = sr * (float)os, ifos = 1.0f / fos;
    const float inst = (float)P[P_INSTAB] / 100.0f;
    const int st = stackSize();

    /* INSTABILITY's slow wander (a little always: it is analog) */
    if (--v.drn <= 0) {
        v.dt = rndF(v.seed) * (0.015f + 0.12f * inst);
        v.drn = (int)((0.3f + 0.6f * rnd01(v.seed)) / dt);
    }
    v.dr += (v.dt - v.dr) * (1.0f - std::exp(-dt / 0.4f));

    /* glide */
    if (v.gr > 0.0f) {
        float s = v.gr * dt;
        v.p = v.p < v.t ? minf(v.p + s, v.t) : maxf(v.p - s, v.t);
    } else
        v.p = v.t;

    /* ---- the LFOs (this chunk's end) ---- */
    float eff[NPARAM];
    for (int p = 0; p < NPARAM; p++)
        if (PARAMS[p].voice)
            eff[p] = norm(p, P[p]);
    float src[NMODSRC];
    src[MS_LFO1] = v.l1v;
    src[MS_LFO2] = v.l2v;
    src[MS_VCAEG] = P[P_ENV_SHARED] ? v.fe : v.ae;
    src[MS_FILTEREG] = v.fe;
    src[MS_VEL] = v.vel;
    src[MS_MW] = ctl[1];
    src[MS_AT] = maxf(ctl[2], v.at);
    src[MS_KEY] = clampf(((float)v.note - 60.0f) / 60.0f, -1.0f, 1.0f);
    src[MS_CC74] = 2.0f * ((mpe ? v.cc74 : cc74G) - 0.5f);   /* (centred: 64 moves nothing) */
    for (int i = 0; i < nvR; i++) {
        const Route &r = vRoutes[i];
        eff[r.dst] += r.amt * src[r.src];
    }
    for (int p = 0; p < NPARAM; p++)
        if (PARAMS[p].voice)
            eff[p] = clampf(eff[p], 0.0f, 1.0f);

    const int sync = P[P_LFO_SYNC];
    const bool bpmSync = sync == LS_BPM || sync == LS_BPMKEY;
    const float beatHz = bpmNow / 60.0f;
    {
        /* LFO 2 first: it moves LFO 1's rate (XMOD) */
        float hz2 = bpmSync ? beatHz / LFODIV_BEATS[lfoDivOf(eff[P_L2_RATE])] : lfo2Hz(eff[P_L2_RATE]);
        int w2 = P[P_L2_WAV];
        if (w2 >= LW_ENVP) {                     /* one falling cycle a note */
            if (!v.l2done) {
                v.l2ph += hz2 * dt;
                if (v.l2ph >= 1.0f) {
                    v.l2ph = 1.0f;
                    v.l2done = true;
                }
            }
            float y = 1.0f - v.l2ph;
            v.l2v = w2 == LW_ENVP ? y : -y;
        } else {
            if (sync == LS_BPM)
                v.l2ph = (float)std::fmod((clockRunning() ? pos : fpos) / LFODIV_BEATS[lfoDivOf(eff[P_L2_RATE])], 1.0);
            else
                v.l2ph += hz2 * dt;
            if (v.l2ph >= 1.0f) {
                v.l2ph -= std::floor(v.l2ph);
                v.l2sh = rndF(v.seed);
            }
            v.l2v = lfoWave(w2, v.l2ph, v.l2sh);
        }
        float xm = std::exp2(eff[P_L2_XMOD] * eff[P_L2_XMOD] * 4.0f * v.l2v);
        float hz1 = (bpmSync ? beatHz / LFODIV_BEATS[lfoDivOf(eff[P_L1_RATE])] : lfo1Hz(eff[P_L1_RATE])) * xm;
        int w1 = P[P_L1_WAV];
        if (w1 >= LW_ENVP) {                     /* one cycle a note: FADE IN its rise against its fall */
            if (!v.l1done) {
                v.l1ph += hz1 * dt;
                if (v.l1ph >= 1.0f) {
                    v.l1ph = 1.0f;
                    v.l1done = true;
                }
            }
            float r = clampf(eff[P_L1_FADE], 0.02f, 0.98f), ph = v.l1ph;
            float y = v.l1done ? 0.0f : ph < r ? ph / r : 1.0f - (ph - r) / (1.0f - r);
            v.l1v = w1 == LW_ENVP ? y : -y;
            v.fade = 1.0f;
        } else {
            if (sync == LS_BPM && eff[P_L2_XMOD] < 0.001f)
                v.l1ph = (float)std::fmod((clockRunning() ? pos : fpos) / LFODIV_BEATS[lfoDivOf(eff[P_L1_RATE])], 1.0);
            else
                v.l1ph += hz1 * dt;
            if (v.l1ph >= 1.0f) {
                v.l1ph -= std::floor(v.l1ph);
                v.l1sh = rndF(v.seed);
            }
            v.l1v = lfoWave(w1, v.l1ph, v.l1sh);
            /* FADE IN: from nothing at the note to all of it */
            float ft = eff[P_L1_FADE] * eff[P_L1_FADE] * 10.0f;
            v.fade = ft < 0.002f ? 1.0f : minf(1.0f, v.fade + dt / ft);
        }
    }

    /* ---- this chunk's targets ---- */
    const float l1 = v.l1v * v.fade, l2 = v.l2v;
    float bendS = ctl[0] * (float)P[P_PB_RANGE] + (mpe ? v.bend * mpePb : 0.0f);
    float dtn = eff[P_DTN];
    float uni = st > 1 ? v.det * dtn * dtn * 0.5f : 0.0f;
    float l1semi = 24.0f * eff[P_L1_VCO] * eff[P_L1_VCO] * l1;
    int tgt = P[P_L1_TGT];
    /* INSTABILITY: mostly the tuning (each VCO its own offset, quickly more than a little), the tracking of each
     * VCO's expo converter a hair off (a little stretch or squeeze over the keyboard: always, as in any analog) */
    const float wonk = 0.5f * inst * std::sqrt(inst);
    const float trk1 = 0.0012f * v.vr[6] * (0.4f + inst), trk2 = 0.0012f * v.vr[7] * (0.4f + inst);
    float s1 = v.p + semisOf(P_VCO1_TUNE, eff[P_VCO1_TUNE]) + bendS + uni + v.dr + wonk * v.vr[0] + trk1 * (v.p - 60.0f) +
               (tgt != LT_VCO2 ? l1semi : 0.0f);
    float s2 = v.p + semisOf(P_VCO2, eff[P_VCO2]) + dtn * dtn * 1.0f + bendS + uni + v.dr * 0.8f + wonk * v.vr[1] + trk2 * (v.p - 60.0f) +
               (tgt != LT_VCO1 ? l1semi : 0.0f);
    float morph = clampf(eff[P_WAVE] + l2 * eff[P_L2_WAVE] * 0.75f, 0.0f, 1.0f);
    float pwb = eff[P_PW] + 0.03f * inst * v.vr[2];
    /* (each comparator's threshold a little its own: the squares never quite square) */
    float duty = (1.0f - 0.996f * clampf(pwb, 0.0f, 1.0f)) * (1.0f + 0.012f * v.vr[14]);
    float duty1 = (1.0f - 0.996f * clampf(pwb + l2 * eff[P_L2_PW] * 0.5f, 0.0f, 1.0f)) * (1.0f - 0.012f * v.vr[14]);
    float sn = eff[P_SUBNOISE] * 2.0f - 1.0f;
    float m = eff[P_VCOMIX];
    /* the crossfader: each alone at full level, both halfway at the middle (two VCOs together sound no louder than
     * one, as the Artemis' do) */
    float g1 = 1.0f - m, g2 = m;
    float trk = P[P_TRACK] == TR_FULL ? 1.0f : P[P_TRACK] == TR_HALF ? 0.5f : 0.0f;
    float cut = eff[P_CUT] * 10.0f + trk * (v.p - 60.0f) / 12.0f + 6.0f * eff[P_L1_VCF] * eff[P_L1_VCF] * l1 +
                0.35f * inst * v.vr[3];
    float ampVel = (float)P[P_AMP_VEL] / 100.0f;
    float amp = (1.0f - ampVel * (1.0f - v.vel)) * VOICE_GAIN / std::sqrt((float)st);

    float tg[15] = {s1, s2, cut, morph, duty, duty1, 0.55f * g1, 0.55f * g2, 0.30f * maxf(0.0f, -sn),
                    0.45f * maxf(0.0f, sn) * maxf(0.0f, sn), 6.0f * eff[P_FM] * eff[P_FM], 6.0f * eff[P_FFM] * eff[P_FFM],
                    eff[P_RES], 9.0f * eff[P_F_ENV], amp};
    float *sv[15] = {&v.s1, &v.s2, &v.sCut, &v.sMorph, &v.sDuty, &v.sDuty1, &v.sG1, &v.sG2, &v.sSub, &v.sNoise, &v.sFm,
                     &v.sFfm, &v.sRes, &v.sEnv, &v.sAmp};
    if (!v.sOk) {
        for (int i = 0; i < 15; i++)
            *sv[i] = tg[i];
        v.sOk = true;
    }
    const int n = CH * os;
    const float inv = 1.0f / (float)n;
    float cur[15], stp[15];
    for (int i = 0; i < 15; i++) {
        cur[i] = *sv[i];
        stp[i] = (tg[i] - cur[i]) * inv;
        *sv[i] = tg[i];
    }
    /* the oscillators' increments, ramped (from the last chunk's pitch to this one's) */
    float i1a = hzOf(cur[0]) * ifos, i1b = hzOf(tg[0]) * ifos, i2a = hzOf(cur[1]) * ifos, i2b = hzOf(tg[1]) * ifos;
    float di1 = (i1b - i1a) * inv, di2 = (i2b - i2a) * inv;
    float inc1 = i1a, inc2 = i2a;

    /* the envelopes' rates (INSTABILITY: each voice's times a little its own) */
    const float tsc = 1.0f + 0.25f * inst * v.vr[4];
    auto rates = [&](int a, int d, int s, int r, float &kA, float &kD, float &kR, float &sus) {
        kA = 1.0f - std::exp(-1.466f / (attSec(eff[a]) * tsc * fos));
        kD = 1.0f - std::exp(-4.6f / (decSec(eff[d]) * tsc * fos));
        kR = 1.0f - std::exp(-4.6f / (decSec(eff[r]) * tsc * fos));
        sus = eff[s];
    };
    float fA, fD, fR, fS, aA, aD, aR, aS;
    rates(P_FE_A, P_FE_D, P_FE_S, P_FE_R, fA, fD, fR, fS);
    rates(P_AE_A, P_AE_D, P_AE_S, P_AE_R, aA, aD, aR, aS);
    const bool shared = P[P_ENV_SHARED] != 0;
    auto envStep = [](float &e, uint8_t &stt, float kA, float kD, float kR, float sus) {
        switch (stt) {
        case ES_ATT:
            e += (1.3f - e) * kA;
            if (e >= 1.0f) {
                e = 1.0f;
                stt = ES_DEC;
            }
            break;
        case ES_DEC: e += (sus - e) * kD; break;
        case ES_REL: e += (0.0f - e) * kR; break;
        default: e = 0.0f;
        }
    };

    /* the filter */
    const bool lp12 = P[P_LP12] != 0;
    const int drv = P[P_DRIVE];
    const float dg = drv == DR_LOW ? 2.0f : drv == DR_MID ? 3.5f : drv == DR_HIGH ? 6.0f : 1.0f;
    const float dcomp = 1.0f / (1.0f + 0.18f * (dg - 1.0f));
    const bool noOsc = drv == DR_NOOSC;
    const float wmax = 0.42f * fos, wk = 2.0f * PI / fos;
    const bool fmEnv = P[P_FM_ENV] != 0, sync2 = P[P_SYNC] != 0, ffmNoise = P[P_FFM_NOISE] != 0;
    const float steal = 1.0f / (0.003f * fos);
    /* SPREAD: odd voices (1, 3, 5) left, even right */
    float pan = (idx & 1 ? 1.0f : -1.0f) * gSpread;
    const float gl = minf(1.0f, 1.0f - pan), gr = minf(1.0f, 1.0f + pan);

    const float bow = 0.05f + 0.025f * v.vr[8], bow2 = 0.05f - 0.025f * v.vr[8];
    const float kBuf = 1.0f - std::exp(-2.0f * PI * 15500.0f * ifos), kAc = 1.0f - 2.0f * PI * 11.0f * ifos;
    /* the filter's parts: its four capacitors a little apart, its input pair's offset (even harmonics) */
    const float gm[4] = {1.0f + 0.016f * v.vr[9], 1.0f + 0.016f * v.vr[10], 1.0f + 0.016f * v.vr[11], 1.0f + 0.016f * v.vr[12]};
    const float bias = 0.05f + 0.03f * v.vr[13], bias0 = sat(bias);
    Shape sh1, sh2;
    for (int s = 0; s < n; s++) {
        for (int i = 2; i < 15; i++)
            cur[i] += stp[i];
        inc1 += di1;
        inc2 += di2;
        /* the envelopes */
        envStep(v.fe, v.fst, fA, fD, fR, fS);
        if (!shared)
            envStep(v.ae, v.ast, aA, aD, aR, aS);
        else {
            v.ae = v.fe;
            v.ast = v.fst;
        }
        float ae = v.ae;
        if (v.kill < 1.0f) {                     /* taken: out on a smooth curve, then the note it was taken for */
            v.kill -= steal;
            if (v.kill <= 0.0f) {
                v.kill = 1.0f;
                v.fe = v.ae = 0.0f;
                v.fst = v.ast = ES_IDLE;
                if (v.pend) {
                    v.pend = false;
                    v.on = false;
                    startVoice(v, v.pkey, v.pch, v.pnote, v.pvel, v.pdet, true);
                } else {
                    v.on = false;
                    v.key = -1;
                }
                continue;
            }
            float f = v.kill;
            ae *= f * f * (3.0f - 2.0f * f);
        }
        const float fe = v.fe;

        /* VCO 1, frequency-modulated through zero by VCO 2 (its last sample), then VCO 2 (synced to it) */
        sh1.set(cur[3], cur[5]);
        sh2.set(cur[3], cur[4]);
        float idx_ = cur[10] * (fmEnv ? fe : 1.0f);
        v.pn += (rndF(v.seed) - v.pn) * 0.002f;             /* (the pitch never perfectly still: a breath of jitter) */
        float i1 = inc1 * (1.0f + idx_ * v.o2) * (1.0f + 0.00006f * v.pn);
        float a1 = std::fabs(i1);
        float p1 = v.ph1 + i1;
        bool wrapped = false;
        float since = 0.0f;
        if (p1 >= 1.0f) {
            p1 -= 1.0f;
            wrapped = true;
            since = a1 > 0.0f ? p1 / a1 : 0.0f;
        } else if (p1 < 0.0f) {
            p1 += 1.0f;
            wrapped = true;
            since = a1 > 0.0f ? (1.0f - p1) / a1 : 0.0f;
        }
        if (p1 >= 1.0f || p1 < 0.0f)
            p1 -= std::floor(p1);
        v.ph1 = p1;
        float y1 = sh1.tick(p1, minf(a1, 0.49f));
        float p2 = v.ph2 + inc2 * (1.0f - 0.00005f * v.pn);
        if (p2 >= 1.0f)
            p2 -= 1.0f;
        float corr = 0.0f;
        if (sync2 && wrapped) {
            /* hard sync: VCO 2 back to its start when VCO 1 comes round; the jump smoothed (what tick() smooths
             * at the start by itself taken off) */
            since = minf(since, 0.999f);
            float before = sh2.at(clampf(v.ph2 + inc2 * (1.0f - since), 0.0f, 0.99999f));
            p2 = inc2 * since;
            float after = sh2.at(p2);
            float self = (sh2.ws + sh2.wq - sh2.wt) - (sh2.d >= 0.999f ? (-sh2.ws - sh2.wq - sh2.wt) : 0.0f);
            corr = ((after - before) - self) * blepR(since);
        }
        v.ph2 = p2;
        float y2 = sh2.tick(p2, minf(inc2, 0.49f)) + corr;
        v.o2 = y2;
        /* the sub: a square an octave under VCO 1 */
        float ys = 0.0f;
        if (cur[8] > 1e-4f) {
            float is = inc1 * 0.5f;
            v.phs += is;
            if (v.phs >= 1.0f)
                v.phs -= 1.0f;
            ys = v.phs < 0.5f ? 1.0f : -1.0f;
            float t;
            if ((t = tauOf(v.phs, 0.0f, is)) < 1.5f)
                ys += 2.0f * blepR(t);
            if ((t = tauOf(v.phs, 0.5f, is)) < 1.5f)
                ys -= 2.0f * blepR(t);
        }
        float nz = rndF(v.seed);
        /* each VCO's output as its circuit gives it: the core's ramp a little bowed (the capacitor charging), the
         * buffer's top end rounded, its coupling capacitor letting the square's top sag (round, Roland-like) */
        float b1 = y1 + bow * (y1 * y1 - 0.33f), b2 = y2 + bow2 * (y2 * y2 - 0.33f);
        v.lp1 += (b1 - v.lp1) * kBuf;
        v.lp2 += (b2 - v.lp2) * kBuf;
        v.ac1 = v.lp1 - v.acx1 + kAc * v.ac1;
        v.acx1 = v.lp1;
        v.ac2 = v.lp2 - v.acx2 + kAc * v.ac2;
        v.acx2 = v.lp2;
        float x = cur[6] * v.ac1 + cur[7] * v.ac2 + cur[8] * ys + cur[9] * nz;
        x += 3e-5f * nz;                         /* (the circuit's own hiss) */

        /* the cutoff, per sample: the envelope, FFM at audio rate */
        float oct = cur[2] + cur[13] * fe + cur[11] * (ffmNoise ? nz : y2);
        float hz = minf(20.0f * fexp2(clampf(oct, -0.5f, 10.3f)), wmax);
        float res = cur[12], y;
        x *= dg;
        if (!lp12) {
            /* 24 dB: four OTA stages, each saturating, the resonance from the last (averaged over two samples) */
            float g = 1.0f - fexp2(-wk * hz * 1.4427f * 1.06f);
            float k = minf(res * 1.24f, noOsc ? 0.93f : 1.3f);
            /* the resonance through its own OTA (it rounds off: creamy, it sings without screaming), a part of the
             * input given back to it (the bass lost to the resonance partly made up, less lost than most) */
            float fb = 0.5f * (v.f[3] + v.ffb);
            v.ffb = v.f[3];
            float xin = 0.6f * x;
            float fbv = softT(1.15f * (fb - 0.22f * xin)) * (1.0f / 1.15f);
            float u = sat(xin - 4.0f * k * fbv + bias) - bias0;
            v.f[0] += g * gm[0] * ota(u - v.f[0]);
            v.f[1] += g * gm[1] * ota(v.f[0] - v.f[1]);
            v.f[2] += g * gm[2] * ota(v.f[1] - v.f[2]);
            v.f[3] += g * gm[3] * ota(v.f[2] - v.f[3]);
            y = v.f[3] * (1.0f + 0.22f * k) * 1.5f;
        } else {
            /* 12 dB: a 2-pole state variable of OTAs, its band saturating */
            float g = tanW(minf(PI * hz * ifos, 1.2f));
            float kd = maxf(2.0f - 2.15f * std::pow(res, 0.8f), noOsc ? 0.12f : -0.15f);
            float a1_ = 1.0f / (1.0f + g * (g + kd)), a2 = g * a1_;
            float in = sat(0.6f * x);
            float v3 = in - v.f[1], v1 = a1_ * v.f[0] + a2 * v3, v2 = v.f[1] + g * v1;
            v.f[0] = 3.0f * softT((2.0f * v1 - v.f[0]) * (1.0f / 3.0f));
            v.f[1] = 2.0f * v2 - v.f[1];
            y = v2 * 1.1f / (1.0f + 0.4f * res);       /* (quieter than the 24 dB, as the Artemis' is) */
        }
        y *= dcomp;
        /* the OTA VCA: its own gentle curve at the top */
        float out = softT(y * ae * 1.4f) * (1.0f / 1.4f) * cur[14];
        bufL[s] += out * gl;
        bufR[s] += out * gr;
    }
    /* finished: its amplifier's envelope at its end, its key let go */
    if (v.ast == ES_REL && v.ae < 2.5e-4f && !v.pend) {
        v.fe = v.ae = 0.0f;
        v.fst = v.ast = ES_IDLE;
        v.on = false;
    }
    for (float &f : v.f)
        f = flushDn(f);
    v.ffb = flushDn(v.ffb);
}

/* ------------------------------------------------------------------ render --- */
void Engine::chunk()
{
    const int16_t *P = prog.p;
    const float dt = (float)CH / sr;
    {
        const float in[3] = {bendG, mwV, atG}, kc = 1.0f - std::exp(-dt / 0.005f);
        for (int i = 0; i < 3; i++)
            ctl[i] = flushDn(ctl[i] + (in[i] - ctl[i]) * kc);
    }
    clockChunk();

    /* the monophonic sources (for the global destinations) */
    {
        const int sync = P[P_LFO_SYNC];
        const bool bpmSync = sync == LS_BPM || sync == LS_BPMKEY;
        const float bh = bpmNow / 60.0f;
        float x1 = norm(P_L1_RATE, P[P_L1_RATE]), x2 = norm(P_L2_RATE, P[P_L2_RATE]);
        float h1 = bpmSync ? bh / LFODIV_BEATS[lfoDivOf(x1)] : lfo1Hz(x1);
        float h2 = bpmSync ? bh / LFODIV_BEATS[lfoDivOf(x2)] : lfo2Hz(x2);
        gL1ph += h1 * dt;
        gL2ph += h2 * dt;
        if (gL1ph >= 1.0f) {
            gL1ph -= std::floor(gL1ph);
            gL1sh = rndF(seed);
        }
        if (gL2ph >= 1.0f) {
            gL2ph -= std::floor(gL2ph);
            gL2sh = rndF(seed);
        }
        int w1 = P[P_L1_WAV], w2 = P[P_L2_WAV];
        gL1 = w1 >= LW_ENVP ? V[lastVoice].l1v : lfoWave(w1, gL1ph, gL1sh);
        gL2 = w2 >= LW_ENVP ? V[lastVoice].l2v : lfoWave(w2, gL2ph, gL2sh);
    }
    float geff[NPARAM];
    for (int p = 0; p < NPARAM; p++)
        geff[p] = norm(p, P[p]);
    if (ngR > 0) {
        const Voice &lv = V[lastVoice];
        float src[NMODSRC];
        src[MS_LFO1] = gL1;
        src[MS_LFO2] = gL2;
        src[MS_VCAEG] = P[P_ENV_SHARED] ? lv.fe : lv.ae;
        src[MS_FILTEREG] = lv.fe;
        src[MS_VEL] = lastVel;
        src[MS_MW] = ctl[1];
        float at = ctl[2];
        for (const auto &v : V)
            if (v.on)
                at = maxf(at, v.at);
        src[MS_AT] = at;
        src[MS_KEY] = clampf((lastKey - 60.0f) / 60.0f, -1.0f, 1.0f);
        src[MS_CC74] = 2.0f * ((mpe ? lv.cc74 : cc74G) - 0.5f);
        for (int i = 0; i < ngR; i++)
            geff[gRoutes[i].dst] += gRoutes[i].amt * src[gRoutes[i].src];
        for (int p = 0; p < NPARAM; p++)
            geff[p] = clampf(geff[p], 0.0f, 1.0f);
    }
    gSpread = geff[P_SPREAD];

    /* the voices, at the high rate */
    int os = quality ? 4 : 2;
    if (sr > 80000.0f)
        os = 2;
    if (sr > 160000.0f)
        os = 1;
    const int n = CH * os;
    std::memset(bufL, 0, sizeof(float) * (size_t)n);
    std::memset(bufR, 0, sizeof(float) * (size_t)n);
    for (int i = 0; i < NV; i++) {
        Voice &v = V[i];
        if (!v.on && !v.pend)
            continue;
        voiceChunk(v, i, os);
    }
    float bl[CH], br[CH];
    for (int i = 0; i < CH; i++) {
        if (os == 4) {
            float a0 = dnL[1].down(bufL[4 * i], bufL[4 * i + 1]), a1 = dnL[1].down(bufL[4 * i + 2], bufL[4 * i + 3]);
            float b0 = dnR[1].down(bufR[4 * i], bufR[4 * i + 1]), b1 = dnR[1].down(bufR[4 * i + 2], bufR[4 * i + 3]);
            bl[i] = dnL[0].down(a0, a1);
            br[i] = dnR[0].down(b0, b1);
        } else if (os == 2) {
            bl[i] = dnL[0].down(bufL[2 * i], bufL[2 * i + 1]);
            br[i] = dnR[0].down(bufR[2 * i], bufR[2 * i + 1]);
        } else {
            bl[i] = bufL[i];
            br[i] = bufR[i];
        }
    }
    dnL[0].flush();
    dnL[1].flush();
    dnR[0].flush();
    dnR[1].flush();
    /* the DC let go (6 Hz), then the 12 dB stereo high-pass (at the bottom out of the way) */
    {
        float hz = 20.0f * std::exp2(geff[P_HP_CUT] * 9.0f);
        float g = tanW(minf(PI * hz / sr, 1.2f)), k = 2.0f - 1.9f * std::pow(geff[P_HP_RES], 0.7f);
        float den = 1.0f / (1.0f + k * g + g * g);
        for (int i = 0; i < CH; i++) {
            float xl = bl[i], xr = br[i];
            dcL = xl - dcxL + dcK * dcL;
            dcR = xr - dcxR + dcK * dcR;
            dcxL = xl;
            dcxR = xr;
            float in[2] = {dcL, dcR};
            for (int c = 0; c < 2; c++) {
                float h = (in[c] - (k + g) * hp[c][0] - hp[c][1]) * den;
                float b = g * h + hp[c][0];
                hp[c][0] = g * h + b;
                float l = g * b + hp[c][1];
                hp[c][1] = g * b + l;
                in[c] = h;
            }
            bl[i] = in[0];
            br[i] = in[1];
        }
        dcL = flushDn(dcL);
        dcR = flushDn(dcR);
        for (auto &h : hp)
            for (float &x : h)
                x = flushDn(x);
    }
    /* the effects */
    {
        static const int TY[4] = {P_DIST_TYPE, P_MOD_TYPE, P_DLY_TYPE, P_REV_TYPE};
        FxPageBase *pg[4] = {&fx.dist, &fx.mod, &fx.delay, &fx.rev};
        for (int k = 0; k < 4; k++) {
            float v[5];
            for (int s = 0; s < 5; s++)
                v[s] = geff[TY[k] + 1 + s];
            pg[k]->set(P[TY[k]], v, bpmNow, P[P_DLY_SYNC] != 0);
        }
        fx.process(bl, br, CH);
    }
    /* PRESET LEVEL, then the output stage: clean to full scale, then rounding off */
    const float lev = std::pow(10.0f, (float)P[P_LEVEL] / 200.0f);
    for (int i = 0; i < CH; i++) {
        auto stage = [](float x) {
            float a = std::fabs(x);
            return a <= 1.0f ? x : (x > 0.0f ? 1.0f : -1.0f) * (1.0f + 0.5f * softT((a - 1.0f) * 2.0f));
        };
        outL[i] = stage(bl[i] * lev);
        outR[i] = stage(br[i] * lev);
    }
}

void Engine::render(float *L, float *R, int n)
{
    while (n > 0) {
        if (gpos >= CH) {
            chunk();
            gpos = 0;
        }
        int k = std::min(CH - gpos, n);
        std::memcpy(L, outL + gpos, sizeof(float) * (size_t)k);
        std::memcpy(R, outR + gpos, sizeof(float) * (size_t)k);
        gpos += k;
        L += k;
        R += k;
        n -= k;
    }
}

int Engine::voicesSounding() const
{
    int n = 0;
    for (const auto &v : V)
        n += v.on && v.ast != ES_IDLE;
    return n;
}
} // namespace hm
