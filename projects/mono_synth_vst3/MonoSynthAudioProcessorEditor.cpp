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

    waveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processorRef.getValueTreeState(),
                                                                                                    "waveform",
                                                                                                    waveformSelector);

    setResizable (true, false);
    setResizeLimits (280, 140, 640, 200);
    setSize (360, 160);
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

    const auto contentWidth = juce::jmin (320, bounds.getWidth());
    auto row = juce::Rectangle<int> (contentWidth, 28).withCentre (bounds.getCentre());

    waveformLabel.setBounds (row.removeFromLeft (90));
    waveformSelector.setBounds (row.reduced (4, 0));
}
