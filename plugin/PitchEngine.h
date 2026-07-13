#pragma once

// DSP core, ported from tagodsp.pitch.PitchShifter (Python reference).
// Robin reviews this file; keep UI concerns out.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <signalsmith-stretch/signalsmith-stretch.h>

namespace tagopitch
{

// Pitch/formant engine plus latency-compensated dry path and dry/wet mix.
// Output gain lives in the processor (after the mix), matching the Python
// signal order: input -> pitch/formant -> mix -> gain.
class PitchEngine
{
public:
    // Same tonality limit as the prototype. Tuned by ear 2026-07-13: 12 kHz is
    // audibly less grainy than Signalsmith's ~8 kHz voice suggestion on sung
    // vocals (up5/up12), no regressions on female material or downshifts.
    static constexpr float tonalityLimitHz = 12000.0f;

    // Wet level loss per semitone, measured on the two v1 test vocals
    // (2026-07-13, RMS wet vs dry). Index 0 = -12 st. Same table as the
    // prototype's _LEVEL_COMP_DB; 0 dB at pitch 0 keeps neutral transparent.
    static constexpr float levelCompDb[25] = {
        -0.9f, -0.2f, 0.2f, 0.3f, 0.6f, 0.6f, 1.1f, 1.3f, 1.5f, 1.6f, 1.6f, 1.0f, 0.0f,
        1.3f, 2.4f, 3.1f, 3.6f, 4.1f, 4.6f, 4.6f, 5.4f, 5.8f, 6.2f, 5.9f, 5.4f
    };

    static float levelCompGain (float pitchSemitones)
    {
        const float p = juce::jlimit (-12.0f, 12.0f, pitchSemitones);
        const float pos = p + 12.0f;
        const int i = juce::jmin ((int) pos, 23);
        const float frac = pos - (float) i;
        const float db = levelCompDb[i] + frac * (levelCompDb[i + 1] - levelCompDb[i]);
        return juce::Decibels::decibelsToGain (db);
    }

    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        sr = sampleRate;
        channels = numChannels;

        // Same engine configuration as the Python prototype (presetDefault:
        // block = 0.12 * sr, interval = 0.03 * sr).
        stretch.presetDefault (channels, (float) sampleRate);

        wetBuffer.setSize (channels, maxBlockSize);

        dryDelay.setMaximumDelayInSamples (latencySamples() + maxBlockSize);
        dryDelay.prepare ({ sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) channels });
        dryDelay.setDelay ((float) latencySamples());

        mixSmoothed.reset (sampleRate, 0.02);
        compSmoothed.reset (sampleRate, 0.02);
        snapOnNextSet = true;
    }

    // Total engine latency the host has to compensate.
    // Python reference: 5292 samples at 44.1 kHz.
    int latencySamples() const
    {
        return stretch.inputLatency() + stretch.outputLatency();
    }

    void reset()
    {
        stretch.reset();
        dryDelay.reset();
        mixSmoothed.setCurrentAndTargetValue (mixSmoothed.getTargetValue());
        compSmoothed.setCurrentAndTargetValue (compSmoothed.getTargetValue());
    }

    // Call once per block before process(). mix is 0..1 as in the prototype.
    void setParameters (float pitchSemitones, float formantSemitones, float formantBaseHz, float mix)
    {
        stretch.setTransposeSemitones (pitchSemitones, tonalityLimitHz / (float) sr);
        // true = preserve_formants (AlterBoy behavior, prototype default)
        stretch.setFormantSemitones (formantSemitones, true);
        stretch.setFormantBase (formantBaseHz);
        const float comp = levelCompGain (pitchSemitones);
        // First values after prepare() snap so playback never starts mid-ramp.
        if (snapOnNextSet)
        {
            mixSmoothed.setCurrentAndTargetValue (mix);
            compSmoothed.setCurrentAndTargetValue (comp);
            snapOnNextSet = false;
        }
        else
        {
            mixSmoothed.setTargetValue (mix);
            compSmoothed.setTargetValue (comp);
        }
    }

    // In-place: buffer holds the dry input, leaves with the mixed output.
    // No allocation in here; wetBuffer and the delay line are sized in prepare().
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        jassert (n <= wetBuffer.getNumSamples());
        jassert (buffer.getNumChannels() == channels);

        stretch.process (buffer.getArrayOfReadPointers(), n,
                         wetBuffer.getArrayOfWritePointers(), n);

        for (int i = 0; i < n; ++i)
        {
            const float m = mixSmoothed.getNextValue();
            const float comp = compSmoothed.getNextValue();
            for (int c = 0; c < channels; ++c)
            {
                dryDelay.pushSample (c, buffer.getSample (c, i));
                const float dry = dryDelay.popSample (c);
                buffer.setSample (c, i, (1.0f - m) * dry + m * comp * wetBuffer.getSample (c, i));
            }
        }
    }

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    juce::AudioBuffer<float> wetBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> compSmoothed { 1.0f };
    bool snapOnNextSet = true;
    double sr = 44100.0;
    int channels = 2;
};

} // namespace tagopitch
