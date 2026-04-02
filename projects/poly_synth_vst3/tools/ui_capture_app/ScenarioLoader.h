#pragma once

#include <juce_core/juce_core.h>
#include <optional>

struct CaptureCrop
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct CaptureScenario
{
    juce::File sourceFile;
    int editorWidth = 920;
    int editorHeight = 700;
    double scaleFactor = 1.0;
    juce::String page = "main";
    std::optional<bool> showGlobalPanel;
    std::optional<bool> voicePanelExpanded;
    std::optional<bool> outputPanelExpanded;
    std::optional<juce::String> densityMode;
    std::optional<int> selectedLayer;
    std::optional<juce::String> selectedPreset;
    juce::NamedValueSet parameterValues;
    juce::NamedValueSet controlValues;
    std::optional<CaptureCrop> crop;
};

class ScenarioLoader final
{
public:
    static std::optional<CaptureScenario> loadFromFile (const juce::File& scenarioFile, juce::String& errorMessage);
};
