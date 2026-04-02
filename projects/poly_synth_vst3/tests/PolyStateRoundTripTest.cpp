#include "../PolySynthAudioProcessor.h"

#include <iostream>

namespace
{
constexpr auto schemaVersionPropertyId = "schemaVersion";
constexpr auto layersNodeId = "LAYERS";
constexpr auto selectedLayerIdPropertyId = "selectedLayerId";
constexpr auto nextLayerIdPropertyId = "nextLayerId";
constexpr auto layerNodeId = "LAYER";
constexpr auto layerOrderNodeId = "LAYER_ORDER";
constexpr auto layerStateNodeId = "layerState";
constexpr auto layerIdPropertyId = "layerId";
constexpr auto orderLayerIdPropertyId = "layerId";
constexpr auto rootNoteAbsolutePropertyId = "rootNoteAbsolute";
constexpr auto mutePropertyId = "mute";
constexpr auto soloPropertyId = "solo";
constexpr auto layerVolumePropertyId = "layerVolume";
constexpr auto uiDensityModePropertyId = "uiDensityMode";
constexpr auto voiceAdvancedPanelExpandedPropertyId = "voiceAdvancedPanelExpanded";
constexpr auto outputAdvancedPanelExpandedPropertyId = "outputAdvancedPanelExpanded";

bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';

    return condition;
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

bool validateLayeredStateRoundTripRetainsIdentityOrderSelectionAndParameters()
{
    PolySynthAudioProcessor processor;
    processor.addDefaultLayerAndSelect();
    processor.addDefaultLayerAndSelect();

    processor.moveLayerDown (0);
    processor.selectLayerByVisualIndex (2);
    processor.setLayerRootNoteAbsolute (2, 72);
    processor.setLayerMute (2, true);
    processor.setLayerSolo (2, true);
    processor.setLayerVolume (2, 0.42f);
    processor.setUiDensityMode (PolySynthAudioProcessor::UiDensityMode::advanced);
    processor.setVoiceAdvancedPanelExpanded (true);
    processor.setOutputAdvancedPanelExpanded (true);

    juce::MemoryBlock before;
    processor.getStateInformation (before);

    auto beforeState = getStateTreeFromBlock (before);
    auto beforeLayers = beforeState.getChildWithName (layersNodeId);
    auto beforeOrder = beforeLayers.getChildWithName (layerOrderNodeId);

    const auto order0 = static_cast<juce::int64> (beforeOrder.getChild (0).getProperty (orderLayerIdPropertyId, 0));
    const auto order1 = static_cast<juce::int64> (beforeOrder.getChild (1).getProperty (orderLayerIdPropertyId, 0));
    const auto order2 = static_cast<juce::int64> (beforeOrder.getChild (2).getProperty (orderLayerIdPropertyId, 0));
    const auto selectedId = static_cast<juce::int64> (beforeLayers.getProperty (selectedLayerIdPropertyId, 0));
    const auto nextLayerId = static_cast<juce::int64> (beforeLayers.getProperty (nextLayerIdPropertyId, 0));

    processor.setStateInformation (before.getData(), static_cast<int> (before.getSize()));

    juce::MemoryBlock after;
    processor.getStateInformation (after);

    auto afterState = getStateTreeFromBlock (after);
    auto afterLayers = afterState.getChildWithName (layersNodeId);
    auto afterOrder = afterLayers.getChildWithName (layerOrderNodeId);

    const auto schemaOk = expect (afterState.getProperty (schemaVersionPropertyId) == juce::var (6), "serialized schema version mismatch");
    const auto orderOk = expect (static_cast<juce::int64> (afterOrder.getChild (0).getProperty (orderLayerIdPropertyId, 0)) == order0,
                                 "round-trip order[0] mismatch")
                      && expect (static_cast<juce::int64> (afterOrder.getChild (1).getProperty (orderLayerIdPropertyId, 0)) == order1,
                                 "round-trip order[1] mismatch")
                      && expect (static_cast<juce::int64> (afterOrder.getChild (2).getProperty (orderLayerIdPropertyId, 0)) == order2,
                                 "round-trip order[2] mismatch");

    const auto afterSelected = static_cast<juce::int64> (afterLayers.getProperty (selectedLayerIdPropertyId, 0));
    auto afterLayerNode = getLayerNodeById (afterLayers, selectedId);
    auto afterLayerState = afterLayerNode.getChildWithName (layerStateNodeId);

    const auto paramsOk = expect (static_cast<int> (afterLayerState.getProperty (rootNoteAbsolutePropertyId, -1)) == 72,
                                  "round-trip root note mismatch")
                       && expect (static_cast<bool> (afterLayerState.getProperty (mutePropertyId, false)),
                                  "round-trip mute mismatch")
                       && expect (static_cast<bool> (afterLayerState.getProperty (soloPropertyId, false)),
                                  "round-trip solo mismatch")
                       && expect (juce::approximatelyEqual (static_cast<float> (afterLayerState.getProperty (layerVolumePropertyId, 0.0f)), 0.42f),
                                  "round-trip layer volume mismatch");

    const auto nextLayerIdOk = expect (static_cast<juce::int64> (afterLayers.getProperty (nextLayerIdPropertyId, 0)) == nextLayerId,
                                       "round-trip nextLayerId mismatch");
    const auto uiPrefsOk = expect (static_cast<int> (afterState.getProperty (uiDensityModePropertyId, 0)) == 1,
                                   "round-trip uiDensityMode mismatch")
                        && expect (static_cast<bool> (afterState.getProperty (voiceAdvancedPanelExpandedPropertyId, false)),
                                   "round-trip voiceAdvancedPanelExpanded mismatch")
                        && expect (static_cast<bool> (afterState.getProperty (outputAdvancedPanelExpandedPropertyId, false)),
                                   "round-trip outputAdvancedPanelExpanded mismatch");

    return schemaOk && orderOk && paramsOk && nextLayerIdOk && uiPrefsOk
        && expect (afterSelected == selectedId, "round-trip selected layer mismatch");
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

bool validateBaseLayerStateWinsOverStaleApvtsOnRestore()
{
    PolySynthAudioProcessor source;
    source.setLayerWaveformByVisualIndex (0, PolySynthAudioProcessor::Waveform::triangle);
    source.setLayerVoiceCountByVisualIndex (0, 7);
    source.setLayerStealPolicyByVisualIndex (0, SynthEngine::VoiceStealPolicy::quietest);
    source.setLayerAdsrByVisualIndex (0, 0.31f, 0.42f, 0.37f, 0.54f);
    source.setLayerModParametersByVisualIndex (0, 0.83f, 8.5f, SynthVoice::ModulationDestination::pitch);
    source.setLayerVelocitySensitivityByVisualIndex (0, 0.61f);
    source.setLayerUnisonByVisualIndex (0, 5, 17.25f);
    source.setLayerOutputStageByVisualIndex (0, SynthEngine::OutputStage::softLimit);

    juce::MemoryBlock serialized;
    source.getStateInformation (serialized);
    PolySynthAudioProcessor restored;
    const auto staleSetOk = expect (setRawParameterValue (restored, "waveform", 0.0f), "missing waveform parameter")
                          && expect (setRawParameterValue (restored, "maxVoices", 1.0f), "missing maxVoices parameter")
                          && expect (setRawParameterValue (restored, "stealPolicy", 0.0f), "missing stealPolicy parameter")
                          && expect (setRawParameterValue (restored, "attack", 0.005f), "missing attack parameter")
                          && expect (setRawParameterValue (restored, "decay", 0.08f), "missing decay parameter")
                          && expect (setRawParameterValue (restored, "sustain", 0.8f), "missing sustain parameter")
                          && expect (setRawParameterValue (restored, "release", 0.03f), "missing release parameter")
                          && expect (setRawParameterValue (restored, "modDepth", 0.0f), "missing modDepth parameter")
                          && expect (setRawParameterValue (restored, "modRate", 2.0f), "missing modRate parameter")
                          && expect (setRawParameterValue (restored, "velocitySensitivity", 0.0f), "missing velocitySensitivity parameter")
                          && expect (setRawParameterValue (restored, "modDestination", 0.0f), "missing modDestination parameter")
                          && expect (setRawParameterValue (restored, "unisonVoices", 1.0f), "missing unisonVoices parameter")
                          && expect (setRawParameterValue (restored, "unisonDetuneCents", 0.0f), "missing unisonDetuneCents parameter")
                          && expect (setRawParameterValue (restored, "outputStage", 1.0f), "missing outputStage parameter");

    if (! staleSetOk)
        return false;

    restored.setStateInformation (serialized.getData(), static_cast<int> (serialized.getSize()));
    const auto base = restored.getLayerStateByVisualIndex (0);
    if (! expect (base.has_value(), "restored base layer missing"))
        return false;

    return expect (base->waveform == PolySynthAudioProcessor::Waveform::triangle, "waveform overwritten by stale APVTS value")
        && expect (base->voiceCount == 7, "voice count overwritten by stale APVTS value")
        && expect (base->stealPolicy == SynthEngine::VoiceStealPolicy::quietest, "steal policy overwritten by stale APVTS value")
        && expect (juce::approximatelyEqual (base->attackSeconds, 0.31f), "attack overwritten by stale APVTS value")
        && expect (juce::approximatelyEqual (base->decaySeconds, 0.42f), "decay overwritten by stale APVTS value")
        && expect (juce::approximatelyEqual (base->sustainLevel, 0.37f), "sustain overwritten by stale APVTS value")
        && expect (juce::approximatelyEqual (base->releaseSeconds, 0.54f), "release overwritten by stale APVTS value")
        && expect (juce::approximatelyEqual (base->modulationDepth, 0.83f), "mod depth overwritten by stale APVTS value")
        && expect (juce::approximatelyEqual (base->modulationRateHz, 8.5f), "mod rate overwritten by stale APVTS value")
        && expect (base->modulationDestination == SynthVoice::ModulationDestination::pitch, "mod destination overwritten by stale APVTS value")
        && expect (juce::approximatelyEqual (base->velocitySensitivity, 0.61f), "velocity sensitivity overwritten by stale APVTS value")
        && expect (base->unisonVoices == 5, "unison voices overwritten by stale APVTS value")
        && expect (juce::approximatelyEqual (base->unisonDetuneCents, 17.25f), "unison detune overwritten by stale APVTS value")
        && expect (base->outputStage == SynthEngine::OutputStage::softLimit, "output stage overwritten by stale APVTS value");
}

} // namespace

int main()
{
    if (! validateLayeredStateRoundTripRetainsIdentityOrderSelectionAndParameters())
        return 1;

    if (! validateBaseLayerStateWinsOverStaleApvtsOnRestore())
        return 1;

    std::cout << "poly state round-trip validation passed." << '\n';
    return 0;
}
