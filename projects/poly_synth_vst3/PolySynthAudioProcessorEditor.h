#pragma once

#include "PolySynthAudioProcessor.h"
#include <array>
#include <functional>

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
    bool setCapturePageByName (juce::StringRef pageName, juce::String& errorMessage);
    bool setDensityModeByName (juce::StringRef densityMode, juce::String& errorMessage);
    void setVoiceAdvancedPanelExpandedForCapture (bool shouldExpand);
    void setOutputAdvancedPanelExpandedForCapture (bool shouldExpand);
    void setGlobalPanelVisibleForCapture (bool shouldShow);
    bool selectLayerByVisualIndexForCapture (std::size_t layerIndex);
    bool loadPresetForCapture (juce::StringRef presetName, juce::String& errorMessage);
    bool setControlValueByComponentIdForCapture (juce::StringRef componentId, const juce::var& value, juce::String& errorMessage);
    void syncUiForCapture();

private:
    enum class EditorPage
    {
        main = 0,
        library
    };

    class PaneComponent final : public juce::Component
    {
    public:
        enum class Style
        {
            sidebar = 0,
            workspace
        };

        explicit PaneComponent (Style paneStyle);

        void paint (juce::Graphics& g) override;

    private:
        Style style;
    };

    class SectionPanel final : public juce::Component
    {
    public:
        void setTitle (juce::String newTitle);
        juce::Rectangle<int> getContentBounds() const;

        void paint (juce::Graphics& g) override;

    private:
        juce::String title;
    };

    class WaveformDisplayPanel final : public juce::Component
    {
    public:
        void setLayerWaveforms (const std::vector<SynthVoice::Waveform>& waveformsToDraw);
        void setAnimatedMode (bool shouldAnimate, float animationPhaseToUse);

        void paint (juce::Graphics& g) override;

    private:
        static float evaluateWaveformSample (SynthVoice::Waveform waveformType, float phase) noexcept;
        std::vector<SynthVoice::Waveform> waveforms;
        bool animate = false;
        float animationPhase = 0.0f;
    };

    class AdsrGraphPanel final : public juce::Component
    {
    public:
        using EnvelopeChangedCallback = std::function<void (float, float, float, float)>;

        void setEnvelope (float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds);
        void setAnimatedMode (bool shouldAnimate, float animationProgressToUse);
        void setEnvelopeChangedCallback (EnvelopeChangedCallback callback);

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseUp (const juce::MouseEvent& event) override;

    private:
        enum class DragHandle
        {
            none = 0,
            attackPeak,
            decaySustain
        };

        struct EnvelopePoints
        {
            juce::Point<float> start;
            juce::Point<float> attackPeak;
            juce::Point<float> decaySustain;
            juce::Point<float> releaseEnd;
        };

        EnvelopePoints computeEnvelopePoints (juce::Rectangle<float> area) const;
        juce::Rectangle<float> getContentBounds() const;
        void notifyEnvelopeChangedIfNeeded();

        float attack = 0.005f;
        float decay = 0.08f;
        float sustain = 0.8f;
        float release = 0.03f;
        bool animate = false;
        float animationProgress = 0.0f;
        DragHandle dragHandle = DragHandle::none;
        EnvelopeChangedCallback envelopeChangedCallback;
    };

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
    void showMainPage();
    void showLibraryPage();
    void refreshDensityUiState();
    static juce::String midiNoteToDisplayString (int midiNote);

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PolySynthAudioProcessor& processorRef;
    juce::Component libraryPageContainer;
    PaneComponent sidebarContainer { PaneComponent::Style::sidebar };
    PaneComponent workspaceContainer { PaneComponent::Style::workspace };
    juce::ToggleButton globalPanelToggle;
    juce::GroupComponent globalPanel;
    juce::Label globalPanelPlaceholderLabel;
    juce::GroupComponent sidebarPanel;
    juce::GroupComponent layerListPanel;
    juce::GroupComponent presetPanel;
    juce::TextButton viewLibraryButton;
    juce::TextButton addLayerButton;
    juce::Label actionStatusLabel;
    juce::Label presetLabel;
    juce::ComboBox presetSelector;
    juce::TextButton presetLoadButton;
    juce::TextButton presetSaveButton;
    juce::TextButton presetSaveAsNewButton;
    juce::Label presetStatusLabel;
    juce::TextButton backToSynthPageButton;
    juce::GroupComponent libraryMarketplacePanel;
    juce::TextButton libraryMarketplaceBrowseButton;
    juce::TextButton libraryMarketplaceUploadButton;
    juce::TextButton libraryMarketplaceSyncButton;
    juce::Label libraryMarketplaceLoginStatusLabel;
    juce::Label inspectorTitleLabel;
    juce::Label densityModeLabel;
    juce::ComboBox densityModeSelector;
    juce::Label emptyInspectorLabel;
    juce::Label titleLabel;
    SectionPanel oscillatorSection;
    SectionPanel voiceUnisonSection;
    SectionPanel envelopeSection;
    SectionPanel modulationSection;
    SectionPanel outputSection;
    SectionPanel tuningAdvancedSection;
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
    juce::ToggleButton voiceAdvancedPanelToggle;
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
    juce::ToggleButton outputAdvancedPanelToggle;
    WaveformDisplayPanel waveformDisplayPanel;
    AdsrGraphPanel adsrGraphPanel;
    static constexpr std::size_t maxLayerRows = InstrumentState::maxLayerCount;
    std::array<LayerRow, maxLayerRows> layerRows;
    std::size_t visibleLayerCount = 0;
    std::size_t selectedLayerIndex = 0;
    bool suppressRootNoteCallbacks = false;
    bool suppressInspectorCallbacks = false;
    float waveformAnimationPhase = 0.0f;
    float adsrAnimationProgress = 0.0f;
    bool usingAdvancedDensity = false;
    EditorPage currentPage = EditorPage::main;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolySynthAudioProcessorEditor)
};
