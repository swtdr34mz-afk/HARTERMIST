/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
/* The panel's parts rendered pixel by pixel, as light falls on their materials: a key light from the upper left, the
 * view from above. Knobs (a black knurled satin skirt, a spun aluminium cap: its highlight the anisotropic bow tie of
 * turned metal, fine concentric rings), slider caps (black satin cylinders, their tops dished), domed translucent
 * buttons (and lit, their red LED through them), the anodised panel (its brushed grain, the light's fall-off). And
 * what years of use leave: a cap a little off centre, dust, fingerprints (the grease of a finger, its ridges, smeared),
 * fine scratches, the anodising rubbed bright at the edges, the print worn where hands go. Every imperfection comes
 * from a seed: the same each time the panel opens. */
#pragma once
#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace hmui
{
/* d: the part's diameter (panel units); sc: physical pixels a unit. The images are larger than the part (its shadow):
 * their centre is the part's centre, their size (d + 2 * margin) units, margin = shadowMargin(d) */
float shadowMargin(float d);
juce::Image renderKnob(float d, int kind, uint32_t seed, float sc);
juce::Image renderFaderCap(float d, uint32_t seed, float sc);
juce::Image renderButton(float d, uint32_t seed, float sc, bool lit);

/* the panel's anodised face (w x h units) into an RGB image of w*sc x h*sc; hot: where hands go (the wear) */
juce::Image renderPanelFace(float w, float h, float sc, const std::vector<juce::Point<float>> &hot);
/* the print worn: a layer's alpha eaten by noise, more where hands go */
void wearPrint(juce::Image &layer, float sc, const std::vector<juce::Point<float>> &hot, uint32_t seed);
/* finger grease (on the finished face), scratches, dust: drawn on top */
void addFingerprints(juce::Image &img, float sc, const std::vector<juce::Point<float>> &where, uint32_t seed);
void drawScratches(juce::Graphics &, juce::Rectangle<float> area, int n, uint32_t seed);
void drawDust(juce::Graphics &, juce::Rectangle<float> area, int n, uint32_t seed, float sc);
/* a smudge for glass (the screen): an ARGB image w x h units */
juce::Image renderSmudge(float w, float h, float sc, uint32_t seed);
/* a recessed slot for a slider */
void drawSlotReal(juce::Graphics &, float x, float top, float bottom);
/* a deterministic small random -1..1 */
float jitter(uint32_t seed, int k);
} // namespace hmui
