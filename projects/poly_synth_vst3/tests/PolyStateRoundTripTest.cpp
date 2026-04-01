#include "../PolySynthAudioProcessor.h"

#include <iostream>
#include <vector>

namespace
{
struct ParameterFixture
{
    juce::StringRef parameterId;
    float value;
    bool expectInteger;
};

constexpr auto waveformParameterId = "waveform";
constexpr auto maxVoicesParameterId = "maxVoices";
constexpr auto stealPolicyParameterId = "stealPolicy";
constexpr auto attackParameterId = "attack";
constexpr auto decayParameterId = "decay";
constexpr auto sustainParameterId = "sustain";
constexpr auto releaseParameterId = "release";
constexpr auto modulationDepthParameterId = "modDepth";
constexpr auto modulationRateParameterId = "modRate";
constexpr auto modulationDestinationParameterId = "modDestination";
constexpr auto velocitySensitivityParameterId = "velocitySensitivity";
constexpr auto unisonVoicesParameterId = "unisonVoices";
constexpr auto unisonDetuneCentsParameterId = "unisonDetuneCents";
constexpr auto outputStageParameterId = "outputStage";

const std::vector<ParameterFixture> allUserFacingParameterFixtures {
    { waveformParameterId, 3.0f, true },
    { maxVoicesParameterId, 12.0f, true },
    { stealPolicyParameterId, 2.0f, true },
    { attackParameterId, 0.031f, false },
    { decayParameterId, 0.12f, false },
    { sustainParameterId, 0.45f, false },
    { releaseParameterId, 0.61f, false },
    { modulationDepthParameterId, 0.75f, false },
    { modulationRateParameterId, 6.5f, false },
    { modulationDestinationParameterId, 1.0f, true },
    { velocitySensitivityParameterId, 0.85f, false },
    { unisonVoicesParameterId, 5.0f, true },
    { unisonDetuneCentsParameterId, 22.0f, false },
    { outputStageParameterId, 2.0f, true }
};

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
    // Regression guard: keep this fixture in sync with the current UI/APVTS user-facing parameter list.
    PolySynthAudioProcessor source;

    for (const auto& fixture : allUserFacingParameterFixtures)
    {
        if (! setRawParameterValue (source, fixture.parameterId, fixture.value))
        {
            std::cerr << "unable to configure source processor for round-trip test: "
                      << juce::String (fixture.parameterId) << '\n';
            return false;
        }
    }

    juce::MemoryBlock state;
    source.getStateInformation (state);

    PolySynthAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    for (const auto& fixture : allUserFacingParameterFixtures)
    {
        const auto actualValue = getRawParameterValue (restored, fixture.parameterId);

        if (fixture.expectInteger)
        {
            if (juce::roundToInt (actualValue) != juce::roundToInt (fixture.value))
            {
                std::cerr << "round-trip mismatch for " << juce::String (fixture.parameterId)
                          << ": expected " << juce::roundToInt (fixture.value)
                          << ", got " << juce::roundToInt (actualValue) << '\n';
                return false;
            }
        }
        else if (! juce::approximatelyEqual (actualValue, fixture.value))
        {
            std::cerr << "round-trip mismatch for " << juce::String (fixture.parameterId)
                      << ": expected " << fixture.value
                      << ", got " << actualValue << '\n';
            return false;
        }
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
