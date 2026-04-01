#include "PresetLibrary.h"

namespace
{
constexpr auto presetNameProperty = "presetName";
constexpr auto createdUtcMsProperty = "createdUtcMs";
constexpr auto updatedUtcMsProperty = "updatedUtcMs";
constexpr auto schemaVersionProperty = "schemaVersion";
}

PresetLibrary::PresetLibrary (juce::String applicationName,
                              int supportedSchemaVersion,
                              juce::File storageRootOverride)
    : appName (std::move (applicationName)),
      schemaVersion (juce::jmax (1, supportedSchemaVersion))
{
    if (storageRootOverride == juce::File())
    {
        const auto appDataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
        storageRoot = appDataDir.getChildFile ("JUCEPolySynth").getChildFile (appName).getChildFile ("Presets");
    }
    else
    {
        storageRoot = std::move (storageRootOverride);
    }
}

juce::File PresetLibrary::getStorageDirectory() const
{
    return storageRoot;
}

PresetLibrary::SaveResult PresetLibrary::savePreset (const juce::String& currentPresetName,
                                                     const juce::ValueTree& fullInstrumentState,
                                                     SaveMode mode,
                                                     const juce::String& requestedName)
{
    if (! fullInstrumentState.isValid())
        return { SaveResultCode::ioError, "Preset save failed: instrument state is invalid." };

    if (! ensureStorageDirectoryExists())
        return { SaveResultCode::ioError, "Preset save failed: unable to create preset directory." };

    const auto trimmedCurrent = currentPresetName.trim();
    const auto trimmedRequested = requestedName.trim();
    const auto targetName = (mode == SaveMode::overwriteCurrent ? trimmedCurrent : trimmedRequested);

    if (targetName.isEmpty())
        return { SaveResultCode::invalidName, "Preset name cannot be empty." };

    const auto lowerTarget = normaliseNameKey (targetName);
    if (mode == SaveMode::saveAsNew)
    {
        for (const auto& preset : listPresets())
        {
            if (normaliseNameKey (preset.name) == lowerTarget)
                return { SaveResultCode::duplicateNameRejected, "Preset name already exists. Please choose a unique name." };
        }
    }

    PresetSummary summary;
    summary.name = targetName;
    summary.schemaVersion = schemaVersion;

    const auto destination = getPresetFileForName (targetName);
    if (destination.existsAsFile())
    {
        if (const auto parsed = loadPreset (targetName); parsed.success)
            summary.created = parsed.summary.created;
        else
            summary.created = juce::Time::getCurrentTime();
    }
    else
    {
        summary.created = juce::Time::getCurrentTime();
    }

    summary.updated = juce::Time::getCurrentTime();

    const auto payload = PresetSerialization::createPresetPayload (summary, fullInstrumentState);
    if (const auto xml = payload.createXml())
    {
        if (xml->writeTo (destination))
            return { SaveResultCode::saved, "Preset saved." };
    }

    return { SaveResultCode::ioError, "Preset save failed: unable to write preset file." };
}

juce::Array<PresetLibrary::PresetSummary> PresetLibrary::listPresets() const
{
    juce::Array<PresetSummary> presets;

    if (! storageRoot.isDirectory())
        return presets;

    for (const auto& candidate : storageRoot.findChildFiles (juce::File::findFiles, false, "*.jucepreset"))
    {
        const auto xml = juce::XmlDocument::parse (candidate);
        if (xml == nullptr)
            continue;

        const auto payload = juce::ValueTree::fromXml (*xml);
        PresetSummary summary;

        if (PresetSerialization::parseSummary (payload, summary))
            presets.add (summary);
    }

    std::sort (presets.begin(), presets.end(), [] (const PresetSummary& lhs, const PresetSummary& rhs)
    {
        return lhs.name.compareNatural (rhs.name) < 0;
    });

    return presets;
}

PresetLibrary::LoadResult PresetLibrary::loadPreset (const juce::String& presetName) const
{
    LoadResult result;
    const auto target = getPresetFileForName (presetName.trim());

    if (! target.existsAsFile())
    {
        result.message = "Preset not found.";
        return result;
    }

    const auto xml = juce::XmlDocument::parse (target);
    if (xml == nullptr)
    {
        result.message = "Preset load failed: invalid file format.";
        return result;
    }

    const auto payload = juce::ValueTree::fromXml (*xml);
    if (! PresetSerialization::parseSummary (payload, result.summary))
    {
        result.message = "Preset load failed: missing required metadata.";
        return result;
    }

    result.instrumentState = PresetSerialization::extractInstrumentState (payload);
    if (! result.instrumentState.isValid())
    {
        result.message = "Preset load failed: missing instrument payload.";
        return result;
    }

    const auto hasNewerSchema = result.summary.schemaVersion > schemaVersion;
    const auto hasUnknownFields = PresetSerialization::hasUnknownFieldsForKnownSubset (payload);

    result.success = true;
    if (hasNewerSchema || hasUnknownFields)
    {
        result.warnedAboutPartialLoad = true;
        result.message = createDefaultWarningMessage();
    }
    else
    {
        result.message = "Preset loaded.";
    }

    return result;
}

bool PresetLibrary::deletePreset (const juce::String& presetName) const
{
    const auto target = getPresetFileForName (presetName.trim());
    return target.existsAsFile() && target.deleteFile();
}

juce::String PresetLibrary::normaliseNameKey (const juce::String& name)
{
    return name.trim().toLowerCase();
}

juce::String PresetLibrary::toSafeFileStem (const juce::String& presetName)
{
    return juce::File::createLegalFileName (presetName.trim());
}

juce::String PresetLibrary::createDefaultWarningMessage()
{
    return "Preset loaded with warnings: newer or unknown fields were detected. Known fields were loaded.";
}


const juce::Identifier& PresetLibrary::PresetSerialization::presetRootType() noexcept
{
    static const juce::Identifier id { "POLY_SYNTH_PRESET" };
    return id;
}

const juce::Identifier& PresetLibrary::PresetSerialization::instrumentStateType() noexcept
{
    static const juce::Identifier id { "INSTRUMENT_STATE" };
    return id;
}

juce::ValueTree PresetLibrary::PresetSerialization::createPresetPayload (const PresetSummary& summary,
                                                                         const juce::ValueTree& fullInstrumentState)
{
    juce::ValueTree payload (presetRootType());
    payload.setProperty (presetNameProperty, summary.name, nullptr);
    payload.setProperty (createdUtcMsProperty, summary.created.toMilliseconds(), nullptr);
    payload.setProperty (updatedUtcMsProperty, summary.updated.toMilliseconds(), nullptr);
    payload.setProperty (schemaVersionProperty, summary.schemaVersion, nullptr);

    juce::ValueTree wrappedInstrumentState (instrumentStateType());
    wrappedInstrumentState.addChild (fullInstrumentState.createCopy(), -1, nullptr);
    payload.addChild (wrappedInstrumentState, -1, nullptr);
    return payload;
}

bool PresetLibrary::PresetSerialization::parseSummary (const juce::ValueTree& payload, PresetSummary& summary)
{
    if (! payload.hasType (presetRootType()))
        return false;

    if (! payload.hasProperty (presetNameProperty)
        || ! payload.hasProperty (createdUtcMsProperty)
        || ! payload.hasProperty (updatedUtcMsProperty)
        || ! payload.hasProperty (schemaVersionProperty))
    {
        return false;
    }

    summary.name = payload.getProperty (presetNameProperty).toString();
    summary.created = juce::Time (static_cast<juce::int64> (payload.getProperty (createdUtcMsProperty)));
    summary.updated = juce::Time (static_cast<juce::int64> (payload.getProperty (updatedUtcMsProperty)));
    summary.schemaVersion = static_cast<int> (payload.getProperty (schemaVersionProperty));
    return summary.name.isNotEmpty();
}

juce::ValueTree PresetLibrary::PresetSerialization::extractInstrumentState (const juce::ValueTree& payload)
{
    if (! payload.hasType (presetRootType()))
        return {};

    const auto wrapper = payload.getChildWithName (instrumentStateType());
    if (! wrapper.isValid() || wrapper.getNumChildren() == 0)
        return {};

    return wrapper.getChild (0).createCopy();
}

bool PresetLibrary::PresetSerialization::hasUnknownFieldsForKnownSubset (const juce::ValueTree& payload)
{
    if (! payload.hasType (presetRootType()))
        return true;

    static const juce::StringArray knownProperties {
        presetNameProperty,
        createdUtcMsProperty,
        updatedUtcMsProperty,
        schemaVersionProperty
    };

    for (int i = 0; i < payload.getNumProperties(); ++i)
    {
        const auto propertyName = payload.getPropertyName (i).toString();
        if (! knownProperties.contains (propertyName))
            return true;
    }

    for (int i = 0; i < payload.getNumChildren(); ++i)
    {
        const auto child = payload.getChild (i);
        if (! child.hasType (instrumentStateType()))
            return true;
    }

    return false;
}

juce::File PresetLibrary::getPresetFileForName (const juce::String& presetName) const
{
    return storageRoot.getChildFile (toSafeFileStem (presetName) + ".jucepreset");
}

bool PresetLibrary::ensureStorageDirectoryExists() const
{
    return storageRoot.isDirectory() || storageRoot.createDirectory();
}
