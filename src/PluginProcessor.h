/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
/* HARTERMIST as a plugin (VST3, AU on the Mac) and a standalone app: the engine (dsp/Engine.h), its parameters for
 * the host (automation, saved with the project), the programs as the host's programs (HARTERMIST's own in bank A,
 * then the user's, SAVE: Documents/HARTERMIST/Programs). MIDI in (notes, velocity, poly and channel pressure, pitch
 * bend, the mod wheel, CC74, sustain; MPE), the host's tempo and position for the clock. The panel's buttons that act
 * on the engine (the transport, REC, the mod matrix) come through a small queue. */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "Engine.h"
#include "Presets.h"

class HartermistProcessor : public juce::AudioProcessor
{
public:
    HartermistProcessor();
    ~HartermistProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "HARTERMIST"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return hm::numFactory() + (int)userPrograms.size(); }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter *param(int p) const { return params[p]; }
    int value(int p) const;
    void setValue(int p, int v);

    enum Cmd { C_PLAY, C_STOP, C_STOP2, C_REC, C_REST, C_TIE, C_SCROLL, C_PANIC, C_MOD, C_INIT };
    void command(Cmd c, int v = 0);

    /* the MOD MATRIX (the live program's) */
    int modAmount(int src, int dst) const;
    void setModAmount(int src, int dst, int amt);
    void clearMods(int src);                     /* (-1: all) */
    int routesOf(int src) const;

    /* what the engine shows */
    std::atomic<float> voiceLevel[hm::NV] = {};
    std::atomic<int> seqStepNow{-1}, recStepNow{0}, keysHeld{0};
    std::atomic<bool> recOn{false}, runningNow{false}, beatNow{false};
    std::atomic<float> bpmNow{120.0f}, lfoNow[2] = {};
    juce::MidiKeyboardState keyboardState;
    std::atomic<float> uiScale{0.8f};
    std::atomic<bool> uiScaleSet{false};
    /* the panel's state (kept while the editor is closed) */
    std::atomic<int> sliderPage{hm::FP_DIST};    /* the five sliders: 0..3 an effect page, 4 the arpeggiator, 5 the sequencer */
    std::atomic<int> screenPage{0};              /* 0 main, 1 the sliders' page, 2 REC */
    std::atomic<int> envPage{0};                 /* the ENV sliders: 0 the filter's, 1 the VCA's */

    juce::String programName() const;
    juce::String programPlace(int index) const;  /* "A01": its bank and slot */
    hm::Program currentProgram() const;
    hm::SeqData seqCopy() const;

    std::vector<hm::Program> userPrograms;
    static juce::File programFolder();
    void loadUserPrograms();
    int writeProgram(const juce::String &name);
    void initialize();                           /* MENU > INITIALIZE */

private:
    hm::Engine engine;
    juce::RangedAudioParameter *params[hm::NPARAM] = {};
    int16_t sent[hm::NPARAM] = {};
    int program = 0;
    float lastGain = -1.0f;
    bool sentMpe = false;
    int sentMpePb = -1;

    mutable juce::SpinLock progLock;
    hm::Program pending, live;
    std::atomic<bool> pendingSet{false};

    struct Command {
        int c, v;
    };
    juce::AbstractFifo fifo{512};
    std::array<Command, 512> cmds;
    int held = 0;

    const hm::Program &programAt(int index) const;
    void handleMidi(const juce::MidiMessage &m);
    void applyProgram(const hm::Program &p, bool toHost);
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void programToXml(const hm::Program &p, juce::XmlElement &x);
    static bool programFromXml(const juce::XmlElement &x, hm::Program &p);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HartermistProcessor)
};
