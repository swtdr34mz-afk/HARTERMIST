/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
#include "PluginProcessor.h"
#include "PanelDialog.h"
#include "PluginEditor.h"

using namespace hm;

/* ------------------------------------------------------------------------------- parameters --- */
juce::AudioProcessorValueTreeState::ParameterLayout HartermistProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int p = 0; p < NPARAM; p++) {
        const ParamDef &d = PARAMS[p];
        layout.add(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{d.id, 1}, d.name, d.lo, d.hi, d.def,
            juce::AudioParameterIntAttributes().withStringFromValueFunction([p](int v, int) {
                char t[40];
                paramText(p, v, t, sizeof t);
                return juce::String(t);
            })));
    }
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"volume", 1}, "Master", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) {
            return v <= 0.0001f ? juce::String("-inf") : juce::String(20.0f * std::log10(v * v * 2.0f), 1) + " dB";
        })));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"voices", 1}, "Voices",
                                                            juce::StringArray{"6 Voices", "8 Voices"}, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"mpe", 1}, "MPE", false));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"mpe_pb", 1}, "MPE Pitch Wheel", 0, 96, 48));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"host_sync", 1}, "Host Sync", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"quality", 1}, "Quality",
                                                            juce::StringArray{"ECO (2x)", "HIGH (4x)"}, 1));
    return layout;
}

HartermistProcessor::HartermistProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "HARTERMIST", createLayout())
{
    for (int p = 0; p < NPARAM; p++)
        params[p] = apvts.getParameter(PARAMS[p].id);
    loadUserPrograms();
    live = factory(0);
    applyProgram(factory(0), true);
}

int HartermistProcessor::value(int p) const { return (int)std::lround(params[p]->convertFrom0to1(params[p]->getValue())); }
void HartermistProcessor::setValue(int p, int v)
{
    auto *q = params[p];
    v = juce::jlimit(PARAMS[p].lo, PARAMS[p].hi, v);
    q->beginChangeGesture();
    q->setValueNotifyingHost(q->convertTo0to1((float)v));
    q->endChangeGesture();
}

void HartermistProcessor::command(Cmd c, int v)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite(1, s1, n1, s2, n2);
    if (n1 > 0)
        cmds[(size_t)s1] = {(int)c, v};
    else if (n2 > 0)
        cmds[(size_t)s2] = {(int)c, v};
    fifo.finishedWrite(n1 + n2);
}

/* ------------------------------------------------------------------------------- the matrix --- */
int HartermistProcessor::modAmount(int src, int dst) const
{
    juce::SpinLock::ScopedLockType lock(progLock);
    return live.mod[src][dst];
}
void HartermistProcessor::setModAmount(int src, int dst, int amt)
{
    amt = juce::jlimit(-100, 100, amt);
    {
        juce::SpinLock::ScopedLockType lock(progLock);
        live.mod[src][dst] = (int8_t)amt;
    }
    command(C_MOD, src * 100000 + dst * 1000 + (amt + 100));
}
void HartermistProcessor::clearMods(int src)
{
    for (int s = 0; s < NMODSRC; s++)
        if (src < 0 || s == src)
            for (int d = 0; d < NPARAM; d++)
                if (modAmount(s, d) != 0)
                    setModAmount(s, d, 0);
}
int HartermistProcessor::routesOf(int src) const
{
    juce::SpinLock::ScopedLockType lock(progLock);
    int n = 0;
    for (int s = 0; s < NMODSRC; s++)
        if (src < 0 || s == src)
            for (int d = 0; d < NPARAM; d++)
                n += live.mod[s][d] != 0;
    return n;
}

/* ------------------------------------------------------------------------------- programs --- */
const Program &HartermistProcessor::programAt(int index) const
{
    if (index < numFactory())
        return factory(index);
    return userPrograms[(size_t)(index - numFactory())];
}

void HartermistProcessor::applyProgram(const Program &p, bool toHost)
{
    {
        juce::SpinLock::ScopedLockType lock(progLock);
        pending = p;
        live = p;
    }
    pendingSet.store(true);
    if (toHost)
        for (int i = 0; i < NPARAM; i++)
            params[i]->setValueNotifyingHost(params[i]->convertTo0to1((float)p.p[i]));
}

void HartermistProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= programCount())
        return;
    program = index;
    applyProgram(programAt(index), true);
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

juce::String HartermistProcessor::programPlace(int index) const
{
    /* the Artemis' way: eight banks A..H of 64 */
    index = juce::jmax(0, index);
    return juce::String::charToString((juce::juce_wchar)('A' + (index / 64) % 8)) + juce::String(index % 64 + 1).paddedLeft('0', 2);
}

const juce::String HartermistProcessor::getProgramName(int index)
{
    if (index < 0 || index >= programCount())
        return {};
    return programPlace(index) + " " + juce::String(programAt(index).name);
}

juce::String HartermistProcessor::programName() const
{
    juce::SpinLock::ScopedLockType lock(progLock);
    return juce::String(live.name);
}

SeqData HartermistProcessor::seqCopy() const
{
    juce::SpinLock::ScopedLockType lock(progLock);
    return live.seq;
}

Program HartermistProcessor::currentProgram() const
{
    Program p;
    {
        juce::SpinLock::ScopedLockType lock(progLock);
        p = live;
    }
    for (int i = 0; i < NPARAM; i++)
        p.p[i] = (int16_t)value(i);
    return p;
}

void HartermistProcessor::initialize()
{
    Program p;
    programInit(p);
    applyProgram(p, true);
}

juce::File HartermistProcessor::programFolder()
{
    return pdlg::docsOrAppData().getChildFile("HARTERMIST").getChildFile("Programs");
}

void HartermistProcessor::programToXml(const Program &p, juce::XmlElement &x)
{
    x.setAttribute("name", juce::String(p.name));
    x.setAttribute("version", 1);
    for (int i = 0; i < NPARAM; i++)
        x.setAttribute(PARAMS[i].id, (int)p.p[i]);
    juce::MemoryBlock mb(&p.seq, sizeof p.seq);
    x.setAttribute("seq", mb.toBase64Encoding());
    /* the matrix: "source.destination=amount", the destinations by their ids (stable) */
    juce::StringArray mods;
    for (int s = 0; s < NMODSRC; s++)
        for (int d = 0; d < NPARAM; d++)
            if (p.mod[s][d])
                mods.add(juce::String(s) + "." + PARAMS[d].id + "=" + juce::String((int)p.mod[s][d]));
    x.setAttribute("mods", mods.joinIntoString(" "));
}

bool HartermistProcessor::programFromXml(const juce::XmlElement &x, Program &p)
{
    programInit(p);
    std::snprintf(p.name, NAME_LEN, "%s", x.getStringAttribute("name", "Program").substring(0, NAME_LEN - 1).toRawUTF8());
    for (int i = 0; i < NPARAM; i++)
        p.p[i] = (int16_t)juce::jlimit(PARAMS[i].lo, PARAMS[i].hi, x.getIntAttribute(PARAMS[i].id, PARAMS[i].def));
    juce::MemoryBlock mb;
    if (mb.fromBase64Encoding(x.getStringAttribute("seq")) && mb.getSize() == sizeof p.seq)
        std::memcpy(&p.seq, mb.getData(), sizeof p.seq);
    auto mods = juce::StringArray::fromTokens(x.getStringAttribute("mods"), " ", "");
    for (auto &m : mods) {
        int s = m.upToFirstOccurrenceOf(".", false, false).getIntValue();
        auto id = m.fromFirstOccurrenceOf(".", false, false).upToFirstOccurrenceOf("=", false, false);
        int a = m.fromFirstOccurrenceOf("=", false, false).getIntValue();
        for (int d = 0; d < NPARAM; d++)
            if (id == PARAMS[d].id && s >= 0 && s < NMODSRC)
                p.mod[s][d] = (int8_t)juce::jlimit(-100, 100, a);
    }
    return true;
}

void HartermistProcessor::loadUserPrograms()
{
    userPrograms.clear();
    auto files = programFolder().findChildFiles(juce::File::findFiles, false, "*.hartermist");
    files.sort();
    for (auto &f : files)
        if (auto xml = juce::XmlDocument::parse(f))
            if (xml->hasTagName("HARTERMISTPROGRAM")) {
                Program p;
                if (programFromXml(*xml, p))
                    userPrograms.push_back(p);
            }
}

int HartermistProcessor::writeProgram(const juce::String &name)
{
    auto clean = name.trim().substring(0, NAME_LEN - 1);
    if (clean.isEmpty())
        return -1;
    Program p = currentProgram();
    std::snprintf(p.name, NAME_LEN, "%s", clean.toRawUTF8());
    juce::XmlElement xml("HARTERMISTPROGRAM");
    programToXml(p, xml);
    auto folder = programFolder();
    folder.createDirectory();
    if (!xml.writeTo(folder.getChildFile(juce::File::createLegalFileName(clean) + ".hartermist")))
        return -1;
    {
        juce::SpinLock::ScopedLockType lock(progLock);
        std::snprintf(live.name, NAME_LEN, "%s", clean.toRawUTF8());
    }
    loadUserPrograms();
    for (size_t i = 0; i < userPrograms.size(); i++)
        if (juce::String(userPrograms[i].name) == clean) {
            program = numFactory() + (int)i;
            updateHostDisplay(ChangeDetails().withProgramChanged(true));
            return program;
        }
    return -1;
}

/* ------------------------------------------------------------------------------- the block --- */
bool HartermistProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void HartermistProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate);
    Program cur = currentProgram();
    {
        juce::SpinLock::ScopedLockType lock(progLock);
        pending = cur;
        pending.seq = live.seq;
        std::memcpy(pending.mod, live.mod, sizeof pending.mod);
    }
    pendingSet.store(true);
    lastGain = -1.0f;
    held = 0;
}

void HartermistProcessor::handleMidi(const juce::MidiMessage &m)
{
    const int ch = m.getChannel();
    if (m.isNoteOn()) {
        engine.noteOn(ch, m.getNoteNumber(), m.getFloatVelocity());
        held++;
    } else if (m.isNoteOff()) {
        engine.noteOff(ch, m.getNoteNumber());
        held = juce::jmax(0, held - 1);
    } else if (m.isPitchWheel())
        engine.bend(ch, (float)(m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())
        engine.chanPressure(ch, (float)m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())
        engine.polyPressure(ch, m.getNoteNumber(), (float)m.getAfterTouchValue() / 127.0f);
    else if (m.isSustainPedalOn() || m.isSustainPedalOff())
        engine.sustain(m.isSustainPedalOn());
    else if (m.isController()) {
        int cc = m.getControllerNumber();
        float v = (float)m.getControllerValue() / 127.0f;
        switch (cc) {
        case 1: engine.modWheel(v); break;
        case 74: {
            /* (64 exactly the middle: 0..64 to 0..0.5, 64..127 to 0.5..1) */
            int x = m.getControllerValue();
            engine.cc74(ch, x <= 64 ? (float)x / 128.0f : 0.5f + (float)(x - 64) / 126.0f);
            break;
        }
        case 120: engine.panic(); break;
        case 123:
            engine.allOff();
            held = 0;
            break;
        case 121:
            engine.bend(ch, 0.0f);
            engine.modWheel(0.0f);
            engine.chanPressure(ch, 0.0f);
            engine.cc74(ch, 0.5f);               /* (back to 64: the middle, where it moves nothing) */
            engine.sustain(false);
            break;
        default: break;
        }
    } else if (m.isAllNotesOff() || m.isAllSoundOff()) {
        engine.allOff();
        held = 0;
    }
}

void HartermistProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    float *L = buffer.getWritePointer(0), *R = buffer.getWritePointer(buffer.getNumChannels() > 1 ? 1 : 0);

    if (pendingSet.exchange(false)) {
        Program p;
        {
            juce::SpinLock::ScopedLockType lock(progLock);
            p = pending;
        }
        engine.setProgram(p);
        for (int i = 0; i < NPARAM; i++)
            sent[i] = p.p[i];
    }
    for (int i = 0; i < NPARAM; i++) {
        int v = value(i);
        if (v != sent[i]) {
            engine.setParam(i, v);
            sent[i] = (int16_t)v;
        }
    }
    bool mpe = apvts.getRawParameterValue("mpe")->load() > 0.5f;
    int mpb = (int)std::lround(apvts.getRawParameterValue("mpe_pb")->load());
    if (mpe != sentMpe || mpb != sentMpePb) {
        engine.setMpe(mpe, mpb);
        sentMpe = mpe;
        sentMpePb = mpb;
    }
    engine.setQuality(apvts.getRawParameterValue("quality")->load() > 0.5f ? 1 : 0);
    engine.setVoices(apvts.getRawParameterValue("voices")->load() > 0.5f ? 8 : 6);

    {
        int s1, n1, s2, n2;
        fifo.prepareToRead(fifo.getNumReady(), s1, n1, s2, n2);
        auto run = [&](const Command &c) {
            switch (c.c) {
            case C_PLAY: engine.play(); break;
            case C_STOP: engine.stop(); break;
            case C_STOP2:
                engine.stopTwice();
                held = 0;
                break;
            case C_REC: engine.rec(c.v != 0); break;
            case C_REST: engine.seqRest(); break;
            case C_TIE: engine.seqTie(); break;
            case C_SCROLL: engine.seqScroll(c.v); break;
            case C_PANIC:
                engine.panic();
                held = 0;
                break;
            case C_MOD: engine.setMod(c.v / 100000, (c.v / 1000) % 100, c.v % 1000 - 100); break;
            default: break;
            }
        };
        for (int i = 0; i < n1; i++)
            run(cmds[(size_t)(s1 + i)]);
        for (int i = 0; i < n2; i++)
            run(cmds[(size_t)(s2 + i)]);
        fifo.finishedRead(n1 + n2);
    }

    bool sync = apvts.getRawParameterValue("host_sync")->load() > 0.5f;
    double bpm = 120.0, ppq = 0.0;
    bool hostPlaying = false;
    if (auto *ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm())
                bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition())
                ppq = *q;
        }
    engine.host(hostPlaying, ppq, bpm, sync);

    keyboardState.processNextMidiBuffer(midi, 0, n, true);

    int done = 0;
    for (const auto meta : midi) {
        int at = juce::jlimit(0, n, meta.samplePosition);
        if (at > done) {
            engine.render(L + done, R + done, at - done);
            done = at;
        }
        handleMidi(meta.getMessage());
    }
    if (n > done)
        engine.render(L + done, R + done, n - done);

    for (int v = 0; v < NV; v++)
        voiceLevel[v].store(engine.voiceLevel(v), std::memory_order_relaxed);
    seqStepNow.store(engine.seqStepShown(), std::memory_order_relaxed);
    recStepNow.store(engine.recStep(), std::memory_order_relaxed);
    recOn.store(engine.recOn(), std::memory_order_relaxed);
    runningNow.store(engine.running(), std::memory_order_relaxed);
    beatNow.store(engine.beat(), std::memory_order_relaxed);
    bpmNow.store(engine.bpmShown(), std::memory_order_relaxed);
    lfoNow[0].store(engine.lfoShown(0), std::memory_order_relaxed);
    lfoNow[1].store(engine.lfoShown(1), std::memory_order_relaxed);
    keysHeld.store(held, std::memory_order_relaxed);
    {
        const Program &ep = engine.program();
        juce::SpinLock::ScopedTryLockType lock(progLock);
        if (lock.isLocked() && !pendingSet.load())
            live.seq = ep.seq;
    }

    float vol = apvts.getRawParameterValue("volume")->load();
    float gain = vol * vol * 2.0f;
    if (lastGain < 0.0f)
        lastGain = gain;
    buffer.applyGainRamp(0, 0, n, lastGain, gain);
    if (buffer.getNumChannels() > 1)
        buffer.applyGainRamp(1, 0, n, lastGain, gain);
    lastGain = gain;
    for (int c = 2; c < buffer.getNumChannels(); c++)
        buffer.clear(c, 0, n);
}

/* ------------------------------------------------------------------------------ state --- */
void HartermistProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    state.setProperty("program", program, nullptr);
    state.setProperty("uiScale", (double)uiScale.load(), nullptr);
    state.setProperty("uiScaleSet", uiScaleSet.load(), nullptr);
    state.setProperty("sliderPage", sliderPage.load(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    auto *pg = xml->createNewChildElement("HARTERMISTPROGRAM");
    programToXml(currentProgram(), *pg);
    copyXmlToBinary(*xml, destData);
}

void HartermistProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml || !xml->hasTagName(apvts.state.getType()))
        return;
    Program p;
    bool hasProg = false;
    if (auto *pg = xml->getChildByName("HARTERMISTPROGRAM")) {
        hasProg = programFromXml(*pg, p);
        xml->removeChildElement(pg, true);
    }
    auto state = juce::ValueTree::fromXml(*xml);
    program = juce::jlimit(0, juce::jmax(0, programCount() - 1), (int)state.getProperty("program", 0));
    uiScale = (float)(double)state.getProperty("uiScale", 0.8);
    uiScaleSet = (bool)state.getProperty("uiScaleSet", false);
    sliderPage = juce::jlimit(0, 5, (int)state.getProperty("sliderPage", 0));
    apvts.replaceState(state);
    if (hasProg) {
        for (int i = 0; i < NPARAM; i++)
            p.p[i] = (int16_t)value(i);
        applyProgram(p, false);
    }
}

juce::AudioProcessorEditor *HartermistProcessor::createEditor() { return new HartermistEditor(*this); }

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new HartermistProcessor(); }
