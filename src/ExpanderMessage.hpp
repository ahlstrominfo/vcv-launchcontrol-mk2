#pragma once
#include <rack.hpp>

// Expander message structure for sharing data from Core module
struct LCXLExpanderMessage {
    // Knob values (all 9 layouts x 24 knobs)
    int knobValues[9][24] = {{0}};

    // Current layout (0 = default, 1-8 = sequencers)
    int currentLayout = 0;

    // Fader values
    int faderValues[8] = {0};

    // Button states (default mode)
    bool buttonStates[16] = {false};

    // Sequencer data for all 8 sequencers
    struct SequencerData {
        bool steps[16] = {false};
        int loopStart = 0;
        int loopEnd = 15;
        int currentStep = 0;
        int currentValueIndex = 0;
        int valueStart = 0;
        int valueEnd = 15;
        bool triggered = false;  // True on clock when step is active
    };
    SequencerData sequencers[8];

    // Module ID for validation
    int64_t moduleId = -1;
};
