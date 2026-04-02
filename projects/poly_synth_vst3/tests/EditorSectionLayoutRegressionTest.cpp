#include "../PolySynthAudioProcessor.h"
#include "../PolySynthAudioProcessorEditor.h"

#include <iostream>

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';

    return condition;
}

template <typename ComponentType>
ComponentType* findById (juce::Component& parent, const char* componentId)
{
    if (auto* directMatch = dynamic_cast<ComponentType*> (&parent); directMatch != nullptr && directMatch->getComponentID() == componentId)
        return directMatch;

    if (auto* directChild = dynamic_cast<ComponentType*> (parent.findChildWithID (componentId)))
        return directChild;

    for (auto* child : parent.getChildren())
        if (auto* nestedMatch = findById<ComponentType> (*child, componentId))
            return nestedMatch;

    return nullptr;
}

bool expectControlInsideSection (PolySynthAudioProcessorEditor& editor,
                                 const char* sectionId,
                                 const char* controlId,
                                 const char* message)
{
    auto* section = findById<juce::Component> (editor, sectionId);
    auto* control = findById<juce::Component> (editor, controlId);

    if (! expect (section != nullptr, "missing section component"))
        return false;

    if (! expect (control != nullptr, "missing control component"))
        return false;

    const auto contains = section->getBounds().contains (control->getBounds());
    if (! contains)
    {
        std::cerr << message << '\n';
        std::cerr << "section(" << sectionId << "): " << section->getBounds().toString() << '\n';
        std::cerr << "control(" << controlId << "): " << control->getBounds().toString() << '\n';
    }

    return contains;
}

bool validateBasicDensityLayout()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;

    PolySynthAudioProcessor processor;
    processor.setUiDensityMode (PolySynthAudioProcessor::UiDensityMode::basic);
    processor.setVoiceAdvancedPanelExpanded (false);
    processor.setOutputAdvancedPanelExpanded (false);

    PolySynthAudioProcessorEditor editor (processor);
    editor.setSize (960, 720);
    editor.resized();

    const auto initial = expectControlInsideSection (editor, "oscillatorSection", "waveformSelector", "waveform selector escaped oscillator section")
        && expectControlInsideSection (editor, "envelopeSection", "attackSlider", "attack slider escaped envelope section")
        && expectControlInsideSection (editor, "modulationSection", "modDestinationSelector", "mod destination escaped modulation section");

    editor.setSize (1180, 760);
    editor.resized();

    const auto resized = expectControlInsideSection (editor, "oscillatorSection", "waveformSelector", "waveform selector escaped oscillator section after resize")
        && expectControlInsideSection (editor, "envelopeSection", "attackSlider", "attack slider escaped envelope section after resize")
        && expectControlInsideSection (editor, "modulationSection", "modDestinationSelector", "mod destination escaped modulation section after resize");

    return initial && resized;
}

bool validateAdvancedDensityLayout()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;

    PolySynthAudioProcessor processor;
    processor.setUiDensityMode (PolySynthAudioProcessor::UiDensityMode::advanced);
    processor.setVoiceAdvancedPanelExpanded (true);
    processor.setOutputAdvancedPanelExpanded (true);

    PolySynthAudioProcessorEditor editor (processor);
    if (auto* densitySelector = findById<juce::ComboBox> (editor, "densityModeSelector"))
        densitySelector->setSelectedItemIndex (1, juce::sendNotificationSync);
    if (auto* voiceToggle = findById<juce::ToggleButton> (editor, "voiceAdvancedPanelToggle"))
        voiceToggle->setToggleState (true, juce::sendNotificationSync);
    if (auto* outputToggle = findById<juce::ToggleButton> (editor, "outputAdvancedPanelToggle"))
        outputToggle->setToggleState (true, juce::sendNotificationSync);

    editor.setSize (1380, 740);
    editor.resized();

    const auto initial = expectControlInsideSection (editor, "voiceUnisonSection", "maxVoicesSlider", "max voices escaped voice/unison section")
        && expectControlInsideSection (editor, "outputSection", "outputStageSelector", "output stage escaped output section")
        && expectControlInsideSection (editor, "tuningAdvancedSection", "relativeRootSemitoneSlider", "relative root escaped tuning section");

    editor.setSize (1560, 820);
    editor.resized();

    const auto resized = expectControlInsideSection (editor, "voiceUnisonSection", "maxVoicesSlider", "max voices escaped voice/unison section after resize")
        && expectControlInsideSection (editor, "outputSection", "outputStageSelector", "output stage escaped output section after resize")
        && expectControlInsideSection (editor, "tuningAdvancedSection", "relativeRootSemitoneSlider", "relative root escaped tuning section after resize");

    return initial && resized;
}

} // namespace

int main()
{
    if (! validateBasicDensityLayout())
        return 1;

    if (! validateAdvancedDensityLayout())
        return 1;

    std::cout << "editor section layout regression validation passed." << '\n';
    return 0;
}
