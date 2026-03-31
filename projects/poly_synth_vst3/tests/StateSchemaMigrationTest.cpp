#include "../PolySynthAudioProcessor.h"

#include <iostream>

namespace
{
struct SchemaExpectation
{
    int currentSchemaVersion = 3;
    int futureExpansionFixtureVersion = 4;
};

constexpr SchemaExpectation expectedSchema {};

constexpr auto waveformParameterId = "waveform";
constexpr auto maxVoicesParameterId = "maxVoices";
constexpr auto stealPolicyParameterId = "stealPolicy";
constexpr auto attackParameterId = "attack";
constexpr auto decayParameterId = "decay";
constexpr auto sustainParameterId = "sustain";
constexpr auto releaseParameterId = "release";
constexpr auto modulationDepthParameterId = "modDepth";
constexpr auto modulationRateParameterId = "modRate";
constexpr auto velocitySensitivityParameterId = "velocitySensitivity";
constexpr auto modulationDestinationParameterId = "modDestination";
constexpr auto unisonVoicesParameterId = "unisonVoices";
constexpr auto unisonDetuneCentsParameterId = "unisonDetuneCents";
constexpr auto schemaVersionPropertyId = "schemaVersion";

float getRawParameterValue (PolySynthAudioProcessor& processor, juce::StringRef parameterId)
{
    if (auto* raw = processor.getValueTreeState().getRawParameterValue (parameterId))
        return raw->load();

    return -1.0f;
}

bool setRawParameterValue (PolySynthAudioProcessor& processor, juce::StringRef parameterId, float rawValue)
{
    if (auto* parameter = processor.getValueTreeState().getParameter (parameterId))
    {
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (rawValue));
        return true;
    }

    return false;
}

bool expectIntParameter (PolySynthAudioProcessor& processor,
                         juce::StringRef parameterId,
                         int expectedValue,
                         juce::StringRef context)
{
    const auto actual = getRawParameterValue (processor, parameterId);

    if (juce::roundToInt (actual) != expectedValue)
    {
        std::cerr << juce::String (context) << " mismatch for " << juce::String (parameterId)
                  << "; expected " << expectedValue << " got " << actual << '\n';
        return false;
    }

    return true;
}

bool expectFloatParameter (PolySynthAudioProcessor& processor,
                           juce::StringRef parameterId,
                           float expectedValue,
                           juce::StringRef context)
{
    const auto actual = getRawParameterValue (processor, parameterId);

    if (! juce::approximatelyEqual (actual, expectedValue))
    {
        std::cerr << juce::String (context) << " mismatch for " << juce::String (parameterId)
                  << "; expected " << expectedValue << " got " << actual << '\n';
        return false;
    }

    return true;
}

bool expectSerializedSchemaVersion (const juce::MemoryBlock& serializedState, int expectedVersion)
{
    if (auto xml = PolySynthAudioProcessor::getXmlFromBinary (serializedState.getData(), static_cast<int> (serializedState.getSize())); xml == nullptr)
    {
        std::cerr << "unable to parse serialized state for schema version check" << '\n';
        return false;
    }
    else if (static_cast<int> (xml->getIntAttribute (schemaVersionPropertyId, -1)) != expectedVersion)
    {
        std::cerr << "serialized state schema version mismatch; expected "
                  << expectedVersion << " got " << xml->getIntAttribute (schemaVersionPropertyId, -1) << '\n';
        return false;
    }

    return true;
}

bool validateLegacyMonoStateMigrationPreservesBackwardsCompatibleIds()
{
    PolySynthAudioProcessor processor;

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
    PolySynthAudioProcessor::copyXmlToBinary (*legacyXml, serializedLegacyState);

    processor.setStateInformation (serializedLegacyState.getData(), static_cast<int> (serializedLegacyState.getSize()));

    return expectIntParameter (processor, waveformParameterId, 3, "legacy migration")
        && expectIntParameter (processor, maxVoicesParameterId, 1, "legacy migration default")
        && expectIntParameter (processor, stealPolicyParameterId, 0, "legacy migration default")
        && expectFloatParameter (processor, attackParameterId, 0.005f, "legacy migration default")
        && expectFloatParameter (processor, decayParameterId, 0.08f, "legacy migration default")
        && expectFloatParameter (processor, sustainParameterId, 0.8f, "legacy migration default")
        && expectFloatParameter (processor, releaseParameterId, 0.03f, "legacy migration default")
        && expectFloatParameter (processor, modulationDepthParameterId, 0.0f, "legacy migration default")
        && expectFloatParameter (processor, modulationRateParameterId, 2.0f, "legacy migration default")
        && expectFloatParameter (processor, velocitySensitivityParameterId, 0.0f, "legacy migration default")
        && expectIntParameter (processor, modulationDestinationParameterId, 0, "legacy migration default")
        && expectIntParameter (processor, unisonVoicesParameterId, 1, "legacy migration default")
        && expectFloatParameter (processor, unisonDetuneCentsParameterId, 0.0f, "legacy migration default");
}

bool validateCurrentVersionRoundTrip()
{
    PolySynthAudioProcessor sourceProcessor;

    if (! setRawParameterValue (sourceProcessor, waveformParameterId, 2.0f)
        || ! setRawParameterValue (sourceProcessor, maxVoicesParameterId, 8.0f)
        || ! setRawParameterValue (sourceProcessor, stealPolicyParameterId, 2.0f)
        || ! setRawParameterValue (sourceProcessor, attackParameterId, 0.42f)
        || ! setRawParameterValue (sourceProcessor, decayParameterId, 0.28f)
        || ! setRawParameterValue (sourceProcessor, sustainParameterId, 0.61f)
        || ! setRawParameterValue (sourceProcessor, releaseParameterId, 0.21f)
        || ! setRawParameterValue (sourceProcessor, modulationDepthParameterId, 0.77f)
        || ! setRawParameterValue (sourceProcessor, modulationRateParameterId, 7.25f)
        || ! setRawParameterValue (sourceProcessor, velocitySensitivityParameterId, 0.63f)
        || ! setRawParameterValue (sourceProcessor, modulationDestinationParameterId, 2.0f)
        || ! setRawParameterValue (sourceProcessor, unisonVoicesParameterId, 4.0f)
        || ! setRawParameterValue (sourceProcessor, unisonDetuneCentsParameterId, 16.5f))
    {
        std::cerr << "unable to set one or more parameters on source processor" << '\n';
        return false;
    }

    juce::MemoryBlock serializedState;
    sourceProcessor.getStateInformation (serializedState);

    if (! expectSerializedSchemaVersion (serializedState, expectedSchema.currentSchemaVersion))
        return false;

    PolySynthAudioProcessor restoredProcessor;
    restoredProcessor.setStateInformation (serializedState.getData(), static_cast<int> (serializedState.getSize()));

    return expectIntParameter (restoredProcessor, waveformParameterId, 2, "current-version restore")
        && expectIntParameter (restoredProcessor, maxVoicesParameterId, 8, "current-version restore")
        && expectIntParameter (restoredProcessor, stealPolicyParameterId, 2, "current-version restore")
        && expectFloatParameter (restoredProcessor, attackParameterId, 0.42f, "current-version restore")
        && expectFloatParameter (restoredProcessor, decayParameterId, 0.28f, "current-version restore")
        && expectFloatParameter (restoredProcessor, sustainParameterId, 0.61f, "current-version restore")
        && expectFloatParameter (restoredProcessor, releaseParameterId, 0.21f, "current-version restore")
        && expectFloatParameter (restoredProcessor, modulationDepthParameterId, 0.77f, "current-version restore")
        && expectFloatParameter (restoredProcessor, modulationRateParameterId, 7.25f, "current-version restore")
        && expectFloatParameter (restoredProcessor, velocitySensitivityParameterId, 0.63f, "current-version restore")
        && expectIntParameter (restoredProcessor, modulationDestinationParameterId, 2, "current-version restore")
        && expectIntParameter (restoredProcessor, unisonVoicesParameterId, 4, "current-version restore")
        && expectFloatParameter (restoredProcessor, unisonDetuneCentsParameterId, 16.5f, "current-version restore");
}

bool validateFuturePolyExpansionFixtureRestoresKnownIdsAndDefaultsMissing()
{
    PolySynthAudioProcessor processor;

    auto futureFixtureXml = juce::parseXML (R"xml(
<PARAMETERS schemaVersion="4">
  <PARAM id="waveform" value="1"/>
  <PARAM id="maxVoices" value="12"/>
  <PARAM id="stealPolicy" value="1"/>
  <PARAM id="attack" value="0.15"/>
  <PARAM id="sustain" value="0.52"/>
  <PARAM id="modDepth" value="0.25"/>
  <PARAM id="spread" value="0.66"/>
</PARAMETERS>)xml");

    if (futureFixtureXml == nullptr)
    {
        std::cerr << "unable to parse future expansion fixture" << '\n';
        return false;
    }

    const auto fixtureSchemaVersion = futureFixtureXml->getIntAttribute (schemaVersionPropertyId, -1);
    if (fixtureSchemaVersion != expectedSchema.futureExpansionFixtureVersion)
    {
        std::cerr << "future fixture schema drifted; expected "
                  << expectedSchema.futureExpansionFixtureVersion << " got " << fixtureSchemaVersion << '\n';
        return false;
    }

    juce::MemoryBlock serializedFutureFixture;
    PolySynthAudioProcessor::copyXmlToBinary (*futureFixtureXml, serializedFutureFixture);

    processor.setStateInformation (serializedFutureFixture.getData(), static_cast<int> (serializedFutureFixture.getSize()));

    return expectIntParameter (processor, waveformParameterId, 1, "future fixture restore")
        && expectIntParameter (processor, maxVoicesParameterId, 12, "future fixture restore")
        && expectIntParameter (processor, stealPolicyParameterId, 1, "future fixture restore")
        && expectFloatParameter (processor, attackParameterId, 0.15f, "future fixture restore")
        && expectFloatParameter (processor, sustainParameterId, 0.52f, "future fixture restore")
        && expectFloatParameter (processor, modulationDepthParameterId, 0.25f, "future fixture restore")
        && expectFloatParameter (processor, decayParameterId, 0.08f, "future fixture missing-param default")
        && expectFloatParameter (processor, releaseParameterId, 0.03f, "future fixture missing-param default")
        && expectFloatParameter (processor, modulationRateParameterId, 2.0f, "future fixture missing-param default")
        && expectFloatParameter (processor, velocitySensitivityParameterId, 0.0f, "future fixture missing-param default")
        && expectIntParameter (processor, modulationDestinationParameterId, 0, "future fixture missing-param default")
        && expectIntParameter (processor, unisonVoicesParameterId, 1, "future fixture missing-param default")
        && expectFloatParameter (processor, unisonDetuneCentsParameterId, 0.0f, "future fixture missing-param default");
}

} // namespace

int main()
{
    if (! validateLegacyMonoStateMigrationPreservesBackwardsCompatibleIds())
        return 1;

    if (! validateCurrentVersionRoundTrip())
        return 1;

    if (! validateFuturePolyExpansionFixtureRestoresKnownIdsAndDefaultsMissing())
        return 1;

    std::cout << "state schema migration validation passed." << '\n';
    return 0;
}
