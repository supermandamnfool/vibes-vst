#include "BeadsEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/utils/gate_flags.h"

namespace
{
constexpr int   kBlockRate    = 4000;     // the firmware processes audio in 4 kHz blocks
constexpr int   kMaxBlock     = 16;
constexpr float kPotLp        = 0.05f;    // PotChannel low-pass coefficient per block
constexpr float kMuteSeconds  = 0.15f;    // codec restart silence after a media/mono change

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float smoothStep (float x) { return x * x * (3.0f - 2.0f * x); }
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
}

BeadsEngine::BeadsEngine()
    : processor (new beads::Processor()),
      synth (new beads::Synth()),
      delayBuffer1 (512 * 1024, 0),
      delayBuffer2 (256 * 1024, 0),
      reverbBuffer (32768, 0)
{
    sTime = sSize = sShape = sDensity = 0.5f;
    sPitch = 0.0f;
    sTimeAmt = sSizeAmt = sShapeAmt = sPitchAmt = 0.0f;
    sFeedback = sReverb = 0.0f;
    sDryWet = 0.5f;
}

BeadsEngine::~BeadsEngine() = default;

float BeadsEngine::internalRateForMedia (int m)
{
    static const float rates[4] = { 24000.0f, 48000.0f, 32000.0f, 48000.0f };
    return rates[std::clamp (m, 0, 3)];
}

// PotChannel::LAW_ATTENUVERTER
float BeadsEngine::attenuverterLaw (float pot)
{
    float v = pot - 0.5f;
    return (2.0f / 0.47f) * v * std::max (std::fabs (v) - 0.03f, 0.0f);
}

// PotChannel::LAW_DETUNE: detented musical intervals, result in semitones
float BeadsEngine::detuneLawSemitones (float pot)
{
    static const float intervals[8] = { 0.0f, 1.0f, 3.0f, 4.0f, 7.0f, 12.0f, 24.0f, 24.0f };

    float detune = 2.0f * pot - 1.0f;
    const float sign = detune < 0.0f ? -1.0f : 1.0f;
    detune = detune * sign * 6.0f;
    int integral = (int) detune;
    float fractional = detune - (float) integral;
    integral = std::clamp (integral, 0, 6);

    fractional = std::max ((fractional - 0.33333333f) * 1.5f, 0.0f);
    const float a = intervals[integral];
    const float b = intervals[integral + 1];
    return (a + (b - a) * smoothStep (std::min (fractional, 1.0f))) * sign;
}

void BeadsEngine::prepare (double hostSampleRate, int maxBlockSize)
{
    hostRate = hostSampleRate;
    maxBlock = std::max (maxBlockSize, 64);

    std::fill (delayBuffer1.begin(), delayBuffer1.end(), 0);
    std::fill (delayBuffer2.begin(), delayBuffer2.end(), 0);
    std::fill (reverbBuffer.begin(), reverbBuffer.end(), 0);

    uint8_t* buffers[3] = { delayBuffer1.data(),
                            delayBuffer1.data() + 256 * 1024,
                            delayBuffer2.data() };
    processor->Init (buffers, reverbBuffer.data());

    tmpL.assign ((size_t) maxBlock, 0.0f);
    tmpR.assign ((size_t) maxBlock, 0.0f);

    synthMode = false;        // processor->Init above left the effect engine active
    media = -1;               // force configure() on the first block
    smoothingPrimed = false;
    prevTrim = false;
    prevSeedFlags = 0;
}

void BeadsEngine::reset()
{
    media = -1;
}

void BeadsEngine::configure (int newMedia, bool newMono, bool newSynth)
{
    media = std::clamp (newMedia, 0, 3);
    mono = newMono;

    // The effect and the synth share one reverb buffer: clear it and re-init
    // whichever engine is taking over, like the firmware does on a mode change.
    if (newSynth != synthMode)
    {
        std::fill (reverbBuffer.begin(), reverbBuffer.end(), 0);
        if (newSynth)
            synth->Init (reverbBuffer.data());
        else
        {
            uint8_t* buffers[3] = { delayBuffer1.data(), delayBuffer1.data() + 256 * 1024, delayBuffer2.data() };
            processor->Init (buffers, reverbBuffer.data());
        }
        synthMode = newSynth;
    }

    if (synthMode)
        procRate = 48000.0f;       // the synth always runs at 48 kHz
    else
    {
        processor->Configure (beads::Processor::Media (media), mono);
        procRate = processor->sample_rate();
    }

    blockSize = std::min (kMaxBlock, (int) (procRate / (float) kBlockRate + 0.5f));

    inResampler.prepare (hostRate, procRate, (size_t) maxBlock);
    outResampler.prepare (procRate, hostRate,
                          (size_t) ((double) maxBlock * procRate / hostRate) + (size_t) blockSize + 64);

    procIn.clear();
    prevSeedFlags = 0;
    lastGateIn = 0;

    // Prime the output FIFO so the block quantisation can never starve it.
    const int prime = (int) std::ceil ((double) blockSize * hostRate / procRate) + 2;
    hostOutL.assign ((size_t) prime, 0.0f);
    hostOutR.assign ((size_t) prime, 0.0f);
    hostOutRd = 0;

    const double inLat  = inResampler.latencyInputSamples();
    const double outLat = outResampler.latencyInputSamples() * hostRate / procRate;
    const int newLatency = prime + (int) std::lround (inLat + outLat);
    if (newLatency != latency)
    {
        latency = newLatency;
        latencyDirty = true;
    }

    muteBlocks = (int) (kMuteSeconds * (float) kBlockRate);
    smoothingPrimed = false;
}

void BeadsEngine::updateSmoothedControls (const EngineSettings& s, bool snap)
{
    auto lp = [snap] (float& state, float target)
    {
        if (snap) state = target; else state += (target - state) * kPotLp;
    };

    lp (sTime,    s.time);
    lp (sSize,    s.size);
    lp (sShape,   s.shape);
    lp (sDensity, s.density);
    lp (sPitch,   detuneLawSemitones (s.pitch));
    lp (sTimeAmt,  attenuverterLaw (s.timeAmt));
    lp (sSizeAmt,  attenuverterLaw (s.sizeAmt));
    lp (sShapeAmt, attenuverterLaw (s.shapeAmt));
    lp (sPitchAmt, attenuverterLaw (s.pitchAmt));
    lp (sFeedback, s.feedback);
    lp (sDryWet,   s.dryWet);
    lp (sReverb,   s.reverb);
}

void BeadsEngine::processBlock (const EngineSettings& s, const Frame* in, Frame* out, int frames)
{
    if (muteBlocks > 0)
    {
        --muteBlocks;
        for (int i = 0; i < frames; ++i) out[i] = { 0.0f, 0.0f, 0 };
        return;
    }

    updateSmoothedControls (s, ! smoothingPrimed);
    smoothingPrimed = true;

    beads::FloatFrame fin[kMaxBlock], fout[kMaxBlock];
    bool freeze[kMaxBlock];
    stmlib::GateFlags seed[kMaxBlock];

    for (int i = 0; i < frames; ++i)
    {
        fin[i].l = in[i].l;
        fin[i].r = in[i].r;
        freeze[i] = s.freeze;
        prevSeedFlags = stmlib::ExtractGateFlags ((stmlib::GateFlags) prevSeedFlags, in[i].gate != 0);
        seed[i] = (stmlib::GateFlags) prevSeedFlags;
    }

    beads::Parameters p;
    p.time.base = sTime;   p.time.attenuverter = sTimeAmt;
    p.time.has_cv = s.timeHasCv;   p.time.cv = clampf (s.timeCv, -1.6f, 1.6f);
    p.size.base = sSize;   p.size.attenuverter = sSizeAmt;
    p.size.has_cv = s.sizeHasCv;   p.size.cv = clampf (s.sizeCv, -1.6f, 1.6f);
    p.shape.base = sShape; p.shape.attenuverter = sShapeAmt;
    p.shape.has_cv = s.shapeHasCv; p.shape.cv = clampf (s.shapeCv, -1.6f, 1.6f);
    p.pitch.base = sPitch; p.pitch.attenuverter = sPitchAmt;
    p.pitch.has_cv = s.pitchHasCv; p.pitch.cv = clampf (s.pitchCvSemitones, -60.0f, 60.0f);
    p.monophonic_mode = false;

    beads::ModulatedParameter d;
    d.base = sDensity;
    d.cv = s.densityHasCv ? clampf (s.densityCv, -1.0f, 1.0f) : 0.0f;

    const float mixBase[3] = { sFeedback, sDryWet, sReverb };
    float mix[3];
    for (int i = 0; i < 3; ++i)
    {
        const float amount = s.assignDestination == i + 1 ? 1.0f : 0.0f;
        mix[i] = clampf (mixBase[i] * 1.005f + (s.assignCv - 0.002f) * amount, 0.0f, 1.0f);
    }

    beads::Processor::MixParameters m;
    m.feedback = mix[0];
    m.dry_wet = mix[1];
    m.reverb_amount = mix[2];
    m.output_gate = s.gateOutput;

    const beads::SeedButtonState button = s.seedButton == 0 ? beads::SEED_BUTTON_STATE_LOCKED
                                        : s.seedButton == 1 ? beads::SEED_BUTTON_STATE_OFF
                                                            : beads::SEED_BUTTON_STATE_ON;

    if (synthMode)
        synth->Render (button, s.seedPatched ? seed : nullptr, freeze, beads::Synth::Resolution (std::clamp (s.media, 0, 3)),
                       d, p, m, fout, (size_t) frames);
    else
        processor->Process (button, s.seedPatched ? seed : nullptr, freeze, d, p, m, fin, fout, (size_t) frames);

    for (int i = 0; i < frames; ++i)
    {
        out[i].l = clampf (fout[i].l, -1.0f, 0.999999f);
        out[i].r = clampf (fout[i].r, -1.0f, 0.999999f);
        out[i].gate = 0;
    }
}

void BeadsEngine::process (const float* inL, const float* inR, float* outL, float* outR,
                           int numSamples, const uint8_t* seedGate, const EngineSettings& s)
{
    // In synth mode the media knob only picks the bit depth and mono is irrelevant,
    // so neither needs the (muting) reconfiguration.
    if (media < 0 || synthMode != s.synthMode || (! s.synthMode && (media != s.media || mono != s.mono)))
        configure (s.media, s.mono, s.synthMode);

    if (s.trim && ! prevTrim && ! synthMode)
        processor->Trim();
    prevTrim = s.trim;

    if ((int) tmpL.size() < numSamples)
    {
        tmpL.assign ((size_t) numSamples, 0.0f);
        tmpR.assign ((size_t) numSamples, 0.0f);
    }

    const float inGain  = dbToGain (s.inputGainDb);
    const float outGain = dbToGain (s.outputGainDb);
    const bool softClip = media == 0 && ! synthMode;   // dirty tape: soft clipping of the input stage, as in the firmware

    for (int i = 0; i < numSamples; ++i)
    {
        float l = inL[i] * inGain;
        float r = (inR != nullptr ? inR[i] : inL[i]) * inGain;
        if (mono) l = r = 0.5f * (l + r);
        if (softClip) { l = stmlib::SoftClip (l); r = stmlib::SoftClip (r); }
        tmpL[(size_t) i] = l;
        tmpR[(size_t) i] = r;
    }

    // Host rate -> internal rate, carrying the seed gate along.
    const double hostPerProc = hostRate / (double) procRate;
    inResampler.process (tmpL.data(), tmpR.data(), (size_t) numSamples,
        [&] (float l, float r, size_t j)
        {
            uint8_t g = 0;
            if (seedGate != nullptr)
            {
                const size_t idx = std::min ((size_t) numSamples - 1, (size_t) ((double) j * hostPerProc));
                g = seedGate[idx];
            }
            procIn.push ({ l, r, g });
        });

    // Run the engine in 4 kHz blocks.
    const size_t cap = procIn.size() + 16;
    procOutL.clear(); procOutR.clear();
    procOutL.reserve (cap); procOutR.reserve (cap);

    Frame inBlock[kMaxBlock], outBlock[kMaxBlock];
    while ((int) procIn.size() >= blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
            inBlock[i] = procIn.v[procIn.rd + (size_t) i];
        procIn.rd += (size_t) blockSize;

        processBlock (s, inBlock, outBlock, blockSize);

        for (int i = 0; i < blockSize; ++i)
        {
            procOutL.push_back (outBlock[i].l);
            procOutR.push_back (outBlock[i].r);
        }
    }
    procIn.compact();

    // Internal rate -> host rate.
    outResampler.process (procOutL.data(), procOutR.data(), procOutL.size(),
        [&] (float l, float r, size_t)
        {
            hostOutL.push_back (l);
            hostOutR.push_back (r);
        });

    for (int i = 0; i < numSamples; ++i)
    {
        if (hostOutRd < hostOutL.size())
        {
            outL[i] = hostOutL[hostOutRd] * outGain;
            outR[i] = hostOutR[hostOutRd] * outGain;
            ++hostOutRd;
        }
        else
        {
            outL[i] = outR[i] = 0.0f;
        }
    }

    if (hostOutRd > 4096 && hostOutRd * 2 > hostOutL.size())
    {
        hostOutL.erase (hostOutL.begin(), hostOutL.begin() + (std::ptrdiff_t) hostOutRd);
        hostOutR.erase (hostOutR.begin(), hostOutR.begin() + (std::ptrdiff_t) hostOutRd);
        hostOutRd = 0;
    }
}
