#include "../PresetLibrary.h"

#include <iostream>

static bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';

    return condition;
}

static juce::ValueTree makeState (int value)
{
    juce::ValueTree state ("PLUGIN_STATE");
    state.setProperty ("value", value, nullptr);
    return state;
}

int main()
{
    bool ok = true;

    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("poly_synth_preset_tests");
    root.deleteRecursively();

    PresetLibrary library ("PolySynthTests", 1, root);

    auto saveAsLead = library.savePreset ("", makeState (10), PresetLibrary::SaveMode::saveAsNew, "Lead");
    ok = expect (saveAsLead.code == PresetLibrary::SaveResultCode::saved, "initial save-as-new should succeed") && ok;

    auto duplicateLead = library.savePreset ("", makeState (11), PresetLibrary::SaveMode::saveAsNew, "Lead");
    ok = expect (duplicateLead.code == PresetLibrary::SaveResultCode::duplicateNameRejected,
                 "save-as-new should reject duplicate name")
         && ok;

    auto overwriteLead = library.savePreset ("Lead", makeState (99), PresetLibrary::SaveMode::overwriteCurrent);
    ok = expect (overwriteLead.code == PresetLibrary::SaveResultCode::saved, "save should overwrite existing preset") && ok;

    const auto list = library.listPresets();
    ok = expect (list.size() == 1, "list should contain exactly one preset") && ok;
    ok = expect (list[0].name == "Lead", "listed preset name mismatch") && ok;

    const auto loaded = library.loadPreset ("Lead");
    ok = expect (loaded.success, "load should succeed for saved preset") && ok;
    if (loaded.instrumentState.isValid())
        ok = expect (static_cast<int> (loaded.instrumentState.getProperty ("value")) == 99,
                     "overwritten state should be loaded")
             && ok;

    ok = expect (library.deletePreset ("Lead"), "delete should remove preset") && ok;
    ok = expect (library.listPresets().isEmpty(), "list should be empty after delete") && ok;

    PresetLibrary partialWarnLibrary ("PolySynthTestsWarn", 1, root.getChildFile ("warn"));
    auto warnSave = partialWarnLibrary.savePreset ("", makeState (5), PresetLibrary::SaveMode::saveAsNew, "Future");
    ok = expect (warnSave.code == PresetLibrary::SaveResultCode::saved, "warning test preset save should succeed") && ok;

    auto presetFile = partialWarnLibrary.getStorageDirectory().getChildFile ("Future.jucepreset");
    if (auto xml = juce::XmlDocument::parse (presetFile))
    {
        auto payload = juce::ValueTree::fromXml (*xml);
        payload.setProperty ("schemaVersion", 2, nullptr);
        payload.setProperty ("futureField", true, nullptr);
        if (const auto modified = payload.createXml())
            modified->writeTo (presetFile);
    }

    const auto warnedLoad = partialWarnLibrary.loadPreset ("Future");
    ok = expect (warnedLoad.success, "partial-load path should still succeed") && ok;
    ok = expect (warnedLoad.warnedAboutPartialLoad, "partial-load path should emit warning") && ok;

    root.deleteRecursively();

    if (! ok)
        return 1;

    std::cout << "preset library validation passed." << '\n';
    return 0;
}
