#include "PolySynthAudioProcessor.h"
#include "PolySynthAudioProcessorEditor.h"

//==============================================================================
PolySynthAudioProcessorEditor::PolySynthAudioProcessorEditor (PolySynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    titleLabel.setText ("Poly Synth", juce::dontSendNotification);
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

    maxVoicesLabel.setText ("Voice Count", juce::dontSendNotification);
    maxVoicesLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (maxVoicesLabel);

    maxVoicesSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    maxVoicesSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    addAndMakeVisible (maxVoicesSlider);

    stealPolicyLabel.setText ("Steal Policy", juce::dontSendNotification);
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

    modDepthLabel.setText ("Mod Depth", juce::dontSendNotification);
    modDepthLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (modDepthLabel);

    modDepthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    modDepthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    modDepthSlider.setRange (0.0, 1.0, 0.001);
    modDepthSlider.setTextValueSuffix ("");
    modDepthSlider.setNumDecimalPlacesToDisplay (3);
    addAndMakeVisible (modDepthSlider);

    modRateLabel.setText ("Mod Rate", juce::dontSendNotification);
    modRateLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (modRateLabel);

    modRateSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    modRateSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    modRateSlider.setRange (0.05, 20.0, 0.01);
    modRateSlider.setTextValueSuffix (" Hz");
    modRateSlider.setNumDecimalPlacesToDisplay (2);
    addAndMakeVisible (modRateSlider);

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
    modDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                                  "modDepth",
                                                                                                  modDepthSlider);
    modRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                                 "modRate",
                                                                                                 modRateSlider);

    setResizable (true, false);
    setResizeLimits (360, 300, 700, 420);
    setSize (460, 320);
}

PolySynthAudioProcessorEditor::~PolySynthAudioProcessorEditor()
{
}

//==============================================================================
void PolySynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PolySynthAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16);

    titleLabel.setBounds (bounds.removeFromTop (28));
    bounds.removeFromTop (8);

    const auto contentWidth = juce::jmin (520, bounds.getWidth());
    auto content = juce::Rectangle<int> (contentWidth, bounds.getHeight()).withCentre (bounds.getCentre());

    auto placeRow = [&content] (juce::Label& label, juce::Component& control)
    {
        auto row = content.removeFromTop (32);
        row.removeFromBottom (6);
        label.setBounds (row.removeFromLeft (112));
        control.setBounds (row.reduced (4, 0));
    };

    placeRow (waveformLabel, waveformSelector);
    placeRow (maxVoicesLabel, maxVoicesSlider);
    placeRow (stealPolicyLabel, stealPolicySelector);
    placeRow (attackLabel, attackSlider);
    placeRow (releaseLabel, releaseSlider);
    placeRow (modDepthLabel, modDepthSlider);
    placeRow (modRateLabel, modRateSlider);
}
