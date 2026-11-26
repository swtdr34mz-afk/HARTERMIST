/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
/* HARTERMIST's parameters, laid out as an Artemis program: every knob, slider and button of its panel, the MENU
 * page's settings, the four effect pages (their algorithm and their five sliders), the arpeggiator's and the
 * sequencer's five sliders. The continuous controls are 0..1000 (shown 0..100, as the Artemis shows them; the
 * bipolar ones -100..+100 with 500 the middle). The MOD MATRIX (each source's amount, -100..+100, to each control)
 * and the sequence live in the program beside them. */
#pragma once
#include <cstdint>

namespace hm
{
enum LfoWave { LW_SINE, LW_TRI, LW_SAW, LW_RAMP, LW_SQR, LW_RND, LW_ENVP, LW_ENVM, NLFOWAVE };
enum LfoSync { LS_FREE, LS_KEY, LS_BPM, LS_BPMKEY, NLFOSYNC };
enum Lfo1Tgt { LT_BOTH, LT_VCO1, LT_VCO2, NLFOTGT };
enum PlayMode { PM_POLY, PM_TRI, PM_DUO, PM_UNISON, PM_MONO, NPLAYMODE };
enum Drive { DR_NOOSC, DR_OFF, DR_LOW, DR_MID, DR_HIGH, NDRIVE };
enum Track { TR_ZERO, TR_HALF, TR_FULL, NTRACK };
enum ArpMode { AM_UP, AM_DOWN, AM_ORD, AM_INC, AM_EXC, AM_RND, NARPMODE };
enum DistType {
    DT_EXP, DT_PARABOLIC, DT_SINECLIP, DT_SCURVE, DT_SOFTCLIP, DT_HARDCLIP, DT_TRICLIP, DT_TRIFOLD, DT_SINGLEFOLD,
    DT_MULTIFOLD, DT_SINEBEND, DT_SINEFOLD, DT_SINESHRED, DT_BINSHRED, DT_SYMWARP, NDIST
};
enum ModType {
    MT_CHORUS, MT_ENSEMBLE, MT_TAPE, MT_BBDCHORUS, MT_FLANGER, MT_BBDFLANGER, MT_TZFLANGER, MT_PHASER, MT_BARBER,
    MT_NOTCH, MT_PITCH, MT_RESONATOR, NMOD
};
enum DelayType { DL_STEREO, DL_PINGPONG, DL_BBD, DL_PLAYGROUND, DL_REPEATER, NDELAY };
enum RevType { RV_SMALL, RV_LARGE, RV_HUGE, RV_CLOUD, RV_SHIMMER, NREV };
/* the effect pages */
enum FxPage { FP_DIST, FP_MOD, FP_DELAY, FP_REV, NFXPAGE };
/* the divisions of the clock (the arpeggiator, the sequencer, the delays synced) */
constexpr int NDIV = 17;
/* the LFOs synced: x8 .. 1/64, with the triplets and dotted */
constexpr int NLFODIV = 19;

enum Param {
    /* LFO 1 */
    P_L1_RATE, P_L1_FADE, P_L1_VCO, P_L1_VCF, P_L1_TGT, P_L1_WAV,
    /* LFO 2 (SYNC: both LFOs) */
    P_L2_RATE, P_L2_XMOD, P_L2_WAVE, P_L2_PW, P_L2_WAV, P_LFO_SYNC,
    /* VCO */
    P_SUBNOISE, P_VCOMIX, P_WAVE, P_SYNC, P_FM_ENV, P_VCO2, P_DTN, P_GLD, P_PW, P_FM,
    /* VCF (and the HIGH PASS at the end) */
    P_CUT, P_RES, P_LP12, P_TRACK, P_HP_CUT, P_HP_RES, P_F_ENV, P_FFM, P_FFM_NOISE,
    /* ENV: the filter's (and FM's), the VCA's, VCA's long press (one shared) */
    P_FE_A, P_FE_D, P_FE_S, P_FE_R, P_AE_A, P_AE_D, P_AE_S, P_AE_R, P_ENV_SHARED,
    /* MASTER OUT */
    P_SPREAD,
    /* the MENU page */
    P_PLAY_MODE, P_VCO1_TUNE, P_DRIVE, P_LEGATO, P_GLIDE_AUTO, P_LEVEL, P_INSTAB, P_DLY_SYNC, P_AMP_VEL, P_PB_RANGE,
    /* the effects: each page's algorithm and five sliders */
    P_DIST_TYPE, P_DIST_1, P_DIST_2, P_DIST_3, P_DIST_4, P_DIST_5,
    P_MOD_TYPE, P_MOD_1, P_MOD_2, P_MOD_3, P_MOD_4, P_MOD_5,
    P_DLY_TYPE, P_DLY_1, P_DLY_2, P_DLY_3, P_DLY_4, P_DLY_5,
    P_REV_TYPE, P_REV_1, P_REV_2, P_REV_3, P_REV_4, P_REV_5,
    /* the arpeggiator: MODE, DIV, PROB, SWNG, GATE */
    P_ARP_ON, P_ARP_MODE, P_ARP_DIV, P_ARP_PROB, P_ARP_SWING, P_ARP_GATE,
    /* the sequencer: STEPS, DIV, PROB, SWNG, GATE */
    P_SEQ_ON, P_SEQ_STEPS, P_SEQ_DIV, P_SEQ_PROB, P_SEQ_SWING, P_SEQ_GATE,
    P_BPM,
    NPARAM
};

enum Kind : uint8_t {
    K_PCT,      /* 0..1000, shown 0..100 */
    K_BIP,      /* 0..1000, 500 the middle: shown -100..+100 */
    K_NUM,      /* as it is */
    K_SEMI,     /* semitones (-24..+24) */
    K_CHOICE,
    K_ONOFF,
    K_DB,       /* tenths of a dB */
    K_FX        /* an effect slider: its meaning the algorithm's */
};

struct ParamDef {
    const char *id;           /* the host's id (stable: saved projects) */
    const char *name;         /* the host's and the display's name */
    const char *label;        /* the panel's legend */
    int lo, hi, def;
    Kind kind;
    const char *const *choice;
    bool voice;               /* a mod matrix destination per voice (else, if modulatable, a global one) */
    bool mod;                 /* a mod matrix destination */
};

extern const ParamDef PARAMS[NPARAM];
extern const char *const LFOWAVE_NAME[NLFOWAVE];
extern const char *const LFOSYNC_NAME[NLFOSYNC];
extern const char *const PLAYMODE_NAME[NPLAYMODE];
extern const char *const DRIVE_NAME[NDRIVE];
extern const char *const ARPMODE_NAME[NARPMODE];
extern const char *const DIST_NAME[NDIST];
extern const char *const MOD_NAME[NMOD];
extern const char *const DELAY_NAME[NDELAY];
extern const char *const REV_NAME[NREV];
extern const char *const DIV_NAME[NDIV];
extern const float DIV_BEATS[NDIV];          /* a division in quarter notes */
extern const char *const LFODIV_NAME[NLFODIV];
extern const float LFODIV_BEATS[NLFODIV];

/* an effect page's first parameter (its TYPE), and the names of its five sliders for an algorithm */
int fxTypeParam(int page);
const char *fxSliderName(int page, int type, int slider);
/* is a slider of this algorithm bipolar (its middle the zero: DAMP, OFST, L/R, TUNE, GRAIN, PAN) */
bool fxSliderBipolar(int page, int type, int slider);
/* the slider's value as the screen shows it (milliseconds, semitones, a percentage, ...) */
void fxSliderText(int page, int type, int slider, int v, bool synced, char *out, int n);

/* the value as the screen shows it */
void paramText(int p, int v, char *out, int n);
inline float norm(int p, int v) { return (float)(v - PARAMS[p].lo) / (float)(PARAMS[p].hi - PARAMS[p].lo); }

/* the LFOs' rates (Hz) free: LFO 1 0.1 .. 25 Hz, LFO 2 0.05 .. 30 Hz */
float lfo1Hz(float x);
float lfo2Hz(float x);
int lfoDivOf(float x);

/* the MOD MATRIX's sources */
enum ModSrc { MS_LFO1, MS_LFO2, MS_VCAEG, MS_FILTEREG, MS_VEL, MS_MW, MS_AT, MS_KEY, MS_CC74, NMODSRC };
extern const char *const MODSRC_NAME[NMODSRC];

/* the poly sequencer: 64 steps of up to six notes, each with its velocity */
constexpr int PSEQ_STEPS = 64, PSEQ_NOTES = 6;
constexpr uint8_t PN_REST = 0, PN_TIE = 1;          /* a step's note slot: 0 rest, 1 tie, else note + 2 */
struct SeqData {
    uint8_t note[PSEQ_STEPS][PSEQ_NOTES];
    uint8_t vel[PSEQ_STEPS][PSEQ_NOTES];
};
void seqClear(SeqData &s);

constexpr int NAME_LEN = 21;
struct Program {
    char name[NAME_LEN];
    int16_t p[NPARAM];
    int8_t mod[NMODSRC][NPARAM];                     /* -100..+100 */
    SeqData seq;
};
void programInit(Program &p);                         /* INITIALIZE: the basic program */
} // namespace hm
