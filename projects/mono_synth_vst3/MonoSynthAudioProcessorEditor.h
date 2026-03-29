#pragma once

#include "MonoSynthAudioProcessor.h"

//==============================================================================
class MonoSynthAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit MonoSynthAudioProcessorEditor (MonoSynthAudioProcessor&);
    ~MonoSynthAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    MonoSynthAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoSynthAudioProcessorEditor)
};
