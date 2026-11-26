/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
#include "Presets.h"
#include <cmath>
#include <cstdio>
#include <vector>

namespace hm
{
namespace
{
/* a program written as the Artemis shows its controls: knobs and sliders 0..100, the bipolar ones -100..+100 */
struct B {
    Program p;
    explicit B(const char *n)
    {
        programInit(p);
        std::snprintf(p.name, NAME_LEN, "%s", n);
    }
    B &k(int prm, float pct)
    {
        p.p[prm] = (int16_t)std::lround(pct * 10.0f);
        return *this;
    }
    B &b(int prm, float bip)
    {
        p.p[prm] = (int16_t)std::lround((bip + 100.0f) * 5.0f);
        return *this;
    }
    B &v(int prm, int raw)
    {
        p.p[prm] = (int16_t)raw;
        return *this;
    }
    B &m(int src, int dst, int amt)
    {
        p.mod[src][dst] = (int8_t)amt;
        return *this;
    }
    /* the filter envelope / the VCA's: A D S R as 0..100 */
    B &fe(float a, float d, float s, float r) { return k(P_FE_A, a).k(P_FE_D, d).k(P_FE_S, s).k(P_FE_R, r); }
    B &ae(float a, float d, float s, float r) { return k(P_AE_A, a).k(P_AE_D, d).k(P_AE_S, s).k(P_AE_R, r); }
    /* an effect page: its algorithm and five sliders (0..100) */
    B &fx(int typeParam, int type, float s1, float s2, float s3, float s4, float mix)
    {
        v(typeParam, type);
        const float s[5] = {s1, s2, s3, s4, mix};
        for (int i = 0; i < 5; i++)
            k(typeParam + 1 + i, s[i]);
        return *this;
    }
    /* vibrato on the mod wheel (LFO 1 at its rate to both VCOs) */
    B &vib(float rate = 72.0f)
    {
        k(P_L1_RATE, rate);
        return m(MS_MW, P_L1_VCO, 22);
    }
    /* a step of the sequence: up to three notes (0: none), or a tie (-1) / a rest (all 0) */
    B &step(int i, int n1, int n2 = 0, int n3 = 0, int vel = 100)
    {
        int ns[3] = {n1, n2, n3};
        for (int k = 0; k < PSEQ_NOTES; k++)
            p.seq.note[i][k] = PN_REST;
        if (n1 < 0) {
            p.seq.note[i][0] = PN_TIE;
            return *this;
        }
        for (int k = 0; k < 3; k++)
            if (ns[k] > 0) {
                p.seq.note[i][k] = (uint8_t)(ns[k] + 2);
                p.seq.vel[i][k] = (uint8_t)vel;
            }
        return *this;
    }
};

std::vector<Program> build()
{
    std::vector<Program> v;
    auto add = [&v](const B &b) { v.push_back(b.p); };

    /* ------------------------------------------------------------------- pads --- */
    add(B("Moonlit Pad").v(P_LEVEL, -10).k(P_WAVE, 28).b(P_VCOMIX, 0).k(P_DTN, 22).k(P_CUT, 46).k(P_RES, 18).k(P_F_ENV, 26)
            .fe(62, 68, 45, 70).ae(58, 60, 90, 72).k(P_L1_RATE, 42).k(P_L1_VCF, 14).k(P_SPREAD, 75)
            .fx(P_MOD_TYPE, MT_ENSEMBLE, 45, 30, 10, 70, 38).fx(P_REV_TYPE, RV_LARGE, 20, 72, 66, 40, 36).vib());
    add(B("Nymph Choir").v(P_LEVEL, -5).k(P_WAVE, 55).b(P_VCOMIX, 0).v(P_VCO2, 12).k(P_DTN, 28).k(P_PW, 20).k(P_CUT, 50).k(P_RES, 30)
            .k(P_F_ENV, 18).fe(55, 70, 40, 65).ae(52, 60, 85, 70).k(P_L2_RATE, 30).k(P_L2_WAVE, 32).k(P_L2_PW, 25)
            .v(P_L2_WAV, LW_TRI).k(P_SPREAD, 80).k(P_HP_CUT, 18).fx(P_MOD_TYPE, MT_CHORUS, 55, 25, 15, 80, 40)
            .fx(P_REV_TYPE, RV_HUGE, 60, 70, 20, 35, 40).vib());
    add(B("Silver Strings").k(P_WAVE, 4).b(P_VCOMIX, 0).k(P_DTN, 30).k(P_CUT, 58).k(P_RES, 8).k(P_F_ENV, 14)
            .fe(45, 60, 60, 60).ae(46, 55, 92, 62).k(P_SPREAD, 70).k(P_HP_CUT, 22)
            .fx(P_MOD_TYPE, MT_ENSEMBLE, 70, 35, 5, 85, 60).fx(P_REV_TYPE, RV_LARGE, 25, 80, 70, 30, 34).vib(70));
    add(B("Cloud Walker").v(P_LEVEL, 35).k(P_WAVE, 80).b(P_VCOMIX, -20).v(P_VCO2, 7).k(P_DTN, 18).k(P_CUT, 52).k(P_RES, 25)
            .k(P_F_ENV, 20).fe(70, 75, 40, 75).ae(66, 70, 88, 78).k(P_L1_RATE, 30).k(P_L1_VCF, 18).k(P_SPREAD, 85)
            .fx(P_REV_TYPE, RV_CLOUD, 40, 55, 60, 75, 55).fx(P_DLY_TYPE, DL_STEREO, 52, 60, 45, 40, 18));
    add(B("Shimmer Grove").v(P_LEVEL, -40).k(P_WAVE, 62).b(P_VCOMIX, 0).v(P_VCO2, 12).k(P_DTN, 15).k(P_PW, 10).k(P_CUT, 55)
            .k(P_RES, 12).fe(60, 70, 50, 70).ae(60, 64, 88, 80).k(P_SPREAD, 70)
            .fx(P_REV_TYPE, RV_SHIMMER, 75, 70, 70, 25, 46).fx(P_MOD_TYPE, MT_CHORUS, 40, 20, 0, 75, 25));
    add(B("Huge Space").v(P_LEVEL, -25).k(P_WAVE, 18).b(P_VCOMIX, 0).v(P_VCO2, -12).k(P_DTN, 20).k(P_CUT, 42).k(P_RES, 22)
            .k(P_F_ENV, 30).fe(70, 80, 35, 80).ae(70, 75, 90, 85).k(P_L1_RATE, 22).k(P_L1_VCF, 20).k(P_SPREAD, 90)
            .fx(P_REV_TYPE, RV_HUGE, 85, 82, 15, 40, 52));
    add(B("Deer Pulse Pad").v(P_LEVEL, -25).k(P_WAVE, 50).b(P_VCOMIX, 0).k(P_DTN, 25).k(P_PW, 30).k(P_L2_RATE, 34).k(P_L2_PW, 55)
            .v(P_L2_WAV, LW_TRI).k(P_CUT, 54).k(P_RES, 15).k(P_F_ENV, 15).fe(40, 60, 50, 60).ae(40, 55, 90, 65)
            .k(P_SPREAD, 65).fx(P_MOD_TYPE, MT_BBDCHORUS, 50, 30, 10, 70, 45).fx(P_REV_TYPE, RV_LARGE, 15, 65, 55, 30, 28)
            .vib());
    add(B("Tri Glass").v(P_LEVEL, -20).v(P_PLAY_MODE, PM_TRI).k(P_WAVE, 96).b(P_VCOMIX, 0).v(P_VCO2, 19).k(P_DTN, 34).k(P_CUT, 70)
            .k(P_RES, 10).ae(35, 70, 70, 75).k(P_SPREAD, 100).fx(P_DLY_TYPE, DL_PINGPONG, 48, 0, 45, -30, 26)
            .fx(P_REV_TYPE, RV_LARGE, 30, 75, 65, 20, 35));
    add(B("Phase Woods").v(P_LEVEL, 35).k(P_WAVE, 10).b(P_VCOMIX, 0).k(P_DTN, 20).k(P_CUT, 60).k(P_RES, 18).ae(50, 60, 90, 70)
            .fe(50, 60, 60, 60).k(P_F_ENV, 12).k(P_SPREAD, 60).fx(P_MOD_TYPE, MT_BARBER, 70, 30, 60, 60, 60)
            .fx(P_REV_TYPE, RV_LARGE, 20, 70, 60, 20, 30));
    add(B("Sub Drone").v(P_LEVEL, -30).k(P_WAVE, 35).b(P_VCOMIX, 10).v(P_VCO2, 7).k(P_DTN, 12).b(P_SUBNOISE, -60).k(P_CUT, 34)
            .k(P_RES, 45).k(P_L1_RATE, 18).k(P_L1_VCF, 30).k(P_L2_RATE, 10).k(P_L2_XMOD, 40).k(P_L2_WAVE, 30)
            .ae(75, 70, 100, 85).k(P_SPREAD, 80).fx(P_DIST_TYPE, DT_SOFTCLIP, 0, 0, 25, 50, 30)
            .fx(P_REV_TYPE, RV_HUGE, 70, 55, 20, 30, 38));

    /* ------------------------------------------------------------------- keys, plucks, bells --- */
    add(B("Silver Bow").v(P_LEVEL, 25).k(P_WAVE, 10).b(P_VCOMIX, 10).k(P_DTN, 14).k(P_CUT, 40).k(P_RES, 32).k(P_F_ENV, 52)
            .fe(0, 36, 0, 32).ae(0, 46, 0, 42).k(P_SPREAD, 50).fx(P_DLY_TYPE, DL_PINGPONG, 44, 0, 38, -25, 22)
            .fx(P_REV_TYPE, RV_SMALL, 10, 60, 50, 0, 18));
    add(B("Forest Keys").v(P_LEVEL, 60).k(P_WAVE, 100).b(P_VCOMIX, -100).v(P_VCO2, 12).k(P_FM, 32).v(P_FM_ENV, 1).k(P_CUT, 70)
            .fe(0, 52, 10, 45).ae(0, 66, 30, 50).k(P_SPREAD, 40).v(P_AMP_VEL, 70)
            .m(MS_VEL, P_FM, 25).fx(P_MOD_TYPE, MT_TAPE, 30, 20, 0, 60, 30).fx(P_REV_TYPE, RV_SMALL, 15, 55, 45, 0, 18)
            .vib());
    add(B("Moon Bell").v(P_LEVEL, 5).k(P_WAVE, 100).b(P_VCOMIX, -100).v(P_VCO2, 19).k(P_FM, 52).v(P_FM_ENV, 1).k(P_CUT, 82)
            .fe(0, 62, 0, 62).ae(0, 72, 0, 70).k(P_SPREAD, 50).fx(P_DLY_TYPE, DL_STEREO, 40, 46, 35, -20, 20)
            .fx(P_REV_TYPE, RV_LARGE, 25, 70, 62, 10, 34));
    add(B("Tape Memory").v(P_LEVEL, -20).k(P_WAVE, 60).b(P_VCOMIX, 0).k(P_PW, 22).k(P_DTN, 10).k(P_CUT, 48).k(P_RES, 14).k(P_F_ENV, 30)
            .fe(0, 50, 30, 45).ae(0, 60, 60, 48).fx(P_MOD_TYPE, MT_TAPE, 55, 25, 0, 50, 55)
            .fx(P_DIST_TYPE, DT_SOFTCLIP, 0, 0, 15, 55, 25).fx(P_REV_TYPE, RV_SMALL, 10, 50, 45, -20, 20).vib());
    add(B("Hollow Reed").v(P_LEVEL, -40).v(P_LP12, 1).k(P_WAVE, 50).k(P_PW, 62).b(P_VCOMIX, -60).k(P_CUT, 52).k(P_RES, 40).k(P_F_ENV, 20)
            .fe(18, 50, 50, 40).ae(14, 50, 90, 40).vib(75).m(MS_AT, P_CUT, 25).fx(P_REV_TYPE, RV_SMALL, 10, 55, 40, 0, 20));
    add(B("Pitch Echo Pluck").v(P_LEVEL, 55).k(P_WAVE, 25).b(P_VCOMIX, 0).v(P_VCO2, 12).k(P_CUT, 44).k(P_RES, 30).k(P_F_ENV, 48)
            .fe(0, 38, 0, 35).ae(0, 50, 0, 45).fx(P_MOD_TYPE, MT_PITCH, 100, 0, 35, -30, 35)
            .fx(P_DLY_TYPE, DL_STEREO, 46, 52, 40, -25, 25).fx(P_REV_TYPE, RV_LARGE, 20, 60, 50, 0, 22));
    add(B("Resonator Tone").v(P_LEVEL, 85).k(P_WAVE, 70).k(P_CUT, 40).k(P_RES, 20).k(P_F_ENV, 40).fe(0, 40, 0, 40).ae(0, 55, 20, 50)
            .fx(P_MOD_TYPE, MT_RESONATOR, 30, 15, 82, 70, 50).fx(P_REV_TYPE, RV_SMALL, 15, 50, 45, 0, 20));

    /* ------------------------------------------------------------------- basses --- */
    add(B("Huntress Bass").v(P_LEVEL, 15).v(P_PLAY_MODE, PM_MONO).k(P_WAVE, 0).b(P_SUBNOISE, -65).k(P_CUT, 24).k(P_RES, 38)
            .k(P_F_ENV, 50).fe(0, 32, 10, 18).ae(0, 50, 72, 16).k(P_GLD, 18).v(P_LEGATO, 1).v(P_GLIDE_AUTO, 1)
            .v(P_DRIVE, DR_LOW).v(P_TRACK, TR_HALF));
    add(B("Unison Beast").v(P_LEVEL, -15).v(P_PLAY_MODE, PM_UNISON).k(P_WAVE, 6).b(P_VCOMIX, 0).k(P_DTN, 42).k(P_CUT, 30).k(P_RES, 30)
            .k(P_F_ENV, 46).fe(0, 40, 22, 25).ae(0, 50, 80, 22).k(P_GLD, 22).v(P_GLIDE_AUTO, 1).k(P_SPREAD, 60)
            .v(P_DRIVE, DR_MID));
    add(B("Bit Wolf").v(P_LEVEL, -10).v(P_PLAY_MODE, PM_MONO).k(P_WAVE, 45).b(P_SUBNOISE, -50).k(P_CUT, 30).k(P_RES, 25).k(P_F_ENV, 40)
            .fe(0, 30, 15, 20).ae(0, 45, 80, 15).fx(P_DIST_TYPE, DT_BINSHRED, 35, 20, 40, 45, 45).v(P_LEGATO, 1));
    add(B("FM Growl").v(P_LEVEL, -25).v(P_PLAY_MODE, PM_MONO).k(P_WAVE, 0).v(P_VCO2, -12).k(P_FM, 40).b(P_SUBNOISE, -40).k(P_CUT, 34)
            .k(P_RES, 42).k(P_F_ENV, 36).fe(0, 42, 20, 20).ae(0, 50, 85, 18).k(P_L1_RATE, 55).m(MS_MW, P_FM, 40)
            .m(MS_MW, P_L1_VCF, 40).v(P_DRIVE, DR_HIGH).fx(P_DIST_TYPE, DT_SYMWARP, 0, 0, 30, 45, 30));
    add(B("Deep Twelve").v(P_LEVEL, -30).v(P_PLAY_MODE, PM_MONO).v(P_LP12, 1).k(P_WAVE, 50).k(P_PW, 10).b(P_SUBNOISE, -80).k(P_CUT, 26)
            .k(P_RES, 55).k(P_F_ENV, 42).fe(0, 34, 0, 20).ae(0, 52, 70, 18).k(P_GLD, 10));

    /* ------------------------------------------------------------------- leads --- */
    add(B("Arrow Sync Lead").v(P_LEVEL, 5).v(P_PLAY_MODE, PM_UNISON).k(P_WAVE, 0).v(P_SYNC, 1).v(P_VCO2, 12).b(P_VCOMIX, 60)
            .k(P_DTN, 25).k(P_CUT, 64).k(P_RES, 20).k(P_F_ENV, 20).fe(0, 50, 30, 30).ae(0, 50, 90, 30)
            .m(MS_FILTEREG, P_VCO2, 30).m(MS_MW, P_VCO2, 40).k(P_GLD, 16).v(P_LEGATO, 1).v(P_GLIDE_AUTO, 1)
            .fx(P_DLY_TYPE, DL_PINGPONG, 42, 0, 35, -20, 22).fx(P_REV_TYPE, RV_SMALL, 10, 60, 45, 0, 18).vib());
    add(B("Fold Hunter").v(P_LEVEL, -55).v(P_PLAY_MODE, PM_MONO).k(P_WAVE, 100).k(P_CUT, 90).ae(0, 50, 90, 30).k(P_GLD, 14)
            .v(P_LEGATO, 1).fx(P_DIST_TYPE, DT_MULTIFOLD, 0, 0, 35, 50, 70).m(MS_MW, P_DIST_3, 50)
            .m(MS_VCAEG, P_DIST_3, 20).fx(P_DLY_TYPE, DL_BBD, 40, 40, 20, 30, 22).vib());
    add(B("Glide Duo").v(P_LEVEL, 55).v(P_PLAY_MODE, PM_DUO).k(P_WAVE, 20).k(P_DTN, 36).k(P_CUT, 56).k(P_RES, 26).k(P_F_ENV, 30)
            .fe(0, 55, 40, 35).ae(4, 55, 90, 35).k(P_GLD, 30).k(P_SPREAD, 70).fx(P_MOD_TYPE, MT_CHORUS, 35, 20, 0, 70, 25)
            .fx(P_DLY_TYPE, DL_STEREO, 40, 48, 30, -20, 18).vib());
    add(B("Wild Brass").v(P_LEVEL, -10).k(P_WAVE, 2).b(P_VCOMIX, 0).k(P_DTN, 18).k(P_CUT, 32).k(P_RES, 12).k(P_F_ENV, 48)
            .fe(18, 50, 45, 35).ae(10, 50, 90, 32).m(MS_VEL, P_F_ENV, 25).k(P_SPREAD, 50)
            .fx(P_REV_TYPE, RV_LARGE, 15, 60, 50, 0, 22).vib());
    add(B("Poly Sync Brass").v(P_LEVEL, 40).k(P_WAVE, 0).v(P_SYNC, 1).v(P_VCO2, 7).b(P_VCOMIX, 30).k(P_CUT, 42).k(P_RES, 10)
            .k(P_F_ENV, 38).fe(10, 45, 40, 35).ae(6, 50, 88, 35).m(MS_FILTEREG, P_VCO2, 25).k(P_SPREAD, 55)
            .fx(P_MOD_TYPE, MT_CHORUS, 35, 20, 0, 70, 25).fx(P_REV_TYPE, RV_LARGE, 15, 60, 50, 0, 20).vib());
    add(B("MPE Velvet").v(P_LEVEL, -15).k(P_WAVE, 40).b(P_VCOMIX, 0).k(P_DTN, 18).k(P_CUT, 36).k(P_RES, 22).fe(20, 60, 60, 50)
            .k(P_F_ENV, 20).ae(18, 60, 92, 50).m(MS_AT, P_CUT, 35).m(MS_CC74, P_WAVE, 50).m(MS_CC74, P_CUT, 20)
            .m(MS_VEL, P_F_ENV, 20).k(P_SPREAD, 55).fx(P_REV_TYPE, RV_LARGE, 15, 65, 55, 0, 24).vib());

    /* ------------------------------------------------------------------- motion: arps, sequences, FX --- */
    add(B("Night Hunt Arp").v(P_LEVEL, 40).v(P_ARP_ON, 1).v(P_ARP_MODE, AM_UP).v(P_ARP_DIV, 11).v(P_ARP_GATE, 45).k(P_WAVE, 12)
            .k(P_CUT, 40).k(P_RES, 36).k(P_F_ENV, 44).fe(0, 34, 0, 30).ae(0, 44, 0, 40).k(P_SPREAD, 60)
            .fx(P_DLY_TYPE, DL_PINGPONG, 55, 0, 45, -25, 30).v(P_DLY_SYNC, 1).fx(P_REV_TYPE, RV_LARGE, 15, 60, 50, 0, 20));
    add(B("Probability Arp").v(P_LEVEL, 60).v(P_ARP_ON, 1).v(P_ARP_MODE, AM_RND).v(P_ARP_DIV, 11).v(P_ARP_PROB, 70)
            .v(P_ARP_SWING, 58).v(P_ARP_GATE, 35).k(P_WAVE, 90).v(P_VCO2, 12).k(P_FM, 20).v(P_FM_ENV, 1).k(P_CUT, 60)
            .fe(0, 40, 0, 30).ae(0, 40, 0, 35).k(P_SPREAD, 80).fx(P_DLY_TYPE, DL_STEREO, 56, 62, 40, -20, 28)
            .v(P_DLY_SYNC, 1).fx(P_REV_TYPE, RV_CLOUD, 30, 45, 40, 60, 25));
    add(B("Repeater Glitch").v(P_LEVEL, 45).v(P_ARP_ON, 1).v(P_ARP_MODE, AM_EXC).v(P_ARP_DIV, 11).v(P_ARP_GATE, 60).k(P_WAVE, 50)
            .k(P_PW, 40).k(P_CUT, 46).k(P_RES, 30).k(P_F_ENV, 30).fe(0, 30, 0, 25).ae(0, 40, 0, 30)
            .fx(P_DLY_TYPE, DL_REPEATER, 30, 30, 60, 30, 45).fx(P_REV_TYPE, RV_SMALL, 10, 50, 40, 0, 15));
    {
        B s("Wild Sequence");
        s.v(P_LEVEL, 25);
        s.v(P_SEQ_ON, 1).v(P_SEQ_STEPS, 16).v(P_SEQ_DIV, 11).v(P_SEQ_GATE, 55).k(P_WAVE, 8).b(P_SUBNOISE, -40)
            .k(P_CUT, 30).k(P_RES, 42).k(P_F_ENV, 46).fe(0, 32, 0, 25).ae(0, 40, 40, 20).k(P_SPREAD, 40)
            .m(MS_LFO2, P_CUT, 18).k(P_L2_RATE, 20).fx(P_DLY_TYPE, DL_PINGPONG, 58, 0, 35, -30, 22).v(P_DLY_SYNC, 1);
        const int pat[16] = {36, 48, 0, 36, 43, 0, 46, 48, 36, 0, 51, 36, 43, 46, 0, 48};
        for (int i = 0; i < 16; i++)
            s.step(i, pat[i], 0, 0, i % 4 == 0 ? 120 : 90);
        add(s);
    }
    {
        B s("Chord Steps");
        s.v(P_LEVEL, -20);
        s.v(P_SEQ_ON, 1).v(P_SEQ_STEPS, 8).v(P_SEQ_DIV, 8).v(P_SEQ_GATE, 70).v(P_SEQ_SWING, 56).k(P_WAVE, 30)
            .b(P_VCOMIX, 0).k(P_DTN, 22).k(P_CUT, 40).k(P_RES, 20).k(P_F_ENV, 32).fe(0, 45, 20, 35).ae(0, 50, 60, 40)
            .k(P_SPREAD, 70).fx(P_MOD_TYPE, MT_ENSEMBLE, 40, 30, 0, 70, 35).fx(P_REV_TYPE, RV_LARGE, 20, 65, 55, 0, 28);
        s.step(0, 57, 60, 64).step(1, -1).step(2, 0).step(3, 55, 59, 62).step(4, -1).step(5, 53, 57, 60).step(6, 0)
            .step(7, 52, 55, 59);
        add(s);
    }
    add(B("Ratchet Pulse").v(P_LEVEL, 25).k(P_WAVE, 20).k(P_CUT, 50).k(P_RES, 40).k(P_L1_RATE, 62).v(P_L1_WAV, LW_SQR).k(P_L1_VCF, 34)
            .k(P_L2_RATE, 26).k(P_L2_XMOD, 55).v(P_L2_WAV, LW_SQR).ae(10, 60, 90, 50).k(P_SPREAD, 60)
            .fx(P_DLY_TYPE, DL_BBD, 45, 45, 20, 20, 22));
    add(B("Noise Wind").v(P_LEVEL, 130).b(P_SUBNOISE, 100).b(P_VCOMIX, 0).k(P_PW, 100).k(P_CUT, 55).k(P_RES, 60).v(P_TRACK, TR_FULL)
            .k(P_L1_RATE, 15).k(P_L1_VCF, 28).v(P_L1_WAV, LW_RND).ae(70, 70, 90, 80).k(P_SPREAD, 100)
            .fx(P_MOD_TYPE, MT_NOTCH, 60, 10, 40, 80, 40).fx(P_REV_TYPE, RV_HUGE, 60, 70, 10, 25, 40));
    add(B("Env Laser").v(P_LEVEL, 45).v(P_L1_WAV, LW_ENVM).k(P_L1_RATE, 52).k(P_L1_FADE, 5).k(P_L1_VCO, 60).k(P_WAVE, 60).k(P_CUT, 70)
            .k(P_RES, 30).ae(0, 50, 0, 40).fx(P_DLY_TYPE, DL_PLAYGROUND, 40, 30, 60, 70, 30)
            .fx(P_REV_TYPE, RV_SMALL, 10, 50, 40, 0, 15));
    add(B("Through Zero").v(P_LEVEL, 5).k(P_WAVE, 100).b(P_VCOMIX, -100).v(P_VCO2, 5).k(P_FM, 62).k(P_CUT, 75).ae(10, 60, 80, 55)
            .k(P_L2_RATE, 15).m(MS_LFO2, P_FM, 18).m(MS_MW, P_FM, 30).k(P_SPREAD, 50)
            .fx(P_MOD_TYPE, MT_TZFLANGER, 50, 15, 70, 60, 35).fx(P_REV_TYPE, RV_LARGE, 20, 60, 55, 0, 25));
    add(B("Basic Program"));
    return v;
}

const std::vector<Program> &bank()
{
    static const std::vector<Program> b = build();
    return b;
}
} // namespace

int numFactory() { return (int)bank().size(); }
const Program &factory(int i) { return bank()[(size_t)(i < 0 ? 0 : i >= numFactory() ? numFactory() - 1 : i)]; }
} // namespace hm
