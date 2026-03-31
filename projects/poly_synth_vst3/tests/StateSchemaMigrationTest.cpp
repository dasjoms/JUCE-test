#include "../MonoSynthAudioProcessor.h"

#include <iostream>

namespace
{
constexpr auto waveformParameterId = "waveform";
constexpr auto maxVoicesParameterId = "maxVoices";
constexpr auto attackParameterId = "attack";
constexpr auto schemaVersionPropertyId = "schemaVersion";
constexpr int currentStateSchemaVersion = 1;

float getRawParameterValue (MonoSynthAudioProcessor& processor, juce::StringRef parameterId)
{
    if (auto* raw = processor.getValueTreeState().getRawParameterValue (parameterId))
        return raw->load();

    return -1.0f;
}

bool setRawParameterValue (MonoSynthAudioProcessor& processor, juce::StringRef parameterId, float rawValue)
{
    if (auto* parameter = processor.getValueTreeState().getParameter (parameterId))
    {
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (rawValue));
        return true;
    }

    return false;
}

bool validateLegacyMonoStateMigrationPreservesWaveform()
{
    MonoSynthAudioProcessor processor;

    auto legacyXml = juce::parseXML (R"xml(
<PARAMETERS>
  <PARAM id="waveform" value="3"/>
</PARAMETERS>)xml");

    if (legacyXml == nullptr)
    {
        std::cerr << "unable to parse legacy mono state fixture" << '\n';
        return false;
    }

    juce::MemoryBlock serializedLegacyState;
    MonoSynthAudioProcessor::copyXmlToBinary (*legacyXml, serializedLegacyState);

    processor.setStateInformation (serializedLegacyState.getData(), static_cast<int> (serializedLegacyState.getSize()));

    const auto waveform = getRawParameterValue (processor, waveformParameterId);
    const auto maxVoices = getRawParameterValue (processor, maxVoicesParameterId);

    if (juce::roundToInt (waveform) != 3)
    {
        std::cerr << "legacy migration failed to preserve waveform value; expected 3 got " << waveform << '\n';
        return false;
    }

    if (juce::roundToInt (maxVoices) != 1)
    {
        std::cerr << "legacy migration should keep missing params at defaults; expected maxVoices 1 got " << maxVoices << '\n';
        return false;
    }

    return true;
}

bool validateCurrentVersionRoundTrip()
{
    MonoSynthAudioProcessor sourceProcessor;

    if (! setRawParameterValue (sourceProcessor, waveformParameterId, 2.0f)
        || ! setRawParameterValue (sourceProcessor, maxVoicesParameterId, 8.0f)
        || ! setRawParameterValue (sourceProcessor, attackParameterId, 0.42f))
    {
        std::cerr << "unable to set one or more parameters on source processor" << '\n';
        return false;
    }

    juce::MemoryBlock serializedState;
    sourceProcessor.getStateInformation (serializedState);

    if (auto xml = MonoSynthAudioProcessor::getXmlFromBinary (serializedState.getData(), static_cast<int> (serializedState.getSize())); xml == nullptr)
    {
        std::cerr << "unable to parse serialized current-version state" << '\n';
        return false;
    }
    else if (static_cast<int> (xml->getIntAttribute (schemaVersionPropertyId, -1)) != currentStateSchemaVersion)
    {
        std::cerr << "serialized state missing expected schema version marker" << '\n';
        return false;
    }

    MonoSynthAudioProcessor restoredProcessor;
    restoredProcessor.setStateInformation (serializedState.getData(), static_cast<int> (serializedState.getSize()));

    const auto waveform = getRawParameterValue (restoredProcessor, waveformParameterId);
    const auto maxVoices = getRawParameterValue (restoredProcessor, maxVoicesParameterId);
    const auto attack = getRawParameterValue (restoredProcessor, attackParameterId);

    if (juce::roundToInt (waveform) != 2)
    {
        std::cerr << "current-version restore mismatch for waveform; expected 2 got " << waveform << '\n';
        return false;
    }

    if (juce::roundToInt (maxVoices) != 8)
    {
        std::cerr << "current-version restore mismatch for maxVoices; expected 8 got " << maxVoices << '\n';
        return false;
    }

    if (! juce::approximatelyEqual (attack, 0.42f))
    {
        std::cerr << "current-version restore mismatch for attack; expected 0.42 got " << attack << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    if (! validateLegacyMonoStateMigrationPreservesWaveform())
        return 1;

    if (! validateCurrentVersionRoundTrip())
        return 1;

    std::cout << "state schema migration validation passed." << '\n';
    return 0;
}
