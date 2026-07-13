#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class TagoPitchEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit TagoPitchEditor (TagoPitchProcessor&);

    void resized() override;

private:
    // Pushes the block peaks from the processor to the UI meters (~30 Hz).
    void timerCallback() override;

    TagoPitchProcessor& pitchProcessor;
    static std::optional<juce::WebBrowserComponent::Resource> lookupResource (const juce::String& url);

    // Relays bridge WebView controls to APVTS parameters; their names are the
    // IDs the frontend queries via getSliderState()/getToggleState().
    juce::WebSliderRelay pitchRelay { tagopitch::param::pitch };
    juce::WebSliderRelay formantRelay { tagopitch::param::formant };
    juce::WebSliderRelay mixRelay { tagopitch::param::mix };
    juce::WebSliderRelay gainRelay { tagopitch::param::gain };
    juce::WebToggleButtonRelay bypassRelay { tagopitch::param::bypass };

    juce::WebBrowserComponent browser;

    juce::WebSliderParameterAttachment pitchAttachment;
    juce::WebSliderParameterAttachment formantAttachment;
    juce::WebSliderParameterAttachment mixAttachment;
    juce::WebSliderParameterAttachment gainAttachment;
    juce::WebToggleButtonParameterAttachment bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TagoPitchEditor)
};
