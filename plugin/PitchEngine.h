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
        snapMixOnNextSet = true;
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
    }

    // Call once per block before process(). mix is 0..1 as in the prototype.
    void setParameters (float pitchSemitones, float formantSemitones, float formantBaseHz, float mix)
    {
        stretch.setTransposeSemitones (pitchSemitones, tonalityLimitHz / (float) sr);
        // true = preserve_formants (AlterBoy behavior, prototype default)
        stretch.setFormantSemitones (formantSemitones, true);
        stretch.setFormantBase (formantBaseHz);
        // First value after prepare() snaps so playback never starts mid-ramp.
        if (snapMixOnNextSet)
        {
            mixSmoothed.setCurrentAndTargetValue (mix);
            snapMixOnNextSet = false;
        }
        else
        {
            mixSmoothed.setTargetValue (mix);
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
            for (int c = 0; c < channels; ++c)
            {
                dryDelay.pushSample (c, buffer.getSample (c, i));
                const float dry = dryDelay.popSample (c);
                buffer.setSample (c, i, (1.0f - m) * dry + m * wetBuffer.getSample (c, i));
            }
        }
    }

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    juce::AudioBuffer<float> wetBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay;
    juce::SmoothedValue<float> mixSmoothed;
    bool snapMixOnNextSet = true;
    double sr = 44100.0;
    int channels = 2;
};

} // namespace tagopitch
