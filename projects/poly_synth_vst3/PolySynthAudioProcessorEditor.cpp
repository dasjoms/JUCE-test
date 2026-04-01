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

    moveUpButton.setButtonText ("↑");
    addAndMakeVisible (moveUpButton);

    moveDownButton.setButtonText ("↓");
    addAndMakeVisible (moveDownButton);

    duplicateButton.setButtonText ("Dup");
    addAndMakeVisible (duplicateButton);

    deleteButton.setButtonText ("Del");
    addAndMakeVisible (deleteButton);

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
    layerNameLabel.setBounds (controls.removeFromLeft (54));
    moveUpButton.setBounds (controls.removeFromLeft (32).reduced (2, 0));
    moveDownButton.setBounds (controls.removeFromLeft (32).reduced (2, 0));
    duplicateButton.setBounds (controls.removeFromLeft (44).reduced (2, 0));
    deleteButton.setBounds (controls.removeFromLeft (40).reduced (2, 0));
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

    sidebarPanel.setText ("Sidebar");
    addAndMakeVisible (sidebarPanel);

    layerListPanel.setText ("Layers");
    addAndMakeVisible (layerListPanel);

    presetPanel.setText ("Preset Browser");
    addAndMakeVisible (presetPanel);

    marketplacePanel.setText ("Marketplace");
    addAndMakeVisible (marketplacePanel);

    addLayerButton.setButtonText ("Add Layer");
    addLayerButton.onClick = [this] { showAddLayerMenu(); };
    addAndMakeVisible (addLayerButton);

    actionStatusLabel.setJustificationType (juce::Justification::centredLeft);
    actionStatusLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible (actionStatusLabel);

    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetLabel);

    presetSelector.setTextWhenNothingSelected ("No presets");
    addAndMakeVisible (presetSelector);

    presetLoadButton.setButtonText ("Load");
    presetLoadButton.onClick = [this] { loadSelectedPreset(); };
    addAndMakeVisible (presetLoadButton);

    presetSaveButton.setButtonText ("Save");
    presetSaveButton.onClick = [this] { savePresetOverwrite(); };
    addAndMakeVisible (presetSaveButton);

    presetSaveAsNewButton.setButtonText ("Save As New");
    presetSaveAsNewButton.onClick = [this] { savePresetAsNew(); };
    addAndMakeVisible (presetSaveAsNewButton);

    presetStatusLabel.setJustificationType (juce::Justification::centredLeft);
    presetStatusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgoldenrodyellow);
    addAndMakeVisible (presetStatusLabel);

    marketplaceBrowseButton.setButtonText ("Browse");
    marketplaceBrowseButton.setEnabled (false);
    addAndMakeVisible (marketplaceBrowseButton);

    marketplaceUploadButton.setButtonText ("Upload");
    marketplaceUploadButton.setEnabled (false);
    addAndMakeVisible (marketplaceUploadButton);

    marketplaceSyncButton.setButtonText ("Sync");
    marketplaceSyncButton.setEnabled (false);
    addAndMakeVisible (marketplaceSyncButton);

    marketplaceLoginStatusLabel.setText ("Login: Not connected", juce::dontSendNotification);
    marketplaceLoginStatusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (marketplaceLoginStatusLabel);

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
    waveformSelector.setComponentID ("waveformSelector");
    addAndMakeVisible (waveformSelector);

    maxVoicesLabel.setText ("Voice Count", juce::dontSendNotification);
    maxVoicesLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (maxVoicesLabel);

    configureLinearSlider (maxVoicesSlider, 1.0, 16.0, 1.0, 0);
    maxVoicesSlider.setComponentID ("maxVoicesSlider");
    addAndMakeVisible (maxVoicesSlider);

    stealPolicyLabel.setText ("Steal Policy", juce::dontSendNotification);
    stealPolicyLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (stealPolicyLabel);

    stealPolicySelector.addItem ("Released First", 1);
    stealPolicySelector.addItem ("Oldest", 2);
    stealPolicySelector.addItem ("Quietest", 3);
    stealPolicySelector.setComponentID ("stealPolicySelector");
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
    releaseSlider.setComponentID ("releaseSlider");
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
    modDestinationSelector.addItem ("Off", 1);
    modDestinationSelector.addItem ("Amplitude", 2);
    modDestinationSelector.addItem ("Pitch", 3);
    modDestinationSelector.addItem ("Pulse Width", 4);
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

    waveformSelector.onChange = [this]
    {
        if (! suppressInspectorCallbacks)
            processorRef.setLayerWaveformByVisualIndex (selectedLayerIndex, PolySynthAudioProcessor::Waveform (waveformSelector.getSelectedItemIndex()));
    };
    maxVoicesSlider.onValueChange = [this]
    {
        if (! suppressInspectorCallbacks)
            processorRef.setLayerVoiceCountByVisualIndex (selectedLayerIndex, juce::roundToInt (maxVoicesSlider.getValue()));
    };
    stealPolicySelector.onChange = [this]
    {
        if (! suppressInspectorCallbacks)
            processorRef.setLayerStealPolicyByVisualIndex (selectedLayerIndex, SynthEngine::VoiceStealPolicy (stealPolicySelector.getSelectedItemIndex()));
    };
    auto updateAdsr = [this]
    {
        if (! suppressInspectorCallbacks)
            processorRef.setLayerAdsrByVisualIndex (selectedLayerIndex,
                                                    static_cast<float> (attackSlider.getValue()),
                                                    static_cast<float> (decaySlider.getValue()),
                                                    static_cast<float> (sustainSlider.getValue()),
                                                    static_cast<float> (releaseSlider.getValue()));
    };
    attackSlider.onValueChange = updateAdsr;
    decaySlider.onValueChange = updateAdsr;
    sustainSlider.onValueChange = updateAdsr;
    releaseSlider.onValueChange = updateAdsr;
    auto updateModulation = [this]
    {
        if (! suppressInspectorCallbacks)
            processorRef.setLayerModParametersByVisualIndex (selectedLayerIndex,
                                                             static_cast<float> (modDepthSlider.getValue()),
                                                             static_cast<float> (modRateSlider.getValue()),
                                                             SynthVoice::ModulationDestination (modDestinationSelector.getSelectedItemIndex()));
    };
    modDepthSlider.onValueChange = updateModulation;
    modRateSlider.onValueChange = updateModulation;
    modDestinationSelector.onChange = updateModulation;
    velocitySensitivitySlider.onValueChange = [this]
    {
        if (! suppressInspectorCallbacks)
            processorRef.setLayerVelocitySensitivityByVisualIndex (selectedLayerIndex, static_cast<float> (velocitySensitivitySlider.getValue()));
    };
    auto updateUnison = [this]
    {
        if (! suppressInspectorCallbacks)
            processorRef.setLayerUnisonByVisualIndex (selectedLayerIndex,
                                                      juce::roundToInt (unisonVoicesSlider.getValue()),
                                                      static_cast<float> (unisonDetuneCentsSlider.getValue()));
    };
    unisonVoicesSlider.onValueChange = updateUnison;
    unisonDetuneCentsSlider.onValueChange = updateUnison;
    outputStageSelector.onChange = [this]
    {
        if (! suppressInspectorCallbacks)
            processorRef.setLayerOutputStageByVisualIndex (selectedLayerIndex, SynthEngine::OutputStage (outputStageSelector.getSelectedItemIndex()));
    };

    for (std::size_t i = 0; i < layerRows.size(); ++i)
    {
        auto& row = layerRows[i];
        row.selectButton.onClick = [this, i] { selectLayer (i); };
        row.moveUpButton.onClick = [this, i] { moveLayerUp (i); };
        row.moveDownButton.onClick = [this, i] { moveLayerDown (i); };
        row.duplicateButton.onClick = [this, i] { duplicateLayerFromIndex (i); };
        row.deleteButton.onClick = [this, i] { deleteLayer (i); };
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
    syncInspectorControlsFromSelectedLayer();
    syncRootNoteControlsFromProcessor();
    refreshPresetControls();
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
    constexpr int sidebarMinWidth = 250;
    constexpr int sidebarMaxWidth = 340;
    constexpr int sidebarFixedWidth = 290;
    const auto sidebarWidth = juce::jlimit (sidebarMinWidth, sidebarMaxWidth, sidebarFixedWidth);
    auto sidebarBounds = mainArea.removeFromLeft (juce::jmin (sidebarWidth, mainArea.getWidth() - 220));
    sidebarPanel.setBounds (sidebarBounds);

    auto sidebarContent = sidebarBounds.reduced (8, 28);
    auto layersBounds = sidebarContent.removeFromTop (juce::jmin (300, sidebarContent.getHeight() / 2));
    layerListPanel.setBounds (layersBounds);

    auto rowArea = layersBounds.reduced (8, 28);
    addLayerButton.setBounds (rowArea.removeFromTop (28));
    rowArea.removeFromTop (4);
    actionStatusLabel.setBounds (rowArea.removeFromTop (20));
    rowArea.removeFromTop (4);
    for (std::size_t i = 0; i < layerRows.size(); ++i)
    {
        auto& row = layerRows[i];
        if (i < visibleLayerCount)
            row.setBounds (rowArea.removeFromTop (36));
        else
            row.setBounds ({});
    }

    sidebarContent.removeFromTop (8);
    auto presetBounds = sidebarContent.removeFromTop (124);
    presetPanel.setBounds (presetBounds);
    auto presetContent = presetBounds.reduced (8, 28);
    auto presetRow = presetContent.removeFromTop (28);
    presetLabel.setBounds (presetRow.removeFromLeft (54));
    presetSelector.setBounds (presetRow);
    presetContent.removeFromTop (4);
    auto presetButtonsRow = presetContent.removeFromTop (28);
    presetLoadButton.setBounds (presetButtonsRow.removeFromLeft (56).reduced (1, 0));
    presetSaveButton.setBounds (presetButtonsRow.removeFromLeft (56).reduced (1, 0));
    presetSaveAsNewButton.setBounds (presetButtonsRow.reduced (1, 0));
    presetContent.removeFromTop (4);
    presetStatusLabel.setBounds (presetContent.removeFromTop (34));

    sidebarContent.removeFromTop (8);
    marketplacePanel.setBounds (sidebarContent);
    auto marketplaceContent = sidebarContent.reduced (8, 28);
    auto marketplaceButtonsRow = marketplaceContent.removeFromTop (28);
    marketplaceBrowseButton.setBounds (marketplaceButtonsRow.removeFromLeft (74).reduced (1, 0));
    marketplaceUploadButton.setBounds (marketplaceButtonsRow.removeFromLeft (74).reduced (1, 0));
    marketplaceSyncButton.setBounds (marketplaceButtonsRow.removeFromLeft (74).reduced (1, 0));
    marketplaceContent.removeFromTop (6);
    marketplaceLoginStatusLabel.setBounds (marketplaceContent.removeFromTop (22));

    auto content = mainArea.reduced (12, 0);
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
    syncInspectorControlsFromSelectedLayer();
    syncRootNoteControlsFromProcessor();
    refreshPresetControls();
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
        selectedLayerIndex = juce::jmin (processorRef.getSelectedLayerVisualIndex(), visibleLayerCount - 1);

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
        row.moveUpButton.setEnabled (i > 0);
        row.moveDownButton.setEnabled (i + 1 < visibleLayerCount);
        row.deleteButton.setEnabled (visibleLayerCount > 1);
    }

    updateInspectorBindingState();
    resized();
}

void PolySynthAudioProcessorEditor::syncInspectorControlsFromSelectedLayer()
{
    if (visibleLayerCount == 0 || selectedLayerIndex >= visibleLayerCount)
        return;

    const auto selectedLayerState = processorRef.getLayerStateByVisualIndex (selectedLayerIndex);
    if (! selectedLayerState.has_value())
        return;

    suppressInspectorCallbacks = true;
    waveformSelector.setSelectedItemIndex (static_cast<int> (selectedLayerState->waveform), juce::dontSendNotification);
    maxVoicesSlider.setValue (selectedLayerState->voiceCount, juce::dontSendNotification);
    stealPolicySelector.setSelectedItemIndex (static_cast<int> (selectedLayerState->stealPolicy), juce::dontSendNotification);
    attackSlider.setValue (selectedLayerState->attackSeconds, juce::dontSendNotification);
    decaySlider.setValue (selectedLayerState->decaySeconds, juce::dontSendNotification);
    sustainSlider.setValue (selectedLayerState->sustainLevel, juce::dontSendNotification);
    releaseSlider.setValue (selectedLayerState->releaseSeconds, juce::dontSendNotification);
    modDepthSlider.setValue (selectedLayerState->modulationDepth, juce::dontSendNotification);
    modRateSlider.setValue (selectedLayerState->modulationRateHz, juce::dontSendNotification);
    velocitySensitivitySlider.setValue (selectedLayerState->velocitySensitivity, juce::dontSendNotification);
    modDestinationSelector.setSelectedItemIndex (static_cast<int> (selectedLayerState->modulationDestination), juce::dontSendNotification);
    unisonVoicesSlider.setValue (selectedLayerState->unisonVoices, juce::dontSendNotification);
    unisonDetuneCentsSlider.setValue (selectedLayerState->unisonDetuneCents, juce::dontSendNotification);
    outputStageSelector.setSelectedItemIndex (static_cast<int> (selectedLayerState->outputStage), juce::dontSendNotification);
    suppressInspectorCallbacks = false;
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

    if (! processorRef.selectLayerByVisualIndex (layerIndex))
        return;

    selectedLayerIndex = processorRef.getSelectedLayerVisualIndex();
    syncInspectorControlsFromSelectedLayer();
    syncRootNoteControlsFromProcessor();
    syncLayerListFromProcessor();
}

void PolySynthAudioProcessorEditor::showAddLayerMenu()
{
    juce::PopupMenu addLayerMenu;
    addLayerMenu.addItem ("Add default", [this]
    {
        if (! processorRef.addDefaultLayerAndSelect())
        {
            setActionStatusMessage ("Cannot add layer: max layer count reached.");
            return;
        }

        setActionStatusMessage ("Added default layer.");
        syncLayerListFromProcessor();
        syncRootNoteControlsFromProcessor();
    });
    addLayerMenu.addItem ("Add duplicate...", [this] { showDuplicateLayerMenu(); });
    addLayerMenu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addLayerButton));
}

void PolySynthAudioProcessorEditor::showDuplicateLayerMenu()
{
    if (visibleLayerCount == 0)
    {
        setActionStatusMessage ("Cannot duplicate: no layers available.");
        return;
    }

    if (visibleLayerCount == 1)
    {
        duplicateLayerFromIndex (0);
        return;
    }

    juce::PopupMenu chooseSourceMenu;
    for (std::size_t i = 0; i < visibleLayerCount; ++i)
    {
        chooseSourceMenu.addItem ("Duplicate Layer " + juce::String (static_cast<int> (i + 1)),
                                  [this, i] { duplicateLayerFromIndex (i); });
    }

    chooseSourceMenu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addLayerButton));
}

void PolySynthAudioProcessorEditor::duplicateLayerFromIndex (std::size_t sourceLayerIndex)
{
    if (! processorRef.duplicateLayerAndSelect (sourceLayerIndex))
    {
        setActionStatusMessage ("Cannot duplicate layer: invalid source or max layer count reached.");
        return;
    }

    setActionStatusMessage ("Duplicated layer.");
    syncLayerListFromProcessor();
    syncRootNoteControlsFromProcessor();
}

void PolySynthAudioProcessorEditor::deleteLayer (std::size_t layerIndex)
{
    if (visibleLayerCount <= 1)
    {
        setActionStatusMessage ("Cannot delete the last remaining layer.");
        return;
    }

    if (layerIndex >= visibleLayerCount)
    {
        setActionStatusMessage ("Cannot delete: invalid layer selection.");
        return;
    }

    const auto confirmed = juce::AlertWindow::showOkCancelBox (juce::AlertWindow::WarningIcon,
                                                                "Delete Layer",
                                                                "Delete Layer " + juce::String (static_cast<int> (layerIndex + 1)) + "?",
                                                                "Delete",
                                                                "Cancel",
                                                                nullptr,
                                                                nullptr);
    if (! confirmed)
    {
        setActionStatusMessage ("Delete cancelled.");
        return;
    }

    if (! processorRef.removeLayerWithSelectionFallback (layerIndex))
    {
        setActionStatusMessage ("Delete failed. Layer may be invalid.");
        return;
    }

    setActionStatusMessage ("Layer deleted.");
    syncLayerListFromProcessor();
    syncRootNoteControlsFromProcessor();
}

void PolySynthAudioProcessorEditor::moveLayerUp (std::size_t layerIndex)
{
    if (! processorRef.moveLayerUp (layerIndex))
    {
        setActionStatusMessage ("Cannot move layer up.");
        return;
    }

    setActionStatusMessage ("Layer moved up.");
    syncLayerListFromProcessor();
    syncRootNoteControlsFromProcessor();
}

void PolySynthAudioProcessorEditor::moveLayerDown (std::size_t layerIndex)
{
    if (! processorRef.moveLayerDown (layerIndex))
    {
        setActionStatusMessage ("Cannot move layer down.");
        return;
    }

    setActionStatusMessage ("Layer moved down.");
    syncLayerListFromProcessor();
    syncRootNoteControlsFromProcessor();
}

void PolySynthAudioProcessorEditor::setActionStatusMessage (const juce::String& message)
{
    actionStatusLabel.setText (message, juce::dontSendNotification);
}

void PolySynthAudioProcessorEditor::refreshPresetControls()
{
    const auto available = processorRef.listLocalPresetNames();
    const auto previousSelection = presetSelector.getText();

    presetSelector.clear (juce::dontSendNotification);
    for (int i = 0; i < available.size(); ++i)
        presetSelector.addItem (available[i], i + 1);

    const auto activePreset = processorRef.getCurrentPresetName();
    if (activePreset.isNotEmpty())
    {
        const auto activeIndex = available.indexOf (activePreset);
        if (activeIndex >= 0)
            presetSelector.setSelectedItemIndex (activeIndex, juce::dontSendNotification);
        else
            presetSelector.setText (activePreset);
    }
    else if (available.contains (previousSelection))
    {
        presetSelector.setText (previousSelection);
    }
}

void PolySynthAudioProcessorEditor::savePresetOverwrite()
{
    const auto result = processorRef.saveCurrentPresetOverwrite();
    presetStatusLabel.setText (result.message, juce::dontSendNotification);

    if (! result.success && processorRef.getCurrentPresetName().isEmpty())
        presetStatusLabel.setText ("No current preset selected. Use Save As New first.", juce::dontSendNotification);

    refreshPresetControls();
}

void PolySynthAudioProcessorEditor::savePresetAsNew()
{
    auto* prompt = new juce::AlertWindow ("Save Preset As New",
                                          "Enter a unique preset name.",
                                          juce::AlertWindow::NoIcon);
    prompt->addTextEditor ("presetName", processorRef.getCurrentPresetName(), "Name:");
    prompt->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    prompt->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    auto safePrompt = juce::Component::SafePointer<juce::AlertWindow> (prompt);
    prompt->enterModalState (true,
                             juce::ModalCallbackFunction::create ([this, safePrompt] (int modalResult)
                             {
                                 if (modalResult != 1 || safePrompt == nullptr)
                                     return;

                                 const auto requestedName = safePrompt->getTextEditorContents ("presetName").trim();
                                 const auto result = processorRef.saveCurrentPresetAsNew (requestedName);
                                 presetStatusLabel.setText (result.message, juce::dontSendNotification);
                                 refreshPresetControls();
                             }),
                             true);
}

void PolySynthAudioProcessorEditor::loadSelectedPreset()
{
    const auto selectedPresetName = presetSelector.getText().trim();
    if (selectedPresetName.isEmpty())
    {
        presetStatusLabel.setText ("Select a preset to load.", juce::dontSendNotification);
        return;
    }

    const auto result = processorRef.loadPresetByName (selectedPresetName);
    presetStatusLabel.setColour (juce::Label::textColourId,
                                 result.warnedAboutPartialLoad ? juce::Colours::yellow : juce::Colours::lightgreen);
    presetStatusLabel.setText (result.message, juce::dontSendNotification);

    if (result.success)
    {
        syncLayerListFromProcessor();
        syncInspectorControlsFromSelectedLayer();
        syncRootNoteControlsFromProcessor();
    }

    refreshPresetControls();
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
