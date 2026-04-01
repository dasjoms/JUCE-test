#include "../SynthVoice.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
std::vector<float> renderVoice (float modulationDepth,
                                float modulationRateHz,
                                SynthVoice::ModulationDestination destination,
                                SynthVoice::Waveform waveform,
                                int sampleCount)
{
    SynthVoice voice;
    voice.prepare (48000.0);
    voice.setWaveform (waveform);
    voice.setEnvelopeTimes (0.001f, 0.04f, 0.9f, 0.05f);
    voice.setModulationParameters (modulationDepth, modulationRateHz, destination);
    voice.noteOn (60, 1.0f);

    std::vector<float> rendered (static_cast<size_t> (sampleCount));

    for (int sample = 0; sample < sampleCount; ++sample)
        rendered[static_cast<size_t> (sample)] = voice.renderSample();

    return rendered;
}

bool renderedBuffersDiffer (const std::vector<float>& lhs, const std::vector<float>& rhs, float epsilon)
{
    for (size_t sample = 0; sample < lhs.size(); ++sample)
    {
        if (std::abs (lhs[sample] - rhs[sample]) > epsilon)
            return true;
    }

    return false;
}

bool validateAmplitudeDestinationChangesOutput()
{
    const auto dry = renderVoice (0.0f, 5.0f, SynthVoice::ModulationDestination::amplitude, SynthVoice::Waveform::sine, 512);
    const auto tremolo = renderVoice (1.0f, 5.0f, SynthVoice::ModulationDestination::amplitude, SynthVoice::Waveform::sine, 512);

    if (renderedBuffersDiffer (dry, tremolo, 1.0e-5f))
        return true;

    std::cerr << "expected amplitude modulation destination to change rendered output" << '\n';
    return false;
}

bool validatePitchDestinationChangesOutput()
{
    const auto dry = renderVoice (0.0f, 5.0f, SynthVoice::ModulationDestination::pitch, SynthVoice::Waveform::sine, 512);
    const auto vibrato = renderVoice (0.8f, 5.0f, SynthVoice::ModulationDestination::pitch, SynthVoice::Waveform::sine, 512);

    if (renderedBuffersDiffer (dry, vibrato, 1.0e-5f))
        return true;

    std::cerr << "expected pitch modulation destination to change rendered output" << '\n';
    return false;
}

bool validatePulseWidthDestinationChangesSquareOutput()
{
    const auto dry = renderVoice (0.0f, 5.0f, SynthVoice::ModulationDestination::pulseWidth, SynthVoice::Waveform::square, 512);
    const auto pulseWidthModulated = renderVoice (0.9f, 5.0f, SynthVoice::ModulationDestination::pulseWidth, SynthVoice::Waveform::square, 512);

    if (renderedBuffersDiffer (dry, pulseWidthModulated, 1.0e-5f))
        return true;

    std::cerr << "expected pulse-width destination to change square-wave output" << '\n';
    return false;
}

bool validateModulationIsDeterministicPerVoice()
{
    const auto firstPass = renderVoice (0.67f, 7.5f, SynthVoice::ModulationDestination::pitch, SynthVoice::Waveform::sine, 512);
    const auto secondPass = renderVoice (0.67f, 7.5f, SynthVoice::ModulationDestination::pitch, SynthVoice::Waveform::sine, 512);

    for (size_t sample = 0; sample < firstPass.size(); ++sample)
    {
        if (std::abs (firstPass[sample] - secondPass[sample]) > 1.0e-7f)
        {
            std::cerr << "voice modulation output changed between identical renders at sample " << sample << '\n';
            return false;
        }
    }

    return true;
}

bool validateOffDestinationBypassesModulation()
{
    const auto dry = renderVoice (0.0f, 6.0f, SynthVoice::ModulationDestination::off, SynthVoice::Waveform::square, 512);
    const auto modulatedButOff = renderVoice (0.95f, 6.0f, SynthVoice::ModulationDestination::off, SynthVoice::Waveform::square, 512);

    for (size_t sample = 0; sample < dry.size(); ++sample)
    {
        if (std::abs (dry[sample] - modulatedButOff[sample]) > 1.0e-7f)
        {
            std::cerr << "expected modulation destination Off to bypass modulation at sample " << sample << '\n';
            return false;
        }
    }

    return true;
}

} // namespace

int main()
{
    if (! validateAmplitudeDestinationChangesOutput())
        return 1;

    if (! validatePitchDestinationChangesOutput())
        return 1;

    if (! validatePulseWidthDestinationChangesSquareOutput())
        return 1;

    if (! validateModulationIsDeterministicPerVoice())
        return 1;

    if (! validateOffDestinationBypassesModulation())
        return 1;

    std::cout << "synth voice modulation validation passed." << '\n';
    return 0;
}
