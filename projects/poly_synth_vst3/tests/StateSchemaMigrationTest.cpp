#include "../PolySynthAudioProcessor.h"

#include <iostream>
#include <vector>

namespace
{
struct SchemaExpectation
{
    int currentSchemaVersion = 6;
    int futureExpansionFixtureVersion = 7;
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
constexpr auto layersNodeId = "LAYERS";
constexpr auto layerNodeId = "LAYER";
constexpr auto layerStateNodeId = "layerState";
constexpr auto layerOrderNodeId = "LAYER_ORDER";
constexpr auto selectedLayerIdPropertyId = "selectedLayerId";
constexpr auto layerIdPropertyId = "layerId";
constexpr auto orderLayerIdPropertyId = "layerId";
constexpr auto rootNoteAbsolutePropertyId = "rootNoteAbsolute";
constexpr auto mutePropertyId = "mute";
constexpr auto soloPropertyId = "solo";
constexpr auto layerVolumePropertyId = "layerVolume";
constexpr auto nextLayerIdPropertyId = "nextLayerId";

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

juce::ValueTree getStateTreeFromBlock (const juce::MemoryBlock& state)
{
    auto xml = PolySynthAudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    return xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
}

juce::ValueTree getLayerNodeById (juce::ValueTree& layersNode, juce::int64 targetLayerId)
{
    for (int i = 0; i < layersNode.getNumChildren(); ++i)
    {
        auto child = layersNode.getChild (i);
        if (child.hasType (layerNodeId) && static_cast<juce::int64> (child.getProperty (layerIdPropertyId, 0)) == targetLayerId)
            return child;
    }

    return {};
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

bool expectSerializedSingleBaseLayerWithSelection (PolySynthAudioProcessor& processor, juce::StringRef context)
{
    juce::MemoryBlock state;
    processor.getStateInformation (state);

    auto xml = PolySynthAudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    if (xml == nullptr)
    {
        std::cerr << "unable to parse serialized state for " << juce::String (context) << '\n';
        return false;
    }

    auto layersNode = xml->getChildByName (layersNodeId);
    if (layersNode == nullptr)
    {
        std::cerr << "missing layered state node for " << juce::String (context) << '\n';
        return false;
    }

    const auto layerNodes = layersNode->getChildIterator();
    int layerCount = 0;
    int orderCount = 0;
    int64_t firstLayerId = 0;
    int64_t firstOrderId = 0;

    for (auto* child : layerNodes)
    {
        if (child->hasTagName (layerNodeId))
        {
            ++layerCount;
            if (layerCount == 1)
            {
                firstLayerId = child->getIntAttribute (layerIdPropertyId, 0);
                if (child->getChildByName (layerStateNodeId) == nullptr)
                {
                    std::cerr << "missing layerState payload for " << juce::String (context) << '\n';
                    return false;
                }
            }
        }
        else if (child->hasTagName (layerOrderNodeId))
        {
            for (auto* item : child->getChildIterator())
            {
                if (item->hasTagName ("ITEM"))
                {
                    ++orderCount;
                    if (orderCount == 1)
                        firstOrderId = item->getIntAttribute (orderLayerIdPropertyId, 0);
                }
            }
        }
    }

    if (layerCount != 1 || orderCount != 1)
    {
        std::cerr << "expected exactly one layer/order entry for " << juce::String (context)
                  << " got layers=" << layerCount << " order=" << orderCount << '\n';
        return false;
    }

    if (firstLayerId <= 0 || firstOrderId != firstLayerId)
    {
        std::cerr << "layer identity/order mismatch for " << juce::String (context) << '\n';
        return false;
    }

    if (layersNode->getIntAttribute (selectedLayerIdPropertyId, 0) != firstLayerId)
    {
        std::cerr << "selected layer id mismatch for " << juce::String (context) << '\n';
        return false;
    }

    auto* layerNode = layersNode->getChildByName (layerNodeId);
    if (layerNode == nullptr)
        return false;

    auto* layerState = layerNode->getChildByName (layerStateNodeId);
    if (layerState == nullptr)
        return false;

    if (layerState->getIntAttribute (rootNoteAbsolutePropertyId, -1) != 60)
    {
        std::cerr << "root note default mismatch for " << juce::String (context) << '\n';
        return false;
    }

    return true;
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

bool validateLegacyModDestinationMappingPreservesIntent()
{
    PolySynthAudioProcessor processor;

    return restoreFromFixtureXml (processor,
                                  R"xml(
<PARAMETERS schemaVersion="5">
  <PARAM id="modDestination" value="0"/>
</PARAMETERS>)xml",
                                  "legacy mod destination mapping")
        && expectIntParameter (processor, modulationDestinationParameterId, 1, "legacy mod destination mapping");
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

bool validateV1ThroughV4MigrateToSingleLayerAndPreserveSound()
{
    struct Fixture { int schemaVersion; juce::String xml; };
    const std::vector<Fixture> fixtures {
        { 1, R"xml(<PARAMETERS schemaVersion="1"><PARAM id="waveform" value="2"/><PARAM id="attack" value="0.17"/><PARAM id="decay" value="0.13"/><PARAM id="sustain" value="0.42"/><PARAM id="release" value="0.66"/><PARAM id="modDepth" value="0.3"/><PARAM id="modRate" value="5.5"/><PARAM id="modDestination" value="1"/><PARAM id="unisonVoices" value="4"/><PARAM id="unisonDetuneCents" value="12.0"/><PARAM id="outputStage" value="2"/></PARAMETERS>)xml" },
        { 2, R"xml(<PARAMETERS schemaVersion="2"><PARAM id="waveform" value="2"/><PARAM id="attack" value="0.17"/><PARAM id="decay" value="0.13"/><PARAM id="sustain" value="0.42"/><PARAM id="release" value="0.66"/><PARAM id="modDepth" value="0.3"/><PARAM id="modRate" value="5.5"/><PARAM id="modDestination" value="1"/><PARAM id="unisonVoices" value="4"/><PARAM id="unisonDetuneCents" value="12.0"/><PARAM id="outputStage" value="2"/></PARAMETERS>)xml" },
        { 3, R"xml(<PARAMETERS schemaVersion="3"><PARAM id="waveform" value="2"/><PARAM id="attack" value="0.17"/><PARAM id="decay" value="0.13"/><PARAM id="sustain" value="0.42"/><PARAM id="release" value="0.66"/><PARAM id="modDepth" value="0.3"/><PARAM id="modRate" value="5.5"/><PARAM id="modDestination" value="1"/><PARAM id="unisonVoices" value="4"/><PARAM id="unisonDetuneCents" value="12.0"/><PARAM id="outputStage" value="2"/></PARAMETERS>)xml" },
        { 4, R"xml(<PARAMETERS schemaVersion="4"><PARAM id="waveform" value="2"/><PARAM id="attack" value="0.17"/><PARAM id="decay" value="0.13"/><PARAM id="sustain" value="0.42"/><PARAM id="release" value="0.66"/><PARAM id="modDepth" value="0.3"/><PARAM id="modRate" value="5.5"/><PARAM id="modDestination" value="1"/><PARAM id="unisonVoices" value="4"/><PARAM id="unisonDetuneCents" value="12.0"/><PARAM id="outputStage" value="2"/></PARAMETERS>)xml" }
    };

    for (const auto& fixture : fixtures)
    {
        PolySynthAudioProcessor processor;
        const auto context = "schema-v" + juce::String (fixture.schemaVersion) + " layered migration";
        if (! restoreFromFixtureXml (processor, fixture.xml, context)
            || ! expectIntParameter (processor, waveformParameterId, 2, context)
            || ! expectFloatParameter (processor, attackParameterId, 0.17f, context)
            || ! expectFloatParameter (processor, decayParameterId, 0.13f, context)
            || ! expectFloatParameter (processor, sustainParameterId, 0.42f, context)
            || ! expectFloatParameter (processor, releaseParameterId, 0.66f, context)
            || ! expectFloatParameter (processor, modulationDepthParameterId, 0.3f, context)
            || ! expectFloatParameter (processor, modulationRateParameterId, 5.5f, context)
            || ! expectIntParameter (processor, modulationDestinationParameterId, 2, context)
            || ! expectIntParameter (processor, unisonVoicesParameterId, 4, context)
            || ! expectFloatParameter (processor, unisonDetuneCentsParameterId, 12.0f, context)
            || ! expectIntParameter (processor, outputStageParameterId, 2, context)
            || ! expectSerializedSingleBaseLayerWithSelection (processor, context))
            return false;
    }

    return true;
}

bool validateMigratedDefaultsDoNotColorSound()
{
    PolySynthAudioProcessor processor;
    if (! restoreFromFixtureXml (processor,
                                 R"xml(<PARAMETERS schemaVersion="1"><PARAM id="waveform" value="0"/></PARAMETERS>)xml",
                                 "sound-color default regression"))
        return false;

    return expectFloatParameter (processor, modulationDepthParameterId, 0.0f, "sound-color default regression")
        && expectFloatParameter (processor, velocitySensitivityParameterId, 0.0f, "sound-color default regression")
        && expectIntParameter (processor, modulationDestinationParameterId, 0, "sound-color default regression")
        && expectIntParameter (processor, unisonVoicesParameterId, 1, "sound-color default regression")
        && expectFloatParameter (processor, unisonDetuneCentsParameterId, 0.0f, "sound-color default regression")
        && expectIntParameter (processor, outputStageParameterId, 1, "sound-color default regression");
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
        || ! setRawParameterValue (sourceProcessor, modulationDestinationParameterId, 3.0f)
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
        && expectIntParameter (restoredProcessor, modulationDestinationParameterId, 3, "current-version restore")
        && expectIntParameter (restoredProcessor, unisonVoicesParameterId, 4, "current-version restore")
        && expectFloatParameter (restoredProcessor, unisonDetuneCentsParameterId, 16.5f, "current-version restore")
        && expectIntParameter (restoredProcessor, outputStageParameterId, 2, "current-version restore");
}

bool validateLayeredStateRestoreRepairsOrderAndNextLayerId()
{
    PolySynthAudioProcessor processor;
    return restoreFromFixtureXml (processor,
                                  R"xml(
<PARAMETERS schemaVersion="6">
  <LAYERS selectedLayerId="5000" nextLayerId="2">
    <LAYER layerId="10">
      <layerState waveform="1" rootNoteAbsolute="62" mute="0" solo="0" layerVolume="0.91"/>
    </LAYER>
    <LAYER layerId="11">
      <layerState waveform="2" rootNoteAbsolute="63" mute="1" solo="0" layerVolume="0.33"/>
    </LAYER>
    <LAYER layerId="12">
      <layerState waveform="3" rootNoteAbsolute="64" mute="0" solo="1" layerVolume="0.27"/>
    </LAYER>
    <LAYER_ORDER>
      <ITEM layerId="11"/>
      <ITEM layerId="9999"/>
      <ITEM layerId="10"/>
    </LAYER_ORDER>
  </LAYERS>
</PARAMETERS>)xml",
                                  "layered-order repair")
        && [&processor]
           {
               juce::MemoryBlock state;
               processor.getStateInformation (state);
               auto restoredState = getStateTreeFromBlock (state);
               auto layersNode = restoredState.getChildWithName (layersNodeId);
               auto orderNode = layersNode.getChildWithName (layerOrderNodeId);

               const auto hasThreeLayers = static_cast<int> (processor.getLayerCount()) == 3;
               if (! hasThreeLayers)
               {
                   std::cerr << "expected 3 layers after restore" << '\n';
                   return false;
               }

               const auto order0 = static_cast<juce::int64> (orderNode.getChild (0).getProperty (orderLayerIdPropertyId, 0));
               const auto order1 = static_cast<juce::int64> (orderNode.getChild (1).getProperty (orderLayerIdPropertyId, 0));
               const auto order2 = static_cast<juce::int64> (orderNode.getChild (2).getProperty (orderLayerIdPropertyId, 0));
               if (order0 != 11 || order1 != 10 || order2 != 12)
               {
                   std::cerr << "restored order mismatch after repair" << '\n';
                   return false;
               }

               if (static_cast<juce::int64> (layersNode.getProperty (selectedLayerIdPropertyId, 0)) != 11)
               {
                   std::cerr << "selected layer did not fall back to first valid ordered layer" << '\n';
                   return false;
               }

               if (static_cast<juce::int64> (layersNode.getProperty (nextLayerIdPropertyId, 0)) != 13)
               {
                   std::cerr << "next layer id was not repaired from allocator max+1" << '\n';
                   return false;
               }

               auto layer11 = getLayerNodeById (layersNode, 11);
               auto layerState11 = layer11.getChildWithName (layerStateNodeId);
               if (static_cast<int> (layerState11.getProperty (rootNoteAbsolutePropertyId, -1)) != 63
                   || ! static_cast<bool> (layerState11.getProperty (mutePropertyId, false))
                   || static_cast<bool> (layerState11.getProperty (soloPropertyId, false))
                   || ! juce::approximatelyEqual (static_cast<float> (layerState11.getProperty (layerVolumePropertyId, 0.0f)), 0.33f))
               {
                   std::cerr << "restored per-layer payload mismatch" << '\n';
                   return false;
               }

               return true;
           }();
}

bool validateLayeredStateRestorePreservesHighEndAndLongEnvelopeValues()
{
    PolySynthAudioProcessor processor;
    if (! restoreFromFixtureXml (processor,
                                 R"xml(
<PARAMETERS schemaVersion="6">
  <LAYERS selectedLayerId="101" nextLayerId="102">
    <LAYER layerId="101">
      <layerState waveform="2" maxVoices="16" attack="4.75" decay="5.0" sustain="1.0" release="4.2" modDepth="1.0" modRate="20.0" velocitySensitivity="1.0" modDestination="3" unisonVoices="8" unisonDetuneCents="50.0" outputStage="2"/>
    </LAYER>
    <LAYER_ORDER>
      <ITEM layerId="101"/>
    </LAYER_ORDER>
  </LAYERS>
</PARAMETERS>)xml",
                                 "layered-high-end restore"))
    {
        return false;
    }

    const auto layer = processor.getLayerStateByVisualIndex (0);
    if (! layer.has_value())
    {
        std::cerr << "missing restored layer for layered-high-end restore" << '\n';
        return false;
    }

    return expectIntParameter (processor, maxVoicesParameterId, 16, "layered-high-end restore")
        && expectFloatParameter (processor, attackParameterId, 4.75f, "layered-high-end restore")
        && expectFloatParameter (processor, decayParameterId, 5.0f, "layered-high-end restore")
        && expectFloatParameter (processor, releaseParameterId, 4.2f, "layered-high-end restore")
        && expectFloatParameter (processor, modulationRateParameterId, 20.0f, "layered-high-end restore")
        && expectIntParameter (processor, unisonVoicesParameterId, 8, "layered-high-end restore")
        && expectFloatParameter (processor, unisonDetuneCentsParameterId, 50.0f, "layered-high-end restore")
        && juce::approximatelyEqual (layer->attackSeconds, 4.75f)
        && juce::approximatelyEqual (layer->decaySeconds, 5.0f)
        && juce::approximatelyEqual (layer->releaseSeconds, 4.2f);
}

bool validateLayeredStateRestoreClampsInvalidValuesPredictably()
{
    PolySynthAudioProcessor processor;
    if (! restoreFromFixtureXml (processor,
                                 R"xml(
<PARAMETERS schemaVersion="6">
  <LAYERS selectedLayerId="201" nextLayerId="202">
    <LAYER layerId="201">
      <layerState maxVoices="99" attack="-0.5" decay="-0.2" sustain="2.0" release="0.001" modDepth="2.0" modRate="0.001" velocitySensitivity="-1.0" unisonVoices="0" unisonDetuneCents="-15.0"/>
    </LAYER>
    <LAYER_ORDER>
      <ITEM layerId="201"/>
    </LAYER_ORDER>
  </LAYERS>
</PARAMETERS>)xml",
                                 "layered-invalid clamp restore"))
    {
        return false;
    }

    const auto layer = processor.getLayerStateByVisualIndex (0);
    if (! layer.has_value())
    {
        std::cerr << "missing restored layer for layered-invalid clamp restore" << '\n';
        return false;
    }

    return expectIntParameter (processor, maxVoicesParameterId, 16, "layered-invalid clamp restore")
        && expectFloatParameter (processor, attackParameterId, 0.001f, "layered-invalid clamp restore")
        && expectFloatParameter (processor, decayParameterId, 0.001f, "layered-invalid clamp restore")
        && expectFloatParameter (processor, sustainParameterId, 1.0f, "layered-invalid clamp restore")
        && expectFloatParameter (processor, releaseParameterId, 0.005f, "layered-invalid clamp restore")
        && expectFloatParameter (processor, modulationDepthParameterId, 1.0f, "layered-invalid clamp restore")
        && expectFloatParameter (processor, modulationRateParameterId, 0.05f, "layered-invalid clamp restore")
        && expectFloatParameter (processor, velocitySensitivityParameterId, 0.0f, "layered-invalid clamp restore")
        && expectIntParameter (processor, unisonVoicesParameterId, 1, "layered-invalid clamp restore")
        && expectFloatParameter (processor, unisonDetuneCentsParameterId, 0.0f, "layered-invalid clamp restore")
        && juce::approximatelyEqual (layer->attackSeconds, 0.001f)
        && juce::approximatelyEqual (layer->decaySeconds, 0.001f)
        && juce::approximatelyEqual (layer->releaseSeconds, 0.005f);
}

bool validateFuturePolyExpansionFixtureRestoresKnownIdsAndDefaultsMissing()
{
    PolySynthAudioProcessor processor;

    auto futureFixtureXml = juce::parseXML (R"xml(
<PARAMETERS schemaVersion="7">
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

    if (! validateLegacyModDestinationMappingPreservesIntent())
        return 1;

    if (! validateOutOfRangeValuesAreClampedDuringMigration())
        return 1;

    if (! validateV1ThroughV4MigrateToSingleLayerAndPreserveSound())
        return 1;

    if (! validateMigratedDefaultsDoNotColorSound())
        return 1;

    if (! validateCurrentVersionRoundTrip())
        return 1;

    if (! validateFuturePolyExpansionFixtureRestoresKnownIdsAndDefaultsMissing())
        return 1;

    if (! validateLayeredStateRestoreRepairsOrderAndNextLayerId())
        return 1;

    if (! validateLayeredStateRestorePreservesHighEndAndLongEnvelopeValues())
        return 1;

    if (! validateLayeredStateRestoreClampsInvalidValuesPredictably())
        return 1;

    std::cout << "state schema migration validation passed." << '\n';
    return 0;
}
