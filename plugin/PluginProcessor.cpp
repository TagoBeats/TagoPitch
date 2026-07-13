#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String formatSemitones (float v, int)
{
    return (v > 0 ? "+" : "") + juce::String (v, 1) + " st";
}
} // namespace

TagoPitchProcessor::TagoPitchProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (tagopitch::param::bypass));
    jassert (bypassParam != nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout TagoPitchProcessor::createLayout()
{
    using namespace tagopitch::param;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { pitch, 1 }, "Pitch", -12, 12, 0,
        juce::AudioParameterIntAttributes {}.withStringFromValueFunction (
            [] (int v, int) { return (v > 0 ? "+" : "") + juce::String (v) + " st"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { formant, 1 }, "Formant",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes {}.withStringFromValueFunction (formatSemitones)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gain, 1 }, "Gain",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { bypass, 1 }, "Bypass", false));

    // Not exposed in the UI: 0 = engine's automatic envelope estimate. Will be driven
    // by internal pitch detection once the v2 hard-tune detection lands.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { formantBase, 1 }, "Formant Base",
        juce::NormalisableRange<float> (0.0f, 500.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("Hz").withAutomatable (false)));

    return layout;
}

void TagoPitchProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Order matters: prepare() snaps the ramp to the current target, so the
    // first block starts at the parameter value instead of ramping from 0 dB.
    outputGain.setRampDurationSeconds (0.02);
    outputGain.setGainDecibels (apvts.getRawParameterValue (tagopitch::param::gain)->load());
    outputGain.prepare ({ sampleRate, (juce::uint32) samplesPerBlock,
                          (juce::uint32) getTotalNumOutputChannels() });

    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (engine.latencySamples());
}

bool TagoPitchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

float TagoPitchProcessor::bufferPeak (const juce::AudioBuffer<float>& buffer) noexcept
{
    float peak = 0.0f;
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        peak = juce::jmax (peak, buffer.getMagnitude (c, 0, buffer.getNumSamples()));
    return peak;
}

void TagoPitchProcessor::storePeak (std::atomic<float>& peak, float value) noexcept
{
    // Keep the max until the editor timer consumes it (exchange with 0).
    float current = peak.load (std::memory_order_relaxed);
    while (value > current
           && ! peak.compare_exchange_weak (current, value, std::memory_order_relaxed))
    {
    }
}

void TagoPitchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float inPeak = bufferPeak (buffer);
    storePeak (inputPeak, inPeak);

    if (bypassParam->get())
    {
        storePeak (outputPeak, inPeak);
        return;
    }

    using namespace tagopitch::param;
    engine.setParameters (
        (float) apvts.getRawParameterValue (pitch)->load(),
        apvts.getRawParameterValue (formant)->load(),
        apvts.getRawParameterValue (formantBase)->load(),
        apvts.getRawParameterValue (mix)->load() * 0.01f);
    engine.process (buffer);

    outputGain.setGainDecibels (apvts.getRawParameterValue (gain)->load());
    juce::dsp::AudioBlock<float> block (buffer);
    outputGain.process (juce::dsp::ProcessContextReplacing<float> (block));

    storePeak (outputPeak, bufferPeak (buffer));
}

void TagoPitchProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TagoPitchProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* TagoPitchProcessor::createEditor()
{
    return new TagoPitchEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TagoPitchProcessor();
}
