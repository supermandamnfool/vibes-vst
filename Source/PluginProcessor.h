#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "BeadsEngine.h"

namespace ParamIDs
{
    // main knobs
    inline constexpr const char* time = "time";
    inline constexpr const char* size = "size";
    inline constexpr const char* shape = "shape";
    inline constexpr const char* pitch = "pitch";
    inline constexpr const char* density = "density";
    // attenuverters
    inline constexpr const char* timeAmt = "time_amt";
    inline constexpr const char* sizeAmt = "size_amt";
    inline constexpr const char* shapeAmt = "shape_amt";
    inline constexpr const char* pitchAmt = "pitch_amt";
    // CV inputs and "patched" switches
    inline constexpr const char* timeCv = "time_cv";
    inline constexpr const char* sizeCv = "size_cv";
    inline constexpr const char* shapeCv = "shape_cv";
    inline constexpr const char* pitchCv = "pitch_cv";
    inline constexpr const char* densityCv = "density_cv";
    inline constexpr const char* assignCv = "assign_cv";
    inline constexpr const char* timeCvOn = "time_cv_on";
    inline constexpr const char* sizeCvOn = "size_cv_on";
    inline constexpr const char* shapeCvOn = "shape_cv_on";
    inline constexpr const char* pitchCvOn = "pitch_cv_on";
    inline constexpr const char* densityCvOn = "density_cv_on";
    // mix section
    inline constexpr const char* feedback = "feedback";
    inline constexpr const char* dryWet = "dry_wet";
    inline constexpr const char* reverb = "reverb";
    inline constexpr const char* assignDest = "assign_dest";
    // switches
    inline constexpr const char* freeze = "freeze";
    inline constexpr const char* trim = "trim";
    inline constexpr const char* media = "media";
    inline constexpr const char* mono = "mono";
    inline constexpr const char* gateOut = "gate_out";
    inline constexpr const char* seedMode = "seed_mode";
    inline constexpr const char* seedSource = "seed_source";
    inline constexpr const char* clockRate = "clock_rate";
    inline constexpr const char* midiPitch = "midi_pitch";
    inline constexpr const char* synthMode = "synth_mode";
    // levels
    inline constexpr const char* inputGain = "input_gain";
    inline constexpr const char* outputGain = "output_gain";
}

class BeadsAudioProcessor : public juce::AudioProcessor,
                            private juce::AsyncUpdater
{
public:
    BeadsAudioProcessor();
    ~BeadsAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void handleAsyncUpdate() override;
    void buildSeedGate (juce::MidiBuffer& midi, int numSamples, int source, int clockIndex,
                        double sampleRate, bool& pitchFromMidi, float& midiPitchSemitones);

    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  flag  (const char* id) const { return param (id) > 0.5f; }

    BeadsEngine engine;
    double sampleRate = 48000.0;

    std::vector<uint8_t> seedGate;
    std::array<bool, 128> heldNotes {};
    int heldCount = 0;
    int lastNote = 60;
    bool midiGate = false;

    double freeRunBeats = 0.0;   // clock position when the transport is stopped
    double lastBpm = 120.0;
    long long lastPulseIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeadsAudioProcessor)
};
