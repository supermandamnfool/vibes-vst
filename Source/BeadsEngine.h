#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "Resampler.h"

namespace beads { class Processor; class Synth; }

// Everything the "front panel" of the module controls, in the units of the
// hardware (knob positions 0..1, CV in the same ranges the firmware uses).
struct EngineSettings
{
    // Main knobs (pot positions 0..1)
    float time = 0.5f, size = 0.5f, shape = 0.5f, pitch = 0.5f, density = 0.5f;
    // Attenuverter knobs (pot positions 0..1, 0.5 = centre)
    float timeAmt = 0.5f, sizeAmt = 0.5f, shapeAmt = 0.5f, pitchAmt = 0.5f;
    // CV inputs. Pitch CV is in semitones (the module's V/oct input).
    float timeCv = 0, sizeCv = 0, shapeCv = 0, pitchCvSemitones = 0, densityCv = 0, assignCv = 0;
    // "Is something patched into this CV jack?"
    // With no cable the attenuverter becomes the internal randomness amount.
    bool timeHasCv = false, sizeHasCv = false, shapeHasCv = false, pitchHasCv = false, densityHasCv = false;

    float feedback = 0.0f, dryWet = 0.5f, reverb = 0.0f;
    int   assignDestination = 0;   // 0 none, 1 feedback, 2 dry/wet, 3 reverb

    int   media = 3;               // 0 dirty tape, 1 clean tape, 2 vintage digital, 3 digital
    bool  mono = false;
    bool  gateOutput = false;
    bool  synthMode = false;       // wavetable granular synth (the module's no-input mode); audio input is ignored
    bool  freeze = false;
    bool  trim = false;            // rising edge trims the frozen buffer
    int   seedButton = 0;          // 0 locked, 1 released(off), 2 pressed(on)
    bool  seedPatched = false;     // external gate supplied through seedGate[]

    float inputGainDb = 0.0f;
    float outputGainDb = 0.0f;
};

class BeadsEngine
{
public:
    BeadsEngine();
    ~BeadsEngine();

    void prepare (double hostSampleRate, int maxBlockSize);
    void reset();

    // Optionally configure the media mode ahead of the first block (so the latency is known).
    void preConfigure (int mediaMode, bool monoMode, bool synth = false) { configure (mediaMode, monoMode, synth); }

    // seedGate: one 0/1 value per host sample, or nullptr when unpatched.
    void process (const float* inL, const float* inR, float* outL, float* outR,
                  int numSamples, const uint8_t* seedGate, const EngineSettings& settings);

    // End-to-end latency in host samples for the current configuration.
    int latencySamples() const { return latency; }
    bool consumeLatencyChanged() { const bool c = latencyDirty; latencyDirty = false; return c; }

    // Internal sample rate used by a media mode.
    static float internalRateForMedia (int media);

    // Pot laws from the firmware (cv_reader.h)
    static float attenuverterLaw (float pot);
    static float detuneLawSemitones (float pot);

private:
    struct Frame { float l, r; uint8_t gate; };
    struct Fifo
    {
        std::vector<Frame> v; size_t rd = 0;
        size_t size() const { return v.size() - rd; }
        void push (const Frame& f) { v.push_back (f); }
        void clear() { v.clear(); rd = 0; }
        void compact() { if (rd > 4096 && rd * 2 > v.size()) { v.erase (v.begin(), v.begin() + (std::ptrdiff_t) rd); rd = 0; } }
    };

    void configure (int media, bool mono, bool synth);
    void processBlock (const EngineSettings& s, const Frame* in, Frame* out, int frames);
    void updateSmoothedControls (const EngineSettings& s, bool snap);

    double hostRate = 48000.0;
    int maxBlock = 512;

    std::unique_ptr<beads::Processor> processor;
    std::unique_ptr<beads::Synth> synth;
    bool synthMode = false;
    std::vector<uint8_t> delayBuffer1, delayBuffer2;
    std::vector<uint16_t> reverbBuffer;

    StereoResampler inResampler, outResampler;
    Fifo procIn;
    std::vector<float> hostOutL, hostOutR;   // FIFO of host-rate output
    size_t hostOutRd = 0;

    std::vector<float> tmpL, tmpR;
    std::vector<float> procOutL, procOutR;   // engine output at the internal rate, per host call
    float lastGateIn = 0;

    int media = -1;
    bool mono = false;
    int blockSize = 12;
    float procRate = 48000.0f;
    int muteBlocks = 0;
    int latency = 0;
    bool latencyDirty = false;
    bool prevTrim = false;
    uint8_t prevSeedFlags = 0;
    bool smoothingPrimed = false;

    // Block-rate smoothed pot values (the firmware low-passes pots at 0.05 per block)
    float sTime, sSize, sShape, sPitch, sDensity, sTimeAmt, sSizeAmt, sShapeAmt, sPitchAmt;
    float sFeedback, sDryWet, sReverb;
};
