#include "MonoSynthAudioProcessor.h"
#include "MonoSynthAudioProcessorEditor.h"

//==============================================================================
MonoSynthAudioProcessorEditor::MonoSynthAudioProcessorEditor (MonoSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    titleLabel.setText ("Mono Synth", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    waveformLabel.setText ("Waveform", juce::dontSendNotification);
    waveformLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (waveformLabel);

    waveformSelector.addItem ("Sine", 1);
    waveformSelector.addItem ("Saw", 2);
    waveformSelector.addItem ("Square", 3);
    waveformSelector.addItem ("Triangle", 4);
    addAndMakeVisible (waveformSelector);

    maxVoicesLabel.setText ("Max Voices", juce::dontSendNotification);
    maxVoicesLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (maxVoicesLabel);

    maxVoicesSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    maxVoicesSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    addAndMakeVisible (maxVoicesSlider);

    stealPolicyLabel.setText ("Steal", juce::dontSendNotification);
    stealPolicyLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (stealPolicyLabel);

    stealPolicySelector.addItem ("Released First", 1);
    stealPolicySelector.addItem ("Oldest", 2);
    stealPolicySelector.addItem ("Quietest", 3);
    addAndMakeVisible (stealPolicySelector);

    attackLabel.setText ("Attack (s)", juce::dontSendNotification);
    attackLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (attackLabel);

    attackSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    attackSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    addAndMakeVisible (attackSlider);

    releaseLabel.setText ("Release (s)", juce::dontSendNotification);
    releaseLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (releaseLabel);

    releaseSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    releaseSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    addAndMakeVisible (releaseSlider);

    waveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processorRef.getValueTreeState(),
                                                                                                    "waveform",
                                                                                                    waveformSelector);
    maxVoicesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                                   "maxVoices",
                                                                                                   maxVoicesSlider);
    stealPolicyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processorRef.getValueTreeState(),
                                                                                                       "stealPolicy",
                                                                                                       stealPolicySelector);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                                "attack",
                                                                                                attackSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                                 "release",
                                                                                                 releaseSlider);

    setResizable (true, false);
    setResizeLimits (360, 220, 760, 320);
    setSize (460, 240);
}

MonoSynthAudioProcessorEditor::~MonoSynthAudioProcessorEditor()
{
}

//==============================================================================
void MonoSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MonoSynthAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16);

    titleLabel.setBounds (bounds.removeFromTop (28));
    bounds.removeFromTop (8);

    const auto contentWidth = juce::jmin (520, bounds.getWidth());
    auto content = juce::Rectangle<int> (contentWidth, bounds.getHeight()).withCentre (bounds.getCentre());

    auto placeRow = [&content] (juce::Label& label, juce::Component& control)
    {
        auto row = content.removeFromTop (30);
        row.removeFromBottom (4);
        label.setBounds (row.removeFromLeft (100));
        control.setBounds (row.reduced (4, 0));
    };

    placeRow (waveformLabel, waveformSelector);
    placeRow (maxVoicesLabel, maxVoicesSlider);
    placeRow (stealPolicyLabel, stealPolicySelector);
    placeRow (attackLabel, attackSlider);
    placeRow (releaseLabel, releaseSlider);
}
