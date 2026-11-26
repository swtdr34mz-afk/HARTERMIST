/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
#include "PluginEditor.h"

using namespace hmui;
using namespace hm;
using Panel = HartermistPanel;

namespace
{
constexpr int P_MASTER = -2;                     /* (the MASTER knob: the global volume, not the program's) */
/* the sliders' travel: the cap's centre at the top and at the bottom */
constexpr float FT = 497.0f, FB = 711.0f, CAP = 36.0f;

juce::RangedAudioParameter *paramOf(HartermistProcessor &proc, int p)
{
    if (p == P_MASTER)
        return proc.apvts.getParameter("volume");
    return p >= 0 ? proc.param(p) : nullptr;
}

/* ------------------------------------------------------------------- what a control does when moved --- */
/* (the value, or with the matrix being set, that source's amount on it) */
struct Mover {
    Panel &panel;
    std::function<int()> which;
    float startV = 0.0f;
    int startAmt = 0;
    bool modding = false;

    bool canMod(int p) const { return panel.modSrc >= 0 && p >= 0 && PARAMS[p].mod; }
    void begin()
    {
        int p = which();
        modding = canMod(p);
        if (modding) {
            startAmt = panel.proc.modAmount(panel.modSrc, p);
            panel.modDst = p;
            panel.touch(p);
            return;
        }
        if (auto *q = paramOf(panel.proc, p)) {
            startV = q->getValue();
            q->beginChangeGesture();
            panel.touch(p);
        }
    }
    /* d: how far moved, in the control's whole travel (1 = all of it) */
    void move(float d)
    {
        int p = which();
        if (modding) {
            panel.proc.setModAmount(panel.modSrc, p, juce::jlimit(-100, 100, startAmt + (int)std::lround(d * 200.0f)));
            panel.modDst = p;
            panel.touch(p);
            return;
        }
        if (auto *q = paramOf(panel.proc, p)) {
            q->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, startV + d));
            panel.touch(p);
        }
    }
    void end()
    {
        if (modding)
            return;
        if (auto *q = paramOf(panel.proc, which()))
            q->endChangeGesture();
    }
    void reset()
    {
        int p = which();
        if (canMod(p)) {
            panel.proc.setModAmount(panel.modSrc, p, 0);
            panel.touch(p);
            return;
        }
        if (auto *q = paramOf(panel.proc, p)) {
            q->beginChangeGesture();
            q->setValueNotifyingHost(q->getDefaultValue());
            q->endChangeGesture();
            panel.touch(p);
        }
    }
    void step(int d)
    {
        int p = which();
        if (canMod(p)) {
            panel.proc.setModAmount(panel.modSrc, p, juce::jlimit(-100, 100, panel.proc.modAmount(panel.modSrc, p) + d));
            panel.modDst = p;
            panel.touch(p);
            return;
        }
        auto *q = paramOf(panel.proc, p);
        if (q == nullptr)
            return;
        q->beginChangeGesture();
        if (auto *ip = dynamic_cast<juce::AudioParameterInt *>(q)) {
            int range = ip->getRange().getLength(), st = range > 200 ? range / 100 : 1;
            q->setValueNotifyingHost(q->convertTo0to1((float)juce::jlimit(ip->getRange().getStart(), ip->getRange().getEnd(), ip->get() + d * st)));
        } else
            q->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, q->getValue() + 0.01f * (float)d));
        q->endChangeGesture();
        panel.touch(p);
    }
    float pos() const
    {
        auto *q = paramOf(panel.proc, which());
        return q ? q->getValue() : 0.0f;
    }
    float amt() const
    {
        int p = which();
        return canMod(p) ? (float)panel.proc.modAmount(panel.modSrc, p) / 100.0f : 0.0f;
    }
};

/* ------------------------------------------------------------------------------- a knob --- */
class Knob : public juce::Component, public Panel::Live
{
public:
    Knob(Panel &p, std::function<int()> which, int kind) : mv{p, std::move(which)}, kind(kind) {}
    void paint(juce::Graphics &g) override
    {
        float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
        auto r = getLocalBounds().toFloat().reduced(12.0f);
        auto full = getLocalBounds().toFloat();
        if (body.isNull() || sc != bodyScale) {
            bodyScale = sc;
            body = juce::Image(juce::Image::ARGB, juce::jmax(1, (int)std::ceil(full.getWidth() * sc)), juce::jmax(1, (int)std::ceil(full.getHeight() * sc)), true);
            juce::Graphics bg(body);
            bg.addTransform(juce::AffineTransform::scale(sc));
            drawKnobBody(bg, r, kind);
        }
        g.drawImage(body, full, juce::RectanglePlacement::stretchToFit);
        drawKnobPointer(g, r, kind == 2 ? turns : shown, kind);
        if (shownAmt != 0.0f)
            drawModArc(g, r, shown, shownAmt);
    }
    void poll() override
    {
        if (kind == 2)
            return;
        float p = mv.pos(), a = mv.amt();
        if (p != shown || a != shownAmt) {
            shown = p;
            shownAmt = a;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        startY = e.position.y;
        acc = 0.0f;
        if (kind == 2)
            return;
        mv.begin();
    }
    void mouseDrag(const juce::MouseEvent &e) override
    {
        float dy = startY - e.position.y;
        if (kind == 2) {                         /* the encoder: a detent every 14 px */
            while (dy - acc >= 14.0f) {
                acc += 14.0f;
                turn(1);
            }
            while (dy - acc <= -14.0f) {
                acc -= 14.0f;
                turn(-1);
            }
            return;
        }
        mv.move(dy / (e.mods.isShiftDown() ? 900.0f : 230.0f));
    }
    void mouseUp(const juce::MouseEvent &e) override
    {
        if (kind == 2) {
            if (e.getDistanceFromDragStart() < 4 && onPress)
                onPress(e);
            return;
        }
        mv.end();
    }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override
    {
        int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (d == 0)
            return;
        if (kind == 2)
            turn(d);
        else
            mv.step(d);
    }
    void mouseDoubleClick(const juce::MouseEvent &) override
    {
        if (kind != 2)
            mv.reset();
    }
    std::function<void(int)> onTurn;
    std::function<void(const juce::MouseEvent &)> onPress;

private:
    void turn(int d)
    {
        turns += (float)d / 24.0f;
        turns -= std::floor(turns);
        repaint();
        if (onTurn)
            onTurn(d);
    }
    Mover mv;
    int kind;
    float shown = -1.0f, shownAmt = 0.0f, startY = 0.0f, acc = 0.0f, turns = 0.6f, bodyScale = 0.0f;
    juce::Image body;
};

/* ------------------------------------------------------------------------------ a slider --- */
class Fader : public juce::Component, public Panel::Live
{
public:
    Fader(Panel &p, std::function<int()> which) : mv{p, std::move(which)} {}
    void paint(juce::Graphics &g) override
    {
        float cx = (float)getWidth() * 0.5f;
        float y = capY(shown);
        if (shownAmt != 0.0f) {                  /* the matrix's amount: a blue bar beside the slot */
            float y1 = capY(juce::jlimit(0.0f, 1.0f, shown + shownAmt));
            auto bar = juce::Rectangle<float>(cx + 9.0f, juce::jmin(y, y1), 4.0f, std::fabs(y1 - y));
            g.setColour(col::modArc.withAlpha(0.35f));
            g.fillRect(bar.expanded(2.0f, 0.0f));
            g.setColour(col::modArc);
            g.fillRect(bar);
        }
        drawFaderCap(g, {cx, y}, CAP);
    }
    void poll() override
    {
        float p = mv.pos(), a = mv.amt();
        if (p != shown || a != shownAmt) {
            shown = p;
            shownAmt = a;
            repaint();
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        startY = e.position.y;
        mv.begin();
    }
    void mouseDrag(const juce::MouseEvent &e) override { mv.move((startY - e.position.y) / ((FB - FT) * (e.mods.isShiftDown() ? 4.0f : 1.0f))); }
    void mouseUp(const juce::MouseEvent &) override { mv.end(); }
    void mouseDoubleClick(const juce::MouseEvent &) override { mv.reset(); }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override
    {
        int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (d)
            mv.step(d);
    }

private:
    float capY(float v) const { return (FB - (FB - FT) * v) - (float)getY() * 0.0f - top0; }
    Mover mv;
    float shown = -1.0f, shownAmt = 0.0f, startY = 0.0f;

public:
    float top0 = 0.0f;                           /* (the component's top in panel units) */
};

/* ------------------------------------------------------------------------------ a button --- */
class Btn : public juce::Component, public Panel::Live
{
public:
    Btn(std::function<void()> press, std::function<float()> lit) : onPress(std::move(press)), isLit(std::move(lit)) {}
    std::function<void(const juce::MouseEvent &)> onRight;
    std::function<void()> onLong;
    void setPress(std::function<void()> f) { onPress = std::move(f); }
    void paint(juce::Graphics &g) override
    {
        auto c = getLocalBounds().toFloat().getCentre();
        drawButton(g, c, 30.0f, shown, down);
    }
    void poll() override
    {
        float l = isLit ? juce::jlimit(0.0f, 1.0f, isLit()) : 0.0f;
        l = std::round(l * 12.0f) / 12.0f;
        if (l != shown) {
            shown = l;
            repaint();
        }
        if (down && onLong && !longDone && Panel::now() - downAt > 700.0) {
            longDone = true;
            onLong();
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        if (e.mods.isPopupMenu() && onRight) {
            onRight(e);
            return;
        }
        down = true;
        downAt = Panel::now();
        longDone = false;
        repaint();
        if (!onLong && onPress)
            onPress();
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        if (down && onLong && !longDone && onPress)
            onPress();
        down = false;
        repaint();
    }

private:
    std::function<void()> onPress;
    std::function<float()> isLit;
    float shown = -1.0f;
    bool down = false, longDone = false;
    double downAt = 0.0;
};

/* ------------------------------------------------------------------------------ the screen --- */
class Oled : public juce::Component, public Panel::Live
{
public:
    explicit Oled(Panel &p) : panel(p) {}
    std::function<void(const juce::MouseEvent &)> onClick;
    void mouseDown(const juce::MouseEvent &e) override
    {
        if (onClick)
            onClick(e);
    }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override
    {
        int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (d)
            panel.encoderTurn(d);
    }
    void poll() override
    {
        juce::StringArray s = compose();
        if (s != lines) {
            lines = s;
            repaint();
        }
    }
    void paint(juce::Graphics &g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour(Colour(0xff0a0a0c));
        g.fillRoundedRectangle(r, 4.0f);
        auto glass = r.reduced(5.0f);
        g.setColour(col::oledBg);
        g.fillRect(glass);
        if (lines.size() < 1)
            return;
        auto in = glass.reduced(7.0f, 5.0f);
        const String kind = lines[0];
        auto txt = [&](const String &s, Rectangle<float> a, float h, Justification j, Colour c = col::oled) {
            g.setColour(c);
            g.setFont(font(Face::mono, h));
            g.drawText(s, a, j, true);
        };
        if (kind == "COLS") {
            /* an effect page / the arp / the sequencer: its title, then five columns over the sliders */
            auto top = in.removeFromTop(17.0f);
            txt(lines[1], top, 14.0f, Justification::centredLeft);
            txt(lines[2], top, 12.0f, Justification::centredRight, col::oledDim.brighter(0.6f));
            g.setColour(col::oledDim);
            g.drawHorizontalLine((int)(top.getBottom() + 2.0f), in.getX(), in.getRight());
            in.removeFromTop(5.0f);
            float cw = in.getWidth() / 5.0f;
            for (int i = 0; i < 5; i++) {
                auto c = Rectangle<float>(in.getX() + cw * (float)i, in.getY(), cw, in.getHeight());
                bool hi = lines[13] == String(i);
                if (hi) {
                    g.setColour(col::oled.withAlpha(0.18f));
                    g.fillRect(c.reduced(1.0f, 0.0f));
                }
                txt(lines[3 + i], c.removeFromTop(c.getHeight() * 0.5f), 11.5f, Justification::centred, hi ? col::oled : col::oled.withAlpha(0.75f));
                txt(lines[8 + i], c, 11.5f, Justification::centred);
            }
            return;
        }
        if (kind == "BIG") {
            /* the main page, or a value: a small line, the big one, a small line */
            auto top = in.removeFromTop(15.0f);
            txt(lines[1], top, 12.0f, Justification::centredLeft, col::oledDim.brighter(0.7f));
            txt(lines[2], top, 12.0f, Justification::centredRight, col::oledDim.brighter(0.7f));
            auto bot = in.removeFromBottom(15.0f);
            g.setColour(col::oledDim);
            g.drawHorizontalLine((int)(bot.getY() - 1.0f), in.getX(), in.getRight());
            txt(lines[4], bot, 12.0f, Justification::centredLeft);
            txt(lines[5], bot, 12.0f, Justification::centredRight);
            /* the big text: the panel's squared letters, as the Artemis' screen */
            auto mid = in;
            float h = juce::jmin(mid.getHeight() * 0.62f, 30.0f);
            float w = technoWidth(lines[3], h, 0.2f);
            if (w > mid.getWidth() - 6.0f)
                h *= (mid.getWidth() - 6.0f) / w;
            g.setColour(col::oled);
            drawTechno(g, lines[3], mid.getCentre(), h, juce::jmax(1.6f, h / 6.0f), 0.2f);
            return;
        }
    }

private:
    juce::StringArray compose() const;
    Panel &panel;
    juce::StringArray lines;
};

juce::StringArray Oled::compose() const
{
    using juce::String;
    auto &proc = panel.proc;
    juce::StringArray s;
    const bool val = panel.showingValue();
    if (panel.modSrc >= 0) {
        s.add("BIG");
        s.add("MOD MATRIX");
        s.add(String(proc.routesOf(panel.modSrc)) + " SET");
        s.add(String(MODSRC_NAME[panel.modSrc]).toUpperCase());
        int d = panel.modDst;
        if (d >= 0) {
            int a = proc.modAmount(panel.modSrc, d);
            s.add(String(PARAMS[d].name).toUpperCase());
            s.add((a > 0 ? "+" : "") + String(a));
        } else {
            s.add("MOVE A CONTROL");
            s.add("MENU: DONE");
        }
        return s;
    }
    if (proc.screenPage.load() == 2 && proc.recOn.load() && proc.value(P_SEQ_ON)) {
        /* REC: the step, its notes */
        int st = proc.recStepNow.load(), n = juce::jmax(1, proc.value(P_SEQ_STEPS));
        auto seq = proc.seqCopy();
        String notes;
        static const char *NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        for (int k = 0; k < PSEQ_NOTES; k++) {
            int v = seq.note[st % PSEQ_STEPS][k];
            if (v == PN_TIE)
                notes = "TIE";
            else if (v >= 2)
                notes += String(NN[(v - 2) % 12]) + String((v - 2) / 12 - 2) + " ";
        }
        s.add("BIG");
        s.add("REC");
        s.add("STEP " + String(st + 1) + "/" + String(n));
        s.add(String(st + 1).paddedLeft('0', 2));
        s.add(notes.isEmpty() ? String("REST") : notes.trim());
        s.add("ENC: REST/TIE");
        return s;
    }
    const int page = proc.sliderPage.load();
    const bool pageShown = proc.screenPage.load() >= 1;
    if (pageShown && !(val && panel.shownParam != -1 && [&] {
            for (int i = 0; i < 5; i++)
                if (panel.sliderParam(i) == panel.shownParam)
                    return false;
            return true;
        }())) {
        s.add("COLS");
        if (page < 4) {
            static const char *T[4] = {"DIST", "MOD", "DELAY", "REV"};
            int tp = fxTypeParam(page), t = proc.value(tp);
            const char *const *nm = page == FP_DIST ? DIST_NAME : page == FP_MOD ? MOD_NAME : page == FP_DELAY ? DELAY_NAME : REV_NAME;
            s.add(String(T[page]) + " " + String(nm[t]).toUpperCase());
            s.add(page == FP_DELAY && proc.value(P_DLY_SYNC) ? String((int)std::lround(proc.bpmNow.load())) + " BPM" : String());
            bool synced = page == FP_DELAY && proc.value(P_DLY_SYNC);
            for (int i = 0; i < 5; i++)
                s.add(fxSliderName(page, t, i));
            for (int i = 0; i < 5; i++) {
                char t2[24];
                fxSliderText(page, t, i, proc.value(tp + 1 + i), synced, t2, sizeof t2);
                s.add(t2);
            }
        } else {
            bool arp = page == 4;
            s.add(String(arp ? "ARP " : "SEQ ") + (proc.value(arp ? P_ARP_ON : P_SEQ_ON) ? "ON" : "OFF"));
            s.add(String((int)std::lround(proc.bpmNow.load())) + " BPM");
            for (int i = 0; i < 5; i++)
                s.add(PARAMS[panel.sliderParam(i)].label);
            for (int i = 0; i < 5; i++) {
                char t2[24];
                int p = panel.sliderParam(i);
                paramText(p, proc.value(p), t2, sizeof t2);
                s.add(String(t2).toUpperCase());
            }
        }
        int hi = -1;
        for (int i = 0; i < 5; i++)
            if (val && panel.sliderParam(i) == panel.shownParam)
                hi = i;
        s.add(String(hi));
        return s;
    }
    s.add("BIG");
    if (Panel::now() - panel.messageAt < 2500.0 && panel.message.isNotEmpty()) {
        s.add("");
        s.add("");
        s.add(panel.message.toUpperCase());
        s.add(proc.programName().toUpperCase());
        s.add("");
        return s;
    }
    if (val && panel.shownParam != -1) {
        int p = panel.shownParam;
        String name, v;
        if (p == P_MASTER) {
            name = "MASTER";
            v = proc.apvts.getParameter("volume")->getCurrentValueAsText();
        } else {
            char t[40];
            paramText(p, proc.value(p), t, sizeof t);
            name = PARAMS[p].name;
            v = t;
        }
        s.add("VALUE");
        s.add(proc.programPlace(proc.getCurrentProgram()));
        s.add(v.toUpperCase());
        s.add(name.toUpperCase());
        s.add("");
        return s;
    }
    s.add("PRESET");
    s.add(String(PLAYMODE_NAME[proc.value(P_PLAY_MODE)]).toUpperCase());
    s.add(proc.programPlace(proc.getCurrentProgram()));
    s.add(proc.programName().toUpperCase());
    String st = proc.value(P_ARP_ON) ? "ARP" : proc.value(P_SEQ_ON) ? "SEQ" : "";
    s.add(String((int)std::lround(proc.bpmNow.load())) + " BPM " + st);
    return s;
}
} // namespace

/* ================================================================================= the panel === */
template <class T, class... A> T *Panel::add(A &&...a)
{
    auto c = std::make_unique<T>(std::forward<A>(a)...);
    T *raw = c.get();
    addAndMakeVisible(raw);
    if (auto *l = dynamic_cast<Live *>(raw))
        live.push_back(l);
    parts.push_back(std::move(c));
    return raw;
}

Panel::HartermistPanel(HartermistProcessor &p) : proc(p)
{
    setOpaque(true);
    setLookAndFeel(&look);
    build();
    startTimerHz(30);
}
Panel::~HartermistPanel()
{
    stopTimer();
    if (dialog)
        dialog->exitModalState(0);
    delete dialog.getComponent();
    setLookAndFeel(nullptr);
}

void Panel::touch(int p)
{
    shownParam = p;
    shownAt = now();
}
void Panel::say(const juce::String &m)
{
    message = m;
    messageAt = now();
}
bool Panel::showingValue() const { return shownParam != -1 && now() - shownAt < 2000.0; }
float Panel::blink(double period, float duty) { return std::fmod(now(), period) < period * (double)duty ? 1.0f : 0.0f; }

int Panel::sliderParam(int i) const
{
    static const int ARP[5] = {P_ARP_MODE, P_ARP_DIV, P_ARP_PROB, P_ARP_SWING, P_ARP_GATE};
    static const int SEQ[5] = {P_SEQ_STEPS, P_SEQ_DIV, P_SEQ_PROB, P_SEQ_SWING, P_SEQ_GATE};
    int page = proc.sliderPage.load();
    if (page == 4)
        return ARP[i];
    if (page == 5)
        return SEQ[i];
    return fxTypeParam(page) + 1 + i;
}

/* ------------------------------------------------------------------------------ the encoder --- */
void Panel::encoderTurn(int d)
{
    if (proc.screenPage.load() == 2 && proc.recOn.load() && proc.value(P_SEQ_ON)) {
        proc.command(HartermistProcessor::C_SCROLL, d);
        return;
    }
    if (proc.screenPage.load() >= 1 && modSrc < 0) {
        int page = proc.sliderPage.load();
        if (page < 4) {                          /* the algorithm */
            int tp = fxTypeParam(page);
            int n = PARAMS[tp].hi + 1;
            proc.setValue(tp, (proc.value(tp) + d + n) % n);
        } else {                                 /* the tempo */
            proc.setValue(P_BPM, proc.value(P_BPM) + d);
            touch(P_BPM);
        }
        return;
    }
    /* the programs */
    int n = proc.getNumPrograms();
    proc.setCurrentProgram(((proc.getCurrentProgram() + d) % n + n) % n);
    shownParam = -1;
}

void Panel::encoderPress(const juce::MouseEvent &e)
{
    if (proc.screenPage.load() == 2 && proc.recOn.load() && proc.value(P_SEQ_ON)) {
        proc.command(proc.keysHeld.load() > 0 ? HartermistProcessor::C_TIE : HartermistProcessor::C_REST);
        return;
    }
    if (proc.screenPage.load() >= 1 && proc.sliderPage.load() >= 4) {
        bool arp = proc.sliderPage.load() == 4;
        int p = arp ? P_ARP_ON : P_SEQ_ON;
        bool on = !proc.value(p);
        proc.setValue(p, on ? 1 : 0);
        if (on)
            proc.setValue(arp ? P_SEQ_ON : P_ARP_ON, 0);   /* (the one or the other, as on the Artemis) */
        return;
    }
    programMenu(e.eventComponent->getScreenBounds());
}

void Panel::pagePressed(int page)
{
    if (page < 4) {
        if (proc.sliderPage.load() == page && proc.screenPage.load() == 1)
            proc.screenPage = 0;
        else {
            proc.sliderPage = page;
            proc.screenPage = 1;
        }
        return;
    }
    /* SEQ: the one on first, then the other, then back to the main page */
    int cur = proc.sliderPage.load();
    bool shown = proc.screenPage.load() >= 1 && cur >= 4;
    int first = proc.value(P_SEQ_ON) ? 5 : 4, second = first == 4 ? 5 : 4;
    if (!shown) {
        proc.sliderPage = first;
        proc.screenPage = 1;
    } else if (cur == first) {
        proc.sliderPage = second;
        proc.screenPage = 1;
    } else
        proc.screenPage = 0;
}

/* ------------------------------------------------------------------------------ the menus --- */
void Panel::programMenu(juce::Rectangle<int> screen)
{
    juce::PopupMenu m;
    m.addSectionHeader("HARTERMIST");
    for (int i = 0; i < numFactory(); i++)
        m.addItem(i + 1, proc.getProgramName(i), true, i == proc.getCurrentProgram());
    m.addSectionHeader("YOUR PROGRAMS");
    for (int i = numFactory(); i < proc.getNumPrograms(); i++)
        m.addItem(i + 1, proc.getProgramName(i), true, i == proc.getCurrentProgram());
    if (proc.userPrograms.empty())
        m.addItem(-1, "(SAVE keeps the sound as a program of your own)", false, false);
    juce::Component::SafePointer<Panel> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screen).withMinimumWidth(260), [self](int r) {
        if (self && r > 0) {
            self->proc.setCurrentProgram(r - 1);
            self->shownParam = -1;
        }
    });
}

void Panel::menu(juce::Rectangle<int> screen)
{
    using juce::PopupMenu;
    PopupMenu m;
    auto v = [this](int p) { return proc.value(p); };
    auto choices = [&](int p, int base) {
        PopupMenu s;
        for (int i = PARAMS[p].lo; i <= PARAMS[p].hi; i++) {
            char t[32];
            paramText(p, i, t, sizeof t);
            s.addItem(base + i - PARAMS[p].lo, t, true, v(p) == i);
        }
        return s;
    };
    auto stepped = [&](int p, int base, int step) {
        PopupMenu s;
        for (int i = PARAMS[p].lo; i <= PARAMS[p].hi; i += step) {
            char t[32];
            paramText(p, i, t, sizeof t);
            s.addItem(base + i - PARAMS[p].lo, t, true, v(p) == i);
        }
        return s;
    };
    m.addSectionHeader("MENU");
    m.addSubMenu("Play Mode", choices(P_PLAY_MODE, 1000));
    m.addSubMenu("VCO 1 Tune", choices(P_VCO1_TUNE, 2000));
    m.addSubMenu("Drive", choices(P_DRIVE, 3000));
    m.addItem(10, "Legato", true, v(P_LEGATO) != 0);
    m.addSubMenu("Glide", choices(P_GLIDE_AUTO, 4000));
    m.addSubMenu("Preset Level", stepped(P_LEVEL, 5000, 10));
    m.addSubMenu("Instability", stepped(P_INSTAB, 6000, 5));
    m.addItem(11, "Delays BPM Sync", true, v(P_DLY_SYNC) != 0);
    m.addSubMenu("Amp Velocity", stepped(P_AMP_VEL, 7000, 10));
    m.addSubMenu("Pitch Wheel", choices(P_PB_RANGE, 8000));
    m.addSeparator();
    {
        PopupMenu mm;
        for (int s = 0; s < NMODSRC; s++) {
            int n = proc.routesOf(s);
            mm.addItem(100 + s, juce::String(MODSRC_NAME[s]) + (n ? "   (" + juce::String(n) + ")" : juce::String()));
        }
        mm.addSeparator();
        PopupMenu list;
        int k = 0;
        for (int s = 0; s < NMODSRC; s++)
            for (int d = 0; d < NPARAM; d++) {
                int a = proc.modAmount(s, d);
                if (a)
                    list.addItem(-1, juce::String(MODSRC_NAME[s]) + "  >  " + PARAMS[d].name + "   " + (a > 0 ? "+" : "") + juce::String(a), false, false), k++;
            }
        if (k == 0)
            list.addItem(-1, "(none)", false, false);
        mm.addSubMenu("Routings", list);
        mm.addItem(120, "Clear all", proc.routesOf(-1) > 0);
        m.addSubMenu("Mod Matrix", mm);
    }
    {
        PopupMenu gl;
        auto glob = [this](const char *id) { return proc.apvts.getParameter(id); };
        bool mpe = glob("mpe")->getValue() > 0.5f;
        gl.addItem(200, "MIDI: Omni", true, !mpe);
        gl.addItem(201, "MIDI: MPE", true, mpe);
        PopupMenu pb;
        int cur = (int)std::lround(glob("mpe_pb")->convertFrom0to1(glob("mpe_pb")->getValue()));
        for (int r : {2, 12, 24, 48, 96})
            pb.addItem(300 + r, juce::String(r) + " semitones", true, cur == r);
        gl.addSubMenu("MPE Pitch Wheel", pb, mpe);
        gl.addSeparator();
        gl.addItem(210, "Clock follows the host", true, glob("host_sync")->getValue() > 0.5f);
        bool high = glob("quality")->getValue() > 0.5f;
        gl.addItem(211, "Quality HIGH (4x)", true, high);
        gl.addItem(212, "Quality ECO (2x, less CPU)", true, !high);
        gl.addSeparator();
        gl.addItem(213, "Show the HARTERMIST folder");
        m.addSubMenu("Global", gl);
    }
    m.addSeparator();
    m.addItem(20, "Initialize");
    m.addItem(21, "Panic (all sound off)");
    juce::Component::SafePointer<Panel> self(this);
    m.showMenuAsync(PopupMenu::Options().withTargetScreenArea(screen).withMinimumWidth(220), [self](int r) {
        if (!self || r == 0)
            return;
        auto &pr = self->proc;
        auto setG = [&](const char *id, float x) {
            auto *q = pr.apvts.getParameter(id);
            q->beginChangeGesture();
            q->setValueNotifyingHost(x);
            q->endChangeGesture();
        };
        auto pick = [&](int base, int p) {
            if (r >= base && r < base + 1000) {
                pr.setValue(p, PARAMS[p].lo + r - base);
                self->touch(p);
                return true;
            }
            return false;
        };
        if (pick(1000, P_PLAY_MODE) || pick(2000, P_VCO1_TUNE) || pick(3000, P_DRIVE) || pick(4000, P_GLIDE_AUTO) ||
            pick(5000, P_LEVEL) || pick(6000, P_INSTAB) || pick(7000, P_AMP_VEL) || pick(8000, P_PB_RANGE))
            return;
        switch (r) {
        case 10: pr.setValue(P_LEGATO, pr.value(P_LEGATO) ? 0 : 1); self->touch(P_LEGATO); break;
        case 11: pr.setValue(P_DLY_SYNC, pr.value(P_DLY_SYNC) ? 0 : 1); self->touch(P_DLY_SYNC); break;
        case 120: pr.clearMods(-1); self->say("Matrix cleared"); break;
        case 200: setG("mpe", 0.0f); break;
        case 201: setG("mpe", 1.0f); break;
        case 210: setG("host_sync", pr.apvts.getParameter("host_sync")->getValue() > 0.5f ? 0.0f : 1.0f); break;
        case 211: setG("quality", 1.0f); break;
        case 212: setG("quality", 0.0f); break;
        case 213: {
            auto f = HartermistProcessor::programFolder().getParentDirectory();
            f.createDirectory();
            f.startAsProcess();
            break;
        }
        case 20: pr.initialize(); self->say("Initialized"); break;
        case 21: pr.command(HartermistProcessor::C_PANIC); break;
        default:
            if (r >= 100 && r < 100 + NMODSRC) {
                self->modSrc = r - 100;
                self->modDst = -1;
            } else if (r >= 300 && r <= 396) {
                auto *q = pr.apvts.getParameter("mpe_pb");
                setG("mpe_pb", q->convertTo0to1((float)(r - 300)));
            }
        }
    });
}

void Panel::saveDialog()
{
    auto *w = new juce::AlertWindow("SAVE", "Keep the sound as a program of your own:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel(&look);
    w->addTextEditor("name", proc.programName(), "Name");
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Panel> self(this);
    dialog = w;
    w->enterModalState(true, juce::ModalCallbackFunction::create([self, w](int r) {
                           if (self && r == 1 && self->proc.writeProgram(w->getTextEditorContents("name")) >= 0)
                               self->say("Saved");
                       }),
                       true);
}

/* ------------------------------------------------------------------------------- the build --- */
void Panel::build()
{
    using juce::Rectangle;
    auto P = [](int p) { return [p] { return p; }; };
    auto knob = [&](float x, float y, std::function<int()> which, float d, int kind = 0) {
        auto *k = add<Knob>(*this, std::move(which), kind);
        k->setBounds(Rectangle<float>(d + 24.0f, d + 24.0f).withCentre({x, y}).toNearestInt());
        return k;
    };
    auto fader = [&](float x, std::function<int()> which) {
        auto *f = add<Fader>(*this, std::move(which));
        f->setBounds(Rectangle<float>(x - 26.0f, FT - 26.0f, 52.0f, FB - FT + 52.0f).toNearestInt());
        f->top0 = FT - 26.0f;
    };
    auto button = [&](float x, float y, std::function<void()> press, std::function<float()> lit) {
        auto *b = add<Btn>(std::move(press), std::move(lit));
        b->setBounds(Rectangle<float>(44.0f, 44.0f).withCentre({x, y}).toNearestInt());
        return b;
    };
    auto cycle = [this](int p) {
        return [this, p] {
            int n = PARAMS[p].hi - PARAMS[p].lo + 1;
            proc.setValue(p, PARAMS[p].lo + (proc.value(p) - PARAMS[p].lo + 1) % n);
            touch(p);
        };
    };
    auto onOff = [this](int p) { return [this, p] { return proc.value(p) ? 1.0f : 0.0f; }; };

    /* LFO 1, LFO 2 */
    knob(95, 157, P(P_L1_FADE), 60);
    knob(95, 312, P(P_L1_RATE), 60);
    knob(240, 157, P(P_L2_XMOD), 60);
    knob(240, 312, P(P_L2_RATE), 60);
    /* 1/2: VCO 1 (lit), VCO 2 (flashing), both (off) */
    button(67, 424, cycle(P_L1_TGT), [this] {
        int t = proc.value(P_L1_TGT);
        return t == LT_VCO1 ? 1.0f : t == LT_VCO2 ? blink(400.0) : 0.0f;
    });
    /* WAV: the LFO's shape, the button showing it move */
    button(140, 424, cycle(P_L1_WAV), [this] { return 0.15f + 0.85f * (0.5f + 0.5f * proc.lfoNow[0].load()); });
    button(212, 424, cycle(P_L2_WAV), [this] { return 0.15f + 0.85f * (0.5f + 0.5f * proc.lfoNow[1].load()); });
    /* SYNC: free (off), key (flashing), BPM (lit), BPM + key (flashing, mostly on) */
    button(284, 424, cycle(P_LFO_SYNC), [this] {
        int s = proc.value(P_LFO_SYNC);
        return s == LS_KEY ? blink(500.0) : s == LS_BPM ? 1.0f : s == LS_BPMKEY ? blink(500.0, 0.75f) : 0.0f;
    });
    fader(67, P(P_L1_VCO));
    fader(133, P(P_L1_VCF));
    fader(205, P(P_L2_WAVE));
    fader(280, P(P_L2_PW));

    /* VCO */
    knob(404, 158, P(P_SUBNOISE), 48);
    knob(655, 155, P(P_VCOMIX), 48);
    knob(530, 297, P(P_WAVE), 86, 1);
    button(390, 424, [this] { proc.setValue(P_SYNC, proc.value(P_SYNC) ? 0 : 1); touch(P_SYNC); }, onOff(P_SYNC));
    button(678, 424, [this] { proc.setValue(P_FM_ENV, proc.value(P_FM_ENV) ? 0 : 1); touch(P_FM_ENV); }, onOff(P_FM_ENV));
    fader(385, P(P_VCO2));
    fader(458, P(P_DTN));
    fader(530, P(P_GLD));
    fader(603, P(P_PW));
    fader(677, P(P_FM));

    /* VCF */
    knob(830, 180, P(P_CUT), 88, 1);
    knob(968, 280, P(P_RES), 58);
    button(785, 336, [this] { proc.setValue(P_LP12, proc.value(P_LP12) ? 0 : 1); touch(P_LP12); }, onOff(P_LP12));
    button(855, 336, cycle(P_TRACK), [this] {
        int t = proc.value(P_TRACK);
        return t == TR_FULL ? 1.0f : t == TR_HALF ? blink(400.0) : 0.0f;
    });
    button(1000, 424, [this] { proc.setValue(P_FFM_NOISE, proc.value(P_FFM_NOISE) ? 0 : 1); touch(P_FFM_NOISE); }, onOff(P_FFM_NOISE));
    fader(783, P(P_HP_CUT));
    fader(857, P(P_HP_RES));
    fader(930, P(P_F_ENV));
    fader(1002, P(P_FFM));

    /* MASTER OUT */
    knob(1150, 262, P(P_MASTER), 58);
    knob(1293, 262, P(P_SPREAD), 58);

    /* ENV: the sliders the filter's, or (VCA lit) the VCA's; held: one shared (flashing) */
    {
        auto *b = button(1323, 424, [this] { proc.envPage = proc.envPage.load() ? 0 : 1; }, [this] {
            if (proc.value(P_ENV_SHARED))
                return blink(600.0);
            return proc.envPage.load() ? 1.0f : 0.0f;
        });
        b->onLong = [this] {
            proc.setValue(P_ENV_SHARED, proc.value(P_ENV_SHARED) ? 0 : 1);
            touch(P_ENV_SHARED);
        };
        b->onRight = [this](const juce::MouseEvent &) {
            proc.setValue(P_ENV_SHARED, proc.value(P_ENV_SHARED) ? 0 : 1);
            touch(P_ENV_SHARED);
        };
        const int fe[4] = {P_FE_A, P_FE_D, P_FE_S, P_FE_R}, ae[4] = {P_AE_A, P_AE_D, P_AE_S, P_AE_R};
        const float xs[4] = {1103, 1180, 1255, 1328};
        for (int i = 0; i < 4; i++)
            fader(xs[i], [this, i, fe, ae] { return proc.envPage.load() && !proc.value(P_ENV_SHARED) ? ae[i] : fe[i]; });
    }

    /* the screen, the encoder */
    {
        auto *o = add<Oled>(*this);
        o->setBounds(1405, 198, 212, 112);
        o->onClick = [this, o](const juce::MouseEvent &) {
            if (modSrc >= 0) {
                modSrc = -1;
                return;
            }
            if (proc.screenPage.load() != 0)
                proc.screenPage = 0;
            else
                programMenu(o->getScreenBounds());
        };
        auto *enc = knob(1698, 252, [] { return -1; }, 92, 2);
        enc->onTurn = [this](int d) { encoderTurn(d); };
        enc->onPress = [this](const juce::MouseEvent &e) { encoderPress(e); };
    }
    /* PLAY, STOP (twice: back to the start, all notes off), REC, MENU, SAVE */
    const float bx[5] = {1428, 1500, 1573, 1646, 1718};
    button(bx[0], 368, [this] { proc.command(HartermistProcessor::C_PLAY); }, [this] {
        return proc.runningNow.load() ? (proc.beatNow.load() ? 1.0f : 0.25f) : 0.0f;
    });
    button(bx[1], 368, [this] {
        double t = now();
        proc.command(t - lastStop < 450.0 ? HartermistProcessor::C_STOP2 : HartermistProcessor::C_STOP);
        lastStop = t;
    }, [this] { return proc.runningNow.load() ? 0.0f : 1.0f; });
    button(bx[2], 368, [this] {
        bool on = !proc.recOn.load();
        proc.command(HartermistProcessor::C_REC, on ? 1 : 0);
        if (proc.value(P_SEQ_ON))
            proc.screenPage = on ? 2 : 0;
        else if (!proc.value(P_ARP_ON))
            say("Rec: arp or seq on first");
    }, [this] { return proc.recOn.load() ? 1.0f : 0.0f; });
    {
        /* MENU: opened where it is (with the matrix being set: done) */
        auto *mb = button(bx[3], 368, nullptr, [this] { return modSrc >= 0 ? blink(500.0) : 0.0f; });
        juce::Component::SafePointer<juce::Component> sp(mb);
        mb->setPress([this, sp] {
            if (modSrc >= 0) {
                modSrc = -1;
                say("Matrix: done");
            } else if (sp)
                menu(sp->getScreenBounds());
        });
    }
    button(bx[4], 368, [this] { saveDialog(); }, [] { return 0.0f; });
    /* DIST, MOD, DELAY, REV, SEQ */
    for (int k = 0; k < 5; k++)
        button(bx[k], 424, [this, k] { pagePressed(k); }, [this, k] {
            int pg = proc.sliderPage.load();
            bool sel = k < 4 ? pg == k : pg >= 4;
            if (!sel)
                return k == 4 && (proc.value(P_ARP_ON) || proc.value(P_SEQ_ON)) ? 0.25f : 0.0f;
            return proc.screenPage.load() >= 1 ? 1.0f : 0.45f;
        });
    const float sx[5] = {1435, 1508, 1580, 1655, 1727};
    for (int i = 0; i < 5; i++)
        fader(sx[i], [this, i] { return sliderParam(i); });

}

/* ---------------------------------------------------------------------------- the background --- */
void Panel::paintBackground(juce::Graphics &g, float sc)
{
    using namespace juce;
    drawPanel(g, {0.0f, 0.0f, (float)W, (float)H}, sc);
    const float LW = 2.4f, RR = 14.0f;
    g.setColour(col::legend);
    /* the sections' lines: a left side, the top, a right side, rounded where they meet */
    auto frame = [&](float x0, float y0, float x1, float yb, bool left, bool right) {
        Path p;
        if (left) {
            p.startNewSubPath(x0, yb);
            p.lineTo(x0, y0 + RR);
            p.quadraticTo(x0, y0, x0 + RR, y0);
        } else
            p.startNewSubPath(x0, y0);
        if (right) {
            p.lineTo(x1 - RR, y0);
            p.quadraticTo(x1, y0, x1, y0 + RR);
            p.lineTo(x1, yb);
        } else
            p.lineTo(x1, y0);
        g.strokePath(p, PathStrokeType(LW, PathStrokeType::curved, PathStrokeType::rounded));
    };
    auto dashed = [&](Point<float> a, Point<float> b) {
        Path p;
        p.startNewSubPath(a);
        p.lineTo(b);
        const float d[2] = {10.0f, 7.0f};
        PathStrokeType(LW).createDashedStroke(p, p, d, 2);
        g.fillPath(p);
    };
    frame(18, 60, 335, 770, true, true);
    frame(345, 60, 727, 770, true, true);
    frame(738, 60, 1045, 770, true, false);
    frame(1055, 405, 1350, 770, true, false);
    g.drawLine(1377, 212, 1377, 770, LW);
    g.drawLine(1762, 212, 1762, 770, LW);
    dashed({178, 72}, {178, 382});
    /* HIGH PASS: dashed, its corner rounded */
    {
        Path p;
        p.startNewSubPath(745, 428);
        p.lineTo(880, 428);
        p.quadraticTo(895, 428, 895, 443);
        p.lineTo(895, 770);
        const float d[2] = {10.0f, 7.0f};
        PathStrokeType(LW).createDashedStroke(p, p, d, 2);
        g.fillPath(p);
    }
    /* the bottom: two long lines (the swtdr34mz tag where the maker's name would be) */
    g.drawLine(445, 813, 1762, 813, LW);
    g.drawLine(445, 836, 1762, 836, LW);
    drawTag(g, {24.0f, 790.0f, 400.0f, 48.0f});

    /* titles and legends */
    const float LH = 11.5f, TH = 17.0f;
    auto L = [&](const String &t, float x, float y, float h = 11.5f) {
        g.setColour(col::legend);
        drawTechno(g, t, {x, y}, h);
    };
    L("LFO 1", 97, 84, TH);
    L("LFO 2", 245, 84, TH);
    L("VCO", 535, 84, TH);
    L("VCF", 900, 84, TH);
    L("ENV", 1180, 430, TH);
    L("FADE IN", 95, 222);
    L("XMOD", 240, 222);
    L("RATE", 95, 374);
    L("RATE", 240, 374);
    drawInvLabel(g, {38.0f, 447.0f, 58.0f, 22.0f}, "1/2", LH);
    L("WAV", 140, 458);
    L("WAV", 212, 458);
    L("SYNC", 284, 458);
    /* the VCO's knobs */
    L("OFF", 404, 101, 9.5f);
    g.setColour(col::legend);
    g.drawLine(404, 110, 404, 120, 2.0f);
    {
        Path p = technoPath("SUB", 9.5f);
        p.applyTransform(AffineTransform::translation(-technoWidth("SUB", 9.5f) * 0.5f, -4.75f).rotated(-1.0f).translated(368, 128));
        g.strokePath(p, PathStrokeType(1.5f, PathStrokeType::curved, PathStrokeType::rounded));
        Path q = technoPath("NOISE", 9.5f);
        q.applyTransform(AffineTransform::translation(-technoWidth("NOISE", 9.5f) * 0.5f, -4.75f).rotated(1.0f).translated(446, 131));
        g.strokePath(q, PathStrokeType(1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    L("1/2", 655, 98, 9.5f);
    g.setColour(col::legend);
    g.drawLine(655, 107, 655, 117, 2.0f);
    {
        Path p = technoPath("VCO1", 9.5f);
        p.applyTransform(AffineTransform::translation(-technoWidth("VCO1", 9.5f) * 0.5f, -4.75f).rotated(-1.45f).translated(617, 150));
        g.strokePath(p, PathStrokeType(1.5f, PathStrokeType::curved, PathStrokeType::rounded));
        Path q = technoPath("VCO2", 9.5f);
        q.applyTransform(AffineTransform::translation(-technoWidth("VCO2", 9.5f) * 0.5f, -4.75f).rotated(1.45f).translated(693, 150));
        g.strokePath(q, PathStrokeType(1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    /* the WAVE ring: two arcs (VCO 1 outside, VCO 2 inside), the waves along them: saw, square, triangle */
    {
        g.setColour(col::legend);
        const Point<float> c(530, 300);
        const float a0 = -2.45f, a1 = 2.45f;
        for (float rr : {112.0f, 88.0f}) {
            Path p;
            p.addCentredArc(c.x, c.y, rr, rr, 0.0f, a0, a1, true);
            g.strokePath(p, PathStrokeType(2.2f));
        }
        /* the ends cut toward the knob */
        for (float a : {a0, a1})
            g.drawLine(juce::Line<float>(c.getPointOnCircumference(112.0f, a), c.getPointOnCircumference(62.0f, a)), 2.2f);
        auto wave = [&](int kind, Point<float> at, float s) {
            Path w;
            if (kind == 0) {                     /* the saw (falling) */
                w.startNewSubPath(at.x - s, at.y + s * 0.6f);
                w.lineTo(at.x - s, at.y - s * 0.6f);
                w.lineTo(at.x + s, at.y + s * 0.6f);
            } else if (kind == 1) {              /* the square */
                w.startNewSubPath(at.x - s, at.y + s * 0.6f);
                w.lineTo(at.x - s * 0.4f, at.y + s * 0.6f);
                w.lineTo(at.x - s * 0.4f, at.y - s * 0.6f);
                w.lineTo(at.x + s * 0.4f, at.y - s * 0.6f);
                w.lineTo(at.x + s * 0.4f, at.y + s * 0.6f);
                w.lineTo(at.x + s, at.y + s * 0.6f);
            } else {                             /* the triangle */
                w.startNewSubPath(at.x - s, at.y + s * 0.6f);
                w.lineTo(at.x, at.y - s * 0.6f);
                w.lineTo(at.x + s, at.y + s * 0.6f);
            }
            g.strokePath(w, PathStrokeType(1.7f, PathStrokeType::curved, PathStrokeType::rounded));
        };
        for (int ring = 0; ring < 2; ring++) {
            float rr = ring ? 76.0f : 100.0f;
            wave(0, c.getPointOnCircumference(rr, -2.05f), 7.0f);
            wave(1, c.getPointOnCircumference(rr, 0.0f).translated(ring ? 0.0f : 12.0f, 0.0f), 7.0f);
            wave(2, c.getPointOnCircumference(rr, 2.05f), 7.0f);
        }
        L("VCO1", 474, 212, 9.5f);
        L("VCO2", 486, 236, 9.5f);
        L("WAVE", 535, 404);
    }
    drawInvLabel(g, {358.0f, 447.0f, 66.0f, 22.0f}, "SYNC", LH);
    drawInvLabel(g, {648.0f, 447.0f, 66.0f, 22.0f}, "ENV", LH);
    const char *vl[5] = {"VCO2", "DTN", "GLD", "PW", "FM"};
    const float vx[5] = {385, 458, 530, 603, 677};
    for (int i = 0; i < 5; i++)
        L(vl[i], vx[i], 755);
    L("VCO", 67, 755);
    L("VCF", 133, 755);
    L("WAVE", 205, 755);
    L("PW", 280, 755);
    /* VCF */
    L("CUT", 830, 262);
    L("RES", 968, 341);
    L("12DB", 785, 370);
    L("TRK", 855, 370);
    L("HIGH PASS", 820, 458);
    drawInvLabel(g, {963.0f, 447.0f, 76.0f, 22.0f}, "NOISE", LH);
    L("CUT", 783, 755);
    L("RES", 857, 755);
    L("ENV", 930, 755);
    L("FFM", 1002, 755);
    /* the name, as a maker sets its own, and under it what it is */
    {
        g.setColour(col::legend);
        const String name = "HARTERMIST";
        float h = 64.0f, w = technoWidth(name, h, 0.18f);
        Path p = technoPath(name, h, 0.18f);
        p.applyTransform(AffineTransform::translation(1412.0f - w * 0.5f, 72.0f));
        g.strokePath(p, PathStrokeType(9.5f, PathStrokeType::curved, PathStrokeType::rounded));
        drawTechno(g, "6 VOICE ANALOG SYNTHESIZER", {1412.0f, 175.0f}, 12.0f, 2.0f, 1.25f);
    }
    /* MASTER, SPREAD: their plate */
    {
        auto plate = Rectangle<float>(1073.0f, 272.0f, 287.0f, 103.0f);
        g.setColour(Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(plate.translated(0.0f, 2.0f), 5.0f);
        g.setColour(col::plate);
        g.fillRoundedRectangle(plate, 5.0f);
        g.setColour(Colour(0xff1a1a1c));
        drawTechno(g, "MASTER", {1150.0f, 327.0f}, LH);
        drawTechno(g, "SPREAD", {1293.0f, 327.0f}, LH);
        g.fillRoundedRectangle(1198.0f, 304.0f, 10.0f, 4.0f, 1.5f);
        g.fillRoundedRectangle(1212.0f, 304.0f, 10.0f, 4.0f, 1.5f);
        g.setColour(Colour(0xff6a6a6a));
        g.fillRoundedRectangle(1210.0f, 232.0f, 10.0f, 4.0f, 1.5f);
        g.fillRoundedRectangle(1224.0f, 232.0f, 10.0f, 4.0f, 1.5f);
    }
    /* ENV */
    g.setColour(col::legend);
    L("VCA", 1323, 458);
    const char *el[4] = {"A", "D", "S", "R"};
    const float ex[4] = {1103, 1180, 1255, 1328};
    for (int i = 0; i < 4; i++)
        L(el[i], ex[i], 755);
    /* the right: the transport's symbols, MENU, SAVE; the pages' strip */
    {
        const float bx[5] = {1428, 1500, 1573, 1646, 1718};
        Path play;
        play.addTriangle(bx[0] - 5, 332, bx[0] - 5, 344, bx[0] + 6, 338);
        g.fillPath(play);
        g.fillRect(bx[1] - 5.0f, 333.0f, 10.0f, 10.0f);
        g.fillEllipse(bx[2] - 5.0f, 333.0f, 10.0f, 10.0f);
        L("MENU", bx[3], 338);
        L("SAVE", bx[4], 338);
        drawInvLabel(g, {1393.0f, 447.0f, 287.0f, 22.0f}, "", LH);
        g.setColour(Colour(0xff141416));
        const char *pl[4] = {"DIST", "MOD", "DELAY", "REV"};
        for (int i = 0; i < 4; i++)
            drawTechno(g, pl[i], {bx[i], 458.0f}, LH);
        drawInvLabel(g, {1693.0f, 447.0f, 50.0f, 22.0f}, "SEQ", LH);
        g.setColour(col::legend);
        const float sx[5] = {1435, 1508, 1580, 1655, 1727};
        for (int i = 0; i < 5; i++)
            L(String(i + 1), sx[i], 755);
    }
    /* the sliders' slots and the rungs between the pairs */
    {
        const float xs[] = {67, 133, 205, 280, 385, 458, 530, 603, 677, 783, 857, 930, 1002, 1103, 1180, 1255, 1328, 1435, 1508, 1580, 1655, 1727};
        for (float x : xs)
            drawSlot(g, x, FT, FB);
        const float pairs[][2] = {{67, 133}, {205, 280}, {385, 458}, {530, 603}, {603, 677}, {783, 857}, {930, 1002},
                                  {1103, 1180}, {1180, 1255}, {1255, 1328}, {1435, 1508}, {1508, 1580}, {1580, 1655}, {1655, 1727}};
        for (auto &pr : pairs)
            drawRungs(g, pr[0] + 11.0f, pr[1] - 11.0f, FT - 2.0f, FB + 2.0f, 9);
    }
    for (auto c : {Point<float>(18, 28), Point<float>(888, 28), Point<float>(1765, 28), Point<float>(18, 872), Point<float>(888, 872),
                   Point<float>(1765, 872)})
        drawScrew(g, c, 6.0f);
}

void Panel::paint(juce::Graphics &g)
{
    float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (sc != backgroundScale || background.isNull()) {
        backgroundScale = sc;
        background = juce::Image(juce::Image::RGB, juce::jmax(1, (int)std::ceil(W * sc)), juce::jmax(1, (int)std::ceil(H * sc)), true);
        juce::Graphics bg(background);
        bg.addTransform(juce::AffineTransform::scale(sc));
        paintBackground(bg, sc);
    }
    g.drawImage(background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void Panel::timerCallback()
{
    for (auto *l : live)
        l->poll();
}

/* ============================================================================== the editor === */
HartermistEditor::HartermistEditor(HartermistProcessor &p) : AudioProcessorEditor(&p), proc(p), panel(p)
{
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, HartermistPanel::W, HartermistPanel::H);
    const double W = HartermistPanel::W, H = HartermistPanel::H;
    auto area = juce::Rectangle<int>(0, 0, 1920, 1080);
    if (auto *d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        area = d->userArea;
    double fit = juce::jmin((area.getWidth() - 40) / W, (area.getHeight() - 90) / H);
    fit = juce::jlimit(0.35, 1.6, fit);
    setResizable(true, true);
    setResizeLimits((int)(W * 0.35), (int)(H * 0.35), (int)(W * juce::jmax(0.5, fit)), (int)(H * juce::jmax(0.5, fit)));
    getConstrainer()->setFixedAspectRatio(W / H);
    double s = proc.uiScaleSet ? (double)proc.uiScale.load() : juce::jmin(fit, 0.9);
    s = juce::jlimit(0.35, fit, s);
    setSize((int)std::lround(W * s), (int)std::lround(H * s));
}

void HartermistEditor::resized()
{
    float s = (float)getWidth() / (float)HartermistPanel::W;
    panel.setTransform(juce::AffineTransform::scale(s));
    proc.uiScale = s;
    proc.uiScaleSet = true;
}
