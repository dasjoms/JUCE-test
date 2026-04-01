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

    return schemaOk && orderOk && paramsOk && nextLayerIdOk
        && expect (afterSelected == selectedId, "round-trip selected layer mismatch");
}

} // namespace

int main()
{
    if (! validateLayeredStateRoundTripRetainsIdentityOrderSelectionAndParameters())
        return 1;

    std::cout << "poly state round-trip validation passed." << '\n';
    return 0;
}
