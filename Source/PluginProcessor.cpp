#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using juce::AudioParameterBool;
using juce::AudioParameterChoice;
using juce::AudioParameterFloat;
using juce::NormalisableRange;
using juce::ParameterID;

constexpr int kVersion = 1;

// Length of one seed-clock period in beats (quarter notes), same order as the "Clock Rate" choices.
const double kClockBeats[] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
const char* const kClockNames[] = { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };

juce::String signedInt (float v) { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i); }
}

juce::AudioProcessorValueTreeState::ParameterLayout BeadsAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto percent = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; });

    auto attenuverter = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return signedInt (BeadsEngine::attenuverterLaw (v) * 100.0f) + "%"; });

    auto semitones = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int)
        {
            const float st = BeadsEngine::detuneLawSemitones (v);
            return (st > 0.05f ? "+" : "") + juce::String (st, 1) + " st";
        });

    auto cvText = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return juce::String (v, 2); });

    auto volts = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return juce::String (v, 2) + " V"; });

    auto decibels = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return juce::String (v, 1) + " dB"; });

    auto knob = [&] (const char* id, const char* name, float def, juce::AudioParameterFloatAttributes a)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, kVersion }, name, NormalisableRange<float> (0.0f, 1.0f), def, a)); };

    auto ranged = [&] (const char* id, const char* name, float lo, float hi, float def, juce::AudioParameterFloatAttributes a)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, kVersion }, name, NormalisableRange<float> (lo, hi), def, a)); };

    auto toggle = [&] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, kVersion }, name, def)); };

    auto choice = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, kVersion }, name, items, def)); };

    // Main controls
    knob (ParamIDs::time,  "Time",  0.5f, percent);
    knob (ParamIDs::size,  "Size",  0.5f, percent);
    knob (ParamIDs::shape, "Shape", 0.5f, percent);
    knob (ParamIDs::pitch, "Pitch", 0.5f, semitones);
    knob (ParamIDs::density, "Density", 0.5f, percent);

    // Attenuverters: centre = 0. With nothing "patched" they set the amount of randomness.
    knob (ParamIDs::timeAmt,  "Time Amount",  0.5f, attenuverter);
    knob (ParamIDs::sizeAmt,  "Size Amount",  0.5f, attenuverter);
    knob (ParamIDs::shapeAmt, "Shape Amount", 0.5f, attenuverter);
    knob (ParamIDs::pitchAmt, "Pitch Amount", 0.5f, attenuverter);

    // CV inputs (automate these from Ableton envelopes / LFOs / Max for Live)
    ranged (ParamIDs::timeCv,  "Time CV",  -1.6f, 1.6f, 0.0f, cvText);
    ranged (ParamIDs::sizeCv,  "Size CV",  -1.6f, 1.6f, 0.0f, cvText);
    ranged (ParamIDs::shapeCv, "Shape CV", -1.6f, 1.6f, 0.0f, cvText);
    ranged (ParamIDs::pitchCv, "Pitch CV (V/Oct)", -5.0f, 5.0f, 0.0f, volts);
    ranged (ParamIDs::densityCv, "Density CV", -1.0f, 1.0f, 0.0f, cvText);
    ranged (ParamIDs::assignCv,  "Assignable CV", -1.0f, 1.0f, 0.0f, cvText);
    toggle (ParamIDs::timeCvOn,  "Time CV Patched",  false);
    toggle (ParamIDs::sizeCvOn,  "Size CV Patched",  false);
    toggle (ParamIDs::shapeCvOn, "Shape CV Patched", false);
    toggle (ParamIDs::pitchCvOn, "Pitch CV Patched", false);
    toggle (ParamIDs::densityCvOn, "Density CV Patched", false);

    // Mix
    knob (ParamIDs::feedback, "Feedback", 0.0f, percent);
    knob (ParamIDs::dryWet,   "Dry/Wet",  0.5f, percent);
    knob (ParamIDs::reverb,   "Reverb",   0.0f, percent);
    choice (ParamIDs::assignDest, "Assignable CV Destination", { "Off", "Feedback", "Dry/Wet", "Reverb" }, 0);

    // Switches
    toggle (ParamIDs::freeze, "Freeze", false);
    toggle (ParamIDs::trim,   "Trim", false);
    choice (ParamIDs::media,  "Media", { "Dirty Tape", "Clean Tape", "Vintage Digital", "Digital" }, 3);
    toggle (ParamIDs::mono,   "Mono Input", false);
    toggle (ParamIDs::gateOut, "Gate Output Mode", false);
    choice (ParamIDs::seedMode,   "Seed Button", { "Locked", "Gate Off", "Gate On" }, 0);
    choice (ParamIDs::seedSource, "Seed Input", { "None", "Host Clock", "MIDI Notes" }, 0);
    juce::StringArray rates; for (auto* n : kClockNames) rates.add (n);
    choice (ParamIDs::clockRate, "Clock Rate", rates, 4);
    toggle (ParamIDs::midiPitch, "MIDI Note to Pitch CV", false);
    toggle (ParamIDs::synthMode, "Wavetable Synth Mode", false);

    ranged (ParamIDs::inputGain,  "Input Gain",  -24.0f, 24.0f, 0.0f, decibels);
    ranged (ParamIDs::outputGain, "Output Gain", -24.0f, 24.0f, 0.0f, decibels);

    return { p.begin(), p.end() };
}

BeadsAudioProcessor::BeadsAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VibesState", createLayout())
{
}

BeadsAudioProcessor::~BeadsAudioProcessor()
{
    cancelPendingUpdate();
}

bool BeadsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono();
}

void BeadsAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    engine.prepare (sr, samplesPerBlock);
    engine.preConfigure (juce::roundToInt (param (ParamIDs::media)), flag (ParamIDs::mono), flag (ParamIDs::synthMode));
    engine.consumeLatencyChanged();
    setLatencySamples (engine.latencySamples());

    seedGate.assign ((size_t) juce::jmax (samplesPerBlock, 64), 0);
    heldNotes.fill (false);
    heldCount = 0;
    midiGate = false;
    freeRunBeats = 0.0;
}

void BeadsAudioProcessor::handleAsyncUpdate()
{
    setLatencySamples (engine.latencySamples());
}

void BeadsAudioProcessor::buildSeedGate (juce::MidiBuffer& midi, int n, int source, int clockIndex,
                                         double sr, bool& midiPitchActive, float& midiPitchSemitones)
{
    juce::ignoreUnused (midiPitchActive);

    if ((int) seedGate.size() < n)
        seedGate.assign ((size_t) n, 0);

    // MIDI is always tracked so "MIDI Note to Pitch CV" works with any seed source.
    int cursor = 0;
    bool g = midiGate;
    auto fillTo = [&] (int upTo) { for (; cursor < upTo; ++cursor) seedGate[(size_t) cursor] = g ? 1 : 0; };

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = juce::jlimit (0, n - 1, meta.samplePosition);

        if (m.isNoteOn())
        {
            fillTo (pos);
            if (g)   // every note-on is a fresh trigger: insert one low sample before it
            {
                if (cursor > 0) seedGate[(size_t) cursor - 1] = 0;
                else { seedGate[(size_t) cursor] = 0; ++cursor; }
            }
            const int note = m.getNoteNumber();
            if (! heldNotes[(size_t) note]) { heldNotes[(size_t) note] = true; ++heldCount; }
            lastNote = note;
            g = true;
        }
        else if (m.isNoteOff() || m.isAllNotesOff() || m.isAllSoundOff())
        {
            fillTo (pos);
            if (m.isNoteOff())
            {
                const int note = m.getNoteNumber();
                if (heldNotes[(size_t) note]) { heldNotes[(size_t) note] = false; --heldCount; }
            }
            else { heldNotes.fill (false); heldCount = 0; }
            g = heldCount > 0;
        }
    }
    fillTo (n);
    midiGate = g;
    midiPitchSemitones = (float) (lastNote - 60);

    if (source == 2)
        return;   // gate array already holds the MIDI gate

    if (source == 1)
    {
        double bpm = lastBpm;
        bool playing = false;
        double startBeats = freeRunBeats;

        if (auto* ph = getPlayHead())
        {
            if (auto info = ph->getPosition())
            {
                if (auto b = info->getBpm()) { bpm = *b; lastBpm = bpm; }
                playing = info->getIsPlaying();
                if (playing)
                    if (auto ppq = info->getPpqPosition())
                        startBeats = *ppq;
            }
        }

        const double beatsPerSample = bpm / 60.0 / sr;
        const double len = kClockBeats[juce::jlimit (0, 9, clockIndex)];
        for (int i = 0; i < n; ++i)
        {
            const double x = (startBeats + (double) i * beatsPerSample) / len;
            seedGate[(size_t) i] = (x - std::floor (x)) < 0.5 ? 1 : 0;
        }
        freeRunBeats = startBeats + (double) n * beatsPerSample;
    }
}

void BeadsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if (n <= 0 || buffer.getNumChannels() < 2)
        return;

    EngineSettings s;
    s.time = param (ParamIDs::time);   s.size = param (ParamIDs::size);   s.shape = param (ParamIDs::shape);
    s.pitch = param (ParamIDs::pitch); s.density = param (ParamIDs::density);
    s.timeAmt = param (ParamIDs::timeAmt);   s.sizeAmt = param (ParamIDs::sizeAmt);
    s.shapeAmt = param (ParamIDs::shapeAmt); s.pitchAmt = param (ParamIDs::pitchAmt);
    s.timeCv = param (ParamIDs::timeCv);     s.sizeCv = param (ParamIDs::sizeCv);   s.shapeCv = param (ParamIDs::shapeCv);
    s.densityCv = param (ParamIDs::densityCv); s.assignCv = param (ParamIDs::assignCv);
    s.timeHasCv = flag (ParamIDs::timeCvOn);   s.sizeHasCv = flag (ParamIDs::sizeCvOn);
    s.shapeHasCv = flag (ParamIDs::shapeCvOn); s.densityHasCv = flag (ParamIDs::densityCvOn);

    const int source = juce::roundToInt (param (ParamIDs::seedSource));
    bool unused = false;
    float midiSemitones = 0.0f;
    buildSeedGate (midi, n, source, juce::roundToInt (param (ParamIDs::clockRate)), sampleRate, unused, midiSemitones);

    const bool pitchFromMidi = flag (ParamIDs::midiPitch);
    s.pitchHasCv = flag (ParamIDs::pitchCvOn) || pitchFromMidi;
    s.pitchCvSemitones = param (ParamIDs::pitchCv) * 12.0f + (pitchFromMidi ? midiSemitones : 0.0f);

    s.feedback = param (ParamIDs::feedback); s.dryWet = param (ParamIDs::dryWet); s.reverb = param (ParamIDs::reverb);
    s.assignDestination = juce::roundToInt (param (ParamIDs::assignDest));
    s.media = juce::roundToInt (param (ParamIDs::media));
    s.mono = flag (ParamIDs::mono);
    s.gateOutput = flag (ParamIDs::gateOut);
    s.synthMode = flag (ParamIDs::synthMode);
    s.freeze = flag (ParamIDs::freeze);
    s.trim = flag (ParamIDs::trim);
    s.seedButton = juce::roundToInt (param (ParamIDs::seedMode));
    s.seedPatched = source != 0;
    s.inputGainDb = param (ParamIDs::inputGain);
    s.outputGainDb = param (ParamIDs::outputGain);

    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : nullptr;
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    engine.process (inL, inR, outL, outR, n, s.seedPatched ? seedGate.data() : nullptr, s);

    if (engine.consumeLatencyChanged())
        triggerAsyncUpdate();

    midi.clear();
}

juce::AudioProcessorEditor* BeadsAudioProcessor::createEditor()
{
    return new BeadsAudioProcessorEditor (*this);
}

void BeadsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BeadsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BeadsAudioProcessor();
}
