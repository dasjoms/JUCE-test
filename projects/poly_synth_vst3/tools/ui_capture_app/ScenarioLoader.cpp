#include "ScenarioLoader.h"

namespace
{
std::optional<CaptureCrop> parseCrop (const juce::var& cropValue, juce::String& errorMessage)
{
    const auto* cropObject = cropValue.getDynamicObject();
    if (cropObject == nullptr)
    {
        errorMessage = "Scenario field 'crop' must be an object with x,y,width,height.";
        return std::nullopt;
    }

    const auto x = cropObject->getProperty ("x");
    const auto y = cropObject->getProperty ("y");
    const auto width = cropObject->getProperty ("width");
    const auto height = cropObject->getProperty ("height");

    if ((! x.isInt() && ! x.isInt64())
        || (! y.isInt() && ! y.isInt64())
        || (! width.isInt() && ! width.isInt64())
        || (! height.isInt() && ! height.isInt64()))
    {
        errorMessage = "Scenario field 'crop' requires integer x,y,width,height values.";
        return std::nullopt;
    }

    CaptureCrop crop;
    crop.x = static_cast<int> (x);
    crop.y = static_cast<int> (y);
    crop.width = static_cast<int> (width);
    crop.height = static_cast<int> (height);

    if (crop.width <= 0 || crop.height <= 0)
    {
        errorMessage = "Scenario field 'crop' width/height must be positive.";
        return std::nullopt;
    }

    return crop;
}

bool readNamedValueObject (const juce::var& value,
                           juce::NamedValueSet& destination,
                           const juce::String& fieldName,
                           juce::String& errorMessage)
{
    if (value.isVoid())
        return true;

    const auto* object = value.getDynamicObject();
    if (object == nullptr)
    {
        errorMessage = "Scenario field '" + fieldName + "' must be a JSON object.";
        return false;
    }

    for (const auto& property : object->getProperties())
        destination.set (property.name, property.value);

    return true;
}
} // namespace

std::optional<CaptureScenario> ScenarioLoader::loadFromFile (const juce::File& scenarioFile, juce::String& errorMessage)
{
    if (! scenarioFile.existsAsFile())
    {
        errorMessage = "Scenario file does not exist: " + scenarioFile.getFullPathName();
        return std::nullopt;
    }

    const auto parsed = juce::JSON::parse (scenarioFile);
    if (parsed.isVoid())
    {
        errorMessage = "Failed to parse JSON scenario file: " + scenarioFile.getFullPathName();
        return std::nullopt;
    }

    const auto* root = parsed.getDynamicObject();
    if (root == nullptr)
    {
        errorMessage = "Scenario root must be a JSON object.";
        return std::nullopt;
    }

    CaptureScenario scenario;
    scenario.sourceFile = scenarioFile;

    if (const auto width = root->getProperty ("editorWidth"); width.isInt() || width.isInt64())
        scenario.editorWidth = static_cast<int> (width);

    if (const auto height = root->getProperty ("editorHeight"); height.isInt() || height.isInt64())
        scenario.editorHeight = static_cast<int> (height);

    if (const auto scale = root->getProperty ("scaleFactor"); scale.isDouble() || scale.isInt() || scale.isInt64())
        scenario.scaleFactor = static_cast<double> (scale);

    if (const auto page = root->getProperty ("page"); page.isString())
        scenario.page = page.toString();

    if (const auto showGlobal = root->getProperty ("showGlobalPanel"); showGlobal.isBool())
        scenario.showGlobalPanel = static_cast<bool> (showGlobal);

    if (const auto voiceExpanded = root->getProperty ("voicePanelExpanded"); voiceExpanded.isBool())
        scenario.voicePanelExpanded = static_cast<bool> (voiceExpanded);

    if (const auto outputExpanded = root->getProperty ("outputPanelExpanded"); outputExpanded.isBool())
        scenario.outputPanelExpanded = static_cast<bool> (outputExpanded);

    if (const auto density = root->getProperty ("densityMode"); density.isString())
        scenario.densityMode = density.toString();

    if (const auto selectedLayer = root->getProperty ("selectedLayer"); selectedLayer.isInt() || selectedLayer.isInt64())
        scenario.selectedLayer = static_cast<int> (selectedLayer);

    if (const auto selectedPreset = root->getProperty ("selectedPreset"); selectedPreset.isString())
        scenario.selectedPreset = selectedPreset.toString();

    if (const auto cropValue = root->getProperty ("crop"); ! cropValue.isVoid())
    {
        auto crop = parseCrop (cropValue, errorMessage);
        if (! crop.has_value())
            return std::nullopt;

        scenario.crop = crop;
    }

    if (! readNamedValueObject (root->getProperty ("parameters"), scenario.parameterValues, "parameters", errorMessage))
        return std::nullopt;

    if (! readNamedValueObject (root->getProperty ("controls"), scenario.controlValues, "controls", errorMessage))
        return std::nullopt;

    if (scenario.editorWidth <= 0 || scenario.editorHeight <= 0)
    {
        errorMessage = "Scenario editorWidth/editorHeight must be positive integers.";
        return std::nullopt;
    }

    if (scenario.scaleFactor <= 0.0)
    {
        errorMessage = "Scenario scaleFactor must be > 0.";
        return std::nullopt;
    }

    return scenario;
}
