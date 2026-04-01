#include "PolySynthAudioProcessor.h"
#include "PolySynthAudioProcessorEditor.h"
#include <array>

namespace
{
constexpr std::array<const char*, 12> pitchClassNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

PolySynthAudioProcessorEditor::LayerRow::LayerRow()
{
    selectButton.setButtonText ({});
    selectButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    selectButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    selectButton.setColour (juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    selectButton.setColour (juce::TextButton::textColourOnId, juce::Colours::transparentBlack);
    addAndMakeVisible (selectButton);

    layerNameLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (layerNameLabel);

    muteToggle.setButtonText ("M");
    addAndMakeVisible (muteToggle);

    soloToggle.setButtonText ("S");
    addAndMakeVisible (soloToggle);

    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 20);
    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setNumDecimalPlacesToDisplay (2);
    addAndMakeVisible (volumeSlider);
}

void PolySynthAudioProcessorEditor::LayerRow::resized()
{
    auto area = getLocalBounds().reduced (4, 2);
    selectButton.setBounds (area);
    auto controls = area.reduced (6, 0);
    layerNameLabel.setBounds (controls.removeFromLeft (90));
    muteToggle.setBounds (controls.removeFromLeft (34));
    soloToggle.setBounds (controls.removeFromLeft (34));
    volumeSlider.setBounds (controls.reduced (2, 0));
}

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

    globalPanelToggle.setButtonText ("Show Global Panel");
    globalPanelToggle.setClickingTogglesState (true);
    globalPanelToggle.onClick = [this]
    {
        globalPanel.setVisible (globalPanelToggle.getToggleState());
        globalPanelPlaceholderLabel.setVisible (globalPanelToggle.getToggleState());
        globalPanelToggle.setButtonText (globalPanelToggle.getToggleState() ? "Hide Global Panel" : "Show Global Panel");
        resized();
    };
    addAndMakeVisible (globalPanelToggle);

    globalPanel.setText ("Global / Shared Controls (Phase A Placeholder)");
    globalPanel.setVisible (false);
    addAndMakeVisible (globalPanel);

    globalPanelPlaceholderLabel.setText ("Reserved for cross-layer controls (macros, master FX, preset/global routing).",
                                         juce::dontSendNotification);
    globalPanelPlaceholderLabel.setJustificationType (juce::Justification::centredLeft);
    globalPanelPlaceholderLabel.setVisible (false);
    addAndMakeVisible (globalPanelPlaceholderLabel);

    layerListPanel.setText ("Layers");
    addAndMakeVisible (layerListPanel);

    inspectorTitleLabel.setText ("Selected Layer Controls", juce::dontSendNotification);
    inspectorTitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (inspectorTitleLabel);

    emptyInspectorLabel.setText ("No layer selected.", juce::dontSendNotification);
    emptyInspectorLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (emptyInspectorLabel);

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

    absoluteRootNoteLabel.setText ("Root Note", juce::dontSendNotification);
    absoluteRootNoteLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (absoluteRootNoteLabel);

    absoluteRootNoteSlider.setSliderStyle (juce::Slider::IncDecButtons);
    absoluteRootNoteSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 96, 20);
    absoluteRootNoteSlider.setRange (PolySynthAudioProcessor::minimumMidiNote,
                                     PolySynthAudioProcessor::maximumMidiNote,
                                     1.0);
    absoluteRootNoteSlider.textFromValueFunction = [] (double value)
    {
        return midiNoteToDisplayString (juce::roundToInt (value));
    };
    absoluteRootNoteSlider.onValueChange = [this] { handleAbsoluteRootNoteChange(); };
    addAndMakeVisible (absoluteRootNoteSlider);

    relativeRootSemitoneLabel.setText ("Relative (st)", juce::dontSendNotification);
    relativeRootSemitoneLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (relativeRootSemitoneLabel);

    relativeRootSemitoneSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    relativeRootSemitoneSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    relativeRootSemitoneSlider.setRange (-127.0, 127.0, 1.0);
    relativeRootSemitoneSlider.setNumDecimalPlacesToDisplay (0);
    relativeRootSemitoneSlider.setTextValueSuffix (" st");
    relativeRootSemitoneSlider.onValueChange = [this] { handleRelativeRootSemitoneChange(); };
    addAndMakeVisible (relativeRootSemitoneSlider);

    rootNoteFeedbackLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (rootNoteFeedbackLabel);

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

    for (std::size_t i = 0; i < layerRows.size(); ++i)
    {
        auto& row = layerRows[i];
        row.selectButton.onClick = [this, i] { selectLayer (i); };
        row.muteToggle.onClick = [this, i, &row]
        {
            processorRef.setLayerMute (i, row.muteToggle.getToggleState());
        };
        row.soloToggle.onClick = [this, i, &row]
        {
            processorRef.setLayerSolo (i, row.soloToggle.getToggleState());
        };
        row.volumeSlider.onValueChange = [this, i, &row]
        {
            processorRef.setLayerVolume (i, static_cast<float> (row.volumeSlider.getValue()));
        };
        row.setVisible (false);
        addAndMakeVisible (row);
    }

    setResizable (true, false);
    setResizeLimits (760, 620, 1180, 920);
    setSize (920, 700);

    syncLayerListFromProcessor();
    syncRootNoteControlsFromProcessor();
    updateInspectorBindingState();
    startTimerHz (15);
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

    globalPanelToggle.setBounds (bounds.removeFromTop (26));
    bounds.removeFromTop (8);

    if (globalPanelToggle.getToggleState())
    {
        auto globalBounds = bounds.removeFromTop (84);
        globalPanel.setBounds (globalBounds);
        globalPanelPlaceholderLabel.setBounds (globalBounds.reduced (12, 24));
        bounds.removeFromTop (10);
    }
    else
    {
        globalPanel.setBounds ({});
        globalPanelPlaceholderLabel.setBounds ({});
    }

    auto mainArea = bounds;
    auto layerListBounds = mainArea.removeFromLeft (juce::jmax (220, mainArea.getWidth() / 3));
    layerListPanel.setBounds (layerListBounds);

    auto rowArea = layerListBounds.reduced (8, 28);
    for (std::size_t i = 0; i < layerRows.size(); ++i)
    {
        auto& row = layerRows[i];
        if (i < visibleLayerCount)
            row.setBounds (rowArea.removeFromTop (36));
        else
            row.setBounds ({});
    }

    auto content = mainArea.reduced (8, 0);
    inspectorTitleLabel.setBounds (content.removeFromTop (24));
    content.removeFromTop (6);

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
    placeRow (absoluteRootNoteLabel, absoluteRootNoteSlider);
    placeRow (relativeRootSemitoneLabel, relativeRootSemitoneSlider);

    auto feedbackBounds = content.removeFromTop (24);
    rootNoteFeedbackLabel.setBounds (feedbackBounds);
    emptyInspectorLabel.setBounds (mainArea.reduced (12, 24));
}

void PolySynthAudioProcessorEditor::timerCallback()
{
    syncLayerListFromProcessor();
    syncRootNoteControlsFromProcessor();
}

void PolySynthAudioProcessorEditor::syncRootNoteControlsFromProcessor()
{
    if (visibleLayerCount == 0 || selectedLayerIndex >= visibleLayerCount)
        return;

    suppressRootNoteCallbacks = true;
    absoluteRootNoteSlider.setValue (processorRef.getLayerRootNoteAbsolute (selectedLayerIndex), juce::dontSendNotification);
    relativeRootSemitoneSlider.setValue (processorRef.getLayerRootNoteRelativeSemitones (selectedLayerIndex), juce::dontSendNotification);
    suppressRootNoteCallbacks = false;
}

void PolySynthAudioProcessorEditor::syncLayerListFromProcessor()
{
    visibleLayerCount = static_cast<std::size_t> (juce::jmax (0, processorRef.getLayerCount()));

    if (visibleLayerCount == 0)
        selectedLayerIndex = 0;
    else
        selectedLayerIndex = juce::jmin (selectedLayerIndex, visibleLayerCount - 1);

    for (std::size_t i = 0; i < layerRows.size(); ++i)
    {
        auto& row = layerRows[i];
        const auto isVisible = i < visibleLayerCount;
        row.setVisible (isVisible);
        if (! isVisible)
            continue;

        row.layerNameLabel.setText ("Layer " + juce::String (static_cast<int> (i + 1)),
                                    juce::dontSendNotification);
        row.muteToggle.setToggleState (processorRef.getLayerMute (i), juce::dontSendNotification);
        row.soloToggle.setToggleState (processorRef.getLayerSolo (i), juce::dontSendNotification);
        row.volumeSlider.setValue (processorRef.getLayerVolume (i), juce::dontSendNotification);
        row.selectButton.setColour (juce::TextButton::buttonColourId,
                                    i == selectedLayerIndex ? juce::Colours::darkslategrey.withAlpha (0.35f)
                                                             : juce::Colours::transparentBlack);
    }

    updateInspectorBindingState();
    resized();
}

void PolySynthAudioProcessorEditor::updateInspectorBindingState()
{
    const auto hasValidSelection = visibleLayerCount > 0 && selectedLayerIndex < visibleLayerCount;
    emptyInspectorLabel.setVisible (! hasValidSelection);

    for (auto* component : { static_cast<juce::Component*> (&waveformLabel),
                             static_cast<juce::Component*> (&waveformSelector),
                             static_cast<juce::Component*> (&maxVoicesLabel),
                             static_cast<juce::Component*> (&maxVoicesSlider),
                             static_cast<juce::Component*> (&stealPolicyLabel),
                             static_cast<juce::Component*> (&stealPolicySelector),
                             static_cast<juce::Component*> (&attackLabel),
                             static_cast<juce::Component*> (&attackSlider),
                             static_cast<juce::Component*> (&decayLabel),
                             static_cast<juce::Component*> (&decaySlider),
                             static_cast<juce::Component*> (&sustainLabel),
                             static_cast<juce::Component*> (&sustainSlider),
                             static_cast<juce::Component*> (&releaseLabel),
                             static_cast<juce::Component*> (&releaseSlider),
                             static_cast<juce::Component*> (&modDepthLabel),
                             static_cast<juce::Component*> (&modDepthSlider),
                             static_cast<juce::Component*> (&modRateLabel),
                             static_cast<juce::Component*> (&modRateSlider),
                             static_cast<juce::Component*> (&velocitySensitivityLabel),
                             static_cast<juce::Component*> (&velocitySensitivitySlider),
                             static_cast<juce::Component*> (&modDestinationLabel),
                             static_cast<juce::Component*> (&modDestinationSelector),
                             static_cast<juce::Component*> (&unisonVoicesLabel),
                             static_cast<juce::Component*> (&unisonVoicesSlider),
                             static_cast<juce::Component*> (&unisonDetuneCentsLabel),
                             static_cast<juce::Component*> (&unisonDetuneCentsSlider),
                             static_cast<juce::Component*> (&outputStageLabel),
                             static_cast<juce::Component*> (&outputStageSelector),
                             static_cast<juce::Component*> (&absoluteRootNoteLabel),
                             static_cast<juce::Component*> (&absoluteRootNoteSlider),
                             static_cast<juce::Component*> (&relativeRootSemitoneLabel),
                             static_cast<juce::Component*> (&relativeRootSemitoneSlider),
                             static_cast<juce::Component*> (&rootNoteFeedbackLabel) })
    {
        component->setEnabled (hasValidSelection);
    }
}

void PolySynthAudioProcessorEditor::selectLayer (std::size_t layerIndex)
{
    if (layerIndex >= visibleLayerCount)
        return;

    selectedLayerIndex = layerIndex;
    syncRootNoteControlsFromProcessor();
    syncLayerListFromProcessor();
}

void PolySynthAudioProcessorEditor::handleAbsoluteRootNoteChange()
{
    if (suppressRootNoteCallbacks)
        return;

    const auto requestedMidiNote = juce::roundToInt (absoluteRootNoteSlider.getValue());
    const auto appliedMidiNote = processorRef.setLayerRootNoteAbsolute (selectedLayerIndex, requestedMidiNote);
    const auto wasClamped = requestedMidiNote != appliedMidiNote;

    rootNoteFeedbackLabel.setText (wasClamped ? "Clamped to MIDI range C-1..G9 (0..127)" : "",
                                   juce::dontSendNotification);
    syncRootNoteControlsFromProcessor();
}

void PolySynthAudioProcessorEditor::handleRelativeRootSemitoneChange()
{
    if (suppressRootNoteCallbacks)
        return;

    const auto requestedSemitones = juce::roundToInt (relativeRootSemitoneSlider.getValue());
    const auto baseRootMidiNote = processorRef.getLayerRootNoteAbsolute (0);
    const auto requestedMidiNote = baseRootMidiNote + requestedSemitones;
    const auto appliedMidiNote = processorRef.setLayerRootNoteRelativeSemitones (selectedLayerIndex, requestedSemitones);
    const auto wasClamped = requestedMidiNote != appliedMidiNote;

    rootNoteFeedbackLabel.setText (wasClamped ? "Clamped to MIDI range C-1..G9 (0..127)" : "",
                                   juce::dontSendNotification);
    syncRootNoteControlsFromProcessor();
}

juce::String PolySynthAudioProcessorEditor::midiNoteToDisplayString (int midiNote)
{
    const auto clampedMidiNote = PolySynthAudioProcessor::clampMidiNote (midiNote);
    const auto pitchClassIndex = clampedMidiNote % 12;
    const auto octave = (clampedMidiNote / 12) - 1;
    return juce::String (pitchClassNames[static_cast<std::size_t> (pitchClassIndex)])
           + juce::String (octave)
           + " ("
           + juce::String (clampedMidiNote)
           + ")";
}
