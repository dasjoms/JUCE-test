#include "PolySynthAudioProcessor.h"
#include "PolySynthAudioProcessorEditor.h"

//==============================================================================
PolySynthAudioProcessorEditor::PolySynthAudioProcessorEditor (PolySynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    auto configureLinearSlider = [] (juce::Slider& slider, double min, double max, double interval, int decimals, juce::String suffix = {})
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
        slider.setRange (min, max, interval);
        slider.setNumDecimalPlacesToDisplay (decimals);
        slider.setTextValueSuffix (suffix);
    };

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

    configureLinearSlider (maxVoicesSlider, 1.0, 16.0, 1.0, 0);
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

    attackSlider.setComponentID ("attackSlider");
    configureLinearSlider (attackSlider, 0.001, 5.0, 0.001, 3, " s");
    addAndMakeVisible (attackSlider);

    decayLabel.setText ("Decay (s)", juce::dontSendNotification);
    decayLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (decayLabel);

    decaySlider.setComponentID ("decaySlider");
    configureLinearSlider (decaySlider, 0.001, 5.0, 0.001, 3, " s");
    addAndMakeVisible (decaySlider);

    sustainLabel.setText ("Sustain", juce::dontSendNotification);
    sustainLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (sustainLabel);

    sustainSlider.setComponentID ("sustainSlider");
    configureLinearSlider (sustainSlider, 0.0, 1.0, 0.001, 3);
    addAndMakeVisible (sustainSlider);

    releaseLabel.setText ("Release (s)", juce::dontSendNotification);
    releaseLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (releaseLabel);

    configureLinearSlider (releaseSlider, 0.005, 5.0, 0.001, 3, " s");
    addAndMakeVisible (releaseSlider);

    modDepthLabel.setText ("Mod Depth", juce::dontSendNotification);
    modDepthLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (modDepthLabel);

    configureLinearSlider (modDepthSlider, 0.0, 1.0, 0.001, 3);
    addAndMakeVisible (modDepthSlider);

    modRateLabel.setText ("Mod Rate", juce::dontSendNotification);
    modRateLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (modRateLabel);

    configureLinearSlider (modRateSlider, 0.05, 20.0, 0.01, 2, " Hz");
    addAndMakeVisible (modRateSlider);

    velocitySensitivityLabel.setText ("Velocity Sensitivity", juce::dontSendNotification);
    velocitySensitivityLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (velocitySensitivityLabel);

    velocitySensitivitySlider.setComponentID ("velocitySensitivitySlider");
    configureLinearSlider (velocitySensitivitySlider, 0.0, 1.0, 0.001, 3);
    addAndMakeVisible (velocitySensitivitySlider);

    modDestinationLabel.setText ("Mod Destination", juce::dontSendNotification);
    modDestinationLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (modDestinationLabel);

    modDestinationSelector.setComponentID ("modDestinationSelector");
    modDestinationSelector.addItem ("Amplitude", 1);
    modDestinationSelector.addItem ("Pitch", 2);
    modDestinationSelector.addItem ("Pulse Width", 3);
    addAndMakeVisible (modDestinationSelector);

    unisonVoicesLabel.setText ("Unison Voices", juce::dontSendNotification);
    unisonVoicesLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (unisonVoicesLabel);

    unisonVoicesSlider.setComponentID ("unisonVoicesSlider");
    configureLinearSlider (unisonVoicesSlider, 1.0, 8.0, 1.0, 0);
    addAndMakeVisible (unisonVoicesSlider);

    unisonDetuneCentsLabel.setText ("Unison Detune (cents)", juce::dontSendNotification);
    unisonDetuneCentsLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (unisonDetuneCentsLabel);

    unisonDetuneCentsSlider.setComponentID ("unisonDetuneCentsSlider");
    configureLinearSlider (unisonDetuneCentsSlider, 0.0, 50.0, 0.01, 2, " cents");
    addAndMakeVisible (unisonDetuneCentsSlider);

    outputStageLabel.setText ("Output Stage", juce::dontSendNotification);
    outputStageLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (outputStageLabel);

    outputStageSelector.setComponentID ("outputStageSelector");
    outputStageSelector.addItem ("None", 1);
    outputStageSelector.addItem ("Normalize Voice Sum", 2);
    outputStageSelector.addItem ("Soft Limit", 3);
    addAndMakeVisible (outputStageSelector);

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
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                               "decay",
                                                                                               decaySlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                                 "sustain",
                                                                                                 sustainSlider);
    modDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                                  "modDepth",
                                                                                                  modDepthSlider);
    modRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                                 "modRate",
                                                                                                 modRateSlider);
    velocitySensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                                             "velocitySensitivity",
                                                                                                             velocitySensitivitySlider);
    modDestinationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processorRef.getValueTreeState(),
                                                                                                          "modDestination",
                                                                                                          modDestinationSelector);
    unisonVoicesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                                       "unisonVoices",
                                                                                                       unisonVoicesSlider);
    unisonDetuneCentsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getValueTreeState(),
                                                                                                            "unisonDetuneCents",
                                                                                                            unisonDetuneCentsSlider);
    outputStageAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processorRef.getValueTreeState(),
                                                                                                        "outputStage",
                                                                                                        outputStageSelector);

    setResizable (true, false);
    setResizeLimits (420, 560, 760, 760);
    setSize (520, 570);
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

    const auto contentWidth = juce::jmin (620, bounds.getWidth());
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
    placeRow (decayLabel, decaySlider);
    placeRow (sustainLabel, sustainSlider);
    placeRow (releaseLabel, releaseSlider);
    placeRow (modDepthLabel, modDepthSlider);
    placeRow (modRateLabel, modRateSlider);
    placeRow (velocitySensitivityLabel, velocitySensitivitySlider);
    placeRow (modDestinationLabel, modDestinationSelector);
    placeRow (unisonVoicesLabel, unisonVoicesSlider);
    placeRow (unisonDetuneCentsLabel, unisonDetuneCentsSlider);
    placeRow (outputStageLabel, outputStageSelector);
}
