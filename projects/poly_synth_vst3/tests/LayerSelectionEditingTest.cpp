#include "../PolySynthAudioProcessor.h"
#include "../PolySynthAudioProcessorEditor.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
constexpr auto layersNodeId = "LAYERS";
constexpr auto layerNodeId = "LAYER";
constexpr auto layerOrderNodeId = "LAYER_ORDER";
constexpr auto layerStateNodeId = "layerState";
constexpr auto layerIdPropertyId = "layerId";
constexpr auto waveformParameterId = "waveform";
constexpr auto attackParameterId = "attack";
constexpr auto decayParameterId = "decay";
constexpr auto sustainParameterId = "sustain";
constexpr auto releaseParameterId = "release";
constexpr auto selectedLayerIdPropertyId = "selectedLayerId";
constexpr auto nextLayerIdPropertyId = "nextLayerId";
constexpr auto orderItemNodeId = "ITEM";

bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';
    return condition;
}

juce::ValueTree getLayerNodeByVisualIndex (juce::ValueTree& layersNode, juce::ValueTree& orderNode, int visualIndex)
{
    const auto layerId = static_cast<juce::int64> (orderNode.getChild (visualIndex).getProperty (layerIdPropertyId, 0));
    for (int i = 0; i < layersNode.getNumChildren(); ++i)
    {
        auto child = layersNode.getChild (i);
        if (child.hasType (layerNodeId) && static_cast<juce::int64> (child.getProperty (layerIdPropertyId, 0)) == layerId)
            return child;
    }

    return {};
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

juce::ValueTree stateTreeFromProcessor (PolySynthAudioProcessor& processor)
{
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    auto xml = PolySynthAudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));

    if (xml == nullptr)
        return {};

    return juce::ValueTree::fromXml (*xml);
}

std::vector<uint64_t> getLayerOrderFromStateTree (const juce::ValueTree& stateTree)
{
    std::vector<uint64_t> order;
    const auto layersNode = stateTree.getChildWithName (layersNodeId);
    const auto orderNode = layersNode.getChildWithName (layerOrderNodeId);
    for (int i = 0; i < orderNode.getNumChildren(); ++i)
    {
        const auto item = orderNode.getChild (i);
        if (! item.hasType (orderItemNodeId))
            continue;

        const auto layerId = static_cast<uint64_t> (static_cast<juce::int64> (item.getProperty (layerIdPropertyId, juce::var (0))));
        if (layerId != 0)
            order.push_back (layerId);
    }

    return order;
}

bool loadStateTreeIntoProcessor (PolySynthAudioProcessor& processor, const juce::ValueTree& stateTree)
{
    if (! stateTree.isValid())
        return false;

    if (auto xml = stateTree.createXml())
    {
        juce::MemoryBlock binary;
        PolySynthAudioProcessor::copyXmlToBinary (*xml, binary);
        processor.setStateInformation (binary.getData(), static_cast<int> (binary.getSize()));
        return true;
    }

    return false;
}

float renderPeak (PolySynthAudioProcessor& processor, const juce::MidiBuffer& midi, int numSamples = 512)
{
    juce::AudioBuffer<float> buffer (2, numSamples);
    buffer.clear();

    auto midiCopy = midi;
    processor.processBlock (buffer, midiCopy);

    return juce::jmax (buffer.getMagnitude (0, 0, numSamples), buffer.getMagnitude (1, 0, numSamples));
}

float renderSoloLayerPeak (PolySynthAudioProcessor& processor, std::size_t soloLayerIndex)
{
    const auto layerCount = static_cast<std::size_t> (processor.getLayerCount());
    for (std::size_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
        processor.setLayerSolo (layerIndex, layerIndex == soloLayerIndex);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 127), 0);

    return renderPeak (processor, midi);
}

bool validateSelectedLayerEditingTouchesOnlySelectedLayer()
{
    PolySynthAudioProcessor processor;
    if (! processor.addDefaultLayerAndSelect())
        return false;

    const auto beforeLayer1 = processor.getLayerStateByVisualIndex (0);
    const auto beforeLayer2 = processor.getLayerStateByVisualIndex (1);
    if (! beforeLayer1.has_value() || ! beforeLayer2.has_value())
        return false;

    processor.selectLayerByVisualIndex (1);
    const auto layer2Id = beforeLayer2->layerId;

    const auto edited = processor.setLayerWaveformById (layer2Id, PolySynthAudioProcessor::Waveform::triangle)
        && processor.setLayerAdsrById (layer2Id, 0.41f, 0.32f, 0.52f, 0.77f);

    const auto afterLayer1 = processor.getLayerStateByVisualIndex (0);
    const auto afterLayer2 = processor.getLayerStateByVisualIndex (1);

    return expect (edited, "failed to apply selected-layer edits")
        && expect (afterLayer1.has_value() && afterLayer2.has_value(), "missing layer state after edit")
        && expect (afterLayer1->waveform == beforeLayer1->waveform, "layer 1 waveform should be unchanged")
        && expect (juce::approximatelyEqual (afterLayer1->attackSeconds, beforeLayer1->attackSeconds), "layer 1 attack should be unchanged")
        && expect (afterLayer2->waveform == PolySynthAudioProcessor::Waveform::triangle, "layer 2 waveform should be updated")
        && expect (juce::approximatelyEqual (afterLayer2->attackSeconds, 0.41f), "layer 2 attack should be updated");
}

bool validateInspectorSelectionMirrorsSelectedLayerValues()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;

    PolySynthAudioProcessor processor;
    processor.addDefaultLayerAndSelect();

    processor.setLayerWaveformByVisualIndex (0, PolySynthAudioProcessor::Waveform::saw);
    processor.setLayerAdsrByVisualIndex (0, 0.11f, 0.22f, 0.33f, 0.44f);
    processor.setLayerWaveformByVisualIndex (1, PolySynthAudioProcessor::Waveform::square);
    processor.setLayerAdsrByVisualIndex (1, 0.61f, 0.52f, 0.43f, 0.34f);

    processor.selectLayerByVisualIndex (0);
    PolySynthAudioProcessorEditor layer1Editor (processor);
    layer1Editor.setSize (960, 720);
    layer1Editor.resized();
    auto* layer1WaveformSelector = dynamic_cast<juce::ComboBox*> (layer1Editor.findChildWithID ("waveformSelector"));
    auto* layer1AttackSlider = dynamic_cast<juce::Slider*> (layer1Editor.findChildWithID ("attackSlider"));

    if (layer1WaveformSelector == nullptr || layer1AttackSlider == nullptr)
    {
        std::cerr << "missing inspector controls for layer 1" << '\n';
        return false;
    }

    const auto layer1Ok = expect (layer1WaveformSelector->getSelectedItemIndex() == static_cast<int> (PolySynthAudioProcessor::Waveform::saw),
                                  "layer 1 waveform should populate in inspector")
        && expect (juce::approximatelyEqual (static_cast<float> (layer1AttackSlider->getValue()), 0.11f),
                   "layer 1 attack should populate in inspector");

    processor.selectLayerByVisualIndex (1);
    PolySynthAudioProcessorEditor layer2Editor (processor);
    layer2Editor.setSize (960, 720);
    layer2Editor.resized();
    auto* layer2WaveformSelector = dynamic_cast<juce::ComboBox*> (layer2Editor.findChildWithID ("waveformSelector"));
    auto* layer2AttackSlider = dynamic_cast<juce::Slider*> (layer2Editor.findChildWithID ("attackSlider"));

    if (layer2WaveformSelector == nullptr || layer2AttackSlider == nullptr)
    {
        std::cerr << "missing inspector controls for layer 2" << '\n';
        return false;
    }

    const auto layer2Ok = expect (layer2WaveformSelector->getSelectedItemIndex() == static_cast<int> (PolySynthAudioProcessor::Waveform::square),
                                  "layer 2 waveform should populate in inspector")
        && expect (juce::approximatelyEqual (static_cast<float> (layer2AttackSlider->getValue()), 0.61f),
                   "layer 2 attack should populate in inspector");

    return layer1Ok && layer2Ok;
}

bool validateSerializedStateKeepsDistinctLayerFields()
{
    PolySynthAudioProcessor processor;
    if (! processor.addDefaultLayerAndSelect())
        return false;

    processor.setLayerWaveformByVisualIndex (0, PolySynthAudioProcessor::Waveform::sine);
    processor.setLayerAdsrByVisualIndex (0, 0.07f, 0.09f, 0.27f, 0.11f);
    processor.setLayerWaveformByVisualIndex (1, PolySynthAudioProcessor::Waveform::triangle);
    processor.setLayerAdsrByVisualIndex (1, 0.47f, 0.39f, 0.67f, 0.71f);

    auto tree = stateTreeFromProcessor (processor);
    if (! tree.isValid())
        return expect (false, "failed to serialize state");

    auto layers = tree.getChildWithName (layersNodeId);
    auto order = layers.getChildWithName (layerOrderNodeId);

    auto layer1 = getLayerNodeByVisualIndex (layers, order, 0).getChildWithName (layerStateNodeId);
    auto layer2 = getLayerNodeByVisualIndex (layers, order, 1).getChildWithName (layerStateNodeId);

    return expect (layer1.isValid() && layer2.isValid(), "serialized layer nodes missing")
        && expect (static_cast<int> (layer1.getProperty (waveformParameterId, -1)) == static_cast<int> (PolySynthAudioProcessor::Waveform::sine),
                   "serialized layer 1 waveform mismatch")
        && expect (static_cast<int> (layer2.getProperty (waveformParameterId, -1)) == static_cast<int> (PolySynthAudioProcessor::Waveform::triangle),
                   "serialized layer 2 waveform mismatch")
        && expect (juce::approximatelyEqual (static_cast<float> (layer1.getProperty (attackParameterId, 0.0f)), 0.07f), "serialized layer 1 attack mismatch")
        && expect (juce::approximatelyEqual (static_cast<float> (layer2.getProperty (attackParameterId, 0.0f)), 0.47f), "serialized layer 2 attack mismatch")
        && expect (juce::approximatelyEqual (static_cast<float> (layer2.getProperty (decayParameterId, 0.0f)), 0.39f), "serialized layer 2 decay mismatch")
        && expect (juce::approximatelyEqual (static_cast<float> (layer2.getProperty (sustainParameterId, 0.0f)), 0.67f), "serialized layer 2 sustain mismatch")
        && expect (juce::approximatelyEqual (static_cast<float> (layer2.getProperty (releaseParameterId, 0.0f)), 0.71f), "serialized layer 2 release mismatch");
}

bool validateMultiLayerAudioDifferencesSurviveRoundTrip()
{
    PolySynthAudioProcessor source;
    source.prepareToPlay (48000.0, 512);

    if (! source.addDefaultLayerAndSelect() || ! source.addDefaultLayerAndSelect() || ! source.addDefaultLayerAndSelect())
        return false;

    source.setLayerWaveformByVisualIndex (0, PolySynthAudioProcessor::Waveform::sine);
    source.setLayerWaveformByVisualIndex (1, PolySynthAudioProcessor::Waveform::saw);
    source.setLayerWaveformByVisualIndex (2, PolySynthAudioProcessor::Waveform::square);
    source.setLayerWaveformByVisualIndex (3, PolySynthAudioProcessor::Waveform::triangle);

    source.setLayerVolume (0, 1.00f);
    source.setLayerVolume (1, 0.75f);
    source.setLayerVolume (2, 0.50f);
    source.setLayerVolume (3, 0.25f);

    setRawParameterValue (source, attackParameterId, 0.001f);
    setRawParameterValue (source, decayParameterId, 0.001f);
    setRawParameterValue (source, sustainParameterId, 1.0f);
    setRawParameterValue (source, releaseParameterId, 0.01f);

    std::array<float, 4> beforePeaks {};
    for (std::size_t i = 0; i < beforePeaks.size(); ++i)
        beforePeaks[i] = renderSoloLayerPeak (source, i);

    auto serialized = stateTreeFromProcessor (source);
    if (! serialized.isValid())
        return false;

    PolySynthAudioProcessor restored;
    restored.prepareToPlay (48000.0, 512);

    if (! loadStateTreeIntoProcessor (restored, serialized))
        return false;

    std::array<float, 4> afterPeaks {};
    for (std::size_t i = 0; i < afterPeaks.size(); ++i)
        afterPeaks[i] = renderSoloLayerPeak (restored, i);

    return expect (beforePeaks[0] > beforePeaks[1] && beforePeaks[1] > beforePeaks[2] && beforePeaks[2] > beforePeaks[3],
                   "source layer peaks should be differentiated")
        && expect (afterPeaks[0] > afterPeaks[1] && afterPeaks[1] > afterPeaks[2] && afterPeaks[2] > afterPeaks[3],
                   "restored layer peaks should preserve differentiation")
        && expect (std::abs (afterPeaks[0] - beforePeaks[0]) <= 0.03f, "restored layer 1 peak mismatch")
        && expect (std::abs (afterPeaks[1] - beforePeaks[1]) <= 0.03f, "restored layer 2 peak mismatch")
        && expect (std::abs (afterPeaks[2] - beforePeaks[2]) <= 0.03f, "restored layer 3 peak mismatch")
        && expect (std::abs (afterPeaks[3] - beforePeaks[3]) <= 0.03f, "restored layer 4 peak mismatch");
}

bool validateLayer2NonBaseParameterEditsDoNotAffectLayer1()
{
    PolySynthAudioProcessor processor;
    if (! processor.addDefaultLayerAndSelect())
        return false;

    const auto layer1Before = processor.getLayerStateByVisualIndex (0);
    const auto layer2Before = processor.getLayerStateByVisualIndex (1);

    if (! layer1Before.has_value() || ! layer2Before.has_value())
        return false;

    const auto edited = processor.setLayerModParametersByVisualIndex (1, 0.65f, 7.5f, SynthVoice::ModulationDestination::pitch)
        && processor.setLayerUnisonByVisualIndex (1, 4, 17.0f)
        && processor.setLayerVelocitySensitivityByVisualIndex (1, 0.83f)
        && processor.setLayerOutputStageByVisualIndex (1, SynthEngine::OutputStage::softLimit);

    const auto layer1After = processor.getLayerStateByVisualIndex (0);
    const auto layer2After = processor.getLayerStateByVisualIndex (1);

    return expect (edited, "failed to edit non-base layer parameters")
        && expect (layer1After.has_value() && layer2After.has_value(), "missing layer states after non-base edits")
        && expect (juce::approximatelyEqual (layer1After->modulationDepth, layer1Before->modulationDepth), "layer 1 modulation depth changed unexpectedly")
        && expect (juce::approximatelyEqual (layer1After->modulationRateHz, layer1Before->modulationRateHz), "layer 1 modulation rate changed unexpectedly")
        && expect (layer1After->modulationDestination == layer1Before->modulationDestination, "layer 1 modulation destination changed unexpectedly")
        && expect (layer1After->unisonVoices == layer1Before->unisonVoices, "layer 1 unison voices changed unexpectedly")
        && expect (juce::approximatelyEqual (layer1After->unisonDetuneCents, layer1Before->unisonDetuneCents), "layer 1 unison detune changed unexpectedly")
        && expect (juce::approximatelyEqual (layer1After->velocitySensitivity, layer1Before->velocitySensitivity), "layer 1 velocity sensitivity changed unexpectedly")
        && expect (layer1After->outputStage == layer1Before->outputStage, "layer 1 output stage changed unexpectedly")
        && expect (juce::approximatelyEqual (layer2After->modulationDepth, 0.65f), "layer 2 modulation depth should update")
        && expect (juce::approximatelyEqual (layer2After->modulationRateHz, 7.5f), "layer 2 modulation rate should update")
        && expect (layer2After->modulationDestination == SynthVoice::ModulationDestination::pitch, "layer 2 modulation destination should update")
        && expect (layer2After->unisonVoices == 4, "layer 2 unison voices should update")
        && expect (juce::approximatelyEqual (layer2After->unisonDetuneCents, 17.0f), "layer 2 unison detune should update")
        && expect (juce::approximatelyEqual (layer2After->velocitySensitivity, 0.83f), "layer 2 velocity sensitivity should update")
        && expect (layer2After->outputStage == SynthEngine::OutputStage::softLimit, "layer 2 output stage should update");
}

bool validateNextLayerIdRemainsMonotonicAcrossDeleteAddAndReload()
{
    PolySynthAudioProcessor initial;
    initial.addDefaultLayerAndSelect();
    initial.addDefaultLayerAndSelect();
    initial.addDefaultLayerAndSelect();

    const auto layer0 = initial.getLayerStateByVisualIndex (0);
    const auto layer1 = initial.getLayerStateByVisualIndex (1);
    const auto layer2 = initial.getLayerStateByVisualIndex (2);
    const auto layer3 = initial.getLayerStateByVisualIndex (3);

    if (! layer0.has_value() || ! layer1.has_value() || ! layer2.has_value() || ! layer3.has_value())
        return false;

    const auto deletedLayerId = layer1->layerId;
    if (! initial.removeLayerWithSelectionFallback (1))
        return false;

    auto stateAfterDelete = stateTreeFromProcessor (initial);
    if (! stateAfterDelete.isValid())
        return false;

    PolySynthAudioProcessor reloaded;
    if (! loadStateTreeIntoProcessor (reloaded, stateAfterDelete))
        return false;

    if (! reloaded.addDefaultLayerAndSelect())
        return false;

    const auto addedAfterReload = reloaded.getLayerStateByVisualIndex (reloaded.getSelectedLayerVisualIndex());
    if (! addedAfterReload.has_value())
        return false;

    const auto maxInitialId = juce::jmax (juce::jmax (layer0->layerId, layer1->layerId), juce::jmax (layer2->layerId, layer3->layerId));
    const auto firstNewId = addedAfterReload->layerId;

    if (! expect (firstNewId > maxInitialId, "first reloaded add should allocate monotonic layer id")
        || ! expect (firstNewId != deletedLayerId, "first reloaded add should not reuse deleted id"))
        return false;

    const auto deleteNewestIndex = reloaded.getSelectedLayerVisualIndex();
    if (! reloaded.removeLayerWithSelectionFallback (deleteNewestIndex))
        return false;

    auto secondRoundTripState = stateTreeFromProcessor (reloaded);
    if (! secondRoundTripState.isValid())
        return false;

    PolySynthAudioProcessor reloadedAgain;
    if (! loadStateTreeIntoProcessor (reloadedAgain, secondRoundTripState))
        return false;

    if (! reloadedAgain.addDefaultLayerAndSelect())
        return false;

    const auto secondAdded = reloadedAgain.getLayerStateByVisualIndex (reloadedAgain.getSelectedLayerVisualIndex());
    if (! secondAdded.has_value())
        return false;

    return expect (secondAdded->layerId > firstNewId, "subsequent add after reload should keep ids monotonic")
        && expect (secondAdded->layerId != deletedLayerId, "subsequent add after reload should not reuse deleted ids");
}

bool validateSelectedLayerFallbackAfterDeleteReorderAndReload()
{
    PolySynthAudioProcessor processor;
    processor.addDefaultLayerAndSelect();
    processor.addDefaultLayerAndSelect();

    const auto layerA = processor.getLayerStateByVisualIndex (0);
    const auto layerB = processor.getLayerStateByVisualIndex (1);
    const auto layerC = processor.getLayerStateByVisualIndex (2);
    if (! layerA.has_value() || ! layerB.has_value() || ! layerC.has_value())
        return false;

    processor.selectLayerByVisualIndex (2); // select C
    processor.moveLayerUp (2);              // order A, C, B. selected C now at visual index 1.

    if (processor.getSelectedLayerVisualIndex() != 1)
        return false;

    if (! processor.removeLayerWithSelectionFallback (1))
        return false;

    const auto selectedAfterDelete = processor.getLayerStateByVisualIndex (processor.getSelectedLayerVisualIndex());
    if (! selectedAfterDelete.has_value())
        return false;

    if (! expect (selectedAfterDelete->layerId == layerA->layerId, "delete fallback should select previous visual layer"))
        return false;

    processor.moveLayerDown (0); // order B, A and selected A tracks to visual index 1.

    if (! expect (processor.getSelectedLayerVisualIndex() == 1, "selected layer should track reorder movements"))
        return false;

    auto saved = stateTreeFromProcessor (processor);
    if (! saved.isValid())
        return false;

    PolySynthAudioProcessor restored;
    if (! loadStateTreeIntoProcessor (restored, saved))
        return false;

    const auto restoredSelected = restored.getLayerStateByVisualIndex (restored.getSelectedLayerVisualIndex());
    if (! restoredSelected.has_value())
        return false;

    return expect (restored.getSelectedLayerVisualIndex() == 1, "selected visual index should survive save/load after reorder")
        && expect (restoredSelected->layerId == layerA->layerId, "selected layer identity should survive save/load after fallback and reorder");
}

bool validateMalformedLayersPayloadRepairsDeterministically()
{
    PolySynthAudioProcessor source;
    source.addDefaultLayerAndSelect();
    source.addDefaultLayerAndSelect();

    auto sourceTree = stateTreeFromProcessor (source);
    if (! sourceTree.isValid())
        return false;

    auto layersNode = sourceTree.getChildWithName (layersNodeId);
    auto orderNode = layersNode.getChildWithName (layerOrderNodeId);

    const auto originalOrder = getLayerOrderFromStateTree (sourceTree);
    if (originalOrder.size() < 3)
        return false;

    orderNode.removeAllChildren (nullptr);
    juce::ValueTree unknownOrderItem (orderItemNodeId);
    unknownOrderItem.setProperty (layerIdPropertyId, static_cast<juce::int64> (99999), nullptr);
    orderNode.addChild (unknownOrderItem, -1, nullptr);

    juce::ValueTree validOrderItem (orderItemNodeId);
    validOrderItem.setProperty (layerIdPropertyId, static_cast<juce::int64> (originalOrder[1]), nullptr);
    orderNode.addChild (validOrderItem, -1, nullptr);

    layersNode.setProperty (selectedLayerIdPropertyId, static_cast<juce::int64> (55555), nullptr);
    layersNode.setProperty (nextLayerIdPropertyId, static_cast<juce::int64> (1), nullptr);

    PolySynthAudioProcessor repaired;
    if (! loadStateTreeIntoProcessor (repaired, sourceTree))
        return false;

    auto repairedTree = stateTreeFromProcessor (repaired);
    if (! repairedTree.isValid())
        return false;

    const auto repairedOrder = getLayerOrderFromStateTree (repairedTree);
    if (! expect (repairedOrder.size() == originalOrder.size(), "repaired order should include all known layers once"))
        return false;

    if (! expect (repairedOrder[0] == originalOrder[1], "first repaired order id should preserve valid entry from malformed payload"))
        return false;

    if (! expect (std::find (repairedOrder.begin(), repairedOrder.end(), static_cast<uint64_t> (99999)) == repairedOrder.end(),
                 "repaired order should drop unknown layer ids"))
        return false;

    if (! expect (repaired.getSelectedLayerVisualIndex() == 0, "invalid selectedLayerId should deterministically fall back to first repaired layer"))
        return false;

    if (! repaired.addDefaultLayerAndSelect())
        return false;

    const auto addedLayer = repaired.getLayerStateByVisualIndex (repaired.getSelectedLayerVisualIndex());
    if (! addedLayer.has_value())
        return false;

    const auto maxOriginalId = *std::max_element (originalOrder.begin(), originalOrder.end());
    return expect (addedLayer->layerId > maxOriginalId, "repaired nextLayerId should be monotonic even when payload regresses it");
}

} // namespace

int main()
{
    if (! validateSelectedLayerEditingTouchesOnlySelectedLayer())
        return 1;

    if (! validateInspectorSelectionMirrorsSelectedLayerValues())
        return 1;

    if (! validateSerializedStateKeepsDistinctLayerFields())
        return 1;

    if (! validateMultiLayerAudioDifferencesSurviveRoundTrip())
        return 1;

    if (! validateLayer2NonBaseParameterEditsDoNotAffectLayer1())
        return 1;

    if (! validateNextLayerIdRemainsMonotonicAcrossDeleteAddAndReload())
        return 1;

    if (! validateSelectedLayerFallbackAfterDeleteReorderAndReload())
        return 1;

    if (! validateMalformedLayersPayloadRepairsDeterministically())
        return 1;

    std::cout << "layer selection editing validation passed." << '\n';
    return 0;
}
