#include "../PolySynthAudioProcessor.h"

#include <iostream>

namespace
{

bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';

    return condition;
}

float renderPeak (PolySynthAudioProcessor& processor, const juce::MidiBuffer& midi, int numSamples = 256)
{
    juce::AudioBuffer<float> buffer (2, numSamples);
    buffer.clear();

    auto midiCopy = midi;
    processor.processBlock (buffer, midiCopy);

    return juce::jmax (buffer.getMagnitude (0, 0, numSamples), buffer.getMagnitude (1, 0, numSamples));
}

void tickProcessor (PolySynthAudioProcessor& processor)
{
    processor.prepareToPlay (48000.0, 32);
    juce::AudioBuffer<float> buffer (2, 32);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);
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

bool validateAllLayersReceiveMidiByDefault()
{
    PolySynthAudioProcessor singleLayer;
    singleLayer.prepareToPlay (48000.0, 256);

    PolySynthAudioProcessor dualLayer;
    dualLayer.addDefaultLayerAndSelect();
    dualLayer.prepareToPlay (48000.0, 256);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 120), 0);

    const auto singlePeak = renderPeak (singleLayer, midi);
    const auto dualPeak = renderPeak (dualLayer, midi);

    return expect (singlePeak > 0.0f, "single layer should produce audible output")
        && expect (dualPeak > singlePeak * 1.7f, "all layers should receive MIDI by default");
}

bool validateMuteSoloAudibilityRules()
{
    PolySynthAudioProcessor processor;
    processor.addDefaultLayerAndSelect();
    processor.addDefaultLayerAndSelect();
    processor.prepareToPlay (48000.0, 256);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);

    const auto allAudiblePeak = renderPeak (processor, midi);

    processor.setLayerMute (1, true);
    processor.setLayerMute (2, true);
    const auto mutedPeak = renderPeak (processor, midi);

    processor.setLayerMute (1, false);
    processor.setLayerMute (2, false);
    processor.setLayerSolo (2, true);
    const auto oneSoloPeak = renderPeak (processor, midi);

    processor.setLayerSolo (0, true);
    const auto twoSoloPeak = renderPeak (processor, midi);

    return expect (allAudiblePeak > 0.0f, "all layers should be audible before mute/solo")
        && expect (mutedPeak < allAudiblePeak * 0.6f, "muted layers should be inaudible when no solo is active")
        && expect (oneSoloPeak > 0.0f, "a soloed layer should remain audible")
        && expect (oneSoloPeak < allAudiblePeak * 0.6f, "solo should isolate only soloed layer(s)")
        && expect (twoSoloPeak > oneSoloPeak * 1.7f, "multiple soloed layers should be audible together");
}

bool validatePerLayerVoiceCountRespected()
{
    constexpr auto layersNodeId = "LAYERS";
    constexpr auto layerNodeId = "LAYER";
    constexpr auto layerOrderNodeId = "LAYER_ORDER";
    constexpr auto layerStateNodeId = "layerState";
    constexpr auto layerIdPropertyId = "layerId";
    constexpr auto maxVoicesParameterId = "maxVoices";

    PolySynthAudioProcessor processor;

    if (! setRawParameterValue (processor, "maxVoices", 1.0f))
        return false;

    tickProcessor (processor);
    processor.duplicateLayerAndSelect (0);

    if (! setRawParameterValue (processor, "maxVoices", 4.0f))
        return false;

    tickProcessor (processor);
    processor.duplicateLayerAndSelect (0);

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    auto xml = PolySynthAudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    if (xml == nullptr)
        return false;

    auto tree = juce::ValueTree::fromXml (*xml);
    auto layers = tree.getChildWithName (layersNodeId);
    auto order = layers.getChildWithName (layerOrderNodeId);

    auto findVoiceCountByVisualIndex = [&] (int visualIndex) -> int
    {
        const auto targetLayerId = static_cast<juce::int64> (order.getChild (visualIndex).getProperty (layerIdPropertyId, 0));

        for (int i = 0; i < layers.getNumChildren(); ++i)
        {
            auto layer = layers.getChild (i);
            if (! layer.hasType (layerNodeId))
                continue;

            if (static_cast<juce::int64> (layer.getProperty (layerIdPropertyId, 0)) == targetLayerId)
                return static_cast<int> (layer.getChildWithName (layerStateNodeId).getProperty (maxVoicesParameterId, -1));
        }

        return -1;
    };

    return expect (findVoiceCountByVisualIndex (1) == 1, "first duplicated layer should keep one voice")
        && expect (findVoiceCountByVisualIndex (2) == 4, "second duplicated layer should keep four voices");
}

} // namespace

int main()
{
    if (! validateAllLayersReceiveMidiByDefault())
        return 1;

    if (! validateMuteSoloAudibilityRules())
        return 1;

    if (! validatePerLayerVoiceCountRespected())
        return 1;

    std::cout << "poly layer audio behaviour validation passed." << '\n';
    return 0;
}
