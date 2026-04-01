#include "../PolySynthAudioProcessor.h"
#include "../PolySynthAudioProcessorEditor.h"

#include <iostream>

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

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    auto xml = PolySynthAudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));

    if (xml == nullptr)
        return expect (false, "failed to serialize state");

    auto tree = juce::ValueTree::fromXml (*xml);
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

} // namespace

int main()
{
    if (! validateSelectedLayerEditingTouchesOnlySelectedLayer())
        return 1;

    if (! validateInspectorSelectionMirrorsSelectedLayerValues())
        return 1;

    if (! validateSerializedStateKeepsDistinctLayerFields())
        return 1;

    std::cout << "layer selection editing validation passed." << '\n';
    return 0;
}
