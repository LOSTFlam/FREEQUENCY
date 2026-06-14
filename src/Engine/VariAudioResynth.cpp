#include "Engine/VariAudioResynth.h"

#include <algorithm>
#include <cmath>

namespace freequency::engine
{
    namespace
    {
        double hzToCentsFromC4 (double hz) noexcept
        {
            if (hz <= 0.0)
                return 0.0;

            constexpr double c4Hz = 261.625565;
            return 1200.0 * std::log2 (hz / c4Hz);
        }

        float readInterpolated (const juce::AudioBuffer<float>& buf, int channel, double pos) noexcept
        {
            const int len = buf.getNumSamples();
            if (len <= 0)
                return 0.0f;

            pos = juce::jlimit (0.0, (double) (len - 1), pos);
            const int i0 = (int) pos;
            const int i1 = juce::jmin (i0 + 1, len - 1);
            const float frac = (float) (pos - (double) i0);
            const float s0 = buf.getSample (channel, i0);
            const float s1 = buf.getSample (channel, i1);
            return s0 + frac * (s1 - s0);
        }

        double estimateHzAutocorr (const float* mono, int numSamples, double sampleRate) noexcept
        {
            if (mono == nullptr || numSamples < 512 || sampleRate <= 0.0)
                return 0.0;

            const int minLag = juce::jmax (2, (int) std::floor (sampleRate / 1200.0));
            const int maxLag = juce::jmin (numSamples / 2, (int) std::floor (sampleRate / 55.0));
            if (maxLag <= minLag)
                return 0.0;

            double bestCorr = 0.0;
            int bestLag = 0;

            for (int lag = minLag; lag <= maxLag; ++lag)
            {
                double sum = 0.0;
                double e0 = 0.0;
                double e1 = 0.0;
                const int n = numSamples - lag;

                for (int i = 0; i < n; ++i)
                {
                    const double a = mono[i];
                    const double b = mono[i + lag];
                    sum += a * b;
                    e0 += a * a;
                    e1 += b * b;
                }

                const double denom = std::sqrt (e0 * e1);
                const double corr = denom > 1.0e-12 ? sum / denom : 0.0;

                if (corr > bestCorr)
                {
                    bestCorr = corr;
                    bestLag = lag;
                }
            }

            if (bestLag <= 0 || bestCorr < 0.35)
                return 0.0;

            return sampleRate / (double) bestLag;
        }
    } // namespace

    double VariAudioResynth::estimatePitchCentsAt (const juce::AudioBuffer<float>& buffer,
                                                   double sampleRate,
                                                   double timeSec)
    {
        if (buffer.getNumSamples() <= 0 || sampleRate <= 0.0)
            return 0.0;

        const int centre = (int) std::llround (timeSec * sampleRate);
        const int win = 4096;
        const int start = juce::jlimit (0, buffer.getNumSamples() - 1, centre - win / 2);
        const int count = juce::jmin (win, buffer.getNumSamples() - start);

        std::vector<float> mono ((size_t) count, 0.0f);
        const int ch = juce::jmax (1, buffer.getNumChannels());

        for (int i = 0; i < count; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < ch; ++c)
                s += buffer.getSample (c, start + i);
            mono[(size_t) i] = s / (float) ch;
        }

        const double hz = estimateHzAutocorr (mono.data(), count, sampleRate);
        return hzToCentsFromC4 (hz);
    }

    double VariAudioResynth::formantShiftAt (const std::vector<models::VariAudioSegment>& segments,
                                             double timeSec,
                                             double clipLengthSec)
    {
        if (segments.empty())
            return 0.0;

        auto sorted = segments;
        std::sort (sorted.begin(), sorted.end(),
                   [] (const models::VariAudioSegment& a, const models::VariAudioSegment& b)
                   { return a.time < b.time; });

        if (timeSec <= sorted.front().time)
            return (double) sorted.front().formantShift;

        if (timeSec >= sorted.back().time)
            return (double) sorted.back().formantShift;

        for (size_t i = 1; i < sorted.size(); ++i)
        {
            const auto& a = sorted[i - 1];
            const auto& b = sorted[i];
            if (timeSec >= a.time && timeSec <= b.time)
            {
                const double span = juce::jmax (1.0e-9, b.time - a.time);
                const double t = (timeSec - a.time) / span;
                return (double) a.formantShift + t * ((double) b.formantShift - (double) a.formantShift);
            }
        }

        juce::ignoreUnused (clipLengthSec);
        return 0.0;
    }

    double VariAudioResynth::correctionCentsAt (const std::vector<models::VariAudioSegment>& segments,
                                                double timeSec,
                                                double clipLengthSec)
    {
        if (segments.empty())
            return 0.0;

        auto sorted = segments;
        std::sort (sorted.begin(), sorted.end(),
                   [] (const models::VariAudioSegment& a, const models::VariAudioSegment& b)
                   { return a.time < b.time; });

        if (timeSec <= sorted.front().time)
            return (double) sorted.front().pitchCents;

        if (timeSec >= sorted.back().time)
            return (double) sorted.back().pitchCents;

        for (size_t i = 1; i < sorted.size(); ++i)
        {
            const auto& a = sorted[i - 1];
            const auto& b = sorted[i];
            if (timeSec >= a.time && timeSec <= b.time)
            {
                const double span = juce::jmax (1.0e-9, b.time - a.time);
                const double t = (timeSec - a.time) / span;
                return (double) a.pitchCents + t * ((double) b.pitchCents - (double) a.pitchCents);
            }
        }

        juce::ignoreUnused (clipLengthSec);
        return 0.0;
    }

    std::unique_ptr<juce::AudioBuffer<float>> VariAudioResynth::applySegments (
        const juce::AudioBuffer<float>& in,
        const std::vector<models::VariAudioSegment>& segments,
        double sampleRate,
        double clipLengthSec)
    {
        auto out = std::make_unique<juce::AudioBuffer<float>> (in.getNumChannels(), in.getNumSamples());
        out->makeCopyOf (in);

        if (segments.empty() || in.getNumSamples() < 4 || sampleRate <= 0.0)
            return out;

        const int channels = in.getNumChannels();
        const int len = in.getNumSamples();

        std::vector<double> readPos ((size_t) channels, 0.0);
        auto pitched = std::make_unique<juce::AudioBuffer<float>> (channels, len);
        pitched->clear();

        for (int i = 0; i < len; ++i)
        {
            const double t = (double) i / sampleRate;
            const double cents = correctionCentsAt (segments, t, clipLengthSec);
            const double ratio = std::pow (2.0, cents / 1200.0);

            for (int ch = 0; ch < channels; ++ch)
            {
                pitched->setSample (ch, i, readInterpolated (in, ch, readPos[(size_t) ch]));
                readPos[(size_t) ch] += ratio;
            }
        }

        // Formant preservation: blend low-band resample with user/auto formant shift.
        const float lpCoeff = 0.08f;
        for (int ch = 0; ch < channels; ++ch)
        {
            float lp = 0.0f;
            double formantRead = 0.0;

            for (int i = 0; i < len; ++i)
            {
                const double t = (double) i / sampleRate;
                const float sample = pitched->getSample (ch, i);
                lp += lpCoeff * (sample - lp);

                const double cents = correctionCentsAt (segments, t, clipLengthSec);
                const double userFormant = formantShiftAt (segments, t, clipLengthSec);
                const double autoFormant = -(cents / 1200.0) * 0.45;
                const double formantSemis = userFormant + autoFormant;
                const double formantRatio = std::pow (2.0, formantSemis / 12.0);

                formantRead += formantRatio;
                const float formantBody = readInterpolated (*pitched, ch, formantRead);
                const float preserved = lp * 0.55f + formantBody * 0.45f;
                const float bright = sample - lp;

                out->setSample (ch, i, preserved + bright);
            }
        }

        return out;
    }

    void VariAudioResynth::buildDetectedContour (const juce::AudioBuffer<float>& buffer,
                                                 double sampleRate,
                                                 double clipLengthSec,
                                                 int numPoints,
                                                 std::vector<double>& timesOut,
                                                 std::vector<double>& centsOut)
    {
        timesOut.clear();
        centsOut.clear();

        if (numPoints <= 0 || clipLengthSec <= 0.0)
            return;

        timesOut.reserve ((size_t) numPoints);
        centsOut.reserve ((size_t) numPoints);

        for (int i = 0; i < numPoints; ++i)
        {
            const double t = clipLengthSec * (double) i / (double) (numPoints - 1);
            timesOut.push_back (t);
            centsOut.push_back (estimatePitchCentsAt (buffer, sampleRate, t));
        }
    }
} // namespace freequency::engine
