#include "PolySynthAudioProcessor.h"
#include "PolySynthAudioProcessorEditor.h"
#include <array>
#include <cmath>
#include <utility>

namespace
{
constexpr std::array<const char*, 12> pitchClassNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

struct LayoutTokens
{
    static constexpr int outerPadding = 0;
    static constexpr int sectionPadding = 10;
    static constexpr int rowSpacing = 8;
    static constexpr int majorPaneGap = rowSpacing;
    static constexpr int controlGap = 6;
    static constexpr int sectionHeaderHeight = 26;
    static constexpr int sectionFooterPadding = 10;
    static constexpr int majorSeparatorThickness = 2;
    static constexpr int sidebarPaneGapThickness = majorPaneGap;
    static juce::Colour majorSeparatorColour() noexcept { return juce::Colours::lightgrey; }
};

struct SidebarLayoutTokens
{
    static constexpr int panelHorizontalInset = 8;
    static constexpr int panelVerticalInset = 28;
    static constexpr int compactControlRowHeight = 28;
    static constexpr int presetLabelWidth = 54;

    static constexpr int presetSelectorGap = LayoutTokens::controlGap - 2;
    static constexpr int presetButtonsToLibraryGap = LayoutTokens::controlGap;
    static constexpr int layerHeaderGap = LayoutTokens::controlGap - 2;
    static constexpr int layerStatusGap = LayoutTokens::controlGap - 2;

    static constexpr int actionStatusRowHeight = 20;
    static constexpr int layerRowHeight = 64;
    static constexpr int minimumVisibleLayerRows = 1;

    static constexpr int minimumPresetContentHeight = compactControlRowHeight
                                                    + presetSelectorGap
                                                    + compactControlRowHeight
                                                    + presetButtonsToLibraryGap
                                                    + compactControlRowHeight;
    static constexpr int minimumLayerContentHeight = compactControlRowHeight
                                                   + layerHeaderGap
                                                   + actionStatusRowHeight
                                                   + layerStatusGap
                                                   + (minimumVisibleLayerRows * layerRowHeight);

    static constexpr int minimumPresetBlockHeight = minimumPresetContentHeight + (panelVerticalInset * 2);
    static constexpr int minimumLayerBlockHeight = minimumLayerContentHeight + (panelVerticalInset * 2);
};

juce::Rectangle<int> mapSectionLocalBoundsToEditor (const juce::Component& editor,
                                                    const juce::Component& section,
                                                    juce::Rectangle<int> sectionLocalBounds)
{
    return editor.getLocalArea (&section, sectionLocalBounds);
}
}

void PolySynthAudioProcessorEditor::SectionPanel::setTitle (juce::String newTitle)
{
    title = std::move (newTitle);
    repaint();
}

PolySynthAudioProcessorEditor::PaneComponent::PaneComponent (Style paneStyle)
    : style (paneStyle)
{
    setOpaque (true);
}

void PolySynthAudioProcessorEditor::PaneComponent::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds();

    if (style == Style::separatorFill)
    {
        g.fillAll (LayoutTokens::majorSeparatorColour());
        return;
    }

    if (style == Style::sidebar)
    {
        g.fillAll (juce::Colours::white);

        g.setColour (LayoutTokens::majorSeparatorColour());
        g.fillRect (juce::Rectangle<int> (area.getRight() - LayoutTokens::majorSeparatorThickness,
                                          area.getY(),
                                          LayoutTokens::majorSeparatorThickness,
                                          area.getHeight()));
        return;
    }

    g.fillAll (juce::Colours::white);
}

juce::Rectangle<int> PolySynthAudioProcessorEditor::SectionPanel::getContentBounds() const
{
    auto bounds = getLocalBounds().reduced (LayoutTokens::sectionPadding);
    bounds.removeFromTop (LayoutTokens::sectionHeaderHeight);
    bounds.removeFromBottom (LayoutTokens::sectionFooterPadding);
    return bounds;
}

void PolySynthAudioProcessorEditor::SectionPanel::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    auto background = juce::Colours::darkslategrey.withAlpha (0.24f);
    g.setColour (background);
    g.fillRoundedRectangle (area, 9.0f);

    auto headerArea = area.withHeight (static_cast<float> (LayoutTokens::sectionHeaderHeight + LayoutTokens::sectionPadding / 2));
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    g.fillRoundedRectangle (headerArea, 9.0f);

    const auto dividerY = LayoutTokens::sectionHeaderHeight + LayoutTokens::sectionPadding / 2;
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawHorizontalLine (dividerY, area.getX() + LayoutTokens::sectionPadding, area.getRight() - LayoutTokens::sectionPadding);

    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (title,
                LayoutTokens::sectionPadding,
                LayoutTokens::sectionPadding / 2,
                getWidth() - (LayoutTokens::sectionPadding * 2),
                LayoutTokens::sectionHeaderHeight - 2,
                juce::Justification::centredLeft,
                true);
}

void PolySynthAudioProcessorEditor::WaveformDisplayPanel::setLayerWaveforms (const std::vector<SynthVoice::Waveform>& waveformsToDraw)
{
    waveforms = waveformsToDraw;
    repaint();
}

void PolySynthAudioProcessorEditor::WaveformDisplayPanel::setAnimatedMode (bool shouldAnimate, float animationPhaseToUse)
{
    animate = shouldAnimate;
    animationPhase = animationPhaseToUse;
    repaint();
}

float PolySynthAudioProcessorEditor::WaveformDisplayPanel::evaluateWaveformSample (SynthVoice::Waveform waveformType, float phase) noexcept
{
    const auto wrappedPhase = phase - std::floor (phase);
    constexpr auto twoPi = juce::MathConstants<float>::twoPi;

    switch (waveformType)
    {
        case SynthVoice::Waveform::saw:
            return wrappedPhase * 2.0f - 1.0f;
        case SynthVoice::Waveform::square:
            return wrappedPhase < 0.5f ? 1.0f : -1.0f;
        case SynthVoice::Waveform::triangle:
            return 1.0f - 4.0f * std::abs (wrappedPhase - 0.5f);
        case SynthVoice::Waveform::sine:
        default:
            return std::sin (wrappedPhase * twoPi);
    }
}

void PolySynthAudioProcessorEditor::WaveformDisplayPanel::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRoundedRectangle (area, 6.0f);

    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), area.getX(), area.getRight());
    g.drawRoundedRectangle (area, 6.0f, 1.0f);

    if (waveforms.empty())
    {
        g.setColour (juce::Colours::lightgrey.withAlpha (0.8f));
        g.drawFittedText ("No waveform", getLocalBounds(), juce::Justification::centred, 1);
        return;
    }

    const auto drawSingle = [this, area, &g] (SynthVoice::Waveform waveformType, juce::Colour colour, float strokeWidth)
    {
        juce::Path path;
        constexpr int sampleCount = 256;
        for (int i = 0; i < sampleCount; ++i)
        {
            const auto normalizedX = static_cast<float> (i) / static_cast<float> (sampleCount - 1);
            const auto phase = normalizedX + (animate ? animationPhase : 0.0f);
            const auto value = evaluateWaveformSample (waveformType, phase);
            const auto x = area.getX() + normalizedX * area.getWidth();
            const auto y = area.getCentreY() - value * (area.getHeight() * 0.42f);

            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    if (waveforms.size() == 1)
    {
        drawSingle (waveforms.front(), juce::Colours::deepskyblue, 2.4f);
    }
    else
    {
        for (const auto waveform : waveforms)
            drawSingle (waveform, juce::Colours::lightslategrey.withAlpha (0.45f), 1.3f);
        drawSingle (waveforms.front(), juce::Colours::deepskyblue.withAlpha (0.95f), 2.2f);
    }
}

void PolySynthAudioProcessorEditor::AdsrGraphPanel::setEnvelope (float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds)
{
    attack = juce::jlimit (0.001f, 5.0f, attackSeconds);
    decay = juce::jlimit (0.001f, 5.0f, decaySeconds);
    sustain = juce::jlimit (0.0f, 1.0f, sustainLevel);
    release = juce::jlimit (0.005f, 5.0f, releaseSeconds);
    repaint();
}

void PolySynthAudioProcessorEditor::AdsrGraphPanel::setAnimatedMode (bool shouldAnimate, float animationProgressToUse)
{
    animate = shouldAnimate;
    animationProgress = juce::jlimit (0.0f, 1.0f, animationProgressToUse);
    repaint();
}

void PolySynthAudioProcessorEditor::AdsrGraphPanel::setEnvelopeChangedCallback (EnvelopeChangedCallback callback)
{
    envelopeChangedCallback = std::move (callback);
}

juce::Rectangle<float> PolySynthAudioProcessorEditor::AdsrGraphPanel::getContentBounds() const
{
    return getLocalBounds().toFloat().reduced (10.0f);
}

PolySynthAudioProcessorEditor::AdsrGraphPanel::EnvelopePoints PolySynthAudioProcessorEditor::AdsrGraphPanel::computeEnvelopePoints (juce::Rectangle<float> area) const
{
    const auto totalTime = juce::jmax (0.001f, attack + decay + release);
    const auto attackPortion = attack / totalTime;
    const auto decayPortion = decay / totalTime;
    const auto releasePortion = release / totalTime;

    EnvelopePoints points;
    points.start = { area.getX(), area.getBottom() };
    points.attackPeak = { area.getX() + area.getWidth() * attackPortion, area.getY() };
    points.decaySustain = { points.attackPeak.x + area.getWidth() * decayPortion, area.getBottom() - sustain * area.getHeight() };
    points.releaseEnd = { points.decaySustain.x + area.getWidth() * releasePortion, area.getBottom() };
    points.releaseEnd.x = juce::jlimit (area.getX(), area.getRight(), points.releaseEnd.x);
    return points;
}

void PolySynthAudioProcessorEditor::AdsrGraphPanel::paint (juce::Graphics& g)
{
    const auto content = getContentBounds();
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRoundedRectangle (content, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (content, 6.0f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    for (int row = 1; row < 4; ++row)
    {
        const auto y = content.getY() + content.getHeight() * (static_cast<float> (row) / 4.0f);
        g.drawHorizontalLine (juce::roundToInt (y), content.getX(), content.getRight());
    }

    const auto points = computeEnvelopePoints (content);
    juce::Path path;
    path.startNewSubPath (points.start);
    path.lineTo (points.attackPeak);
    path.lineTo (points.decaySustain);
    path.lineTo (points.releaseEnd);

    g.setColour (juce::Colours::orange.withAlpha (0.95f));
    g.strokePath (path, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto drawHandle = [&g] (juce::Point<float> center)
    {
        g.setColour (juce::Colours::orange.brighter (0.25f));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (center));
    };

    drawHandle (points.attackPeak);
    drawHandle (points.decaySustain);

    if (animate)
    {
        const auto progressX = content.getX() + content.getWidth() * animationProgress;
        g.setColour (juce::Colours::yellow.withAlpha (0.65f));
        g.drawVerticalLine (juce::roundToInt (progressX), content.getY(), content.getBottom());
    }
}

void PolySynthAudioProcessorEditor::AdsrGraphPanel::mouseDown (const juce::MouseEvent& event)
{
    const auto content = getContentBounds();
    const auto points = computeEnvelopePoints (content);
    constexpr auto hitRadius = 14.0f;

    if (event.position.getDistanceFrom (points.attackPeak) <= hitRadius)
        dragHandle = DragHandle::attackPeak;
    else if (event.position.getDistanceFrom (points.decaySustain) <= hitRadius)
        dragHandle = DragHandle::decaySustain;
    else
        dragHandle = DragHandle::none;
}

void PolySynthAudioProcessorEditor::AdsrGraphPanel::mouseDrag (const juce::MouseEvent& event)
{
    const auto content = getContentBounds();
    if (! content.contains (event.position))
        return;

    const auto points = computeEnvelopePoints (content);
    const auto totalTime = juce::jmax (0.001f, attack + decay + release);

    if (dragHandle == DragHandle::attackPeak)
    {
        const auto normalizedX = (event.position.x - content.getX()) / content.getWidth();
        const auto newAttack = juce::jlimit (0.001f, 4.95f, totalTime * juce::jlimit (0.02f, 0.82f, normalizedX));
        attack = newAttack;
        release = juce::jmax (0.005f, totalTime - attack - decay);
        notifyEnvelopeChangedIfNeeded();
    }
    else if (dragHandle == DragHandle::decaySustain)
    {
        const auto decayMinX = points.attackPeak.x + 8.0f;
        const auto decayMaxX = content.getRight() - 18.0f;
        const auto clippedX = juce::jlimit (decayMinX, decayMaxX, event.position.x);
        const auto decayPortion = (clippedX - points.attackPeak.x) / content.getWidth();
        decay = juce::jlimit (0.001f, 4.95f, decayPortion * totalTime);
        release = juce::jmax (0.005f, totalTime - attack - decay);

        const auto normalizedY = (event.position.y - content.getY()) / content.getHeight();
        sustain = 1.0f - juce::jlimit (0.0f, 1.0f, normalizedY);
        notifyEnvelopeChangedIfNeeded();
    }
}

void PolySynthAudioProcessorEditor::AdsrGraphPanel::mouseUp (const juce::MouseEvent&)
{
    dragHandle = DragHandle::none;
}

void PolySynthAudioProcessorEditor::AdsrGraphPanel::notifyEnvelopeChangedIfNeeded()
{
    if (envelopeChangedCallback != nullptr)
        envelopeChangedCallback (attack, decay, sustain, release);

    repaint();
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
    layerNameLabel.setColour (juce::Label::textColourId, juce::Colours::black);
    addAndMakeVisible (layerNameLabel);

    moveUpButton.setButtonText ("Up");
    addAndMakeVisible (moveUpButton);

    moveDownButton.setButtonText ("Down");
    addAndMakeVisible (moveDownButton);

    duplicateButton.setButtonText ("Dup");
    addAndMakeVisible (duplicateButton);

    deleteButton.setButtonText ("Del");
    addAndMakeVisible (deleteButton);

    muteToggle.setButtonText ("Mute");
    muteToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    muteToggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    muteToggle.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::black);
    addAndMakeVisible (muteToggle);

    soloToggle.setButtonText ("Solo");
    soloToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    soloToggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    soloToggle.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::black);
    addAndMakeVisible (soloToggle);

    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 20);
    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setNumDecimalPlacesToDisplay (2);
    volumeSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::black);
    volumeSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::black);
    addAndMakeVisible (volumeSlider);
}

void PolySynthAudioProcessorEditor::LayerRow::resized()
{
    auto area = getLocalBounds().reduced (4, 2);
    selectButton.setBounds (area);
    auto controls = area.reduced (6, 2);

    auto topRow = controls.removeFromTop (28);
    auto bottomRow = controls;

    layerNameLabel.setBounds (topRow.removeFromLeft (64));
    topRow.removeFromLeft (4);
    muteToggle.setBounds (topRow.removeFromLeft (52));
    topRow.removeFromLeft (4);
    soloToggle.setBounds (topRow.removeFromLeft (52));
    topRow.removeFromLeft (4);
    volumeSlider.setBounds (topRow);

    moveUpButton.setBounds (bottomRow.removeFromLeft (52).reduced (1, 0));
    bottomRow.removeFromLeft (4);
    moveDownButton.setBounds (bottomRow.removeFromLeft (62).reduced (1, 0));
    bottomRow.removeFromLeft (4);
    duplicateButton.setBounds (bottomRow.removeFromLeft (76).reduced (1, 0));
    bottomRow.removeFromLeft (4);
    deleteButton.setBounds (bottomRow.reduced (1, 0));
}

//==============================================================================
PolySynthAudioProcessorEditor::PolySynthAudioProcessorEditor (PolySynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    auto configurePrimaryKnob = [] (juce::Slider& slider, double min, double max, double interval, int decimals, juce::String suffix = {})
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
        slider.setRange (min, max, interval);
        slider.setNumDecimalPlacesToDisplay (decimals);
        slider.setTextValueSuffix (suffix);
    };
    auto configureSecondaryKnob = [] (juce::Slider& slider, double min, double max, double interval, int decimals, juce::String suffix = {})
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
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

    addAndMakeVisible (sidebarContainer);
    addAndMakeVisible (workspaceContainer);
    addAndMakeVisible (libraryPageContainer);
    libraryPageContainer.setVisible (false);
    libraryPageContainer.setEnabled (false);

    layersPane.setTitle ("Layers");
    sidebarContainer.addAndMakeVisible (layersPane);

    presetBrowserPane.setTitle ("Preset Browser");
    sidebarContainer.addAndMakeVisible (presetBrowserPane);
    sidebarContainer.addAndMakeVisible (sidebarPaneGap);

    addLayerButton.setButtonText ("Add Layer");
    addLayerButton.onClick = [this] { showAddLayerMenu(); };
    sidebarContainer.addAndMakeVisible (addLayerButton);

    actionStatusLabel.setJustificationType (juce::Justification::centredLeft);
    actionStatusLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
    sidebarContainer.addAndMakeVisible (actionStatusLabel);

    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    sidebarContainer.addAndMakeVisible (presetLabel);

    presetSelector.setTextWhenNothingSelected ("No presets");
    sidebarContainer.addAndMakeVisible (presetSelector);

    presetLoadButton.setButtonText ("Load");
    presetLoadButton.onClick = [this] { loadSelectedPreset(); };
    sidebarContainer.addAndMakeVisible (presetLoadButton);

    presetSaveButton.setButtonText ("Save");
    presetSaveButton.onClick = [this] { savePresetOverwrite(); };
    sidebarContainer.addAndMakeVisible (presetSaveButton);

    presetSaveAsNewButton.setButtonText ("Save As New");
    presetSaveAsNewButton.onClick = [this] { savePresetAsNew(); };
    sidebarContainer.addAndMakeVisible (presetSaveAsNewButton);

    viewLibraryButton.setButtonText ("View Library");
    viewLibraryButton.onClick = [this] { showLibraryPage(); };
    sidebarContainer.addAndMakeVisible (viewLibraryButton);

    presetStatusLabel.setJustificationType (juce::Justification::centredLeft);
    presetStatusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgoldenrodyellow);
    sidebarContainer.addAndMakeVisible (presetStatusLabel);

    backToSynthPageButton.setButtonText ("Back to Main");
    backToSynthPageButton.onClick = [this] { showMainPage(); };
    libraryPageContainer.addAndMakeVisible (backToSynthPageButton);

    libraryMarketplacePanel.setText ("Marketplace");
    libraryPageContainer.addAndMakeVisible (libraryMarketplacePanel);

    libraryMarketplaceBrowseButton.setButtonText ("Browse");
    libraryMarketplaceBrowseButton.setEnabled (false);
    libraryPageContainer.addAndMakeVisible (libraryMarketplaceBrowseButton);

    libraryMarketplaceUploadButton.setButtonText ("Upload");
    libraryMarketplaceUploadButton.setEnabled (false);
    libraryPageContainer.addAndMakeVisible (libraryMarketplaceUploadButton);

    libraryMarketplaceSyncButton.setButtonText ("Sync");
    libraryMarketplaceSyncButton.setEnabled (false);
    libraryPageContainer.addAndMakeVisible (libraryMarketplaceSyncButton);

    libraryMarketplaceLoginStatusLabel.setText ("Login: Not connected", juce::dontSendNotification);
    libraryMarketplaceLoginStatusLabel.setJustificationType (juce::Justification::centredLeft);
    libraryPageContainer.addAndMakeVisible (libraryMarketplaceLoginStatusLabel);

    inspectorTitleLabel.setText ("Selected Layer Controls", juce::dontSendNotification);
    inspectorTitleLabel.setJustificationType (juce::Justification::centredLeft);
    workspaceContainer.addAndMakeVisible (inspectorTitleLabel);

    densityModeLabel.setText ("UI Density", juce::dontSendNotification);
    densityModeLabel.setJustificationType (juce::Justification::centredRight);
    workspaceContainer.addAndMakeVisible (densityModeLabel);

    densityModeSelector.addItem ("Basic", 1);
    densityModeSelector.addItem ("Advanced", 2);
    densityModeSelector.setComponentID ("densityModeSelector");
    densityModeSelector.onChange = [this]
    {
        const auto selected = densityModeSelector.getSelectedItemIndex() <= 0
                                  ? PolySynthAudioProcessor::UiDensityMode::basic
                                  : PolySynthAudioProcessor::UiDensityMode::advanced;
        processorRef.setUiDensityMode (selected);
        usingAdvancedDensity = selected == PolySynthAudioProcessor::UiDensityMode::advanced;
        refreshDensityUiState();
        resized();
    };
    workspaceContainer.addAndMakeVisible (densityModeSelector);

    emptyInspectorLabel.setText ("No layer selected.", juce::dontSendNotification);
    emptyInspectorLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (emptyInspectorLabel);

    oscillatorSection.setTitle ("Oscillator");
    oscillatorSection.setComponentID ("oscillatorSection");
    workspaceContainer.addAndMakeVisible (oscillatorSection);

    voiceUnisonSection.setTitle ("Voice / Unison");
    voiceUnisonSection.setComponentID ("voiceUnisonSection");
    workspaceContainer.addAndMakeVisible (voiceUnisonSection);

    envelopeSection.setTitle ("Envelope");
    envelopeSection.setComponentID ("envelopeSection");
    workspaceContainer.addAndMakeVisible (envelopeSection);

    modulationSection.setTitle ("Modulation");
    modulationSection.setComponentID ("modulationSection");
    workspaceContainer.addAndMakeVisible (modulationSection);

    outputSection.setTitle ("Output");
    outputSection.setComponentID ("outputSection");
    workspaceContainer.addAndMakeVisible (outputSection);

    tuningAdvancedSection.setTitle ("Tuning / Advanced");
    tuningAdvancedSection.setComponentID ("tuningAdvancedSection");
    workspaceContainer.addAndMakeVisible (tuningAdvancedSection);

    waveformLabel.setText ("Waveform", juce::dontSendNotification);
    waveformLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (waveformLabel);

    waveformSelector.addItem ("Sine", 1);
    waveformSelector.addItem ("Saw", 2);
    waveformSelector.addItem ("Square", 3);
    waveformSelector.addItem ("Triangle", 4);
    waveformSelector.setComponentID ("waveformSelector");
    workspaceContainer.addAndMakeVisible (waveformSelector);
    workspaceContainer.addAndMakeVisible (waveformDisplayPanel);

    maxVoicesLabel.setText ("Voice Count", juce::dontSendNotification);
    maxVoicesLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (maxVoicesLabel);

    configureSecondaryKnob (maxVoicesSlider, 1.0, 16.0, 1.0, 0);
    maxVoicesSlider.setComponentID ("maxVoicesSlider");
    workspaceContainer.addAndMakeVisible (maxVoicesSlider);

    stealPolicyLabel.setText ("Steal Policy", juce::dontSendNotification);
    stealPolicyLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (stealPolicyLabel);

    stealPolicySelector.addItem ("Released First", 1);
    stealPolicySelector.addItem ("Oldest", 2);
    stealPolicySelector.addItem ("Quietest", 3);
    stealPolicySelector.setComponentID ("stealPolicySelector");
    workspaceContainer.addAndMakeVisible (stealPolicySelector);

    voiceAdvancedPanelToggle.setButtonText ("Voice & policy details");
    voiceAdvancedPanelToggle.setComponentID ("voiceAdvancedPanelToggle");
    voiceAdvancedPanelToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    voiceAdvancedPanelToggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    voiceAdvancedPanelToggle.setClickingTogglesState (true);
    voiceAdvancedPanelToggle.onClick = [this]
    {
        processorRef.setVoiceAdvancedPanelExpanded (voiceAdvancedPanelToggle.getToggleState());
        refreshDensityUiState();
        resized();
    };
    workspaceContainer.addAndMakeVisible (voiceAdvancedPanelToggle);

    attackLabel.setText ("Atk (s)", juce::dontSendNotification);
    attackLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (attackLabel);

    attackSlider.setComponentID ("attackSlider");
    configurePrimaryKnob (attackSlider, 0.001, 5.0, 0.001, 3, " s");
    attackSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
    workspaceContainer.addAndMakeVisible (attackSlider);

    decayLabel.setText ("Dec (s)", juce::dontSendNotification);
    decayLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (decayLabel);

    decaySlider.setComponentID ("decaySlider");
    configurePrimaryKnob (decaySlider, 0.001, 5.0, 0.001, 3, " s");
    decaySlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
    workspaceContainer.addAndMakeVisible (decaySlider);

    sustainLabel.setText ("Sus", juce::dontSendNotification);
    sustainLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (sustainLabel);

    sustainSlider.setComponentID ("sustainSlider");
    configurePrimaryKnob (sustainSlider, 0.0, 1.0, 0.001, 3);
    sustainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
    workspaceContainer.addAndMakeVisible (sustainSlider);

    releaseLabel.setText ("Rel (s)", juce::dontSendNotification);
    releaseLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (releaseLabel);

    configurePrimaryKnob (releaseSlider, 0.005, 5.0, 0.001, 3, " s");
    releaseSlider.setComponentID ("releaseSlider");
    releaseSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
    workspaceContainer.addAndMakeVisible (releaseSlider);
    workspaceContainer.addAndMakeVisible (adsrGraphPanel);

    modDepthLabel.setText ("Mod Depth", juce::dontSendNotification);
    modDepthLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (modDepthLabel);

    configurePrimaryKnob (modDepthSlider, 0.0, 1.0, 0.001, 3);
    workspaceContainer.addAndMakeVisible (modDepthSlider);

    modRateLabel.setText ("Mod Rate", juce::dontSendNotification);
    modRateLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (modRateLabel);

    configurePrimaryKnob (modRateSlider, 0.05, 20.0, 0.01, 2, " Hz");
    workspaceContainer.addAndMakeVisible (modRateSlider);

    velocitySensitivityLabel.setText ("Velocity Sensitivity", juce::dontSendNotification);
    velocitySensitivityLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (velocitySensitivityLabel);

    velocitySensitivitySlider.setComponentID ("velocitySensitivitySlider");
    configureSecondaryKnob (velocitySensitivitySlider, 0.0, 1.0, 0.001, 3);
    workspaceContainer.addAndMakeVisible (velocitySensitivitySlider);

    modDestinationLabel.setText ("Mod Destination", juce::dontSendNotification);
    modDestinationLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (modDestinationLabel);

    modDestinationSelector.setComponentID ("modDestinationSelector");
    modDestinationSelector.addItem ("Off", 1);
    modDestinationSelector.addItem ("Amplitude", 2);
    modDestinationSelector.addItem ("Pitch", 3);
    modDestinationSelector.addItem ("Pulse Width", 4);
    workspaceContainer.addAndMakeVisible (modDestinationSelector);

    unisonVoicesLabel.setText ("Unison Voices", juce::dontSendNotification);
    unisonVoicesLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (unisonVoicesLabel);

    unisonVoicesSlider.setComponentID ("unisonVoicesSlider");
    configureSecondaryKnob (unisonVoicesSlider, 1.0, 8.0, 1.0, 0);
    workspaceContainer.addAndMakeVisible (unisonVoicesSlider);

    unisonDetuneCentsLabel.setText ("Unison Detune (cents)", juce::dontSendNotification);
    unisonDetuneCentsLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (unisonDetuneCentsLabel);

    unisonDetuneCentsSlider.setComponentID ("unisonDetuneCentsSlider");
    configurePrimaryKnob (unisonDetuneCentsSlider, 0.0, 50.0, 0.01, 2, " cents");
    workspaceContainer.addAndMakeVisible (unisonDetuneCentsSlider);

    outputStageLabel.setText ("Output Stage", juce::dontSendNotification);
    outputStageLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (outputStageLabel);

    outputStageSelector.setComponentID ("outputStageSelector");
    outputStageSelector.addItem ("None", 1);
    outputStageSelector.addItem ("Normalize Voice Sum", 2);
    outputStageSelector.addItem ("Soft Limit", 3);
    workspaceContainer.addAndMakeVisible (outputStageSelector);

    outputAdvancedPanelToggle.setButtonText ("Output & tuning details");
    outputAdvancedPanelToggle.setComponentID ("outputAdvancedPanelToggle");
    outputAdvancedPanelToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::black);
    outputAdvancedPanelToggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::black);
    outputAdvancedPanelToggle.setClickingTogglesState (true);
    outputAdvancedPanelToggle.onClick = [this]
    {
        processorRef.setOutputAdvancedPanelExpanded (outputAdvancedPanelToggle.getToggleState());
        refreshDensityUiState();
        resized();
    };
    workspaceContainer.addAndMakeVisible (outputAdvancedPanelToggle);

    absoluteRootNoteLabel.setText ("Root Note", juce::dontSendNotification);
    absoluteRootNoteLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (absoluteRootNoteLabel);

    absoluteRootNoteSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    absoluteRootNoteSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 18);
    absoluteRootNoteSlider.setRange (PolySynthAudioProcessor::minimumMidiNote,
                                     PolySynthAudioProcessor::maximumMidiNote,
                                     1.0);
    absoluteRootNoteSlider.textFromValueFunction = [] (double value)
    {
        return midiNoteToDisplayString (juce::roundToInt (value));
    };
    absoluteRootNoteSlider.onValueChange = [this] { handleAbsoluteRootNoteChange(); };
    workspaceContainer.addAndMakeVisible (absoluteRootNoteSlider);

    relativeRootSemitoneLabel.setText ("Relative (st)", juce::dontSendNotification);
    relativeRootSemitoneLabel.setJustificationType (juce::Justification::centred);
    workspaceContainer.addAndMakeVisible (relativeRootSemitoneLabel);

    relativeRootSemitoneSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    relativeRootSemitoneSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    relativeRootSemitoneSlider.setComponentID ("relativeRootSemitoneSlider");
    relativeRootSemitoneSlider.setRange (-127.0, 127.0, 1.0);
    relativeRootSemitoneSlider.setNumDecimalPlacesToDisplay (0);
    relativeRootSemitoneSlider.setTextValueSuffix (" st");
    relativeRootSemitoneSlider.onValueChange = [this] { handleRelativeRootSemitoneChange(); };
    workspaceContainer.addAndMakeVisible (relativeRootSemitoneSlider);

    rootNoteFeedbackLabel.setJustificationType (juce::Justification::centredRight);
    workspaceContainer.addAndMakeVisible (rootNoteFeedbackLabel);

    auto setLabelTextBlack = [] (juce::Label& label)
    {
        label.setColour (juce::Label::textColourId, juce::Colours::black);
    };
    auto setGroupTextBlack = [] (juce::GroupComponent& group)
    {
        group.setColour (juce::GroupComponent::textColourId, juce::Colours::black);
    };

    for (auto* label : { &titleLabel,
                         &globalPanelPlaceholderLabel,
                         &presetLabel,
                         &libraryMarketplaceLoginStatusLabel,
                         &inspectorTitleLabel,
                         &densityModeLabel,
                         &emptyInspectorLabel,
                         &waveformLabel,
                         &maxVoicesLabel,
                         &stealPolicyLabel,
                         &attackLabel,
                         &decayLabel,
                         &sustainLabel,
                         &releaseLabel,
                         &modDepthLabel,
                         &modRateLabel,
                         &velocitySensitivityLabel,
                         &modDestinationLabel,
                         &unisonVoicesLabel,
                         &unisonDetuneCentsLabel,
                         &outputStageLabel,
                         &absoluteRootNoteLabel,
                         &relativeRootSemitoneLabel,
                         &rootNoteFeedbackLabel })
        setLabelTextBlack (*label);

    setGroupTextBlack (globalPanel);
    setGroupTextBlack (libraryMarketplacePanel);

    for (auto* slider : { &maxVoicesSlider,
                          &attackSlider,
                          &decaySlider,
                          &sustainSlider,
                          &releaseSlider,
                          &modDepthSlider,
                          &modRateSlider,
                          &velocitySensitivitySlider,
                          &unisonVoicesSlider,
                          &unisonDetuneCentsSlider,
                          &absoluteRootNoteSlider,
                          &relativeRootSemitoneSlider })
    {
        slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::black);
        slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::black);
    }

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
    adsrGraphPanel.setEnvelopeChangedCallback ([this] (float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds)
    {
        if (suppressInspectorCallbacks)
            return;

        suppressInspectorCallbacks = true;
        attackSlider.setValue (attackSeconds, juce::sendNotificationSync);
        decaySlider.setValue (decaySeconds, juce::sendNotificationSync);
        sustainSlider.setValue (sustainLevel, juce::sendNotificationSync);
        releaseSlider.setValue (releaseSeconds, juce::sendNotificationSync);
        suppressInspectorCallbacks = false;
        processorRef.setLayerAdsrByVisualIndex (selectedLayerIndex, attackSeconds, decaySeconds, sustainLevel, releaseSeconds);
    });
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
        sidebarContainer.addAndMakeVisible (row);
    }

    setResizable (true, false);
    setResizeLimits (760, 620, 1180, 920);
    setSize (920, 700);

    syncLayerListFromProcessor();
    syncInspectorControlsFromSelectedLayer();
    syncRootNoteControlsFromProcessor();
    refreshPresetControls();
    densityModeSelector.setSelectedItemIndex (processorRef.getUiDensityMode() == PolySynthAudioProcessor::UiDensityMode::advanced ? 1 : 0,
                                              juce::dontSendNotification);
    usingAdvancedDensity = processorRef.getUiDensityMode() == PolySynthAudioProcessor::UiDensityMode::advanced;
    voiceAdvancedPanelToggle.setToggleState (processorRef.getVoiceAdvancedPanelExpanded(), juce::dontSendNotification);
    outputAdvancedPanelToggle.setToggleState (processorRef.getOutputAdvancedPanelExpanded(), juce::dontSendNotification);
    refreshDensityUiState();
    updateInspectorBindingState();
    startTimerHz (15);
}

PolySynthAudioProcessorEditor::~PolySynthAudioProcessorEditor()
{
}

bool PolySynthAudioProcessorEditor::setCapturePageByName (juce::StringRef pageName, juce::String& errorMessage)
{
    const auto normalized = juce::String (pageName).trim().toLowerCase();
    if (normalized == "main")
    {
        showMainPage();
        return true;
    }

    if (normalized == "library")
    {
        showLibraryPage();
        return true;
    }

    errorMessage = "Unsupported page: " + normalized;
    return false;
}

bool PolySynthAudioProcessorEditor::setDensityModeByName (juce::StringRef densityMode, juce::String& errorMessage)
{
    const auto normalized = juce::String (densityMode).trim().toLowerCase();
    if (normalized == "basic")
    {
        processorRef.setUiDensityMode (PolySynthAudioProcessor::UiDensityMode::basic);
    }
    else if (normalized == "advanced")
    {
        processorRef.setUiDensityMode (PolySynthAudioProcessor::UiDensityMode::advanced);
    }
    else
    {
        errorMessage = "Unsupported density mode: " + normalized;
        return false;
    }

    syncUiForCapture();
    return true;
}

void PolySynthAudioProcessorEditor::setVoiceAdvancedPanelExpandedForCapture (bool shouldExpand)
{
    processorRef.setVoiceAdvancedPanelExpanded (shouldExpand);
    syncUiForCapture();
}

void PolySynthAudioProcessorEditor::setOutputAdvancedPanelExpandedForCapture (bool shouldExpand)
{
    processorRef.setOutputAdvancedPanelExpanded (shouldExpand);
    syncUiForCapture();
}

void PolySynthAudioProcessorEditor::setGlobalPanelVisibleForCapture (bool shouldShow)
{
    globalPanelToggle.setToggleState (shouldShow, juce::dontSendNotification);
    globalPanel.setVisible (shouldShow);
    globalPanelPlaceholderLabel.setVisible (shouldShow);
    resized();
}

bool PolySynthAudioProcessorEditor::selectLayerByVisualIndexForCapture (std::size_t layerIndex)
{
    return processorRef.selectLayerByVisualIndex (layerIndex);
}

bool PolySynthAudioProcessorEditor::loadPresetForCapture (juce::StringRef presetName, juce::String& errorMessage)
{
    const auto result = processorRef.loadPresetByName (juce::String (presetName).trim());
    if (! result.success)
    {
        errorMessage = result.message;
        return false;
    }

    syncUiForCapture();
    return true;
}

bool PolySynthAudioProcessorEditor::setControlValueByComponentIdForCapture (juce::StringRef componentId, const juce::var& value, juce::String& errorMessage)
{
    if (auto* component = findChildWithID (componentId))
    {
        if (auto* slider = dynamic_cast<juce::Slider*> (component))
        {
            if (! value.isDouble() && ! value.isInt() && ! value.isInt64())
            {
                errorMessage = "Component " + component->getComponentID() + " expects numeric value";
                return false;
            }

            slider->setValue (static_cast<double> (value), juce::sendNotificationSync);
            return true;
        }

        if (auto* comboBox = dynamic_cast<juce::ComboBox*> (component))
        {
            if (value.isString())
            {
                int matchIndex = -1;
                for (int itemIndex = 0; itemIndex < comboBox->getNumItems(); ++itemIndex)
                {
                    if (comboBox->getItemText (itemIndex) == value.toString())
                    {
                        matchIndex = itemIndex;
                        break;
                    }
                }

                if (matchIndex < 0)
                {
                    errorMessage = "Unknown combo item text for " + component->getComponentID() + ": " + value.toString();
                    return false;
                }

                comboBox->setSelectedItemIndex (matchIndex, juce::sendNotificationSync);
                return true;
            }

            if (value.isInt() || value.isInt64() || value.isDouble())
            {
                comboBox->setSelectedItemIndex (juce::roundToInt (static_cast<double> (value)), juce::sendNotificationSync);
                return true;
            }
        }

        if (auto* toggleButton = dynamic_cast<juce::ToggleButton*> (component))
        {
            if (! value.isBool())
            {
                errorMessage = "Component " + component->getComponentID() + " expects boolean value";
                return false;
            }

            toggleButton->setToggleState (static_cast<bool> (value), juce::sendNotificationSync);
            return true;
        }

        errorMessage = "Unsupported control type for component id: " + component->getComponentID();
        return false;
    }

    errorMessage = "Unknown component id: " + juce::String (componentId);
    return false;
}

void PolySynthAudioProcessorEditor::syncUiForCapture()
{
    syncLayerListFromProcessor();
    syncInspectorControlsFromSelectedLayer();
    syncRootNoteControlsFromProcessor();
    refreshPresetControls();
    refreshDensityUiState();
    updateInspectorBindingState();
    resized();
}

void PolySynthAudioProcessorEditor::showMainPage()
{
    currentPage = EditorPage::main;
    titleLabel.setVisible (true);
    globalPanelToggle.setVisible (true);
    const auto showGlobalPanel = globalPanelToggle.getToggleState();
    globalPanel.setVisible (showGlobalPanel);
    globalPanelPlaceholderLabel.setVisible (showGlobalPanel);
    sidebarContainer.setVisible (true);
    sidebarContainer.setEnabled (true);
    workspaceContainer.setVisible (true);
    workspaceContainer.setEnabled (true);
    libraryPageContainer.setVisible (false);
    libraryPageContainer.setEnabled (false);
    resized();
}

void PolySynthAudioProcessorEditor::showLibraryPage()
{
    currentPage = EditorPage::library;
    titleLabel.setVisible (false);
    globalPanelToggle.setVisible (false);
    globalPanel.setVisible (false);
    globalPanelPlaceholderLabel.setVisible (false);
    sidebarContainer.setVisible (false);
    sidebarContainer.setEnabled (false);
    workspaceContainer.setVisible (false);
    workspaceContainer.setEnabled (false);
    libraryPageContainer.setVisible (true);
    libraryPageContainer.setEnabled (true);
    resized();
}

//==============================================================================
void PolySynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PolySynthAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (LayoutTokens::outerPadding);
    const auto isMainPage = currentPage == EditorPage::main;

    titleLabel.setBounds ({});
    globalPanelToggle.setBounds ({});
    globalPanel.setBounds ({});
    globalPanelPlaceholderLabel.setBounds ({});
    titleLabel.setVisible (false);
    globalPanelToggle.setVisible (false);
    globalPanel.setVisible (false);
    globalPanelPlaceholderLabel.setVisible (false);

    if (isMainPage)
    {
        // Main page starts immediately after the outer padding.
    }
    else
    {
        // Library page keeps these header/global controls hidden.
    }

    auto mainArea = bounds;
    auto sidebarBounds = juce::Rectangle<int> {};
    auto workspaceBounds = juce::Rectangle<int> {};

    if (isMainPage)
    {
        constexpr float targetSidebarRatio = 0.33f;
        constexpr int sidebarMinWidth = 320;
        constexpr int sidebarMaxWidth = 380;
        constexpr int workspaceMinWidth = 300;
        const auto targetSidebarWidth = juce::roundToInt (static_cast<float> (mainArea.getWidth()) * targetSidebarRatio);
        const auto desiredSidebarWidth = juce::jlimit (sidebarMinWidth, sidebarMaxWidth, targetSidebarWidth);
        const auto maxSidebarWidthForWindow = juce::jmax (sidebarMinWidth, mainArea.getWidth() - workspaceMinWidth);
        const auto resolvedSidebarWidth = juce::jmin (desiredSidebarWidth, maxSidebarWidthForWindow);
        sidebarBounds = mainArea.removeFromLeft (resolvedSidebarWidth);
        mainArea.removeFromLeft (LayoutTokens::majorPaneGap);
        workspaceBounds = mainArea;
    }

    sidebarContainer.setBounds (isMainPage ? sidebarBounds : juce::Rectangle<int> {});
    workspaceContainer.setBounds (isMainPage ? workspaceBounds : juce::Rectangle<int> {});
    libraryPageContainer.setBounds (! isMainPage ? bounds : juce::Rectangle<int> {});

    if (! isMainPage)
    {
        auto libraryBounds = libraryPageContainer.getLocalBounds().reduced (LayoutTokens::majorPaneGap, 16);
        backToSynthPageButton.setBounds (libraryBounds.removeFromTop (30).removeFromLeft (160));
        libraryBounds.removeFromTop (LayoutTokens::majorPaneGap);
        libraryMarketplacePanel.setBounds (libraryBounds);

        auto marketplaceContent = libraryBounds.reduced (8, 28);
        auto marketplaceButtonsRow = marketplaceContent.removeFromTop (28);
        libraryMarketplaceBrowseButton.setBounds (marketplaceButtonsRow.removeFromLeft (90).reduced (1, 0));
        libraryMarketplaceUploadButton.setBounds (marketplaceButtonsRow.removeFromLeft (90).reduced (1, 0));
        libraryMarketplaceSyncButton.setBounds (marketplaceButtonsRow.removeFromLeft (90).reduced (1, 0));
        marketplaceContent.removeFromTop (LayoutTokens::controlGap);
        libraryMarketplaceLoginStatusLabel.setBounds (marketplaceContent.removeFromTop (22));
        return;
    }

    auto sidebarLayout = sidebarContainer.getLocalBounds();

    auto sidebarContent = sidebarLayout.reduced (LayoutTokens::majorPaneGap, SidebarLayoutTokens::panelVerticalInset);

    const auto availableSidebarHeight = sidebarContent.getHeight();
    const auto minimumSidebarContentHeight = SidebarLayoutTokens::minimumPresetBlockHeight
                                           + LayoutTokens::sidebarPaneGapThickness
                                           + SidebarLayoutTokens::minimumLayerBlockHeight;
    const auto sidebarExtraHeight = juce::jmax (0, availableSidebarHeight - minimumSidebarContentHeight);
    const auto presetBlockHeight = SidebarLayoutTokens::minimumPresetBlockHeight;
    const auto layerBlockHeight = SidebarLayoutTokens::minimumLayerBlockHeight + sidebarExtraHeight;

    auto presetBounds = sidebarContent.removeFromTop (juce::jmin (presetBlockHeight, sidebarContent.getHeight()));
    presetBrowserPane.setBounds (presetBounds);
    auto presetContent = presetBounds.reduced (SidebarLayoutTokens::panelHorizontalInset, SidebarLayoutTokens::panelVerticalInset);
    auto presetRow = presetContent.removeFromTop (SidebarLayoutTokens::compactControlRowHeight);
    presetLabel.setBounds (presetRow.removeFromLeft (SidebarLayoutTokens::presetLabelWidth));
    presetSelector.setBounds (presetRow);
    presetContent.removeFromTop (SidebarLayoutTokens::presetSelectorGap);
    auto presetButtonsRow = presetContent.removeFromTop (SidebarLayoutTokens::compactControlRowHeight);
    presetLoadButton.setBounds (presetButtonsRow.removeFromLeft (56).reduced (1, 0));
    presetSaveButton.setBounds (presetButtonsRow.removeFromLeft (56).reduced (1, 0));
    presetSaveAsNewButton.setBounds (presetButtonsRow.reduced (1, 0));
    presetContent.removeFromTop (SidebarLayoutTokens::presetButtonsToLibraryGap);
    viewLibraryButton.setBounds (presetContent.removeFromTop (SidebarLayoutTokens::compactControlRowHeight));
    presetContent.removeFromTop (LayoutTokens::controlGap);
    presetStatusLabel.setBounds (presetContent);

    auto sidebarGapBounds = sidebarContent.removeFromTop (LayoutTokens::sidebarPaneGapThickness);
    sidebarGapBounds.setX (sidebarLayout.getX());
    sidebarGapBounds.setWidth (sidebarLayout.getWidth());
    sidebarPaneGap.setBounds (sidebarGapBounds);
    layersPane.setBounds (sidebarContent.removeFromTop (juce::jmin (layerBlockHeight, sidebarContent.getHeight())));

    auto rowArea = layersPane.getBounds().reduced (SidebarLayoutTokens::panelHorizontalInset, SidebarLayoutTokens::panelVerticalInset);
    actionStatusLabel.setBounds (rowArea.removeFromTop (SidebarLayoutTokens::actionStatusRowHeight));
    rowArea.removeFromTop (SidebarLayoutTokens::layerStatusGap);
    for (std::size_t i = 0; i < layerRows.size(); ++i)
    {
        auto& row = layerRows[i];
        if (i < visibleLayerCount)
            row.setBounds (rowArea.removeFromTop (SidebarLayoutTokens::layerRowHeight));
        else
            row.setBounds ({});
    }
    rowArea.removeFromTop (SidebarLayoutTokens::layerHeaderGap);
    addLayerButton.setBounds (rowArea.removeFromTop (SidebarLayoutTokens::compactControlRowHeight));

    auto content = workspaceContainer.getLocalBounds().reduced (12, 0);
    auto inspectorHeader = content.removeFromTop (24);
    densityModeSelector.setBounds (inspectorHeader.removeFromRight (160).reduced (2, 0));
    densityModeLabel.setBounds (inspectorHeader.removeFromRight (82));
    inspectorTitleLabel.setBounds (inspectorHeader);
    content.removeFromTop (LayoutTokens::controlGap);

    auto inspectorGrid = content;
    constexpr int minColumnWidth = 230;
    const auto maxColumnCount = juce::jlimit (1, 3, inspectorGrid.getWidth() / minColumnWidth);
    const auto activeColumnCount = juce::jmax (1, maxColumnCount);
    const auto totalColumnGap = LayoutTokens::rowSpacing * (activeColumnCount - 1);
    const auto columnWidth = (inspectorGrid.getWidth() - totalColumnGap) / activeColumnCount;

    std::array<juce::Rectangle<int>, 3> columnBounds {};
    for (int columnIndex = 0; columnIndex < activeColumnCount; ++columnIndex)
    {
        auto nextColumn = inspectorGrid.removeFromLeft (columnWidth);
        if (columnIndex + 1 < activeColumnCount)
            inspectorGrid.removeFromLeft (LayoutTokens::rowSpacing);
        columnBounds[static_cast<std::size_t> (columnIndex)] = nextColumn;
    }

    using SectionRef = std::reference_wrapper<SectionPanel>;
    const auto sectionSets = usingAdvancedDensity
                                 ? std::array<std::array<SectionRef, 2>, 3> { std::array<SectionRef, 2> { oscillatorSection, voiceUnisonSection },
                                                                              std::array<SectionRef, 2> { envelopeSection, modulationSection },
                                                                              std::array<SectionRef, 2> { outputSection, tuningAdvancedSection } }
                                 : std::array<std::array<SectionRef, 2>, 3> { std::array<SectionRef, 2> { oscillatorSection, envelopeSection },
                                                                              std::array<SectionRef, 2> { modulationSection, outputSection },
                                                                              std::array<SectionRef, 2> { voiceUnisonSection, tuningAdvancedSection } };

    auto clearSection = [] (SectionPanel& panel) { panel.setBounds ({}); };
    for (auto& sectionSet : sectionSets)
        for (auto& section : sectionSet)
            clearSection (section.get());

    for (std::size_t setIndex = 0; setIndex < sectionSets.size(); ++setIndex)
    {
        const auto columnIndex = static_cast<int> (setIndex % static_cast<std::size_t> (activeColumnCount));
        auto& column = columnBounds[static_cast<std::size_t> (columnIndex)];
        if (column.isEmpty())
            continue;

        const auto availableHeight = column.getHeight();
        const auto sectionHeight = (availableHeight - LayoutTokens::rowSpacing) / 2;
        for (std::size_t sectionIndex = 0; sectionIndex < sectionSets[setIndex].size(); ++sectionIndex)
        {
            auto& section = sectionSets[setIndex][sectionIndex];
            section.get().setBounds (column.removeFromTop (sectionHeight));
            if (sectionIndex + 1 < sectionSets[setIndex].size())
                column.removeFromTop (LayoutTokens::rowSpacing);
        }
    }

    auto placeKnob = [] (juce::Rectangle<int> area, juce::Label& label, juce::Slider& slider, bool primary)
    {
        constexpr int primaryKnobSize = 84;
        constexpr int secondaryKnobSize = 68;
        const auto knobSize = primary ? primaryKnobSize : secondaryKnobSize;
        auto centered = area.withSizeKeepingCentre (knobSize, knobSize + 34);
        slider.setBounds (centered.removeFromTop (knobSize));
        label.setBounds (centered.removeFromTop (18));
    };
    auto placeAdsrKnob = [] (juce::Rectangle<int> area, juce::Label& label, juce::Slider& slider)
    {
        constexpr int knobSize = 62;
        auto centered = area.reduced (4, 0).withSizeKeepingCentre (knobSize + 10, 84);
        label.setBounds (centered.removeFromTop (16));
        centered.removeFromTop (2);
        slider.setBounds (centered.removeFromTop (66));
    };

    auto oscContent = mapSectionLocalBoundsToEditor (workspaceContainer, oscillatorSection, oscillatorSection.getContentBounds());
    auto oscTop = oscContent.removeFromTop (56);
    waveformLabel.setBounds (oscTop.removeFromTop (14));
    waveformSelector.setBounds (oscTop.reduced (4, 0));
    oscContent.removeFromTop (LayoutTokens::controlGap);
    waveformDisplayPanel.setBounds (oscContent.removeFromTop (88).reduced (4, 2));

    auto voiceContent = mapSectionLocalBoundsToEditor (workspaceContainer, voiceUnisonSection, voiceUnisonSection.getContentBounds())
                            .reduced (LayoutTokens::controlGap, 0);
    voiceAdvancedPanelToggle.setBounds (voiceContent.removeFromTop (26));
    voiceContent.removeFromTop (LayoutTokens::controlGap);
    if (usingAdvancedDensity && voiceAdvancedPanelToggle.getToggleState())
    {
        auto voiceTop = voiceContent.removeFromTop (34);
        stealPolicyLabel.setBounds (voiceTop.removeFromTop (14));
        stealPolicySelector.setBounds (voiceTop.reduced (4, 0));
        voiceContent.removeFromTop (LayoutTokens::controlGap);
        auto voiceUpperKnobs = voiceContent.removeFromTop (voiceContent.getHeight() / 2);
        auto voiceLowerKnobs = voiceContent;
        placeKnob (voiceUpperKnobs.removeFromLeft (voiceUpperKnobs.getWidth() / 2), maxVoicesLabel, maxVoicesSlider, false);
        placeKnob (voiceUpperKnobs, unisonVoicesLabel, unisonVoicesSlider, false);
        placeKnob (voiceLowerKnobs.removeFromLeft (voiceLowerKnobs.getWidth() / 2), unisonDetuneCentsLabel, unisonDetuneCentsSlider, true);
        placeKnob (voiceLowerKnobs, absoluteRootNoteLabel, absoluteRootNoteSlider, false);
    }

    auto envContent = mapSectionLocalBoundsToEditor (workspaceContainer, envelopeSection, envelopeSection.getContentBounds());
    adsrGraphPanel.setBounds (envContent.removeFromTop (92));
    envContent.removeFromTop (LayoutTokens::controlGap);
    auto envKnobRow = envContent.removeFromTop (juce::jmin (92, envContent.getHeight()));
    const auto knobColumnWidth = envKnobRow.getWidth() / 4;
    placeAdsrKnob (envKnobRow.removeFromLeft (knobColumnWidth), attackLabel, attackSlider);
    placeAdsrKnob (envKnobRow.removeFromLeft (knobColumnWidth), decayLabel, decaySlider);
    placeAdsrKnob (envKnobRow.removeFromLeft (knobColumnWidth), sustainLabel, sustainSlider);
    placeAdsrKnob (envKnobRow, releaseLabel, releaseSlider);

    auto modContent = mapSectionLocalBoundsToEditor (workspaceContainer, modulationSection, modulationSection.getContentBounds());
    auto modDestinationArea = modContent.removeFromTop (52).reduced (4, 0);
    modDestinationLabel.setBounds (modDestinationArea.removeFromTop (14));
    modDestinationSelector.setBounds (modDestinationArea);
    modContent.removeFromTop (LayoutTokens::controlGap);

    auto velocityArea = modContent.removeFromTop (108).reduced (4, 0);
    velocitySensitivityLabel.setBounds (velocityArea.removeFromTop (16));
    velocityArea.removeFromTop (2);
    velocitySensitivitySlider.setBounds (velocityArea.withSizeKeepingCentre (72, 90));
    modContent.removeFromTop (LayoutTokens::controlGap);

    auto modKnobs = modContent.removeFromTop (juce::jmin (120, modContent.getHeight())).reduced (6, 0);
    placeKnob (modKnobs.removeFromLeft (modKnobs.getWidth() / 2), modRateLabel, modRateSlider, true);
    placeKnob (modKnobs, modDepthLabel, modDepthSlider, true);

    auto outContent = mapSectionLocalBoundsToEditor (workspaceContainer, outputSection, outputSection.getContentBounds());
    outputAdvancedPanelToggle.setBounds (outContent.removeFromTop (26));
    outContent.removeFromTop (LayoutTokens::controlGap);
    if (usingAdvancedDensity && outputAdvancedPanelToggle.getToggleState())
    {
        auto outputRow = outContent.removeFromTop (34);
        outputStageLabel.setBounds (outputRow.removeFromTop (14));
        outputStageSelector.setBounds (outputRow.reduced (4, 0));
    }

    auto tuneContent = mapSectionLocalBoundsToEditor (workspaceContainer, tuningAdvancedSection, tuningAdvancedSection.getContentBounds())
                           .reduced (LayoutTokens::controlGap, 0);
    if (usingAdvancedDensity && outputAdvancedPanelToggle.getToggleState())
    {
        auto tuneArea = tuneContent.removeFromTop (132);
        placeKnob (tuneArea, relativeRootSemitoneLabel, relativeRootSemitoneSlider, false);
        rootNoteFeedbackLabel.setBounds (tuneContent.removeFromTop (26));
    }
    emptyInspectorLabel.setBounds (workspaceContainer.getLocalBounds().reduced (12, 24));
}

void PolySynthAudioProcessorEditor::timerCallback()
{
    const auto latestMode = processorRef.getUiDensityMode();
    const auto latestAdvanced = latestMode == PolySynthAudioProcessor::UiDensityMode::advanced;
    if (latestAdvanced != usingAdvancedDensity)
    {
        densityModeSelector.setSelectedItemIndex (latestAdvanced ? 1 : 0, juce::dontSendNotification);
        usingAdvancedDensity = latestAdvanced;
        refreshDensityUiState();
        resized();
    }

    if (voiceAdvancedPanelToggle.getToggleState() != processorRef.getVoiceAdvancedPanelExpanded())
    {
        voiceAdvancedPanelToggle.setToggleState (processorRef.getVoiceAdvancedPanelExpanded(), juce::dontSendNotification);
        refreshDensityUiState();
        resized();
    }

    if (outputAdvancedPanelToggle.getToggleState() != processorRef.getOutputAdvancedPanelExpanded())
    {
        outputAdvancedPanelToggle.setToggleState (processorRef.getOutputAdvancedPanelExpanded(), juce::dontSendNotification);
        refreshDensityUiState();
        resized();
    }

    syncLayerListFromProcessor();
    syncInspectorControlsFromSelectedLayer();
    syncRootNoteControlsFromProcessor();
    refreshPresetControls();

    const auto noteIsActive = processorRef.isLayerNoteActive (selectedLayerIndex);
    if (noteIsActive)
    {
        waveformAnimationPhase += 0.022f;
        adsrAnimationProgress += 0.03f;
        if (adsrAnimationProgress > 1.0f)
            adsrAnimationProgress -= 1.0f;
    }
    else
    {
        adsrAnimationProgress = 0.0f;
    }

    waveformDisplayPanel.setAnimatedMode (noteIsActive, waveformAnimationPhase);
    adsrGraphPanel.setAnimatedMode (noteIsActive, adsrAnimationProgress);
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

    adsrGraphPanel.setEnvelope (selectedLayerState->attackSeconds,
                                selectedLayerState->decaySeconds,
                                selectedLayerState->sustainLevel,
                                selectedLayerState->releaseSeconds);

    std::vector<SynthVoice::Waveform> waveforms;
    waveforms.push_back (selectedLayerState->waveform);
    const auto allWaveforms = processorRef.getAllLayerWaveforms();
    for (std::size_t i = 0; i < allWaveforms.size(); ++i)
    {
        if (i == selectedLayerIndex)
            continue;
        waveforms.push_back (allWaveforms[i]);
    }
    waveformDisplayPanel.setLayerWaveforms (waveforms);
}

void PolySynthAudioProcessorEditor::refreshDensityUiState()
{
    usingAdvancedDensity = processorRef.getUiDensityMode() == PolySynthAudioProcessor::UiDensityMode::advanced;
    const auto showAdvancedContent = usingAdvancedDensity;
    const auto showVoicePanelContent = showAdvancedContent && voiceAdvancedPanelToggle.getToggleState();
    const auto showOutputPanelContent = showAdvancedContent && outputAdvancedPanelToggle.getToggleState();

    voiceUnisonSection.setTitle (showAdvancedContent ? "Voice / Unison" : "Advanced (Voice / Unison)");
    outputSection.setTitle (showAdvancedContent ? "Output" : "Advanced (Output)");
    tuningAdvancedSection.setTitle (showAdvancedContent ? "Tuning / Advanced" : "Advanced (Tuning)");

    voiceUnisonSection.setVisible (showAdvancedContent);
    outputSection.setVisible (showAdvancedContent);
    tuningAdvancedSection.setVisible (showAdvancedContent);
    voiceAdvancedPanelToggle.setVisible (showAdvancedContent);
    outputAdvancedPanelToggle.setVisible (showAdvancedContent);

    for (auto* component : { static_cast<juce::Component*> (&stealPolicyLabel),
                             static_cast<juce::Component*> (&stealPolicySelector),
                             static_cast<juce::Component*> (&maxVoicesLabel),
                             static_cast<juce::Component*> (&maxVoicesSlider),
                             static_cast<juce::Component*> (&unisonVoicesLabel),
                             static_cast<juce::Component*> (&unisonVoicesSlider),
                             static_cast<juce::Component*> (&unisonDetuneCentsLabel),
                             static_cast<juce::Component*> (&unisonDetuneCentsSlider),
                             static_cast<juce::Component*> (&absoluteRootNoteLabel),
                             static_cast<juce::Component*> (&absoluteRootNoteSlider) })
        component->setVisible (showVoicePanelContent);

    for (auto* component : { static_cast<juce::Component*> (&outputStageLabel),
                             static_cast<juce::Component*> (&outputStageSelector),
                             static_cast<juce::Component*> (&relativeRootSemitoneLabel),
                             static_cast<juce::Component*> (&relativeRootSemitoneSlider),
                             static_cast<juce::Component*> (&rootNoteFeedbackLabel) })
        component->setVisible (showOutputPanelContent);
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
                             static_cast<juce::Component*> (&rootNoteFeedbackLabel),
                             static_cast<juce::Component*> (&waveformDisplayPanel),
                             static_cast<juce::Component*> (&adsrGraphPanel) })
    {
        component->setEnabled (hasValidSelection);
    }

    voiceAdvancedPanelToggle.setEnabled (hasValidSelection);
    outputAdvancedPanelToggle.setEnabled (hasValidSelection);
    densityModeSelector.setEnabled (true);
    refreshDensityUiState();
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
