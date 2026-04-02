#pragma once

#include "ScenarioLoader.h"
#include "../../PolySynthAudioProcessor.h"
#include "../../PolySynthAudioProcessorEditor.h"

class EditorStateApplier final
{
public:
    static bool applyScenario (PolySynthAudioProcessor& processor,
                               PolySynthAudioProcessorEditor& editor,
                               const CaptureScenario& scenario,
                               juce::String& errorMessage);
};
