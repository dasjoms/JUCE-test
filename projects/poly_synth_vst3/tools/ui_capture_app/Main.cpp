#include "EditorStateApplier.h"
#include "ScenarioLoader.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

namespace
{
struct CommandLineOptions
{
    juce::File scenarioFile;
    juce::File outputFile;
    juce::File outputDirectory;
    bool openAfterCapture = false;
    int waitMs = 0;
    std::optional<CaptureCrop> cropOverride;
};

std::optional<CaptureCrop> parseCropSpec (juce::StringRef spec, juce::String& errorMessage)
{
    juce::StringArray tokens;
    tokens.addTokens (spec, ",", "");
    tokens.trim();
    tokens.removeEmptyStrings();

    if (tokens.size() != 4)
    {
        errorMessage = "--crop must be in x,y,width,height format.";
        return std::nullopt;
    }

    CaptureCrop crop;
    if (! tokens[0].containsOnly ("-0123456789")
        || ! tokens[1].containsOnly ("-0123456789")
        || ! tokens[2].containsOnly ("0123456789")
        || ! tokens[3].containsOnly ("0123456789"))
    {
        errorMessage = "--crop values must be integers.";
        return std::nullopt;
    }

    crop.x = tokens[0].getIntValue();
    crop.y = tokens[1].getIntValue();
    crop.width = tokens[2].getIntValue();
    crop.height = tokens[3].getIntValue();

    if (crop.width <= 0 || crop.height <= 0)
    {
        errorMessage = "--crop width and height must be positive.";
        return std::nullopt;
    }

    return crop;
}

bool parseCommandLine (const juce::StringArray& arguments, CommandLineOptions& options, juce::String& errorMessage)
{
    for (int i = 1; i < arguments.size(); ++i)
    {
        const auto arg = arguments[i];
        const auto requireNext = [&arguments, &i, &errorMessage] (juce::StringRef flag) -> juce::String
        {
            if (i + 1 >= arguments.size())
            {
                errorMessage = "Missing value for " + flag;
                return {};
            }

            ++i;
            return arguments[i];
        };

        if (arg == "--scenario")
        {
            options.scenarioFile = juce::File (requireNext (arg));
        }
        else if (arg == "--output")
        {
            options.outputFile = juce::File (requireNext (arg));
        }
        else if (arg == "--output-dir")
        {
            options.outputDirectory = juce::File (requireNext (arg));
        }
        else if (arg == "--wait-ms")
        {
            const auto value = requireNext (arg);
            options.waitMs = value.getIntValue();
            if (options.waitMs < 0)
            {
                errorMessage = "--wait-ms must be >= 0.";
                return false;
            }
        }
        else if (arg == "--crop")
        {
            auto crop = parseCropSpec (requireNext (arg), errorMessage);
            if (! crop.has_value())
                return false;
            options.cropOverride = crop;
        }
        else if (arg == "--open-after-capture")
        {
            options.openAfterCapture = true;
        }
        else
        {
            errorMessage = "Unknown argument: " + arg;
            return false;
        }

        if (errorMessage.isNotEmpty())
            return false;
    }

    if (options.scenarioFile == juce::File())
    {
        errorMessage = "--scenario <file> is required.";
        return false;
    }

    if (options.outputFile == juce::File() && options.outputDirectory == juce::File())
    {
        errorMessage = "Provide --output <png> or --output-dir <dir>.";
        return false;
    }

    return true;
}

juce::File resolveOutputFile (const CommandLineOptions& options, const CaptureScenario& scenario)
{
    if (options.outputFile != juce::File())
        return options.outputFile;

    auto filename = scenario.sourceFile.getFileNameWithoutExtension() + ".png";
    return options.outputDirectory.getChildFile (filename);
}

juce::Rectangle<int> resolveCaptureBounds (const CaptureScenario& scenario,
                                           const std::optional<CaptureCrop>& cropOverride,
                                           juce::Rectangle<int> editorBounds)
{
    const auto crop = cropOverride.has_value() ? cropOverride : scenario.crop;
    if (! crop.has_value())
        return editorBounds;

    juce::Rectangle<int> requested (crop->x, crop->y, crop->width, crop->height);
    return requested.getIntersection (editorBounds);
}
}

int main (int argc, char* argv[])
{
    juce::String errorMessage;
    CommandLineOptions options;

    const juce::StringArray args (argv, argc);
    if (! parseCommandLine (args, options, errorMessage))
    {
        std::cerr << "Error: " << errorMessage << std::endl;
        return 1;
    }

    auto scenario = ScenarioLoader::loadFromFile (options.scenarioFile, errorMessage);
    if (! scenario.has_value())
    {
        std::cerr << "Error: " << errorMessage << std::endl;
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    juce::LookAndFeel_V4 lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    juce::Desktop::getInstance().setGlobalScaleFactor (static_cast<float> (scenario->scaleFactor));

    PolySynthAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);
    PolySynthAudioProcessorEditor editor (processor);

    if (! EditorStateApplier::applyScenario (processor, editor, *scenario, errorMessage))
    {
        std::cerr << "Error applying scenario: " << errorMessage << std::endl;
        return 2;
    }

    if (options.waitMs > 0)
        juce::Thread::sleep (options.waitMs);

    const auto captureBounds = resolveCaptureBounds (*scenario, options.cropOverride, editor.getLocalBounds());
    if (captureBounds.isEmpty())
    {
        std::cerr << "Error: crop region is empty after intersecting with editor bounds." << std::endl;
        return 3;
    }

    const auto snapshot = editor.createComponentSnapshot (captureBounds, true, static_cast<float> (scenario->scaleFactor));
    if (! snapshot.isValid())
    {
        std::cerr << "Error: Failed to create snapshot image." << std::endl;
        return 4;
    }

    auto outputFile = resolveOutputFile (options, *scenario);
    if (! outputFile.getParentDirectory().createDirectory())
    {
        std::cerr << "Error: Failed to create output directory: " << outputFile.getParentDirectory().getFullPathName() << std::endl;
        return 5;
    }

    juce::PNGImageFormat pngFormat;
    juce::FileOutputStream outputStream (outputFile);
    if (! outputStream.openedOk())
    {
        std::cerr << "Error: Failed to open output file: " << outputFile.getFullPathName() << std::endl;
        return 6;
    }

    if (! pngFormat.writeImageToStream (snapshot, outputStream))
    {
        std::cerr << "Error: Failed to write PNG: " << outputFile.getFullPathName() << std::endl;
        return 7;
    }

    outputStream.flush();
    std::cout << "Wrote snapshot: " << outputFile.getFullPathName() << std::endl;

    if (options.openAfterCapture)
        outputFile.startAsProcess();

    processor.releaseResources();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    return 0;
}
