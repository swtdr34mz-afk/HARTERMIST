/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
#include "Look.h"
#include "BinaryData.h"
#include <map>

namespace hmui
{
/* the fonts and the tag: plain pointers, made when first drawn and freed when the last editor closes. Never function
 * statics: those are freed while the host unloads the DLL, under Windows' loader lock (PROPHET 67's lesson) */
struct Store
{
    juce::Typeface::Ptr t[4];
    juce::Image tag;
};
static Store *store = nullptr;
static Store &get()
{
    if (store == nullptr) {
        store = new Store();
        store->t[0] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansSemiBold_ttf, BinaryData::FiraSansSemiBold_ttfSize);
        store->t[1] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansBold_ttf, BinaryData::FiraSansBold_ttfSize);
        store->t[2] = juce::Typeface::createSystemTypefaceFor(BinaryData::FiraSansExtraBold_ttf, BinaryData::FiraSansExtraBold_ttfSize);
        store->t[3] = juce::Typeface::createSystemTypefaceFor(BinaryData::AnonymousProBold_ttf, BinaryData::AnonymousProBold_ttfSize);
        store->tag = juce::ImageFileFormat::loadFrom(BinaryData::swtdr34mztag_png, BinaryData::swtdr34mztag_pngSize);
    }
    return *store;
}
UiResources::~UiResources()
{
    delete store;
    store = nullptr;
    juce::Typeface::clearTypefaceCache();
    juce::ImageCache::releaseUnusedImages();
}

juce::Font font(Face f, float height) { return juce::Font(juce::FontOptions(get().t[(int)f]).withHeight(height)); }

Look::Look()
{
    using namespace juce;
    setColour(ResizableWindow::backgroundColourId, col::panel);
    setColour(PopupMenu::backgroundColourId, Colour(0xff1a1b1e));
    setColour(PopupMenu::textColourId, col::legend);
    setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xffc2283a));
    setColour(PopupMenu::highlightedTextColourId, Colours::white);
    setColour(PopupMenu::headerTextColourId, col::dim);
    setColour(AlertWindow::backgroundColourId, Colour(0xff1a1b1e));
    setColour(AlertWindow::textColourId, col::legend);
    setColour(TextEditor::backgroundColourId, Colour(0xff0b0b0c));
    setColour(TextEditor::textColourId, col::legend);
    setColour(TextButton::buttonColourId, Colour(0xff2a2b2e));
}
juce::Typeface::Ptr Look::getTypefaceForFont(const juce::Font &) { return get().t[0]; }
void Look::drawPopupMenuBackground(juce::Graphics &g, int w, int h)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(col::legend.withAlpha(0.5f));
    g.drawRect(0, 0, w, h, 1);
}

/* ------------------------------------------------------------------------- the letters --- */
/* each glyph on a grid 6 high: M moves, L draws, Z closes, D a dot; its width after the colon */
static const std::map<juce::juce_wchar, const char *> &glyphs()
{
    static const std::map<juce::juce_wchar, const char *> g = {
        {'A', "4:M0,6 L0,0 L4,0 L4,6 M0,3.3 L4,3.3"},
        {'B', "4:M0,6 L0,0 L3.4,0 L3.4,2.9 M0,2.9 L4,2.9 L4,6 L0,6"},
        {'C', "4:M4,0 L0,0 L0,6 L4,6"},
        {'D', "4:M0,0 L3,0 L4,1.1 L4,4.9 L3,6 L0,6 Z"},
        {'E', "4:M4,0 L0,0 L0,6 L4,6 M0,3 L3.2,3"},
        {'F', "4:M4,0 L0,0 L0,6 M0,3 L3.2,3"},
        {'G', "4:M4,0 L0,0 L0,6 L4,6 L4,3.2 L2.2,3.2"},
        {'H', "4:M0,0 L0,6 M4,0 L4,6 M0,3 L4,3"},
        {'I', "0:M0,0 L0,6"},
        {'J', "4:M4,0 L4,6 L0,6 L0,4.2"},
        {'K', "4:M0,0 L0,6 M4,0 L1.2,3 L4,6 M0,3 L1.2,3"},
        {'L', "4:M0,0 L0,6 L4,6"},
        {'M', "5:M0,6 L0,0 L5,0 L5,6 M2.5,0 L2.5,4.4"},
        {'N', "4:M0,6 L0,0 L4,6 L4,0"},
        {'O', "4:M0,0 L4,0 L4,6 L0,6 Z"},
        {'P', "4:M0,6 L0,0 L4,0 L4,3.2 L0,3.2"},
        {'Q', "4:M0,0 L4,0 L4,6 L0,6 Z M2.6,4.4 L4,6"},
        {'R', "4:M0,6 L0,0 L4,0 L4,3.2 L0,3.2 M2.2,3.2 L4,6"},
        {'S', "4:M4,0 L0,0 L0,3 L4,3 L4,6 L0,6"},
        {'T', "4:M0,0 L4,0 M2,0 L2,6"},
        {'U', "4:M0,0 L0,6 L4,6 L4,0"},
        {'V', "4:M0,0 L2,6 L4,0"},
        {'W', "5:M0,0 L0,6 L5,6 L5,0 M2.5,6 L2.5,1.6"},
        {'X', "4:M0,0 L4,6 M4,0 L0,6"},
        {'Y', "4:M0,0 L2,3 L4,0 M2,3 L2,6"},
        {'Z', "4:M0,0 L4,0 L0,6 L4,6"},
        {'0', "4:M0,0 L4,0 L4,6 L0,6 Z"},
        {'1', "2.4:M0,1.2 L1.6,0 L1.6,6"},
        {'2', "4:M0,0 L4,0 L4,3 L0,3 L0,6 L4,6"},
        {'3', "4:M0,0 L4,0 L4,6 L0,6 M1,3 L4,3"},
        {'4', "4:M0,0 L0,3.6 L4,3.6 M3,1.6 L3,6"},
        {'5', "4:M4,0 L0,0 L0,3 L4,3 L4,6 L0,6"},
        {'6', "4:M4,0 L0,0 L0,6 L4,6 L4,3 L0,3"},
        {'7', "4:M0,0 L4,0 L4,6"},
        {'8', "4:M0,0 L4,0 L4,6 L0,6 Z M0,3 L4,3"},
        {'9', "4:M4,3 L0,3 L0,0 L4,0 L4,6 L0,6"},
        {'/', "3:M0,6 L3,0"},
        {'.', "0:D0,6"},
        {'-', "3:M0,3 L3,3"},
        {'+', "4:M0,3 L4,3 M2,1 L2,5"},
        {'<', "3:M3,0.5 L0,3 L3,5.5"},
        {'>', "3:M0,0.5 L3,3 L0,5.5"},
        {' ', "2.4:"},
    };
    return g;
}

static void addGlyph(juce::Path &p, const char *def, float x0, float s, float &adv)
{
    const char *c = std::strchr(def, ':');
    adv = (float)std::atof(def);
    if (c == nullptr)
        return;
    c++;
    juce::Path q;
    bool open = false;
    while (*c) {
        char op = *c;
        if (op == ' ') {
            c++;
            continue;
        }
        if (op == 'Z') {
            q.closeSubPath();
            open = false;
            c++;
            continue;
        }
        float x = (float)std::atof(c + 1);
        const char *comma = std::strchr(c, ',');
        float y = (float)std::atof(comma + 1);
        const char *nx = comma + 1;
        while (*nx && *nx != ' ')
            nx++;
        c = nx;
        juce::Point<float> pt(x0 + x * s, y * s);
        if (op == 'M' || op == 'D') {
            q.startNewSubPath(pt);
            if (op == 'D')
                q.lineTo(pt.translated(0.01f, 0.0f));
            open = true;
        } else if (op == 'L' && open)
            q.lineTo(pt);
    }
    p.addPath(q.createPathWithRoundedCorners(s * 0.85f));
}

juce::Path technoPath(const juce::String &text, float h, float spacing)
{
    juce::Path p;
    const float s = h / 6.0f;
    float x = 0.0f;
    auto &G = glyphs();
    for (auto ch : text.toUpperCase()) {
        auto it = G.find(ch);
        if (it == G.end())
            it = G.find(' ');
        float adv = 0.0f;
        addGlyph(p, it->second, x, s, adv);
        x += (adv + 1.0f + spacing * 4.0f) * s;
    }
    return p;
}

float technoWidth(const juce::String &text, float h, float spacing)
{
    const float s = h / 6.0f;
    float x = 0.0f;
    auto &G = glyphs();
    for (auto ch : text.toUpperCase()) {
        auto it = G.find(ch);
        if (it == G.end())
            it = G.find(' ');
        x += ((float)std::atof(it->second) + 1.0f + spacing * 4.0f) * s;
    }
    return juce::jmax(0.0f, x - (1.0f + spacing * 4.0f) * s);
}

static void strokeTechno(juce::Graphics &g, juce::Path p, float h, float stroke)
{
    g.strokePath(p, juce::PathStrokeType(stroke > 0.0f ? stroke : h / 6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
void drawTechno(juce::Graphics &g, const juce::String &text, juce::Point<float> c, float h, float stroke, float spacing)
{
    auto p = technoPath(text, h, spacing);
    p.applyTransform(juce::AffineTransform::translation(c.x - technoWidth(text, h, spacing) * 0.5f, c.y - h * 0.5f));
    strokeTechno(g, p, h, stroke);
}
void drawTechnoLeft(juce::Graphics &g, const juce::String &text, juce::Point<float> c, float h, float stroke, float spacing)
{
    auto p = technoPath(text, h, spacing);
    p.applyTransform(juce::AffineTransform::translation(c.x, c.y - h * 0.5f));
    strokeTechno(g, p, h, stroke);
}

/* ------------------------------------------------------------------------- the panel --- */
void drawPanel(juce::Graphics &g, juce::Rectangle<float> r, float sc)
{
    using namespace juce;
    g.setColour(Colour(0xff050506));
    g.fillRect(r);
    auto face = r.reduced(3.0f);
    g.setGradientFill(ColourGradient(Colour(0xff1b1b1e), face.getX(), face.getY(), Colour(0xff0f0f11), face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle(face, 10.0f);
    /* the anodised aluminium: a fine brushed grain (fixed, the same every time) */
    Random rnd(4711);
    const float step = juce::jmax(0.5f, 1.0f / sc);
    for (float y = face.getY() + 2.0f; y < face.getBottom() - 2.0f; y += step) {
        float a = 0.012f + 0.02f * rnd.nextFloat();
        g.setColour((rnd.nextBool() ? Colours::white : Colours::black).withAlpha(a));
        float x0 = face.getX() + rnd.nextFloat() * face.getWidth() * 0.3f, x1 = x0 + face.getWidth() * (0.3f + 0.7f * rnd.nextFloat());
        g.drawHorizontalLine((int)y, x0, juce::jmin(x1, face.getRight()));
    }
    /* the edge: a bevel catching light along the top */
    g.setColour(Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(face.reduced(0.5f), 10.0f, 1.0f);
    g.setColour(Colours::black.withAlpha(0.8f));
    g.drawRoundedRectangle(face.expanded(1.5f), 11.0f, 2.0f);
}

void drawScrew(juce::Graphics &g, juce::Point<float> c, float r, float angle)
{
    using namespace juce;
    g.setColour(Colours::black.withAlpha(0.6f));
    g.fillEllipse(Rectangle<float>(2.4f * r, 2.4f * r).withCentre(c.translated(0.5f, 0.9f)));
    /* a black pan head: domed, its light from the upper left */
    g.setGradientFill(ColourGradient(Colour(0xff4a4a4e), c.x - r * 0.5f, c.y - r * 0.6f, Colour(0xff060607), c.x + r * 0.7f, c.y + r * 0.8f, true));
    g.fillEllipse(Rectangle<float>(2 * r, 2 * r).withCentre(c));
    g.setColour(Colours::white.withAlpha(0.25f));
    g.fillEllipse(Rectangle<float>(r * 0.7f, r * 0.45f).withCentre(c.translated(-r * 0.35f, -r * 0.45f)));
    for (float a : {angle, angle + 1.5708f}) {
        auto p0 = c.getPointOnCircumference(r * 0.62f, a), p1 = c.getPointOnCircumference(r * 0.62f, a + MathConstants<float>::pi);
        g.setColour(Colours::white.withAlpha(0.18f));
        g.drawLine(Line<float>(p0, p1).withShortenedStart(0.0f), r * 0.3f);
        g.setColour(Colour(0xff020202));
        g.drawLine(Line<float>(p0.translated(-0.3f, -0.3f), p1.translated(-0.3f, -0.3f)), r * 0.26f);
    }
    g.setColour(Colours::black);
    g.drawEllipse(Rectangle<float>(2 * r, 2 * r).withCentre(c), 0.7f);
}

void drawInvLabel(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &text, float h)
{
    g.setColour(col::plate);
    g.fillRoundedRectangle(r, 2.5f);
    g.setColour(juce::Colour(0xff141416));
    drawTechno(g, text, r.getCentre(), h);
}

/* ------------------------------------------------------------------------- the knob --- */
static constexpr float A0 = -2.42f, A1 = 2.42f;

void drawKnobBody(juce::Graphics &g, juce::Rectangle<float> r, int kind)
{
    using namespace juce;
    const float d = jmin(r.getWidth(), r.getHeight()), R = d * 0.5f;
    const Point<float> c = r.getCentre();
    /* its shadow on the panel */
    g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.75f), c.x, c.y + R * 0.15f, Colours::transparentBlack, c.x + R * 1.15f,
                                     c.y + R * 0.15f, true));
    g.fillEllipse(Rectangle<float>(d * 1.22f, d * 1.22f).withCentre(c.translated(0.0f, R * 0.12f)));
    /* the knurled skirt */
    auto skirt = Rectangle<float>(d, d).withCentre(c);
    g.setGradientFill(ColourGradient(Colour(0xff2c2c2f), c.x - R * 0.5f, c.y - R * 0.7f, Colour(0xff050505), c.x + R * 0.5f, c.y + R * 0.8f, false));
    g.fillEllipse(skirt);
    const int ridges = kind == 2 ? 56 : kind == 1 ? 44 : 36;
    for (int i = 0; i < ridges; i++) {
        float a = MathConstants<float>::twoPi * (float)i / (float)ridges;
        float lit = 0.5f + 0.5f * std::cos(a + 2.3f);
        g.setColour(Colours::white.withAlpha(0.03f + 0.09f * lit));
        g.drawLine(Line<float>(c.getPointOnCircumference(R * 0.80f, a), c.getPointOnCircumference(R * 0.985f, a)), jmax(0.6f, d * 0.012f));
    }
    g.setColour(Colours::black);
    g.drawEllipse(skirt.reduced(0.4f), 1.0f);
    /* the aluminium cap: brushed in circles, lit from the upper left */
    const float cr = R * (kind == 2 ? 0.74f : 0.66f);
    auto cap = Rectangle<float>(2.0f * cr, 2.0f * cr).withCentre(c);
    g.setColour(Colour(0xff0c0c0c));
    g.fillEllipse(cap.expanded(R * 0.05f));
    g.setGradientFill(ColourGradient(Colour(0xfff2f2f0), c.x - cr * 0.55f, c.y - cr * 0.7f, Colour(0xff7d7f82), c.x + cr * 0.6f, c.y + cr * 0.75f, false));
    g.fillEllipse(cap);
    /* (the spun finish: the light catching it in two opposite wedges) */
    for (int i = 0; i < 2; i++) {
        Path w;
        float a = -0.9f + (float)i * MathConstants<float>::pi;
        w.addPieSegment(cap, a - 0.32f, a + 0.32f, 0.0f);
        g.setColour(Colours::white.withAlpha(i ? 0.18f : 0.35f));
        g.fillPath(w);
    }
    for (float k = 0.25f; k < 1.0f; k += 0.12f) {
        g.setColour(Colours::black.withAlpha(0.05f));
        g.drawEllipse(Rectangle<float>(2.0f * cr * k, 2.0f * cr * k).withCentre(c), 0.6f);
    }
    g.setColour(Colours::white.withAlpha(0.5f));
    g.drawEllipse(cap.reduced(0.6f), 0.7f);
}

void drawKnobPointer(juce::Graphics &g, juce::Rectangle<float> r, float pos, int kind)
{
    using namespace juce;
    const float d = jmin(r.getWidth(), r.getHeight()), R = d * 0.5f;
    const Point<float> c = r.getCentre();
    const float ang = kind == 2 ? pos * MathConstants<float>::twoPi : A0 + pos * (A1 - A0);
    const float cr = R * (kind == 2 ? 0.74f : 0.66f);
    /* the white line on the skirt (and a fine dark one on the cap, as the Artemis' caps are marked) */
    g.setColour(Colour(0xfff4f4f2));
    g.drawLine(Line<float>(c.getPointOnCircumference(cr + R * 0.04f, ang), c.getPointOnCircumference(R * 0.95f, ang)), jmax(1.6f, d * 0.05f));
    g.setColour(Colour(0xff1d1d1f).withAlpha(0.85f));
    g.drawLine(Line<float>(c.getPointOnCircumference(cr * 0.35f, ang), c.getPointOnCircumference(cr * 0.92f, ang)), jmax(1.0f, d * 0.025f));
}

void drawModArc(juce::Graphics &g, juce::Rectangle<float> r, float pos, float amt)
{
    using namespace juce;
    const float d = jmin(r.getWidth(), r.getHeight()), R = d * 0.5f + 5.0f;
    const Point<float> c = r.getCentre();
    float a0 = A0 + pos * (A1 - A0), a1 = A0 + jlimit(0.0f, 1.0f, pos + amt) * (A1 - A0);
    Path p;
    p.addCentredArc(c.x, c.y, R, R, 0.0f, jmin(a0, a1), jmax(a0, a1), true);
    g.setColour(col::modArc.withAlpha(0.35f));
    g.strokePath(p, PathStrokeType(6.0f));
    g.setColour(col::modArc);
    g.strokePath(p, PathStrokeType(2.6f, PathStrokeType::curved, PathStrokeType::rounded));
    g.fillEllipse(Rectangle<float>(6.0f, 6.0f).withCentre(c.getPointOnCircumference(R, a1)));
}

/* ------------------------------------------------------------------------- the slider --- */
void drawSlot(juce::Graphics &g, float x, float top, float bottom)
{
    using namespace juce;
    auto r = Rectangle<float>(x - 7.0f, top - 18.0f, 14.0f, bottom - top + 36.0f);
    g.setColour(Colour(0xff020202));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(Colours::white.withAlpha(0.13f));
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
    g.setColour(Colour(0xff1c1c1e));
    g.fillRoundedRectangle(r.reduced(4.5f, 6.0f), 1.5f);
}

void drawRungs(juce::Graphics &g, float x0, float x1, float top, float bottom, int n)
{
    g.setColour(col::legend.withAlpha(0.88f));
    for (int i = 0; i < n; i++) {
        float y = top + (bottom - top) * (float)i / (float)(n - 1);
        g.fillRect(juce::Rectangle<float>(x0, y - 1.1f, x1 - x0, 2.2f));
    }
}

void drawFaderCap(juce::Graphics &g, juce::Point<float> c, float d)
{
    using namespace juce;
    const float R = d * 0.5f;
    g.setGradientFill(ColourGradient(Colours::black.withAlpha(0.8f), c.x, c.y + R * 0.3f, Colours::transparentBlack, c.x + R * 1.3f, c.y + R * 0.3f, true));
    g.fillEllipse(Rectangle<float>(d * 1.3f, d * 1.3f).withCentre(c.translated(0.0f, R * 0.25f)));
    auto b = Rectangle<float>(d, d).withCentre(c);
    g.setGradientFill(ColourGradient(Colour(0xff3d3d40), c.x - R * 0.6f, c.y - R * 0.7f, Colour(0xff09090a), c.x + R * 0.5f, c.y + R * 0.8f, false));
    g.fillEllipse(b);
    g.setGradientFill(ColourGradient(Colour(0xff2a2a2d), c.x - R * 0.3f, c.y - R * 0.4f, Colour(0xff111112), c.x + R * 0.4f, c.y + R * 0.5f, false));
    g.fillEllipse(b.reduced(R * 0.16f));
    g.setColour(Colours::white.withAlpha(0.22f));
    g.drawEllipse(b.reduced(0.6f), 0.9f);
    g.setColour(Colours::black);
    g.drawEllipse(b.expanded(0.4f), 0.9f);
}

/* ------------------------------------------------------------------------- the button --- */
void drawButton(juce::Graphics &g, juce::Point<float> c, float d, float lit, bool down)
{
    using namespace juce;
    const float R = d * 0.5f;
    if (lit > 0.01f) {
        g.setGradientFill(ColourGradient(col::red.withAlpha(0.55f * lit), c.x, c.y, Colours::transparentBlack, c.x + R * 2.2f, c.y, true));
        g.fillEllipse(Rectangle<float>(d * 2.2f, d * 2.2f).withCentre(c));
    }
    g.setColour(Colours::black.withAlpha(0.8f));
    g.fillEllipse(Rectangle<float>(d * 1.12f, d * 1.12f).withCentre(c.translated(0.0f, 1.5f)));
    auto b = Rectangle<float>(d, d).withCentre(c.translated(0.0f, down ? 0.8f : 0.0f));
    Colour top = Colour(0xfffbfbfa).interpolatedWith(Colour(0xffff6878), lit), bot = Colour(0xffb9b9b6).interpolatedWith(Colour(0xffd0182f), lit);
    g.setGradientFill(ColourGradient(top, c.x - R * 0.35f, c.y - R * 0.5f, bot, c.x + R * 0.45f, c.y + R * 0.7f, true));
    g.fillEllipse(b);
    g.setColour(Colours::white.withAlpha(0.7f));
    g.fillEllipse(Rectangle<float>(R * 0.5f, R * 0.32f).withCentre(c.translated(-R * 0.25f, -R * 0.42f)));
    g.setColour(Colours::black.withAlpha(0.6f));
    g.drawEllipse(b, 0.9f);
}

void drawTag(juce::Graphics &g, juce::Rectangle<float> r)
{
    auto &img = get().tag;
    if (img.isValid())
        g.drawImage(img, r, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
}
} // namespace hmui
