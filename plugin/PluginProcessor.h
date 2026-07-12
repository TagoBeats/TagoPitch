#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

namespace tagopitch::param
{
// IDs are the contract between processor, WebView UI and the Python prototype
// (tagodsp.pitch.PitchShifter uses the same names).
inline constexpr auto pitch = "pitch_semitones";
inline constexpr auto formant = "formant_semitones";
inline constexpr auto mix = "mix";
inline constexpr auto gain = "gain_db";
inline constexpr auto bypass = "bypass";
// Hidden: anchored automatically via pitch detection later, no user knob (see vault note).
inline constexpr auto formantBase = "formant_base_hz";
} // namespace tagopitch::param

class TagoPitchProcessor : public juce::AudioProcessor
{
public:
    TagoPitchProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioParameterBool* bypassParam = nullptr;
    juce::dsp::Gain<float> outputGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TagoPitchProcessor)
};
