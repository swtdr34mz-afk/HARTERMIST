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
    /* every control right-clicked and right-dragged: nothing may change (the host's menu, never the value) */
    {
        std::vector<float> before;
        for (auto *p : proc.getParameters())
            before.push_back(p->getValue());
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        juce::ModifierKeys right(juce::ModifierKeys::rightButtonModifier);
        int n = 0;
        for (auto *c : ed->getChildren())
            for (auto *k : c->getChildren()) {
                auto pos = k->getLocalBounds().getCentre().toFloat();
                auto t = juce::Time::getCurrentTime();
                juce::MouseEvent down(src, pos, right, 1.0f, 0, 0, 0, 0, k, k, t, pos, t, 1, false);
                juce::MouseEvent drag(src, pos.translated(0, -60), right, 1.0f, 0, 0, 0, 0, k, k, t, pos, t, 1, true);
                k->mouseDown(down);
                k->mouseDrag(drag);
                k->mouseUp(drag);
                n++;
            }
        int changed = 0;
        auto ps = proc.getParameters();
        for (int i = 0; i < ps.size(); i++)
            if (ps[i]->getValue() != before[(size_t)i]) {
                changed++;
                std::printf("right click changed %s\n", ps[i]->getName(40).toRawUTF8());
            }
        std::printf("right-click test: %d controls, %d parameters changed\n", n, changed);
        /* and a left drag still moves a knob */
        auto *k0 = ed->getChildren()[0]->getChildren()[0];
        float v0 = proc.param(hm::P_L1_FADE)->getValue();
        auto pos = k0->getLocalBounds().getCentre().toFloat();
        auto t = juce::Time::getCurrentTime();
        juce::ModifierKeys left(juce::ModifierKeys::leftButtonModifier);
        k0->mouseDown(juce::MouseEvent(src, pos, left, 1.0f, 0, 0, 0, 0, k0, k0, t, pos, t, 1, false));
        k0->mouseDrag(juce::MouseEvent(src, pos.translated(0, -60), left, 1.0f, 0, 0, 0, 0, k0, k0, t, pos, t, 1, true));
        k0->mouseUp(juce::MouseEvent(src, pos.translated(0, -60), left, 1.0f, 0, 0, 0, 0, k0, k0, t, pos, t, 1, true));
        std::printf("left drag: FADE IN %.3f -> %.3f\n", v0, proc.param(hm::P_L1_FADE)->getValue());
    }
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, scale);
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile(out);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    std::printf("wrote %s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
