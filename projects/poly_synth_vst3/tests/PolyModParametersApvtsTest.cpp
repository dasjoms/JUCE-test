#include "../PolySynthAudioProcessor.h"
#include "../PolySynthAudioProcessorEditor.h"

#include <iostream>

namespace
{
constexpr auto modulationDepthParameterId = "modDepth";
constexpr auto modulationRateParameterId = "modRate";
constexpr auto modulationDestinationParameterId = "modDestination";
constexpr auto velocitySensitivityParameterId = "velocitySensitivity";
constexpr auto decayParameterId = "decay";
constexpr auto sustainParameterId = "sustain";
constexpr auto unisonVoicesParameterId = "unisonVoices";
constexpr auto unisonDetuneCentsParameterId = "unisonDetuneCents";
constexpr auto outputStageParameterId = "outputStage";

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
    auto* unisonVoicesRaw = apvts.getRawParameterValue (unisonVoicesParameterId);
    auto* unisonDetuneRaw = apvts.getRawParameterValue (unisonDetuneCentsParameterId);
    auto* outputStageRaw = apvts.getRawParameterValue (outputStageParameterId);

    if (depthRaw == nullptr || rateRaw == nullptr || destinationRaw == nullptr || velocitySensitivityRaw == nullptr
        || unisonVoicesRaw == nullptr || unisonDetuneRaw == nullptr || outputStageRaw == nullptr)
    {
        std::cerr << "missing one or more modulation parameters in APVTS" << '\n';
        return false;
    }

    constexpr auto depthTarget = 0.67f;
    constexpr auto rateTarget = 7.25f;
    constexpr auto destinationTarget = 1.0f;
    constexpr auto velocitySensitivityTarget = 0.74f;
    constexpr auto unisonVoicesTarget = 4.0f;
    constexpr auto unisonDetuneTarget = 12.5f;
    constexpr auto outputStageTarget = 2.0f;

    if (! setRawParameterValue (processor, modulationDepthParameterId, depthTarget)
        || ! setRawParameterValue (processor, modulationRateParameterId, rateTarget)
        || ! setRawParameterValue (processor, modulationDestinationParameterId, destinationTarget)
        || ! setRawParameterValue (processor, velocitySensitivityParameterId, velocitySensitivityTarget)
        || ! setRawParameterValue (processor, unisonVoicesParameterId, unisonVoicesTarget)
        || ! setRawParameterValue (processor, unisonDetuneCentsParameterId, unisonDetuneTarget)
        || ! setRawParameterValue (processor, outputStageParameterId, outputStageTarget))
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

    if (juce::roundToInt (unisonVoicesRaw->load()) != static_cast<int> (unisonVoicesTarget))
    {
        std::cerr << "unisonVoices write mismatch" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual (unisonDetuneRaw->load(), unisonDetuneTarget))
    {
        std::cerr << "unisonDetuneCents write mismatch" << '\n';
        return false;
    }

    if (juce::roundToInt (outputStageRaw->load()) != static_cast<int> (outputStageTarget))
    {
        std::cerr << "outputStage write mismatch" << '\n';
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

bool validateEditorControlAttachmentsForEngineParameters()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;

    PolySynthAudioProcessor processor;
    PolySynthAudioProcessorEditor editor (processor);
    editor.setSize (520, 540);
    editor.resized();

    auto* decayControl = dynamic_cast<juce::Slider*> (editor.findChildWithID ("decaySlider"));
    auto* sustainControl = dynamic_cast<juce::Slider*> (editor.findChildWithID ("sustainSlider"));
    auto* velocityControl = dynamic_cast<juce::Slider*> (editor.findChildWithID ("velocitySensitivitySlider"));
    auto* destinationControl = dynamic_cast<juce::ComboBox*> (editor.findChildWithID ("modDestinationSelector"));
    auto* unisonVoicesControl = dynamic_cast<juce::Slider*> (editor.findChildWithID ("unisonVoicesSlider"));
    auto* unisonDetuneControl = dynamic_cast<juce::Slider*> (editor.findChildWithID ("unisonDetuneCentsSlider"));
    auto* outputStageControl = dynamic_cast<juce::ComboBox*> (editor.findChildWithID ("outputStageSelector"));

    if (decayControl == nullptr || sustainControl == nullptr || velocityControl == nullptr
        || destinationControl == nullptr || unisonVoicesControl == nullptr || unisonDetuneControl == nullptr
        || outputStageControl == nullptr)
    {
        std::cerr << "missing one or more UI controls for engine-facing APVTS parameters" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual ((float) decayControl->getMaximum(), 5.0f)
        || ! juce::approximatelyEqual ((float) decayControl->getInterval(), 0.001f))
    {
        std::cerr << "decay slider range mismatch" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual ((float) sustainControl->getMinimum(), 0.0f)
        || ! juce::approximatelyEqual ((float) sustainControl->getMaximum(), 1.0f))
    {
        std::cerr << "sustain slider range mismatch" << '\n';
        return false;
    }

    if (! juce::approximatelyEqual ((float) unisonDetuneControl->getMaximum(), 50.0f)
        || unisonDetuneControl->getTextValueSuffix() != " cents")
    {
        std::cerr << "unison detune slider range/suffix mismatch" << '\n';
        return false;
    }

    if (destinationControl->getNumItems() < 2)
    {
        std::cerr << "mod destination control choices missing" << '\n';
        return false;
    }

    if (outputStageControl->getNumItems() != 3)
    {
        std::cerr << "output stage control choices missing" << '\n';
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

    if (! validateEditorControlAttachmentsForEngineParameters())
        return 1;

    std::cout << "poly APVTS modulation parameter validation passed." << '\n';
    return 0;
}
