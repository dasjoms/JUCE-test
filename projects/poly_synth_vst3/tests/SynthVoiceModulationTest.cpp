#include "../SynthVoice.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
std::vector<float> renderVoice (float modulationDepth, float modulationRateHz, int sampleCount)
{
    SynthVoice voice;
    voice.prepare (48000.0);
    voice.setWaveform (SynthVoice::Waveform::sine);
    voice.setEnvelopeTimes (0.001f, 0.05f);
    voice.setModulationParameters (modulationDepth, modulationRateHz);
    voice.noteOn (60);

    std::vector<float> rendered (static_cast<size_t> (sampleCount));

    for (int sample = 0; sample < sampleCount; ++sample)
        rendered[static_cast<size_t> (sample)] = voice.renderSample();

    return rendered;
}

bool validateModulationChangesOutput()
{
    const auto dry = renderVoice (0.0f, 5.0f, 512);
    const auto tremolo = renderVoice (1.0f, 5.0f, 512);

    for (size_t sample = 0; sample < dry.size(); ++sample)
    {
        if (std::abs (dry[sample] - tremolo[sample]) > 1.0e-5f)
            return true;
    }

    std::cerr << "expected non-zero modulation depth to change rendered output" << '\n';
    return false;
}

bool validateModulationIsDeterministicPerVoice()
{
    const auto firstPass = renderVoice (0.67f, 7.5f, 512);
    const auto secondPass = renderVoice (0.67f, 7.5f, 512);

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

} // namespace

int main()
{
    if (! validateModulationChangesOutput())
        return 1;

    if (! validateModulationIsDeterministicPerVoice())
        return 1;

    std::cout << "synth voice modulation validation passed." << '\n';
    return 0;
}
