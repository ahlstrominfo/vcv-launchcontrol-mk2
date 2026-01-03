#pragma once
#include <rack.hpp>

// Message from ClockExpander (left of Core) to Core
struct ClockExpanderMessage {
    float clockA[8] = {0.f};  // Clock A voltages for each sequencer
    float clockB[8] = {0.f};  // Clock B voltages for each sequencer
    bool hasClockA[8] = {false};  // Whether clock A is connected (directly or via chain)
    bool hasClockB[8] = {false};  // Whether clock B is connected
    int64_t moduleId = -1;
};

// Types of changes that can be displayed
enum ChangeType {
    CHANGE_NONE = 0,
    CHANGE_LAYOUT,           // Switched to layout/sequencer
    CHANGE_VALUE_LENGTH_A,
    CHANGE_VALUE_LENGTH_B,
    CHANGE_STEP_LENGTH_A,
    CHANGE_STEP_LENGTH_B,
    CHANGE_BIAS,
    CHANGE_CV1,
    CHANGE_CV2,
    CHANGE_CV3,
    CHANGE_VOLTAGE_A,
    CHANGE_VOLTAGE_B,
    CHANGE_BIPOLAR_A,
    CHANGE_BIPOLAR_B,
    CHANGE_COMP_MODE,
    CHANGE_ROUTE_MODE,
    CHANGE_STEP_TOGGLE,
    CHANGE_UTILITY
};

// Info about the most recent change
struct LastChangeInfo {
    ChangeType type = CHANGE_NONE;
    int sequencer = 0;      // 0 = default layout, 1-8 = sequencer
    int value = 0;          // The new value
    int step = 0;           // For step toggles, which step
    float timestamp = 0.f;  // When the change happened
};

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

    // Button momentary mode (true = momentary, false = toggle)
    bool buttonMomentary[16] = {false};

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

        // Per-sequencer CV values (knobs 6-8, MIDI 0-127)
        int cv1 = 0;
        int cv2 = 0;
        int cv3 = 0;

        // Voltage settings (0=5V, 1=10V, 2=1V)
        int voltageRangeA = 0;
        int voltageRangeB = 0;
        bool bipolarA = false;
        bool bipolarB = false;

        // Slewed CV outputs (already processed with glide)
        float slewedCVA = 0.f;
        float slewedCVB = 0.f;

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

    // Last change info for InfoDisplay
    LastChangeInfo lastChange;

    // Module ID for validation
    int64_t moduleId = -1;
};
