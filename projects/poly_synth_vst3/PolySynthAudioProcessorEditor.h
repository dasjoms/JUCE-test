#pragma once

#include "PolySynthAudioProcessor.h"
#include <array>

//==============================================================================
class PolySynthAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit PolySynthAudioProcessorEditor (PolySynthAudioProcessor&);
    ~PolySynthAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct LayerRow final : public juce::Component
    {
        LayerRow();
        void resized() override;

        juce::TextButton selectButton;
        juce::Label layerNameLabel;
        juce::TextButton moveUpButton;
        juce::TextButton moveDownButton;
        juce::TextButton duplicateButton;
        juce::TextButton deleteButton;
        juce::ToggleButton muteToggle;
        juce::ToggleButton soloToggle;
        juce::Slider volumeSlider;
    };

    void timerCallback() override;
    void syncRootNoteControlsFromProcessor();
    void syncLayerListFromProcessor();
    void syncInspectorControlsFromSelectedLayer();
    void updateInspectorBindingState();
    void selectLayer (std::size_t layerIndex);
    void showAddLayerMenu();
    void showDuplicateLayerMenu();
    void duplicateLayerFromIndex (std::size_t sourceLayerIndex);
    void deleteLayer (std::size_t layerIndex);
    void moveLayerUp (std::size_t layerIndex);
    void moveLayerDown (std::size_t layerIndex);
    void setActionStatusMessage (const juce::String& message);
    void handleAbsoluteRootNoteChange();
    void handleRelativeRootSemitoneChange();
    void refreshPresetControls();
    void savePresetOverwrite();
    void savePresetAsNew();
    void loadSelectedPreset();
    static juce::String midiNoteToDisplayString (int midiNote);

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PolySynthAudioProcessor& processorRef;
    juce::ToggleButton globalPanelToggle;
    juce::GroupComponent globalPanel;
    juce::Label globalPanelPlaceholderLabel;
    juce::GroupComponent sidebarPanel;
    juce::GroupComponent layerListPanel;
    juce::GroupComponent presetPanel;
    juce::GroupComponent marketplacePanel;
    juce::TextButton addLayerButton;
    juce::Label actionStatusLabel;
    juce::Label presetLabel;
    juce::ComboBox presetSelector;
    juce::TextButton presetLoadButton;
    juce::TextButton presetSaveButton;
    juce::TextButton presetSaveAsNewButton;
    juce::Label presetStatusLabel;
    juce::TextButton marketplaceBrowseButton;
    juce::TextButton marketplaceUploadButton;
    juce::TextButton marketplaceSyncButton;
    juce::Label marketplaceLoginStatusLabel;
    juce::Label inspectorTitleLabel;
    juce::Label emptyInspectorLabel;
    juce::Label titleLabel;
    juce::GroupComponent oscillatorCard;
    juce::GroupComponent envelopeCard;
    juce::GroupComponent modulationCard;
    juce::GroupComponent outputVoicingCard;
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
    juce::Label outputStageLabel;
    juce::Label absoluteRootNoteLabel;
    juce::Label relativeRootSemitoneLabel;
    juce::Label rootNoteFeedbackLabel;
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
    juce::Slider absoluteRootNoteSlider;
    juce::Slider relativeRootSemitoneSlider;
    juce::ComboBox outputStageSelector;
    static constexpr std::size_t maxLayerRows = InstrumentState::maxLayerCount;
    std::array<LayerRow, maxLayerRows> layerRows;
    std::size_t visibleLayerCount = 0;
    std::size_t selectedLayerIndex = 0;
    bool suppressRootNoteCallbacks = false;
    bool suppressInspectorCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolySynthAudioProcessorEditor)
};
