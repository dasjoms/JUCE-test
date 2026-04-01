#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

class PresetLibrary final
{
public:
    enum class SaveMode
    {
        overwriteCurrent,
        saveAsNew
    };

    enum class SaveResultCode
    {
        saved,
        duplicateNameRejected,
        invalidName,
        ioError
    };

    struct PresetSummary
    {
        juce::String name;
        juce::Time created;
        juce::Time updated;
        int schemaVersion = 0;
    };

    struct SaveResult
    {
        SaveResultCode code = SaveResultCode::ioError;
        juce::String message;
    };

    struct LoadResult
    {
        bool success = false;
        juce::String message;
        juce::ValueTree instrumentState;
        PresetSummary summary;
        bool warnedAboutPartialLoad = false;
    };

    explicit PresetLibrary (juce::String applicationName,
                            int supportedSchemaVersion,
                            juce::File storageRootOverride = {});

    juce::File getStorageDirectory() const;

    SaveResult savePreset (const juce::String& currentPresetName,
                           const juce::ValueTree& fullInstrumentState,
                           SaveMode mode,
                           const juce::String& requestedName = {});

    juce::Array<PresetSummary> listPresets() const;
    LoadResult loadPreset (const juce::String& presetName) const;
    bool deletePreset (const juce::String& presetName) const;

private:
    static juce::String normaliseNameKey (const juce::String& name);
    static juce::String toSafeFileStem (const juce::String& presetName);
    static juce::String createDefaultWarningMessage();

    struct PresetSerialization
    {
        static const juce::Identifier& presetRootType() noexcept;
        static const juce::Identifier& instrumentStateType() noexcept;

        static juce::ValueTree createPresetPayload (const PresetSummary& summary,
                                                    const juce::ValueTree& fullInstrumentState);
        static bool parseSummary (const juce::ValueTree& payload, PresetSummary& summary);
        static juce::ValueTree extractInstrumentState (const juce::ValueTree& payload);
        static bool hasUnknownFieldsForKnownSubset (const juce::ValueTree& payload);
    };

    juce::File getPresetFileForName (const juce::String& presetName) const;
    bool ensureStorageDirectoryExists() const;

    juce::String appName;
    int schemaVersion = 1;
    juce::File storageRoot;
};
