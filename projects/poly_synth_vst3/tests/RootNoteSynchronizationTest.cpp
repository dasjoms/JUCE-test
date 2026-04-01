#include "../PolySynthAudioProcessor.h"

#include <iostream>

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';

    return condition;
}

bool validateAbsoluteAndRelativeStaySynchronized()
{
    PolySynthAudioProcessor processor;
    processor.addDefaultLayerAndSelect();

    const auto absolute = processor.setLayerRootNoteAbsolute (1, 67);
    const auto relative = processor.getLayerRootNoteRelativeSemitones (1);

    const auto absoluteViaRelative = processor.setLayerRootNoteRelativeSemitones (1, -5);

    return expect (absolute == 67, "absolute root-note setter should return assigned value")
        && expect (relative == 7, "relative representation should reflect absolute assignment")
        && expect (absoluteViaRelative == 55, "relative setter should map back to absolute root note")
        && expect (processor.getLayerRootNoteRelativeSemitones (1) == -5,
                   "absolute and relative root-note representations should remain synchronized");
}

bool validateBaseLayerChangesRetargetRelativeOffsets()
{
    PolySynthAudioProcessor processor;
    processor.addDefaultLayerAndSelect();
    processor.addDefaultLayerAndSelect();

    processor.setLayerRootNoteAbsolute (0, 60);
    processor.setLayerRootNoteRelativeSemitones (1, 7);
    processor.setLayerRootNoteRelativeSemitones (2, -12);

    processor.setLayerRootNoteAbsolute (0, 65);

    return expect (processor.getLayerRootNoteAbsolute (1) == 67, "layer absolute notes should remain unchanged when base moves")
        && expect (processor.getLayerRootNoteAbsolute (2) == 48, "layer absolute notes should remain unchanged when base moves")
        && expect (processor.getLayerRootNoteRelativeSemitones (1) == 2,
                   "base-root changes should update derived relative value for upper layer")
        && expect (processor.getLayerRootNoteRelativeSemitones (2) == -17,
                   "base-root changes should update derived relative value for lower layer");
}

} // namespace

int main()
{
    if (! validateAbsoluteAndRelativeStaySynchronized())
        return 1;

    if (! validateBaseLayerChangesRetargetRelativeOffsets())
        return 1;

    std::cout << "root-note synchronization validation passed." << '\n';
    return 0;
}
