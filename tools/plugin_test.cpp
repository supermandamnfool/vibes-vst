// Plugin-level test: instantiates BeadsAudioProcessor, runs audio/MIDI through it,
// round-trips its state and writes a PNG snapshot of the editor.
//   usage: beads_plugin_test [snapshot.png]   (run under xvfb-run on headless machines)

#include <cmath>
#include <cstdio>

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void setParam (BeadsAudioProcessor& p, const char* id, float plainValue)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
}

struct Run { double rms = 0, peak = 0; bool finite = true; };

static Run render (BeadsAudioProcessor& p, double sr, double seconds, bool withMidi, int block = 512)
{
    Run r; double acc = 0; size_t n = 0;
    juce::AudioBuffer<float> buf (2, block);
    size_t pos = 0;
    const size_t total = (size_t) (seconds * sr);
    while (pos < total)
    {
        juce::MidiBuffer midi;
        for (int i = 0; i < block; ++i)
        {
            const double t = (double) (pos + (size_t) i) / sr, ph = std::fmod (t, 0.5);
            const float v = (float) (std::exp (-ph * 6.0) * 0.5 * std::sin (2 * M_PI * 220.0 * t));
            buf.setSample (0, i, v); buf.setSample (1, i, v);
            if (withMidi && ph < 1.0 / sr * 1.5 && i > 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60 + (int) (t * 2) % 12, (juce::uint8) 100), i);
            if (withMidi && std::fabs (ph - 0.25) < 0.5 / sr) midi.addEvent (juce::MidiMessage::noteOff (1, 60 + (int) (t * 2) % 12), i);
        }
        p.processBlock (buf, midi);
        for (int i = 0; i < block; ++i)
            for (int c = 0; c < 2; ++c)
            {
                const float s = buf.getSample (c, i);
                if (! std::isfinite (s)) r.finite = false;
                acc += (double) s * s; ++n; r.peak = std::fmax (r.peak, std::fabs (s));
            }
        pos += (size_t) block;
    }
    r.rms = std::sqrt (acc / (double) n);
    return r;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const double sr = 44100.0;

    BeadsAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, 512);
    proc.prepareToPlay (sr, 512);

    std::printf ("parameters: %d, reported latency: %d samples\n", (int) proc.getParameters().size(), proc.getLatencySamples());
    CHECK (proc.getParameters().size() > 30, "unexpectedly few parameters");

    // 1. internal seed, dense grains
    setParam (proc, "dry_wet", 1.0f); setParam (proc, "density", 0.9f); setParam (proc, "time", 0.12f);
    setParam (proc, "reverb", 0.3f);
    Run a = render (proc, sr, 6.0, false);
    std::printf ("[internal seed]   rms %.4f peak %.3f finite=%d\n", a.rms, a.peak, (int) a.finite);
    CHECK (a.finite && a.rms > 0.003, "no output with internal seed");

    // 2. host clock seed (no playhead -> free-running at the fallback tempo)
    setParam (proc, "seed_source", 1.0f); setParam (proc, "clock_rate", 5.0f);
    Run b = render (proc, sr, 6.0, false);
    std::printf ("[host clock]      rms %.4f peak %.3f finite=%d\n", b.rms, b.peak, (int) b.finite);
    CHECK (b.finite && b.rms > 0.003, "no output with host clock seed");

    // 3. MIDI seed + MIDI pitch
    setParam (proc, "seed_source", 2.0f); setParam (proc, "midi_pitch", 1.0f); setParam (proc, "pitch_amt", 1.0f);
    Run c = render (proc, sr, 6.0, true);
    std::printf ("[midi seed+pitch] rms %.4f peak %.3f finite=%d\n", c.rms, c.peak, (int) c.finite);
    CHECK (c.finite, "non-finite output with MIDI");

    // 4. state round trip
    setParam (proc, "feedback", 0.77f); setParam (proc, "media", 1.0f);
    juce::MemoryBlock state; proc.getStateInformation (state);
    setParam (proc, "feedback", 0.1f); setParam (proc, "media", 3.0f);
    proc.setStateInformation (state.getData(), (int) state.getSize());
    const float fb = proc.apvts.getRawParameterValue ("feedback")->load();
    const float md = proc.apvts.getRawParameterValue ("media")->load();
    std::printf ("[state]           feedback %.2f media %.0f (expected 0.77 / 1)\n", fb, md);
    CHECK (std::fabs (fb - 0.77f) < 1e-3f && std::fabs (md - 1.0f) < 1e-3f, "state did not round-trip");

    // 5. odd host block sizes, tiny and large, plus a sample-rate change
    proc.prepareToPlay (96000.0, 1024);
    Run d = render (proc, 96000.0, 3.0, true, 1000);
    Run e = render (proc, 96000.0, 1.0, false, 1);
    std::printf ("[96k, odd blocks] rms %.4f finite=%d / %d\n", d.rms, (int) d.finite, (int) e.finite);
    CHECK (d.finite && e.finite, "non-finite at 96 kHz");

    // 5b. wavetable synth mode: input is ignored, banks selected with feedback, and back again
    setParam (proc, "seed_source", 0.0f); setParam (proc, "midi_pitch", 0.0f);
    setParam (proc, "dry_wet", 0.6f); setParam (proc, "density", 0.9f); setParam (proc, "freeze", 0.0f);
    setParam (proc, "synth_mode", 1.0f);
    double bankRms[3];
    const float banks[3] = { 0.0f, 0.5f, 0.95f };
    for (int i = 0; i < 3; ++i)
    {
        setParam (proc, "feedback", banks[i]);
        Run w = render (proc, 96000.0, 2.0, false);
        bankRms[i] = w.rms;
        std::printf ("[wavetable bank %d] rms %.4f peak %.3f finite=%d\n", i, w.rms, w.peak, (int) w.finite);
        CHECK (w.finite && w.rms > 0.003, "no output in wavetable mode (bank %d)", i);
    }
    setParam (proc, "synth_mode", 0.0f); setParam (proc, "feedback", 0.0f);
    Run back = render (proc, 96000.0, 3.0, false);
    std::printf ("[synth -> effect] rms %.4f finite=%d\n", back.rms, (int) back.finite);
    CHECK (back.finite && back.rms > 0.003, "effect did not recover after leaving wavetable mode");

    // 6. editor snapshot
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        if (argc > 1)
        {
            juce::File f (argv[1]);
            f.deleteFile();
            juce::FileOutputStream out (f);
            if (out.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream (img, out); }
            std::printf ("[editor]          %dx%d snapshot -> %s\n", img.getWidth(), img.getHeight(), argv[1]);
        }
    }

    std::printf (failures ? "\n%d FAILURE(S)\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
