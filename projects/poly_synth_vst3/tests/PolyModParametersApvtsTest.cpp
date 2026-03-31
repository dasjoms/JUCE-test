#include "../PolySynthAudioProcessor.h"

#include <iostream>

namespace
{
constexpr auto modulationDepthParameterId = "modDepth";
constexpr auto modulationRateParameterId = "modRate";

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

    if (depthRaw == nullptr || rateRaw == nullptr)
    {
        std::cerr << "missing one or more modulation parameters in APVTS" << '\n';
        return false;
    }

    constexpr auto depthTarget = 0.67f;
    constexpr auto rateTarget = 7.25f;

    if (! setRawParameterValue (processor, modulationDepthParameterId, depthTarget)
        || ! setRawParameterValue (processor, modulationRateParameterId, rateTarget))
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

    return true;
}

} // namespace

int main()
{
    if (! validateModulationParametersPresentAndWritable())
        return 1;

    std::cout << "poly APVTS modulation parameter validation passed." << '\n';
    return 0;
}
