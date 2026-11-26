/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
/* The look of an Artemis: a black anodised panel with white legends and white section lines (rounded at their
 * corners, dashed where a section sits inside another), black knurled knobs under brushed aluminium caps, long
 * black slots with white ladder rungs between them and round black caps, white domed buttons that light red, small
 * white-on-black labels turned inside out where a button has one, a light grey plate under MASTER and SPREAD, a small
 * blue-white OLED. The legends in a squared, rounded technical letter (drawn here as strokes, not a font), the
 * screen's text in Anonymous Pro (SIL OFL, fonts/). */
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>

namespace hmui
{
namespace col
{
const juce::Colour panel{0xff141416}, legend{0xffe9e9e6}, dim{0xff8c8c88}, plate{0xffd9d9d6}, red{0xffff2f45},
    oledBg{0xff02040a}, oled{0xffd6ecff}, oledDim{0xff3b5268}, modArc{0xff39c6ff};
}

enum class Face { label, bold, heavy, mono };

/* the fonts and the tag: they live while an editor is open (a SharedResourcePointer in each) */
struct UiResources
{
    UiResources() = default;
    ~UiResources();
};
juce::Font font(Face f, float height);

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font &) override;
    juce::Font getPopupMenuFont() override { return font(Face::label, 15.0f); }
    void drawPopupMenuBackground(juce::Graphics &, int w, int h) override;
};

/* the panel's letters: squared strokes with rounded corners. h: the capitals' height; w: the stroke (0: h / 7) */
juce::Path technoPath(const juce::String &text, float h, float spacing = 0.32f);
float technoWidth(const juce::String &text, float h, float spacing = 0.32f);
void drawTechno(juce::Graphics &, const juce::String &text, juce::Point<float> centre, float h, float stroke = 0.0f,
                float spacing = 0.32f);
void drawTechnoLeft(juce::Graphics &, const juce::String &text, juce::Point<float> leftCentre, float h, float stroke = 0.0f,
                    float spacing = 0.32f);

void drawPanel(juce::Graphics &, juce::Rectangle<float> r, float sc);
void drawScrew(juce::Graphics &, juce::Point<float> c, float r, float angle = 0.0f);
/* a label turned inside out: a white plate, the letters cut out of it */
void drawInvLabel(juce::Graphics &, juce::Rectangle<float> r, const juce::String &text, float h);

/* a knob in r (square): pos 0..1; kinds: 0 small, 1 big, 2 the encoder (endless) */
void drawKnobBody(juce::Graphics &, juce::Rectangle<float> r, int kind);
void drawKnobPointer(juce::Graphics &, juce::Rectangle<float> r, float pos, int kind);
/* the matrix's amount around a knob (-1..1 from pos) */
void drawModArc(juce::Graphics &, juce::Rectangle<float> r, float pos, float amt);

/* a slider: its slot (x, top..bottom of the cap's travel), its cap */
void drawSlot(juce::Graphics &, float x, float top, float bottom);
void drawRungs(juce::Graphics &, float x0, float x1, float top, float bottom, int n);
void drawFaderCap(juce::Graphics &, juce::Point<float> c, float d);

/* a domed button: lit 0..1 (red) */
void drawButton(juce::Graphics &, juce::Point<float> c, float d, float lit, bool down);
void drawTag(juce::Graphics &, juce::Rectangle<float> r);
} // namespace hmui
