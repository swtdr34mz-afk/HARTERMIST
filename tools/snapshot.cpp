/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
/* A picture of the editor (a PNG), without a window: for checking the panel's drawing.
 *   HartermistShot out.png [scale] [program] [slider page] [screen page] */
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::String out = argc > 1 ? argv[1] : "shot.png";
    float scale = argc > 2 ? (float)std::atof(argv[2]) : 1.0f;
    int prog = argc > 3 ? std::atoi(argv[3]) : 0;
    HartermistProcessor proc;
    proc.setCurrentProgram(prog);
    proc.uiScale = 1.0f;
    if (argc > 4)
        proc.sliderPage = std::atoi(argv[4]);
    if (argc > 5)
        proc.screenPage = std::atoi(argv[5]);
    {
        proc.setPlayConfigDetails(0, 2, 48000.0, 512);
        proc.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512);
        float peak = 0.0f;
        for (int b = 0; b < 200; b++) {
            juce::MidiBuffer m;
            if (b == 2)
                m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 10);
            if (b == 150)
                m.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            buf.clear();
            proc.processBlock(buf, m);
            peak = juce::jmax(peak, buf.getMagnitude(0, 512));
        }
        juce::MemoryBlock st;
        proc.getStateInformation(st);
        proc.setStateInformation(st.getData(), (int)st.getSize());
        std::printf("processed: peak %.3f, state %d bytes, %d programs\n", peak, (int)st.getSize(), proc.getNumPrograms());
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(HartermistPanel::W, HartermistPanel::H);
    for (auto *c : ed->getChildren())
        for (auto *k : c->getChildren())
            if (auto *lv = dynamic_cast<HartermistPanel::Live *>(k))
                lv->poll();
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, scale);
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile(out);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    std::printf("wrote %s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
