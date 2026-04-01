#pragma once

#include "PolySynthAudioProcessor.h"

//==============================================================================
class PolySynthAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PolySynthAudioProcessorEditor (PolySynthAudioProcessor&);
    ~PolySynthAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PolySynthAudioProcessor& processorRef;
    juce::Label titleLabel;
    juce::Label waveformLabel;
    juce::Label maxVoicesLabel;
    juce::Label stealPolicyLabel;
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;
    juce::Label modDepthLabel;
    juce::Label modRateLabel;
    juce::Label velocitySensitivityLabel;
    juce::Label modDestinationLabel;
    juce::Label unisonVoicesLabel;
    juce::Label unisonDetuneCentsLabel;
    juce::ComboBox waveformSelector;
    juce::ComboBox stealPolicySelector;
    juce::Slider maxVoicesSlider;
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;
    juce::Slider modDepthSlider;
    juce::Slider modRateSlider;
    juce::Slider velocitySensitivitySlider;
    juce::ComboBox modDestinationSelector;
    juce::Slider unisonVoicesSlider;
    juce::Slider unisonDetuneCentsSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveformAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stealPolicyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> maxVoicesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocitySensitivityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modDestinationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> unisonVoicesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> unisonDetuneCentsAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolySynthAudioProcessorEditor)
};
