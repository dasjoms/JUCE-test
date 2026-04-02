#include "EditorStateApplier.h"

bool EditorStateApplier::applyScenario (PolySynthAudioProcessor& processor,
                                        PolySynthAudioProcessorEditor& editor,
                                        const CaptureScenario& scenario,
                                        juce::String& errorMessage)
{
    editor.setSize (scenario.editorWidth, scenario.editorHeight);

    if (! editor.setCapturePageByName (scenario.page, errorMessage))
        return false;

    if (scenario.showGlobalPanel.has_value())
        editor.setGlobalPanelVisibleForCapture (*scenario.showGlobalPanel);

    if (scenario.densityMode.has_value() && ! editor.setDensityModeByName (*scenario.densityMode, errorMessage))
        return false;

    if (scenario.voicePanelExpanded.has_value())
        editor.setVoiceAdvancedPanelExpandedForCapture (*scenario.voicePanelExpanded);

    if (scenario.outputPanelExpanded.has_value())
        editor.setOutputAdvancedPanelExpandedForCapture (*scenario.outputPanelExpanded);

    if (scenario.selectedLayer.has_value())
    {
        if (*scenario.selectedLayer < 0 || ! editor.selectLayerByVisualIndexForCapture (static_cast<std::size_t> (*scenario.selectedLayer)))
        {
            errorMessage = "Scenario selectedLayer index is out of range: " + juce::String (*scenario.selectedLayer);
            return false;
        }
    }

    if (scenario.selectedPreset.has_value() && ! editor.loadPresetForCapture (*scenario.selectedPreset, errorMessage))
        return false;

    for (const auto& entry : scenario.parameterValues)
    {
        if (! entry.value.isDouble() && ! entry.value.isInt() && ! entry.value.isInt64())
        {
            errorMessage = "Parameter '" + entry.name.toString() + "' must have a numeric value.";
            return false;
        }

        if (! processor.setParameterValueById (entry.name.toString(), static_cast<float> (entry.value), errorMessage))
            return false;
    }

    for (const auto& entry : scenario.controlValues)
    {
        if (! editor.setControlValueByComponentIdForCapture (entry.name.toString(), entry.value, errorMessage))
            return false;
    }

    editor.syncUiForCapture();
    return true;
}
