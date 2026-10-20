#pragma once

// Streaming stereo band-limited resampler (Kaiser-windowed sinc, arbitrary
// ratio). Used to convert between the host sample rate and the fixed internal
// rates of the Beads engine (24 / 32 / 48 kHz depending on the media mode),
// the way the module's codec does in hardware.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

class StereoResampler
{
public:
    void prepare (double inRate, double outRate, size_t maxChunk)
    {
        bypass = std::abs (inRate - outRate) < 1.0e-6;
        step   = inRate / outRate;

        const double fc = std::min (1.0, outRate / inRate) * 0.94;  // cutoff / input Nyquist
        cutoff    = fc;
        halfWidth = kZeroCrossings / fc;                            // in input samples

        buildTable();

        const size_t margin = (size_t) std::ceil (halfWidth) + 4;
        weights.assign (2 * margin + 8, 0.0f);
        histL.assign (maxChunk + 4 * margin + 16, 0.0f);
        histR.assign (histL.size(), 0.0f);
        reset();
    }

    void reset()
    {
        const size_t lead = (size_t) std::ceil (halfWidth) + 2;
        std::fill (histL.begin(), histL.end(), 0.0f);
        std::fill (histR.begin(), histR.end(), 0.0f);
        count = lead;                   // pre-roll of zeros so the first output is aligned with input 0
        pos   = (double) lead;
    }

    // Latency in input-rate samples (look-ahead needed by the kernel).
    double latencyInputSamples() const { return bypass ? 0.0 : halfWidth; }
    bool   isBypassed() const          { return bypass; }

    // Consumes n input frames and appends the produced frames to outL/outR via
    // the push callback (so the caller can route them to any FIFO).
    template <typename Push>
    void process (const float* inL, const float* inR, size_t n, Push&& push)
    {
        if (bypass)
        {
            for (size_t i = 0; i < n; ++i)
                push (inL[i], inR[i], i);
            return;
        }

        if (count + n > histL.size())
        {
            histL.resize (count + n + 64, 0.0f);
            histR.resize (histL.size(), 0.0f);
        }
        std::copy (inL, inL + n, histL.begin() + (std::ptrdiff_t) count);
        std::copy (inR, inR + n, histR.begin() + (std::ptrdiff_t) count);
        count += n;

        size_t produced = 0;
        while (pos + halfWidth < (double) count)
        {
            const long first = (long) std::ceil (pos - halfWidth);
            const long last  = (long) std::floor (pos + halfWidth);
            const long taps  = last - first + 1;

            float sum = 0.0f;
            for (long k = 0; k < taps; ++k)
            {
                const float w = kernel (((double) (first + k) - pos) * cutoff);
                weights[(size_t) k] = w;
                sum += w;
            }
            const float norm = sum != 0.0f ? 1.0f / sum : 1.0f;

            float accL = 0.0f, accR = 0.0f;
            for (long k = 0; k < taps; ++k)
            {
                const float w = weights[(size_t) k];
                accL += w * histL[(size_t) (first + k)];
                accR += w * histR[(size_t) (first + k)];
            }
            push (accL * norm, accR * norm, produced++);
            pos += step;
        }

        // Drop history that can no longer be reached.
        const long drop = (long) std::floor (pos - halfWidth) - 2;
        if (drop > 0 && (size_t) drop < count)
        {
            const size_t keep = count - (size_t) drop;
            std::copy (histL.begin() + drop, histL.begin() + (std::ptrdiff_t) count, histL.begin());
            std::copy (histR.begin() + drop, histR.begin() + (std::ptrdiff_t) count, histR.begin());
            count = keep;
            pos  -= (double) drop;
        }
    }

private:
    static constexpr double kZeroCrossings = 12.0;
    static constexpr int    kTableRes      = 256;     // entries per zero crossing
    static constexpr double kBeta          = 9.0;     // Kaiser beta (~ -90 dB stopband)

    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        const double q = x * x * 0.25;
        for (int k = 1; k < 40; ++k)
        {
            term *= q / ((double) k * (double) k);
            sum  += term;
            if (term < 1.0e-14 * sum) break;
        }
        return sum;
    }

    void buildTable()
    {
        const int size = (int) kZeroCrossings * kTableRes + 2;
        table.assign ((size_t) size, 0.0f);
        const double pi = 3.14159265358979323846;
        const double i0b = besselI0 (kBeta);
        for (int i = 0; i < size; ++i)
        {
            const double x = (double) i / (double) kTableRes;            // in lowpass zero-crossings
            const double s = x == 0.0 ? 1.0 : std::sin (pi * x) / (pi * x);
            const double r = x / kZeroCrossings;
            const double w = r >= 1.0 ? 0.0 : besselI0 (kBeta * std::sqrt (1.0 - r * r)) / i0b;
            table[(size_t) i] = (float) (s * w);
        }
    }

    inline float kernel (double x) const
    {
        const double a = std::abs (x) * (double) kTableRes;
        const size_t i = (size_t) a;
        if (i + 1 >= table.size()) return 0.0f;
        const float f = (float) (a - (double) i);
        return table[i] + (table[i + 1] - table[i]) * f;
    }

    bool bypass = true;
    double step = 1.0, cutoff = 1.0, halfWidth = 0.0, pos = 0.0;
    size_t count = 0;
    std::vector<float> table, weights, histL, histR;
};
