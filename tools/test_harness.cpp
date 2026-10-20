// Headless sanity tests for the Beads engine wrapper (no JUCE needed).
//   usage: test_harness [outdir]
// Writes a few WAV renders and prints measurements. Exit code != 0 on failure.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../Source/BeadsEngine.h"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) l.size(), dataBytes = n * 4, riff = 36 + dataBytes, fmtLen = 16, rate = (uint32_t) sr, byteRate = rate * 4;
    const uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtLen, 4, 1, f); std::fwrite (&pcm, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&rate, 4, 1, f); std::fwrite (&byteRate, 4, 1, f); std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (size_t i = 0; i < l.size(); ++i)
    {
        int16_t a = (int16_t) std::lround (std::fmax (-1.0f, std::fmin (1.0f, l[i])) * 32767.0f);
        int16_t b = (int16_t) std::lround (std::fmax (-1.0f, std::fmin (1.0f, r[i])) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

struct Stats { double rms = 0, peak = 0; bool finite = true; };
static Stats measure (const std::vector<float>& v, size_t from, size_t to)
{
    Stats s; double acc = 0; size_t n = 0;
    for (size_t i = from; i < to && i < v.size(); ++i)
    {
        if (! std::isfinite (v[i])) s.finite = false;
        acc += (double) v[i] * v[i]; ++n;
        s.peak = std::fmax (s.peak, std::fabs (v[i]));
    }
    s.rms = n ? std::sqrt (acc / (double) n) : 0;
    return s;
}

// Renders `seconds` of audio through the engine in host blocks of `block` samples.
template <typename InputFn, typename SettingsFn>
static void render (BeadsEngine& e, double sr, int block, double seconds, InputFn input, SettingsFn settings,
                    std::vector<float>& outL, std::vector<float>& outR, std::vector<float>* inLout = nullptr)
{
    const size_t total = (size_t) (seconds * sr);
    outL.assign (total, 0); outR.assign (total, 0);
    std::vector<float> iL ((size_t) block), iR ((size_t) block), oL ((size_t) block), oR ((size_t) block);
    if (inLout) inLout->assign (total, 0);
    for (size_t pos = 0; pos < total; pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, total - pos);
        for (int i = 0; i < n; ++i) { input (pos + (size_t) i, sr, iL[(size_t) i], iR[(size_t) i]); if (inLout) (*inLout)[pos + (size_t) i] = iL[(size_t) i]; }
        EngineSettings s = settings (pos, sr);
        e.process (iL.data(), iR.data(), oL.data(), oR.data(), n, nullptr, s);
        for (int i = 0; i < n; ++i) { outL[pos + (size_t) i] = oL[(size_t) i]; outR[pos + (size_t) i] = oR[(size_t) i]; }
    }
}

// A little rhythmic test signal: decaying 220 Hz + 330 Hz plucks every 0.5 s with some noise.
static void pluckInput (size_t i, double sr, float& l, float& r)
{
    const double t = (double) i / sr, ph = std::fmod (t, 0.5);
    const double env = std::exp (-ph * 7.0);
    const double v = env * (0.5 * std::sin (2 * M_PI * 220.0 * t) + 0.25 * std::sin (2 * M_PI * 330.0 * t));
    l = (float) v;
    r = (float) (v * 0.9);
}

int main (int argc, char** argv)
{
    const std::string outdir = argc > 1 ? argv[1] : ".";
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int blocks[] = { 64, 512, 480 };

    // ---- 1. Fully dry path: gain, latency and distortion of the conversion chain --------------
    std::printf ("[1] dry path (dry/wet = 0, no reverb)\n");
    for (int media = 0; media < 4; ++media)
    for (double sr : rates)
    {
        BeadsEngine e; e.prepare (sr, 512);
        std::vector<float> oL, oR, in;
        auto input = [] (size_t i, double r, float& l, float& rr) { l = rr = 0.5f * (float) std::sin (2 * M_PI * 1000.0 * (double) i / r); };
        auto settings = [media] (size_t, double) { EngineSettings s; s.media = media; s.dryWet = 0.0f; s.reverb = 0.0f; s.feedback = 0.0f; return s; };
        render (e, sr, 512, 2.0, input, settings, oL, oR, &in);
        const size_t from = (size_t) (sr * 1.0), to = (size_t) (sr * 1.9);
        Stats so = measure (oL, from, to), si = measure (in, from, to);
        std::printf ("  media %d @ %6.0f Hz: out rms %.4f (in %.4f, ratio %.3f) peak %.3f, latency reported %d\n",
                     media, sr, so.rms, si.rms, so.rms / si.rms, so.peak, e.latencySamples());
        CHECK (so.finite, "non-finite output");
        // dry gain is 0.75 on the module; tape modes degrade the dry signal, so allow a band
        CHECK (so.rms / si.rms > 0.55 && so.rms / si.rms < 0.9, "dry gain out of range: %.3f", so.rms / si.rms);
    }

    // ---- 2. Wet granular render with a plucked input, all four media, various settings ---------
    std::printf ("[2] wet renders\n");
    for (int media = 0; media < 4; ++media)
    {
        const double sr = 48000.0;
        BeadsEngine e; e.prepare (sr, 512);
        std::vector<float> oL, oR;
        auto settings = [media] (size_t, double)
        {
            EngineSettings s; s.media = media;
            s.dryWet = 1.0f; s.reverb = 0.35f; s.feedback = 0.3f;
            s.density = 0.9f; s.size = 0.45f; s.time = 0.12f; s.shape = 0.5f;
            s.pitchAmt = 0.5f; s.timeAmt = 0.65f;
            return s;
        };
        render (e, sr, 480, 8.0, pluckInput, settings, oL, oR);
        Stats a = measure (oL, (size_t) (sr * 1.0), (size_t) (sr * 8.0));
        std::printf ("  media %d: wet rms %.4f peak %.3f\n", media, a.rms, a.peak);
        CHECK (a.finite, "non-finite output");
        CHECK (a.rms > 0.003, "wet output is silent (rms %.5f)", a.rms);
        CHECK (a.peak <= 1.05, "output far past full scale (%.3f)", a.peak);
        writeWav (outdir + "/wet_media" + std::to_string (media) + ".wav", oL, oR, (int) sr);
    }

    // ---- 3. Freeze sustains a texture after the input stops; without it the texture decays ------
    std::printf ("[3] freeze\n");
    {
        const double sr = 44100.0;
        double heldRms[2] = { 0, 0 };
        for (int frozen = 0; frozen < 2; ++frozen)
        {
            BeadsEngine e; e.prepare (sr, 256);
            std::vector<float> oL, oR;
            auto input = [] (size_t i, double r, float& l, float& rr)
            { const double t = (double) i / r; l = rr = t < 4.0 ? (float) (0.4 * std::sin (2 * M_PI * 440.0 * t)) : 0.0f; };
            auto settings = [frozen] (size_t pos, double r)
            {
                EngineSettings s; s.media = 3; s.dryWet = 1.0f; s.density = 0.7f; s.size = 0.5f; s.time = 0.1f;
                s.freeze = frozen && (double) pos / r > 4.0;
                return s;
            };
            render (e, sr, 256, 12.0, input, settings, oL, oR);
            heldRms[frozen] = measure (oL, (size_t) (sr * 8.0), (size_t) (sr * 12.0)).rms;
            if (frozen) writeWav (outdir + "/freeze.wav", oL, oR, (int) sr);
        }
        std::printf ("  rms 8-12 s, input silent since 4 s: unfrozen %.4f, frozen %.4f\n", heldRms[0], heldRms[1]);
        CHECK (heldRms[1] > 0.01, "freeze did not sustain the texture");
        CHECK (heldRms[0] < 0.002, "texture did not decay without freeze");
    }

    // ---- 4. Switching media/mono mid-stream must stay finite -----------------------------------
    std::printf ("[4] media/mono switching + odd block sizes\n");
    {
        const double sr = 48000.0;
        BeadsEngine e; e.prepare (sr, 1024);
        std::vector<float> oL, oR;
        auto settings = [] (size_t pos, double r)
        {
            EngineSettings s; const int k = (int) ((double) pos / r / 0.7);
            s.media = k % 4; s.mono = (k / 2) % 2 == 1; s.dryWet = 0.8f; s.reverb = 0.4f; s.feedback = 0.6f; s.density = 0.6f;
            return s;
        };
        render (e, sr, 37, 8.0, pluckInput, settings, oL, oR);
        Stats a = measure (oL, 0, oL.size());
        CHECK (a.finite, "non-finite output while switching");
        CHECK (a.peak <= 1.05, "peak > 1.05 while switching");
        std::printf ("  peak %.3f finite=%d\n", a.peak, (int) a.finite);
    }

    std::printf (failures ? "\n%d FAILURE(S)\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
