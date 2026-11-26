/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
#include "Params.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace hm
{
static const char *const ONOFF[] = {"Off", "On"};
static const char *const L1TGT[] = {"VCO 1+2", "VCO 1", "VCO 2"};
static const char *const TRACKN[] = {"Zero", "Half", "Full"};
static const char *const LP12N[] = {"24 dB", "12 dB"};
static const char *const FFMSRC[] = {"VCO 2", "Noise"};
static const char *const FMENV[] = {"Manual", "Env"};
static const char *const GLIDEN[] = {"On", "Auto"};
static const char *const SHAREDN[] = {"Separate", "Shared"};
const char *const LFOWAVE_NAME[NLFOWAVE] = {"Sine", "Triangle", "Saw", "Ramp", "Square", "Random", "Env+", "Env-"};
const char *const LFOSYNC_NAME[NLFOSYNC] = {"Free", "Key", "BPM", "BPM+Key"};
const char *const PLAYMODE_NAME[NPLAYMODE] = {"Poly", "Tri", "Duo", "Unison", "Mono"};
const char *const DRIVE_NAME[NDRIVE] = {"No Osc", "Off", "Low", "Mid", "High"};
const char *const ARPMODE_NAME[NARPMODE] = {"Up", "Down", "Ord", "Inc", "Exc", "Rnd"};
const char *const DIST_NAME[NDIST] = {"Exponential", "Parabolic", "Sine Clip",  "S-Curve",   "Soft Clip",
                                      "Hard Clip",   "Tri Clip",  "Tri Fold",   "Single Fold", "Multi Fold",
                                      "Sine Bend",   "Sine Fold", "Sine Shred", "Bin Shred", "Sym Warp"};
const char *const MOD_NAME[NMOD] = {"Chorus",  "Ensemble",           "Tape Chorus",  "BBD Chorus",
                                    "Flanger", "BBD Flanger",        "Thru-Zero Flanger", "Phaser",
                                    "Barber-Pole Phaser", "Double Notch", "Pitch Shifter", "BBD Resonator"};
const char *const DELAY_NAME[NDELAY] = {"Stereo", "Ping-Pong", "BBD Delay", "BBD Playground", "Random Repeater"};
const char *const REV_NAME[NREV] = {"Small Reverb", "Large Reverb", "Huge Reverb", "Cloud Reverb", "Shimmer Reverb"};
const char *const DIV_NAME[NDIV] = {"1/1",  "1/2.", "1/2",   "1/2T", "1/4.",  "1/4",  "1/4T", "1/8.", "1/8",
                                    "1/8T", "1/16.", "1/16", "1/16T", "1/32.", "1/32", "1/32T", "1/64"};
const float DIV_BEATS[NDIV] = {4.0f,  3.0f,   2.0f,  4.0f / 3.0f, 1.5f,   1.0f,   2.0f / 3.0f,  0.75f, 0.5f,
                               1.0f / 3.0f, 0.375f, 0.25f, 1.0f / 6.0f, 0.1875f, 0.125f, 1.0f / 12.0f, 0.0625f};
const char *const LFODIV_NAME[NLFODIV] = {"x8",  "x4",   "x2",   "x1",   "1/2.", "1/2",   "1/2T", "1/4.", "1/4", "1/4T",
                                          "1/8.", "1/8", "1/8T", "1/16.", "1/16", "1/16T", "1/32", "1/32T", "1/64"};
const float LFODIV_BEATS[NLFODIV] = {32.0f, 16.0f, 8.0f,  4.0f,        3.0f,   2.0f,   4.0f / 3.0f,  1.5f,   1.0f, 2.0f / 3.0f,
                                     0.75f, 0.5f,  1.0f / 3.0f, 0.375f, 0.25f,  1.0f / 6.0f, 0.125f, 1.0f / 12.0f, 0.0625f};
const char *const MODSRC_NAME[NMODSRC] = {"LFO 1", "LFO 2", "VCA EG", "Filter EG", "Velocity", "Modwheel", "Aftertouch",
                                          "Key Track", "CC74"};

/* (voice: a destination per voice; mod: in the matrix at all) */
#define PC(id, name, lab, def, v) {id, name, lab, 0, 1000, def, K_PCT, nullptr, v, true}
#define BI(id, name, lab, def, v) {id, name, lab, 0, 1000, def, K_BIP, nullptr, v, true}
#define CH(id, name, lab, n, def, list) {id, name, lab, 0, (n) - 1, def, K_CHOICE, list, false, false}
#define ON(id, name, lab, def) {id, name, lab, 0, 1, def, K_ONOFF, ONOFF, false, false}
#define FX(id, name, lab, def) {id, name, lab, 0, 1000, def, K_FX, nullptr, false, true}
#define NU(id, name, lab, lo, hi, def) {id, name, lab, lo, hi, def, K_NUM, nullptr, false, false}

const ParamDef PARAMS[NPARAM] = {
    PC("l1_rate", "LFO 1 Rate", "RATE", 450, true),
    PC("l1_fade", "LFO 1 Fade In", "FADE IN", 0, true),
    PC("l1_vco", "LFO 1 > VCO", "VCO", 0, true),
    PC("l1_vcf", "LFO 1 > VCF", "VCF", 0, true),
    CH("l1_tgt", "LFO 1 VCO Target", "1/2", NLFOTGT, LT_BOTH, L1TGT),
    CH("l1_wav", "LFO 1 Wave", "WAV", NLFOWAVE, LW_TRI, LFOWAVE_NAME),
    PC("l2_rate", "LFO 2 Rate", "RATE", 300, true),
    PC("l2_xmod", "LFO 2 > LFO 1 Rate", "XMOD", 0, true),
    PC("l2_wave", "LFO 2 > Wave", "WAVE", 0, true),
    PC("l2_pw", "LFO 2 > PW", "PW", 0, true),
    CH("l2_wav", "LFO 2 Wave", "WAV", NLFOWAVE, LW_SINE, LFOWAVE_NAME),
    CH("lfo_sync", "LFO Sync", "SYNC", NLFOSYNC, LS_FREE, LFOSYNC_NAME),
    BI("subnoise", "Sub / Noise", "SUB NOISE", 500, true),
    BI("vcomix", "VCO 1 / VCO 2", "VCO1 VCO2", 0, true),
    PC("wave", "Wave", "WAVE", 0, true),
    ON("sync", "VCO 2 Sync", "SYNC", 0),
    CH("fm_env", "VCO FM Mode", "ENV", 2, 0, FMENV),
    {"vco2", "VCO 2", "VCO2", -24, 24, 0, K_SEMI, nullptr, true, true},
    PC("dtn", "Detune", "DTN", 60, true),
    PC("gld", "Glide", "GLD", 0, true),
    PC("pw", "Pulse Width", "PW", 0, true),
    PC("fm", "VCO FM", "FM", 0, true),
    PC("cut", "LPF Cutoff", "CUT", 1000, true),
    PC("res", "LPF Resonance", "RES", 0, true),
    CH("lp12", "LPF Poles", "12DB", 2, 0, LP12N),
    CH("track", "LPF Track", "TRK", NTRACK, TR_HALF, TRACKN),
    PC("hp_cut", "HPF Cutoff", "CUT", 0, false),
    PC("hp_res", "HPF Resonance", "RES", 0, false),
    PC("f_env", "LPF Cut EG Amount", "ENV", 0, true),
    PC("ffm", "LPF FFM", "FFM", 0, true),
    CH("ffm_noise", "LPF FFM Source", "NOISE", 2, 0, FFMSRC),
    PC("fe_a", "Filter EG Attack", "A", 0, true),
    PC("fe_d", "Filter EG Decay", "D", 400, true),
    PC("fe_s", "Filter EG Sustain", "S", 0, true),
    PC("fe_r", "Filter EG Release", "R", 300, true),
    PC("ae_a", "VCA EG Attack", "A", 0, true),
    PC("ae_d", "VCA EG Decay", "D", 400, true),
    PC("ae_s", "VCA EG Sustain", "S", 1000, true),
    PC("ae_r", "VCA EG Release", "R", 300, true),
    CH("env_shared", "Shared EG", "VCA", 2, 0, SHAREDN),
    PC("spread", "Spread", "SPREAD", 0, false),
    CH("play_mode", "Play Mode", "PLAY MODE", NPLAYMODE, PM_POLY, PLAYMODE_NAME),
    {"vco1_tune", "VCO 1 Tune", "VCO 1 TUNE", -24, 24, 0, K_SEMI, nullptr, true, true},
    CH("drive", "Drive", "DRIVE", NDRIVE, DR_OFF, DRIVE_NAME),
    ON("legato", "Legato", "LEGATO", 0),
    CH("glide_auto", "Glide Mode", "GLIDE", 2, 0, GLIDEN),
    {"level", "Preset Level", "PRESET LEVEL", -130, 130, 0, K_DB, nullptr, false, false},
    NU("instab", "Instability", "INSTABILITY", 0, 100, 20),
    ON("dly_sync", "Delays BPM Sync", "DELAYS BPM SYNC", 0),
    NU("amp_vel", "Amp Velocity", "AMP VELOCITY", 0, 100, 50),
    NU("pb_range", "Pitch Wheel", "PITCH WHEEL", 0, 12, 2),
    CH("dist_type", "Distortion Type", "DIST", NDIST, DT_SOFTCLIP, DIST_NAME),
    FX("dist_1", "Distortion 1", "1", 0), FX("dist_2", "Distortion 2", "2", 0), FX("dist_3", "Distortion 3", "3", 300),
    FX("dist_4", "Distortion 4", "4", 500), FX("dist_5", "Distortion 5", "5", 0),
    CH("mod_type", "Modulation Type", "MOD", NMOD, MT_CHORUS, MOD_NAME),
    FX("mod_1", "Modulation 1", "1", 500), FX("mod_2", "Modulation 2", "2", 300), FX("mod_3", "Modulation 3", "3", 500),
    FX("mod_4", "Modulation 4", "4", 500), FX("mod_5", "Modulation 5", "5", 0),
    CH("dly_type", "Delay Type", "DELAY", NDELAY, DL_STEREO, DELAY_NAME),
    FX("dly_1", "Delay 1", "1", 450), FX("dly_2", "Delay 2", "2", 520), FX("dly_3", "Delay 3", "3", 400),
    FX("dly_4", "Delay 4", "4", 400), FX("dly_5", "Delay 5", "5", 0),
    CH("rev_type", "Reverb Type", "REV", NREV, RV_LARGE, REV_NAME),
    FX("rev_1", "Reverb 1", "1", 150), FX("rev_2", "Reverb 2", "2", 600), FX("rev_3", "Reverb 3", "3", 550),
    FX("rev_4", "Reverb 4", "4", 450), FX("rev_5", "Reverb 5", "5", 0),
    ON("arp_on", "Arp On/Off", "ARP", 0),
    CH("arp_mode", "Arp Mode", "MODE", NARPMODE, AM_UP, ARPMODE_NAME),
    CH("arp_div", "Arp Division", "DIV", NDIV, 11, DIV_NAME),
    NU("arp_prob", "Arp Probability", "PROB", 0, 100, 100),
    NU("arp_swing", "Arp Swing", "SWNG", 50, 75, 50),
    NU("arp_gate", "Arp Gate", "GATE", 1, 100, 50),
    ON("seq_on", "Seq On/Off", "SEQ", 0),
    NU("seq_steps", "Seq Steps", "STEPS", 1, 64, 16),
    CH("seq_div", "Seq Division", "DIV", NDIV, 11, DIV_NAME),
    NU("seq_prob", "Seq Probability", "PROB", 0, 100, 100),
    NU("seq_swing", "Seq Swing", "SWNG", 50, 75, 50),
    NU("seq_gate", "Seq Gate", "GATE", 1, 100, 50),
    NU("bpm", "BPM", "BPM", 30, 300, 120),
};

/* ------------------------------------------------------------------------------ the effects --- */
int fxTypeParam(int page) { return page == FP_DIST ? P_DIST_TYPE : page == FP_MOD ? P_MOD_TYPE : page == FP_DELAY ? P_DLY_TYPE : P_REV_TYPE; }

const char *fxSliderName(int page, int t, int s)
{
    static const char *const DIST[5] = {"BITS", "RATE", "GAIN", "LEVEL", "MIX"};
    static const char *const MODC[5] = {"DPTH", "SPD", "FEED", "WDTH", "MIX"};
    static const char *const NOTCH[5] = {"DPTH", "SPD", "NTCH", "WDTH", "MIX"};
    static const char *const PITCH[5] = {"L", "R", "FEED", "DAMP", "MIX"};
    static const char *const RESO[5] = {"TIME", "OFST", "FEED", "SPRD", "MIX"};
    static const char *const STEREO[5] = {"L", "R", "FEED", "DAMP", "MIX"};
    static const char *const PING[5] = {"TIME", "PAN", "FEED", "DAMP", "MIX"};
    static const char *const BBD[5] = {"TIME", "FEED", "SPD", "DPTH", "MIX"};
    static const char *const REP[5] = {"SIZE", "REP", "PROB", "EG", "MIX"};
    static const char *const ROOM[5] = {"PRE", "SIZE", "FEED", "DAMP", "MIX"};
    static const char *const HUGE[5] = {"SIZE", "FEED", "SPD", "DPTH", "MIX"};
    static const char *const CLOUD[5] = {"TUNE", "SIZE", "FEED", "GRAIN", "MIX"};
    static const char *const SHIM[5] = {"TUNE", "SIZE", "FEED", "DAMP", "MIX"};
    s = s < 0 ? 0 : s > 4 ? 4 : s;
    switch (page) {
    case FP_DIST: return DIST[s];
    case FP_MOD: return t == MT_NOTCH ? NOTCH[s] : t == MT_PITCH ? PITCH[s] : t == MT_RESONATOR ? RESO[s] : MODC[s];
    case FP_DELAY:
        return t == DL_STEREO ? STEREO[s] : t == DL_PINGPONG ? PING[s] : t == DL_BBD ? BBD[s] : t == DL_PLAYGROUND ? RESO[s] : REP[s];
    default: return t == RV_HUGE ? HUGE[s] : t == RV_CLOUD ? CLOUD[s] : t == RV_SHIMMER ? SHIM[s] : ROOM[s];
    }
}

bool fxSliderBipolar(int page, int t, int s)
{
    const char *n = fxSliderName(page, t, s);
    if (!std::strcmp(n, "DAMP") || !std::strcmp(n, "OFST") || !std::strcmp(n, "GRAIN"))
        return true;
    if (page == FP_MOD && t == MT_PITCH && s < 2)
        return true;
    if (page == FP_REV && t == RV_SHIMMER && s == 0)
        return true;
    if (page == FP_MOD && (t == MT_FLANGER || t == MT_BBDFLANGER || t == MT_TZFLANGER) && s == 2)
        return true;                                  /* (positive or negative feedback) */
    return false;
}

void fxSliderText(int page, int t, int s, int v, bool synced, char *out, int n)
{
    float x = (float)v / 1000.0f, b = (float)(v - 500) / 5.0f;
    const char *nm = fxSliderName(page, t, s);
    auto is = [nm](const char *k) { return std::strcmp(nm, k) == 0; };
    if (page == FP_DIST && s == 0) {
        int bits = (int)std::lround(16.0f - 14.0f * x);
        std::snprintf(out, (size_t)n, v == 0 ? "OFF" : "%d BIT", bits);
        return;
    }
    if (page == FP_DIST && s == 1) {
        std::snprintf(out, (size_t)n, v == 0 ? "OFF" : "1/%d", 1 + (int)std::lround(x * x * 63.0f));
        return;
    }
    if (page == FP_DIST && s == 2) {
        std::snprintf(out, (size_t)n, "+%.0f dB", 40.0f * x);
        return;
    }
    if ((page == FP_MOD && t == MT_PITCH && s < 2) || (page == FP_REV && t == RV_SHIMMER && s == 0)) {
        std::snprintf(out, (size_t)n, "%+d st", (int)std::lround((x - 0.5f) * (page == FP_REV ? 48.0f : 24.0f)));
        return;
    }
    if (page == FP_DELAY && is("PAN")) {
        std::snprintf(out, (size_t)n, v < 500 ? "L > R" : "R > L");
        return;
    }
    if (page == FP_DELAY && synced && (is("TIME") || is("L") || is("R"))) {
        int i = (int)std::lround(x * (float)(NDIV - 1));
        std::snprintf(out, (size_t)n, "%s", DIV_NAME[NDIV - 1 - i]);
        return;
    }
    if (page == FP_DELAY && (is("TIME") || is("L") || is("R"))) {
        float ms = t == DL_PLAYGROUND ? 2.0f + 598.0f * x * x : 5.0f + 1995.0f * x * x;
        std::snprintf(out, (size_t)n, ms < 1000.0f ? "%.0f ms" : "%.2f s", ms < 1000.0f ? ms : ms / 1000.0f);
        return;
    }
    if (page == FP_MOD && t == MT_RESONATOR && is("TIME")) {
        std::snprintf(out, (size_t)n, "%.1f ms", 0.3f + 24.7f * x * x);
        return;
    }
    if (fxSliderBipolar(page, t, s)) {
        std::snprintf(out, (size_t)n, "%+.0f", b);
        return;
    }
    std::snprintf(out, (size_t)n, "%.0f", x * 100.0f);
}

/* -------------------------------------------------------------------------------- the text --- */
float lfo1Hz(float x) { return 0.1f * std::exp2(x * 7.966f); }   /* 0.1 .. 25 Hz */
float lfo2Hz(float x) { return 0.05f * std::exp2(x * 9.229f); }  /* 0.05 .. 30 Hz */
int lfoDivOf(float x)
{
    int i = (int)std::lround(x * (float)(NLFODIV - 1));
    return i < 0 ? 0 : i >= NLFODIV ? NLFODIV - 1 : i;
}

void paramText(int p, int v, char *out, int n)
{
    const ParamDef &d = PARAMS[p];
    switch (d.kind) {
    case K_PCT: std::snprintf(out, (size_t)n, "%.1f", (float)v / 10.0f); break;
    case K_BIP: std::snprintf(out, (size_t)n, "%+.1f", (float)(v - 500) / 5.0f); break;
    case K_SEMI: std::snprintf(out, (size_t)n, "%+d st", v); break;
    case K_DB: std::snprintf(out, (size_t)n, "%+.1f dB", (float)v / 10.0f); break;
    case K_CHOICE:
    case K_ONOFF: std::snprintf(out, (size_t)n, "%s", d.choice[v < d.lo ? d.lo : v > d.hi ? d.hi : v]); break;
    case K_FX: std::snprintf(out, (size_t)n, "%.0f", (float)v / 10.0f); break;
    default:
        if (p == P_ARP_PROB || p == P_SEQ_PROB || p == P_ARP_GATE || p == P_SEQ_GATE)
            std::snprintf(out, (size_t)n, "%d%%", v);
        else if (p == P_PB_RANGE)
            std::snprintf(out, (size_t)n, "%d st", v);
        else
            std::snprintf(out, (size_t)n, "%d", v);
    }
}

void seqClear(SeqData &s) { std::memset(&s, 0, sizeof s); }

void programInit(Program &p)
{
    std::memset(&p, 0, sizeof p);
    std::snprintf(p.name, NAME_LEN, "INIT");
    for (int i = 0; i < NPARAM; i++)
        p.p[i] = (int16_t)PARAMS[i].def;
    seqClear(p.seq);
}
} // namespace hm
