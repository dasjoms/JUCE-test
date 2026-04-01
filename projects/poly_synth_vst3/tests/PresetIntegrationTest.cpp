#include "../PolySynthAudioProcessor.h"

#include <iostream>
#include <optional>

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';

    return condition;
}

void configureDistinctState (PolySynthAudioProcessor& processor)
{
    processor.addDefaultLayerAndSelect();

    processor.selectLayerByVisualIndex (0);
    processor.setLayerWaveformByVisualIndex (0, SynthVoice::Waveform::triangle);
    processor.setLayerVoiceCountByVisualIndex (0, 5);
    processor.setLayerRootNoteAbsolute (0, 50);

    processor.selectLayerByVisualIndex (1);
    processor.setLayerWaveformByVisualIndex (1, SynthVoice::Waveform::square);
    processor.setLayerVoiceCountByVisualIndex (1, 3);
    processor.setLayerStealPolicyByVisualIndex (1, SynthEngine::VoiceStealPolicy::oldest);
    processor.setLayerRootNoteAbsolute (1, 67);
    processor.setLayerMute (1, true);
    processor.setLayerVolume (1, 0.31f);

    processor.selectLayerByVisualIndex (0);
}

bool expectLayer (PolySynthAudioProcessor& processor,
                  std::size_t visualIndex,
                  int rootNote,
                  bool mute,
                  float volume,
                  std::optional<SynthVoice::Waveform> waveform = std::nullopt,
                  std::optional<int> voiceCount = std::nullopt)
{
    const auto state = processor.getLayerStateByVisualIndex (visualIndex);
    if (! expect (state.has_value(), "expected layer state to exist"))
        return false;

    auto ok = true;
    if (waveform.has_value())
        ok = expect (state->waveform == *waveform, "waveform mismatch") && ok;

    if (voiceCount.has_value())
        ok = expect (state->voiceCount == *voiceCount, "voice count mismatch") && ok;

    return ok
        && expect (state->rootNoteAbsolute == rootNote, "root note mismatch")
        && expect (state->mute == mute, "mute mismatch")
        && expect (juce::approximatelyEqual (state->layerVolume, volume), "volume mismatch");
}

bool verifyRoundTripAndSaveSemantics (const juce::File& root)
{
    PolySynthAudioProcessor processor (root);
    configureDistinctState (processor);

    auto saveAsNew = processor.saveCurrentPresetAsNew ("StackA");
    auto duplicate = processor.saveCurrentPresetAsNew ("stacka");

    processor.setLayerRootNoteAbsolute (0, 62);
    auto overwrite = processor.saveCurrentPresetOverwrite();

    processor.setLayerRootNoteAbsolute (0, 44);
    auto secondPreset = processor.saveCurrentPresetAsNew ("StackB");

    processor.setLayerRootNoteAbsolute (0, 33);
    auto loadStackA = processor.loadPresetByName ("StackA");

    auto names = processor.listLocalPresetNames();
    names.sortNatural();

    const auto rootRestored = processor.getLayerRootNoteAbsolute (0);
    const auto firstLayerOk = expectLayer (processor, 0, 62, false, 1.0f);
    const auto secondLayerOk = expectLayer (processor, 1, 67, true, 0.31f, SynthVoice::Waveform::square, 3);

    const auto namesOk = expect (names.size() == 2, "expected two presets")
                      && expect (names[0] == "StackA" && names[1] == "StackB", "unexpected preset name list");

    return expect (saveAsNew.success, "save-as-new should succeed")
        && expect (! duplicate.success, "save-as-new should reject duplicate names")
        && expect (overwrite.success, "overwrite should succeed with active preset")
        && expect (secondPreset.success, "second save-as-new should succeed")
        && expect (loadStackA.success, "load StackA should succeed")
        && expect (rootRestored == 62, "overwrite value was not restored")
        && namesOk
        && firstLayerOk
        && secondLayerOk;
}

bool verifyWarningSurfacedWhenFutureFieldsPresent (const juce::File& root)
{
    PolySynthAudioProcessor processor (root);
    processor.saveCurrentPresetAsNew ("FuturePreset");

    auto presetFile = root.getChildFile ("FuturePreset.jucepreset");
    if (auto xml = juce::XmlDocument::parse (presetFile))
    {
        auto payload = juce::ValueTree::fromXml (*xml);
        payload.setProperty ("schemaVersion", 99, nullptr);
        payload.setProperty ("futureField", "x", nullptr);
        if (const auto modified = payload.createXml())
            modified->writeTo (presetFile);
    }

    const auto loadResult = processor.loadPresetByName ("FuturePreset");
    return expect (loadResult.success, "future preset should load")
        && expect (loadResult.warnedAboutPartialLoad, "future preset warning should surface");
}

} // namespace

int main()
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("poly_synth_processor_preset_tests");
    root.deleteRecursively();
    root.createDirectory();

    bool ok = true;
    ok = verifyRoundTripAndSaveSemantics (root) && ok;

    root.deleteRecursively();
    root.createDirectory();
    ok = verifyWarningSurfacedWhenFutureFieldsPresent (root) && ok;

    root.deleteRecursively();

    if (! ok)
        return 1;

    std::cout << "preset processor integration validation passed." << '\n';
    return 0;
}
