/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
/* The editor: the Artemis' front panel, as its photographs show it (1780 x 900, scaled to the window: drag the
 * corner). LFO 1 and LFO 2, the VCO with its WAVE ring, the VCF with HIGH PASS, ENV (its sliders the filter's, or with
 * VCA the VCA's), MASTER and SPREAD on their plate, the screen and its encoder, the transport, MENU and SAVE, the
 * effect pages and SEQ over the five sliders that work whichever page is chosen.
 *
 * The screen as on the Artemis: the program, the value of a control while it moves, an effect page's algorithm and
 * its five sliders, the arpeggiator's and the sequencer's, REC's steps. The encoder turns through what the screen
 * shows (programs, algorithms, the tempo, the steps) and presses (the list, on / off, a rest or a tie).
 *
 * The MOD MATRIX the Artemis' way: MENU > MOD MATRIX, a source; then each knob and slider moved sets that source's
 * amount on it (a blue arc shows it), until MENU again. */
#pragma once
#include "PluginProcessor.h"
#include "ui/Look.h"

class HartermistPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1780, H = 900;
    explicit HartermistPanel(HartermistProcessor &);
    ~HartermistPanel() override;
    void paint(juce::Graphics &) override;

    HartermistProcessor &proc;

    /* what the screen shows */
    int shownParam = -1;                        /* a control in hand (-1: none; -2 MASTER) */
    double shownAt = 0.0;
    juce::String message;
    double messageAt = 0.0;
    int modSrc = -1;                            /* the matrix being set: its source (-1: no) */
    int modDst = -1;
    void touch(int p);
    void say(const juce::String &m);
    bool showingValue() const;
    static double now() { return juce::Time::getMillisecondCounterHiRes(); }
    static float blink(double period = 500.0, float duty = 0.5f);

    /* the five sliders' parameters for the page chosen */
    int sliderParam(int i) const;
    void encoderTurn(int d);
    void encoderPress(const juce::MouseEvent &);
    void programMenu(juce::Rectangle<int> screen);
    void menu(juce::Rectangle<int> screen);
    void saveDialog();
    void pagePressed(int page);                 /* DIST, MOD, DELAY, REV (0..3), SEQ (4) */

    struct Live {
        virtual ~Live() = default;
        virtual void poll() = 0;
    };

private:
    void timerCallback() override;
    void build();
    void paintBackground(juce::Graphics &, float sc);

    hmui::Look look;
    std::vector<std::unique_ptr<juce::Component>> parts;
    std::vector<Live *> live;
    juce::Image background;
    float backgroundScale = 0.0f;
    juce::Component::SafePointer<juce::AlertWindow> dialog;
    double lastStop = 0.0;
    template <class T, class... A> T *add(A &&...a);
};

class HartermistEditor : public juce::AudioProcessorEditor
{
public:
    explicit HartermistEditor(HartermistProcessor &);
    void resized() override;

private:
    HartermistProcessor &proc;
    juce::SharedResourcePointer<hmui::UiResources> resources;
    HartermistPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HartermistEditor)
};
