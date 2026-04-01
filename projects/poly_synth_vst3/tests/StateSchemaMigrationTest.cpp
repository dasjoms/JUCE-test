#include "../PolySynthAudioProcessor.h"

#include <iostream>

namespace
{
struct SchemaExpectation
{
    int currentSchemaVersion = 4;
    int futureExpansionFixtureVersion = 5;
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
constexpr auto outputStageParameterId = "outputStage";
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

bool restoreFromFixtureXml (PolySynthAudioProcessor& processor, const juce::String& xml, juce::StringRef context)
{
    auto fixture = juce::parseXML (xml);

    if (fixture == nullptr)
    {
        std::cerr << "unable to parse fixture for " << juce::String (context) << '\n';
        return false;
    }

    juce::MemoryBlock serializedState;
    PolySynthAudioProcessor::copyXmlToBinary (*fixture, serializedState);
    processor.setStateInformation (serializedState.getData(), static_cast<int> (serializedState.getSize()));
    return true;
}

bool validateLegacyMonoStateMigrationPreservesBackwardsCompatibleIds()
{
    PolySynthAudioProcessor processor;

    return restoreFromFixtureXml (processor,
                                  R"xml(
<PARAMETERS>
  <PARAM id="waveform" value="3"/>
</PARAMETERS>)xml",
                                  "legacy mono migration")
        && expectIntParameter (processor, waveformParameterId, 3, "legacy migration")
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
        && expectFloatParameter (processor, unisonDetuneCentsParameterId, 0.0f, "legacy migration default")
        && expectIntParameter (processor, outputStageParameterId, 1, "legacy migration default");
}

bool validateSchemaV2MissingParametersDefaultAndKnownParametersRestore()
{
    PolySynthAudioProcessor processor;

    return restoreFromFixtureXml (processor,
                                  R"xml(
<PARAMETERS schemaVersion="2">
  <PARAM id="waveform" value="1"/>
  <PARAM id="maxVoices" value="10"/>
  <PARAM id="stealPolicy" value="2"/>
  <PARAM id="attack" value="0.11"/>
  <PARAM id="modDepth" value="0.48"/>
</PARAMETERS>)xml",
                                  "schema-v2 migration")
        && expectIntParameter (processor, waveformParameterId, 1, "schema-v2 known restore")
        && expectIntParameter (processor, maxVoicesParameterId, 10, "schema-v2 known restore")
        && expectIntParameter (processor, stealPolicyParameterId, 2, "schema-v2 known restore")
        && expectFloatParameter (processor, attackParameterId, 0.11f, "schema-v2 known restore")
        && expectFloatParameter (processor, modulationDepthParameterId, 0.48f, "schema-v2 known restore")
        && expectIntParameter (processor, unisonVoicesParameterId, 1, "schema-v2 missing default")
        && expectFloatParameter (processor, unisonDetuneCentsParameterId, 0.0f, "schema-v2 missing default")
        && expectIntParameter (processor, outputStageParameterId, 1, "schema-v2 missing default");
}

bool validateRenamedLegacyParameterRestoresPredictably()
{
    PolySynthAudioProcessor processor;

    return restoreFromFixtureXml (processor,
                                  R"xml(
<PARAMETERS schemaVersion="1">
  <PARAM id="waveform" value="2"/>
  <PARAM id="lfoDepth" value="0.37"/>
</PARAMETERS>)xml",
                                  "renamed parameter migration")
        && expectIntParameter (processor, waveformParameterId, 2, "renamed parameter migration")
        && expectFloatParameter (processor, modulationDepthParameterId, 0.37f, "renamed parameter migration");
}

bool validateOutOfRangeValuesAreClampedDuringMigration()
{
    PolySynthAudioProcessor processor;

    return restoreFromFixtureXml (processor,
                                  R"xml(
<PARAMETERS schemaVersion="2">
  <PARAM id="waveform" value="99"/>
  <PARAM id="attack" value="-5.0"/>
  <PARAM id="modRate" value="123.0"/>
  <PARAM id="unisonVoices" value="100"/>
  <PARAM id="unisonDetuneCents" value="-10"/>
</PARAMETERS>)xml",
                                  "out-of-range migration")
        && expectIntParameter (processor, waveformParameterId, 3, "out-of-range clamped waveform")
        && expectFloatParameter (processor, attackParameterId, 0.001f, "out-of-range clamped attack")
        && expectFloatParameter (processor, modulationRateParameterId, 20.0f, "out-of-range clamped modRate")
        && expectIntParameter (processor, unisonVoicesParameterId, 8, "out-of-range clamped unisonVoices")
        && expectFloatParameter (processor, unisonDetuneCentsParameterId, 0.0f, "out-of-range clamped unisonDetune");
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
        || ! setRawParameterValue (sourceProcessor, unisonDetuneCentsParameterId, 16.5f)
        || ! setRawParameterValue (sourceProcessor, outputStageParameterId, 2.0f))
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
        && expectFloatParameter (restoredProcessor, unisonDetuneCentsParameterId, 16.5f, "current-version restore")
        && expectIntParameter (restoredProcessor, outputStageParameterId, 2, "current-version restore");
}

bool validateFuturePolyExpansionFixtureRestoresKnownIdsAndDefaultsMissing()
{
    PolySynthAudioProcessor processor;

    auto futureFixtureXml = juce::parseXML (R"xml(
<PARAMETERS schemaVersion="5">
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
        && expectFloatParameter (processor, unisonDetuneCentsParameterId, 0.0f, "future fixture missing-param default")
        && expectIntParameter (processor, outputStageParameterId, 1, "future fixture missing-param default");
}

} // namespace

int main()
{
    if (! validateLegacyMonoStateMigrationPreservesBackwardsCompatibleIds())
        return 1;

    if (! validateSchemaV2MissingParametersDefaultAndKnownParametersRestore())
        return 1;

    if (! validateRenamedLegacyParameterRestoresPredictably())
        return 1;

    if (! validateOutOfRangeValuesAreClampedDuringMigration())
        return 1;

    if (! validateCurrentVersionRoundTrip())
        return 1;

    if (! validateFuturePolyExpansionFixtureRestoresKnownIdsAndDefaultsMissing())
        return 1;

    std::cout << "state schema migration validation passed." << '\n';
    return 0;
}
