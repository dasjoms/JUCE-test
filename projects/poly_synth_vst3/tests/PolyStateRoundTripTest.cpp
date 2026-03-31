#include "../PolySynthAudioProcessor.h"

#include <iostream>

namespace
{
constexpr auto maxVoicesParameterId = "maxVoices";
constexpr auto stealPolicyParameterId = "stealPolicy";
constexpr auto decayParameterId = "decay";
constexpr auto sustainParameterId = "sustain";
constexpr auto modulationDepthParameterId = "modDepth";
constexpr auto modulationRateParameterId = "modRate";

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

bool validatePolyParametersRoundTrip()
{
    PolySynthAudioProcessor source;

    if (! setRawParameterValue (source, maxVoicesParameterId, 12.0f)
        || ! setRawParameterValue (source, stealPolicyParameterId, 2.0f)
        || ! setRawParameterValue (source, decayParameterId, 0.12f)
        || ! setRawParameterValue (source, sustainParameterId, 0.45f)
        || ! setRawParameterValue (source, modulationDepthParameterId, 0.75f)
        || ! setRawParameterValue (source, modulationRateParameterId, 6.5f))
    {
        std::cerr << "unable to configure source processor for round-trip test" << '\n';
        return false;
    }

    juce::MemoryBlock state;
    source.getStateInformation (state);

    PolySynthAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    if (juce::roundToInt (getRawParameterValue (restored, maxVoicesParameterId)) != 12)
    {
        std::cerr << "maxVoices mismatch after round-trip" << '\n';
        return false;
    }

    if (juce::roundToInt (getRawParameterValue (restored, stealPolicyParameterId)) != 2)
    {
        std::cerr << "stealPolicy mismatch after round-trip" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual (getRawParameterValue (restored, modulationDepthParameterId), 0.75f))
    {
        std::cerr << "modulation depth mismatch after round-trip" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual (getRawParameterValue (restored, modulationRateParameterId), 6.5f))
    {
        std::cerr << "modulation rate mismatch after round-trip" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual (getRawParameterValue (restored, decayParameterId), 0.12f))
    {
        std::cerr << "decay mismatch after round-trip" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual (getRawParameterValue (restored, sustainParameterId), 0.45f))
    {
        std::cerr << "sustain mismatch after round-trip" << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    if (! validatePolyParametersRoundTrip())
        return 1;

    std::cout << "poly state round-trip validation passed." << '\n';
    return 0;
}
