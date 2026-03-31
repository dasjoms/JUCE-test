#include "../PolySynthAudioProcessor.h"

#include <iostream>

namespace
{
constexpr auto modulationDepthParameterId = "modDepth";
constexpr auto modulationRateParameterId = "modRate";
constexpr auto modulationDestinationParameterId = "modDestination";
constexpr auto velocitySensitivityParameterId = "velocitySensitivity";
constexpr auto decayParameterId = "decay";
constexpr auto sustainParameterId = "sustain";

bool setRawParameterValue (PolySynthAudioProcessor& processor, juce::StringRef parameterId, float rawValue)
{
    if (auto* parameter = processor.getValueTreeState().getParameter (parameterId))
    {
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (rawValue));
        return true;
    }

    return false;
}

bool validateModulationParametersPresentAndWritable()
{
    PolySynthAudioProcessor processor;
    auto& apvts = processor.getValueTreeState();

    auto* depthRaw = apvts.getRawParameterValue (modulationDepthParameterId);
    auto* rateRaw = apvts.getRawParameterValue (modulationRateParameterId);
    auto* destinationRaw = apvts.getRawParameterValue (modulationDestinationParameterId);
    auto* velocitySensitivityRaw = apvts.getRawParameterValue (velocitySensitivityParameterId);

    if (depthRaw == nullptr || rateRaw == nullptr || destinationRaw == nullptr || velocitySensitivityRaw == nullptr)
    {
        std::cerr << "missing one or more modulation parameters in APVTS" << '\n';
        return false;
    }

    constexpr auto depthTarget = 0.67f;
    constexpr auto rateTarget = 7.25f;
    constexpr auto destinationTarget = 1.0f;
    constexpr auto velocitySensitivityTarget = 0.74f;

    if (! setRawParameterValue (processor, modulationDepthParameterId, depthTarget)
        || ! setRawParameterValue (processor, modulationRateParameterId, rateTarget)
        || ! setRawParameterValue (processor, modulationDestinationParameterId, destinationTarget)
        || ! setRawParameterValue (processor, velocitySensitivityParameterId, velocitySensitivityTarget))
    {
        std::cerr << "unable to write modulation parameters through APVTS" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual (depthRaw->load(), depthTarget))
    {
        std::cerr << "modDepth write mismatch" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual (rateRaw->load(), rateTarget))
    {
        std::cerr << "modRate write mismatch" << '\n';
        return false;
    }

    if (juce::roundToInt (destinationRaw->load()) != static_cast<int> (destinationTarget))
    {
        std::cerr << "modDestination write mismatch" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual (velocitySensitivityRaw->load(), velocitySensitivityTarget))
    {
        std::cerr << "velocitySensitivity write mismatch" << '\n';
        return false;
    }

    return true;
}

bool validateAdsrParametersPresentAndWritable()
{
    PolySynthAudioProcessor processor;
    auto& apvts = processor.getValueTreeState();

    auto* decayRaw = apvts.getRawParameterValue (decayParameterId);
    auto* sustainRaw = apvts.getRawParameterValue (sustainParameterId);

    if (decayRaw == nullptr || sustainRaw == nullptr)
    {
        std::cerr << "missing one or more ADSR parameters in APVTS" << '\n';
        return false;
    }

    constexpr auto decayTarget = 0.19f;
    constexpr auto sustainTarget = 0.43f;

    if (! setRawParameterValue (processor, decayParameterId, decayTarget)
        || ! setRawParameterValue (processor, sustainParameterId, sustainTarget))
    {
        std::cerr << "unable to write ADSR parameters through APVTS" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual (decayRaw->load(), decayTarget))
    {
        std::cerr << "decay write mismatch" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual (sustainRaw->load(), sustainTarget))
    {
        std::cerr << "sustain write mismatch" << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    if (! validateModulationParametersPresentAndWritable())
        return 1;

    if (! validateAdsrParametersPresentAndWritable())
        return 1;

    std::cout << "poly APVTS modulation parameter validation passed." << '\n';
    return 0;
}
