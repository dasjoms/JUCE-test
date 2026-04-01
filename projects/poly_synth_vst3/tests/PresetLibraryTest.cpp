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

    auto duplicateLead = library.savePreset ("", makeState (11), PresetLibrary::SaveMode::saveAsNew, "lead");
    ok = expect (duplicateLead.code == PresetLibrary::SaveResultCode::duplicateNameRejected,
                 "save-as-new should reject duplicate name case-insensitively")
         && ok;

    auto saveAsBass = library.savePreset ("Lead", makeState (12), PresetLibrary::SaveMode::saveAsNew, "Bass");
    ok = expect (saveAsBass.code == PresetLibrary::SaveResultCode::saved, "save-as-new should allow unique names") && ok;

    auto overwriteLead = library.savePreset ("Lead", makeState (99), PresetLibrary::SaveMode::overwriteCurrent);
    ok = expect (overwriteLead.code == PresetLibrary::SaveResultCode::saved, "overwrite mode should update current preset") && ok;

    auto invalidOverwrite = library.savePreset ("", makeState (20), PresetLibrary::SaveMode::overwriteCurrent);
    ok = expect (invalidOverwrite.code == PresetLibrary::SaveResultCode::invalidName,
                 "overwrite should reject empty current preset name")
         && ok;

    const auto list = library.listPresets();
    ok = expect (list.size() == 2, "list should contain both Lead and Bass presets") && ok;

    const auto loadedLead = library.loadPreset ("Lead");
    ok = expect (loadedLead.success, "load should succeed for overwritten preset") && ok;
    if (loadedLead.instrumentState.isValid())
        ok = expect (static_cast<int> (loadedLead.instrumentState.getProperty ("value")) == 99,
                     "overwritten state should be loaded")
             && ok;

    PresetLibrary warningLibrary ("PolySynthTestsWarn", 1, root.getChildFile ("warn"));
    auto warningSave = warningLibrary.savePreset ("", makeState (5), PresetLibrary::SaveMode::saveAsNew, "Future");
    ok = expect (warningSave.code == PresetLibrary::SaveResultCode::saved, "warning fixture save should succeed") && ok;

    auto presetFile = warningLibrary.getStorageDirectory().getChildFile ("Future.jucepreset");
    if (auto xml = juce::XmlDocument::parse (presetFile))
    {
        auto payload = juce::ValueTree::fromXml (*xml);
        payload.setProperty ("schemaVersion", 2, nullptr);
        payload.setProperty ("futureField", true, nullptr);

        juce::ValueTree unknownChild ("UNHANDLED_CHILD");
        payload.addChild (unknownChild, -1, nullptr);

        if (const auto modified = payload.createXml())
            modified->writeTo (presetFile);
    }

    const auto warnedLoad = warningLibrary.loadPreset ("Future");
    ok = expect (warnedLoad.success, "partial/unknown preset load should still succeed") && ok;
    ok = expect (warnedLoad.warnedAboutPartialLoad, "partial/unknown preset fields should emit warning") && ok;

    ok = expect (library.deletePreset ("Lead"), "delete should remove Lead preset") && ok;
    ok = expect (library.deletePreset ("Bass"), "delete should remove Bass preset") && ok;
    ok = expect (library.listPresets().isEmpty(), "list should be empty after deleting all presets") && ok;

    root.deleteRecursively();

    if (! ok)
        return 1;

    std::cout << "preset library validation passed." << '\n';
    return 0;
}
