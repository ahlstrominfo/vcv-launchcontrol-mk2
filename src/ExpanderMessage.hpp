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

        // Sequence A (uses steps 0-7 in dual, 0-15 in single)
        int currentStepA = 0;
        int currentValueIndexA = 0;
        int stepLengthA = 8;
        int valueLengthA = 8;
        bool triggeredA = false;

        // Sequence B (uses steps 8-15, only in dual mode)
        int currentStepB = 0;
        int currentValueIndexB = 0;
        int stepLengthB = 4;
        int valueLengthB = 4;
        bool triggeredB = false;

        // Mode flags
        bool isValueSingleMode = false;  // true = all 16 values for A
        bool isStepSingleMode = false;   // true = all 16 steps for A

        // Legacy fields for compatibility
        int loopStart = 0;
        int loopEnd = 15;
        int currentStep = 0;
        int currentValueIndex = 0;
        int valueStart = 0;
        int valueEnd = 15;
        bool triggered = false;
    };
    SequencerData sequencers[8];

    // Module ID for validation
    int64_t moduleId = -1;
};
