#include "plugin.hpp"
#include "ExpanderMessage.hpp"
#include <midi.hpp>

// Launch Control XL Factory Template 1 MIDI mappings (Channel 9)
namespace LCXL {
    // CC numbers for knobs (3 rows of 8)
    constexpr int KNOB_ROW1[] = {13, 14, 15, 16, 17, 18, 19, 20};  // Send A
    constexpr int KNOB_ROW2[] = {29, 30, 31, 32, 33, 34, 35, 36};  // Send B
    constexpr int KNOB_ROW3[] = {49, 50, 51, 52, 53, 54, 55, 56};  // Pan/Device

    // CC numbers for faders
    constexpr int FADERS[] = {77, 78, 79, 80, 81, 82, 83, 84};

    // Note numbers for buttons
    constexpr int TRACK_FOCUS[] = {41, 42, 43, 44, 57, 58, 59, 60};
    constexpr int TRACK_CONTROL[] = {73, 74, 75, 76, 89, 90, 91, 92};

    // CC numbers for navigation buttons
    constexpr int BTN_UP = 104;
    constexpr int BTN_DOWN = 105;
    constexpr int BTN_LEFT = 106;
    constexpr int BTN_RIGHT = 107;

    // Button indices for Device, Mute, Solo, Record Arm
    constexpr int BTN_DEVICE = 105;  // Note number
    constexpr int BTN_MUTE = 106;
    constexpr int BTN_SOLO = 107;
    constexpr int BTN_REC_ARM = 108;

    // LED color values (bits 0-1: red 0-3, bits 4-5: green 0-3, base 12)
    constexpr uint8_t LED_OFF = 12;
    constexpr uint8_t LED_RED_LOW = 13;
    constexpr uint8_t LED_RED_FULL = 15;
    constexpr uint8_t LED_GREEN_LOW = 28;
    constexpr uint8_t LED_GREEN_FULL = 60;
    constexpr uint8_t LED_AMBER_LOW = 29;
    constexpr uint8_t LED_AMBER_FULL = 63;
    constexpr uint8_t LED_YELLOW_LOW = 30;
    constexpr uint8_t LED_YELLOW_FULL = 62;

    // SysEx header for Launch Control XL
    constexpr uint8_t SYSEX_HEADER[] = {0x00, 0x20, 0x29, 0x02, 0x11};

    // Factory template 1 uses MIDI channel 9 (index 8)
    constexpr int MIDI_CHANNEL = 8;
}

struct Core : Module {
    enum ParamId {
        TAKEOVER_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        CLOCK_A_INPUT,
        CLOCK_B_INPUT,
        RESET_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        FADER_OUTPUT_1,
        FADER_OUTPUT_2,
        FADER_OUTPUT_3,
        FADER_OUTPUT_4,
        FADER_OUTPUT_5,
        FADER_OUTPUT_6,
        FADER_OUTPUT_7,
        FADER_OUTPUT_8,
        SEQ_TRIG_A_OUTPUT,
        SEQ_CV_A_OUTPUT,
        SEQ_TRIG_B_OUTPUT,
        SEQ_CV_B_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        CONNECTED_LIGHT,
        TAKEOVER_LIGHT,
        LIGHTS_LEN
    };

    // Competition modes (dual mode)
    enum CompetitionMode {
        COMP_INDEPENDENT = 0,
        COMP_STEAL,
        COMP_A_PRIORITY,
        COMP_B_PRIORITY,
        COMP_MOMENTUM,
        COMP_REVENGE,
        COMP_ECHO,
        COMP_VALUE_THEFT
    };

    // Routing modes (single mode)
    enum RoutingMode {
        ROUTE_ALL_A = 0,
        ROUTE_ALL_B,
        ROUTE_BERNOULLI,
        ROUTE_ALTERNATE,
        ROUTE_TWO_TWO,
        ROUTE_BURST,
        ROUTE_PROBABILITY,
        ROUTE_PATTERN
    };

    bool takenOver = false;
    dsp::BooleanTrigger takeoverTrigger;
    dsp::SchmittTrigger clockTriggerA;
    dsp::SchmittTrigger clockTriggerB;
    dsp::SchmittTrigger resetTrigger;
    dsp::SchmittTrigger perSeqClockTriggerA[8];  // Per-sequencer clock A triggers
    dsp::SchmittTrigger perSeqClockTriggerB[8];  // Per-sequencer clock B triggers
    dsp::PulseGenerator trigPulseA[8];   // Trigger A pulse for each sequencer
    dsp::PulseGenerator trigPulseB[8];   // Trigger B pulse for each sequencer

    // Left expander (ClockExpander) message buffers
    ClockExpanderMessage leftMessages[2];

    midi::InputQueue midiInput;
    midi::Output midiOutput;

    // Current state
    int currentLayout = 0;  // 0 = default, 1-8 = sequencers
    int outputLayout = 0;   // Which layout's sequencer to output (0 = follow currentLayout)
    bool deviceButtonHeld = false;
    bool recArmHeld = false;          // For mode selection (hold + track focus)
    int lastMidiOutputDeviceId = -1;  // Track MIDI output connection for auto-init

    // Fader values (0-127 MIDI, converted to 0-10V)
    int faderValues[8] = {0};

    // Knob values per layout (0 = default, 1-8 = sequencers)
    int knobValues[9][24] = {{0}};

    // Button toggle states for default layout (16 buttons)
    bool buttonStates[16] = {false};

    // Sequencer states (8 sequencers)
    struct Sequencer {
        bool steps[16] = {false};       // Step on/off (all 16 buttons)

        // Length parameters (from knobs 1-4)
        int valueLengthA = 8;           // Value length for Seq A (1-16, >=9 = single mode)
        int valueLengthB = 4;           // Value length for Seq B (0-8, 0=disabled)
        int stepLengthA = 8;            // Step length for Seq A (1-16)
        int stepLengthB = 4;            // Step length for Seq B (0-8, 0=disabled)

        // Probability and bias (from knobs 5-7)
        float probA = 1.0f;             // Probability A fires (0-1)
        float probB = 1.0f;             // Probability B fires (0-1)
        float bias = 0.5f;              // Competition/routing bias (0-1)

        // Mode selection
        int competitionMode = COMP_INDEPENDENT;  // For dual mode
        int routingMode = ROUTE_ALL_A;           // For single mode

        // Voltage settings (0=5V green, 1=10V amber, 2=1V red)
        int voltageRangeA = 0;
        int voltageRangeB = 0;
        bool bipolarA = false;
        bool bipolarB = false;

        // Playback state
        int currentStepA = 0;           // Current step position for Seq A
        int currentStepB = 0;           // Current step position for Seq B
        int currentValueIndexA = 0;     // Current value index for Seq A
        int currentValueIndexB = 0;     // Current value index for Seq B

        // Momentum/revenge state for competition modes
        float momentumA = 0.5f;         // Current win chance for A
        float momentumB = 0.5f;         // Current win chance for B
        bool lastWinnerA = true;        // Who won last (for revenge mode)
        bool pendingEchoB = false;      // Echo pending for B
        bool pendingEchoA = false;      // Echo pending for A

        // Routing state for single mode
        int alternateCounter = 0;       // Counter for alternate/2+2 modes

        // Helper to check if values are in single mode (all 16 knobs for one seq)
        bool isValueSingleMode() const { return valueLengthA >= 9; }
        // Helper to check if steps are in single mode (all 16 buttons for one seq)
        bool isStepSingleMode() const { return stepLengthA >= 9; }
    };
    Sequencer sequencers[8];

    // Soft takeover state
    int lastPhysicalKnobPos[24] = {};  // Will be initialized to -1 in constructor
    bool knobPickedUp[24] = {};        // Will be initialized to true in constructor

    // Amber display timer for length parameters (0=valLenA, 1=valLenB, 2=stepLenA, 3=stepLenB)
    float lengthChangeTime[4] = {-1.f, -1.f, -1.f, -1.f};
    static constexpr float AMBER_DISPLAY_TIME = 0.2f;  // 200ms
    float currentTime = 0.f;  // Accumulated time for amber timeout

    // Expander message for right-side expanders
    LCXLExpanderMessage expanderMessage;
    bool seqTriggeredAThisFrame[8] = {false};  // Track which sequencers triggered on A
    bool seqTriggeredBThisFrame[8] = {false};  // Track which sequencers triggered on B

    // Last change tracking for InfoDisplay
    LastChangeInfo lastChange;

    void recordChange(ChangeType type, int seq, int value, int step = 0) {
        lastChange.type = type;
        lastChange.sequencer = seq;
        lastChange.value = value;
        lastChange.step = step;
        lastChange.timestamp = currentTime;
    }

    Core() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Configure takeover button
        configButton(TAKEOVER_PARAM, "Take Over LEDs");

        // Configure inputs
        configInput(CLOCK_A_INPUT, "Clock A");
        configInput(CLOCK_B_INPUT, "Clock B (normaled to A)");
        configInput(RESET_INPUT, "Reset");

        // Configure fader outputs
        for (int i = 0; i < 8; i++) {
            configOutput(FADER_OUTPUT_1 + i, string::f("Fader %d", i + 1));
        }

        // Configure sequencer outputs
        configOutput(SEQ_TRIG_A_OUTPUT, "Trigger A");
        configOutput(SEQ_CV_A_OUTPUT, "CV A");
        configOutput(SEQ_TRIG_B_OUTPUT, "Trigger B");
        configOutput(SEQ_CV_B_OUTPUT, "CV B");

        // Initialize soft takeover state
        for (int i = 0; i < 24; i++) {
            lastPhysicalKnobPos[i] = -1;  // Unknown position
            knobPickedUp[i] = true;        // Start as picked up
        }

        // Setup left expander message buffers (for ClockExpander)
        leftExpander.producerMessage = &leftMessages[0];
        leftExpander.consumerMessage = &leftMessages[1];

    }

    void onAdd(const AddEvent& e) override {
        Module::onAdd(e);
    }

    void process(const ProcessArgs& args) override {
        // Track time for amber display timeout
        currentTime += args.sampleTime;

        // Check if any amber timer has expired and needs LED update (skip if holding Device/RecArm)
        if (currentLayout > 0 && !deviceButtonHeld && !recArmHeld) {
            bool needsUpdate = false;
            for (int i = 0; i < 4; i++) {
                if (lengthChangeTime[i] >= 0) {
                    float elapsed = currentTime - lengthChangeTime[i];
                    if (elapsed >= AMBER_DISPLAY_TIME) {
                        lengthChangeTime[i] = -1.f;  // Clear the expired timer
                        needsUpdate = true;
                    }
                }
            }
            if (needsUpdate) {
                updateSequencerLEDs();
            }
        }

        // Check for MIDI output device connection change - auto-initialize
        int currentMidiOutputId = midiOutput.getDeviceId();
        if (currentMidiOutputId >= 0 && currentMidiOutputId != lastMidiOutputDeviceId) {
            lastMidiOutputDeviceId = currentMidiOutputId;
            initializeDevice();
        } else if (currentMidiOutputId < 0 && lastMidiOutputDeviceId >= 0) {
            // Device disconnected
            lastMidiOutputDeviceId = -1;
            takenOver = false;
        }

        // Check takeover button using trigger for edge detection
        if (takeoverTrigger.process(params[TAKEOVER_PARAM].getValue() > 0.f)) {
            if (!takenOver) {
                performTakeover();
            }
        }

        // Process incoming MIDI messages
        midi::Message msg;
        while (midiInput.tryPop(&msg, args.frame)) {
            processMidiMessage(msg);
        }

        // Output fader CVs (always active)
        for (int i = 0; i < 8; i++) {
            float voltage = faderValues[i] / 127.f * 10.f;
            outputs[FADER_OUTPUT_1 + i].setVoltage(voltage);
        }

        // Process reset input (resets all sequencers)
        if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
            for (int s = 0; s < 8; s++) {
                sequencers[s].currentStepA = 0;
                sequencers[s].currentStepB = 0;
                sequencers[s].currentValueIndexA = 0;
                sequencers[s].currentValueIndexB = 0;
                sequencers[s].alternateCounter = 0;
            }
            // Update LEDs if viewing a sequencer (skip if holding Device/RecArm for selection)
            if (currentLayout > 0 && !deviceButtonHeld && !recArmHeld) {
                updateSequencerLEDs();
            }
        }

        // Check for ClockExpander on the left
        bool hasClockExpander = false;
        ClockExpanderMessage* clockMsg = nullptr;
        if (leftExpander.module && leftExpander.module->model == modelClockExpander) {
            clockMsg = reinterpret_cast<ClockExpanderMessage*>(leftExpander.consumerMessage);
            if (clockMsg && clockMsg->moduleId >= 0) {
                hasClockExpander = true;
            }
        }

        // Get default clock voltages (Clock B normals to Clock A)
        float defaultClockAVoltage = inputs[CLOCK_A_INPUT].getVoltage();
        float defaultClockBVoltage = inputs[CLOCK_B_INPUT].isConnected() ?
            inputs[CLOCK_B_INPUT].getVoltage() : defaultClockAVoltage;

        // Track if any clock rose (for LED updates)
        bool anyClockARose = false;
        bool anyClockBRose = false;

        // Process clock inputs for all sequencers
        for (int s = 0; s < 8; s++) {
            Sequencer& seq = sequencers[s];

            // Determine clock sources for this sequencer
            float clockAVoltage, clockBVoltage;
            if (hasClockExpander && clockMsg->hasClockA[s]) {
                // Use per-sequencer clocks from expander
                clockAVoltage = clockMsg->clockA[s];
                clockBVoltage = clockMsg->hasClockB[s] ? clockMsg->clockB[s] : clockAVoltage;
            } else {
                // Fall back to module's shared clock inputs
                clockAVoltage = defaultClockAVoltage;
                clockBVoltage = defaultClockBVoltage;
            }

            // Process triggers per-sequencer
            bool clockARose = perSeqClockTriggerA[s].process(clockAVoltage);
            bool clockBRose = perSeqClockTriggerB[s].process(clockBVoltage);

            if (clockARose) anyClockARose = true;
            if (clockBRose) anyClockBRose = true;

            if (seq.isStepSingleMode()) {
                // Single step mode: only use Clock A
                if (clockARose) {
                    processSequencerClockSingle(s);
                }
            } else {
                // Dual step mode: Clock A for Seq A, Clock B for Seq B
                if (clockARose) {
                    processSequencerClockDualA(s);
                }
                if (clockBRose) {
                    processSequencerClockDualB(s);
                }
            }
        }

        // Update LEDs if viewing a sequencer and clock happened (skip if holding Device/RecArm)
        if (currentLayout > 0 && (anyClockARose || anyClockBRose) && !deviceButtonHeld && !recArmHeld) {
            updateSequencerLEDs();
        }

        // Process all pulse generators
        bool trigOutA[8], trigOutB[8];
        for (int s = 0; s < 8; s++) {
            trigOutA[s] = trigPulseA[s].process(args.sampleTime);
            trigOutB[s] = trigPulseB[s].process(args.sampleTime);
        }

        // Output trigger and CV for the output sequencer
        // outputLayout: 0 = follow currentLayout, 1-8 = fixed sequencer
        int outSeq = (outputLayout > 0) ? outputLayout : currentLayout;
        if (outSeq > 0) {
            Sequencer& seq = sequencers[outSeq - 1];

            // Output A
            outputs[SEQ_TRIG_A_OUTPUT].setVoltage(trigOutA[outSeq - 1] ? 10.f : 0.f);
            int knobIndexA = seq.isValueSingleMode() ? seq.currentValueIndexA : seq.currentValueIndexA;
            float cvA = knobToVoltage(knobValues[outSeq][knobIndexA], seq.voltageRangeA, seq.bipolarA);
            outputs[SEQ_CV_A_OUTPUT].setVoltage(cvA);

            // Output B
            outputs[SEQ_TRIG_B_OUTPUT].setVoltage(trigOutB[outSeq - 1] ? 10.f : 0.f);
            int knobIndexB = seq.isValueSingleMode() ? seq.currentValueIndexA : (8 + seq.currentValueIndexB);
            float cvB = knobToVoltage(knobValues[outSeq][knobIndexB], seq.voltageRangeB, seq.bipolarB);
            outputs[SEQ_CV_B_OUTPUT].setVoltage(cvB);
        } else {
            outputs[SEQ_TRIG_A_OUTPUT].setVoltage(0.f);
            outputs[SEQ_CV_A_OUTPUT].setVoltage(0.f);
            outputs[SEQ_TRIG_B_OUTPUT].setVoltage(0.f);
            outputs[SEQ_CV_B_OUTPUT].setVoltage(0.f);
        }

        // Set connected light based on MIDI input device
        bool midiConnected = midiInput.getDeviceId() >= 0;
        lights[CONNECTED_LIGHT].setBrightness(midiConnected ? 1.f : 0.f);

        // Takeover light - stays on once takeover happens
        lights[TAKEOVER_LIGHT].setBrightness(takenOver ? 1.f : 0.f);

        // Update and send expander message to right-side expanders
        updateExpanderMessage();
        if (rightExpander.module) {
            LCXLExpanderMessage* msg = reinterpret_cast<LCXLExpanderMessage*>(rightExpander.module->leftExpander.producerMessage);
            if (msg) {
                *msg = expanderMessage;
                rightExpander.module->leftExpander.messageFlipRequested = true;
            }
        }

        // Reset trigger flags for next frame
        for (int s = 0; s < 8; s++) {
            seqTriggeredAThisFrame[s] = false;
            seqTriggeredBThisFrame[s] = false;
        }
    }

    // Single mode clock processing: one sequencer, routing to A or B
    void processSequencerClockSingle(int seqIndex) {
        Sequencer& seq = sequencers[seqIndex];

        // Advance step
        seq.currentStepA = (seq.currentStepA + 1) % seq.stepLengthA;

        // Check if step is active
        if (!seq.steps[seq.currentStepA]) return;

        // Apply probability
        if (random::uniform() >= seq.probA) return;

        // Advance value index
        seq.currentValueIndexA = (seq.currentValueIndexA + 1) % seq.valueLengthA;

        // Determine routing destination
        bool fireA = false, fireB = false;

        switch (seq.routingMode) {
            case ROUTE_ALL_A:
                fireA = true;
                break;
            case ROUTE_ALL_B:
                fireB = true;
                break;
            case ROUTE_BERNOULLI:
                if (random::uniform() < seq.bias) fireB = true;
                else fireA = true;
                break;
            case ROUTE_ALTERNATE:
                if (seq.alternateCounter % 2 == 0) fireA = true;
                else fireB = true;
                seq.alternateCounter++;
                break;
            case ROUTE_TWO_TWO:
                if ((seq.alternateCounter / 2) % 2 == 0) fireA = true;
                else fireB = true;
                seq.alternateCounter++;
                break;
            case ROUTE_BURST: {
                // Random bursts - bias controls probability of switching
                static bool burstToA = true;
                if (random::uniform() < seq.bias * 0.3f) burstToA = !burstToA;
                if (burstToA) fireA = true;
                else fireB = true;
                break;
            }
            case ROUTE_PROBABILITY:
                if (random::uniform() < seq.bias) fireB = true;
                else fireA = true;
                break;
            case ROUTE_PATTERN:
                // Odd steps to A, even to B
                if (seq.currentStepA % 2 == 0) fireA = true;
                else fireB = true;
                break;
        }

        if (fireA) {
            trigPulseA[seqIndex].trigger(1e-3f);
            seqTriggeredAThisFrame[seqIndex] = true;
        }
        if (fireB) {
            trigPulseB[seqIndex].trigger(1e-3f);
            seqTriggeredBThisFrame[seqIndex] = true;
        }
    }

    // Dual mode clock A processing: Seq A step
    void processSequencerClockDualA(int seqIndex) {
        Sequencer& seq = sequencers[seqIndex];

        // Advance step A
        seq.currentStepA = (seq.currentStepA + 1) % seq.stepLengthA;

        // Check if step is active (top row buttons = steps 0-7)
        if (!seq.steps[seq.currentStepA]) return;

        // Apply probability
        if (random::uniform() >= seq.probA) return;

        // Check for competition with B
        bool aWantsToFire = true;
        bool bWantsToFire = (seq.stepLengthB > 0) && seq.steps[8 + (seq.currentStepB % seq.stepLengthB)];

        bool aWins = resolveCompetition(seq, aWantsToFire, bWantsToFire, true);

        if (aWins) {
            // A fires - advance value index
            seq.currentValueIndexA = (seq.currentValueIndexA + 1) % seq.valueLengthA;
            trigPulseA[seqIndex].trigger(1e-3f);
            seqTriggeredAThisFrame[seqIndex] = true;
        }
    }

    // Dual mode clock B processing: Seq B step
    void processSequencerClockDualB(int seqIndex) {
        Sequencer& seq = sequencers[seqIndex];

        // B disabled if length is 0
        if (seq.stepLengthB <= 0) {
            INFO("SeqB[%d]: stepLengthB=0, skipping", seqIndex);
            return;
        }

        // Advance step B
        seq.currentStepB = (seq.currentStepB + 1) % seq.stepLengthB;

        // Check if step is active (bottom row buttons = steps 8-15)
        if (!seq.steps[8 + seq.currentStepB]) {
            INFO("SeqB[%d]: step %d inactive, skipping", seqIndex, seq.currentStepB);
            return;
        }

        // Apply probability
        float probRoll = random::uniform();
        if (probRoll >= seq.probB) {
            INFO("SeqB[%d]: prob failed (roll=%.2f >= probB=%.2f)", seqIndex, probRoll, seq.probB);
            return;
        }

        // Check for competition with A
        bool aWantsToFire = seq.steps[seq.currentStepA % seq.stepLengthA];
        bool bWantsToFire = true;

        bool bWins = !resolveCompetition(seq, aWantsToFire, bWantsToFire, false);
        INFO("SeqB[%d]: aWants=%d, compMode=%d, bWins=%d", seqIndex, aWantsToFire, seq.competitionMode, bWins);

        if (bWins) {
            // B fires - advance value index
            if (seq.valueLengthB > 0) {
                seq.currentValueIndexB = (seq.currentValueIndexB + 1) % seq.valueLengthB;
            }
            trigPulseB[seqIndex].trigger(1e-3f);
            seqTriggeredBThisFrame[seqIndex] = true;
            INFO("SeqB[%d]: FIRED, valueIdx=%d", seqIndex, seq.currentValueIndexB);
        } else {
            INFO("SeqB[%d]: lost competition to A", seqIndex);
        }
    }

    // Resolve competition between A and B, returns true if A wins
    bool resolveCompetition(Sequencer& seq, bool aWants, bool bWants, bool isAClock) {
        // If only one wants to fire, they win
        if (aWants && !bWants) return true;
        if (bWants && !aWants) return false;
        if (!aWants && !bWants) return isAClock;  // Neither wants, default

        // Both want to fire - competition!
        switch (seq.competitionMode) {
            case COMP_INDEPENDENT:
                // Both can fire - let each win on their own clock
                return isAClock;  // A wins on A's clock, B wins on B's clock

            case COMP_STEAL:
                // Bernoulli decides
                return random::uniform() >= seq.bias;

            case COMP_A_PRIORITY:
                // A wins if bias is high enough
                return random::uniform() < (0.5f + seq.bias * 0.5f);

            case COMP_B_PRIORITY:
                // B wins if bias is high enough
                return random::uniform() >= (0.5f + seq.bias * 0.5f);

            case COMP_MOMENTUM: {
                // Winner gets boost next time
                bool aWins = random::uniform() < seq.momentumA;
                if (aWins) {
                    seq.momentumA = std::min(1.0f, seq.momentumA + seq.bias * 0.2f);
                    seq.momentumB = std::max(0.0f, seq.momentumB - seq.bias * 0.1f);
                } else {
                    seq.momentumB = std::min(1.0f, seq.momentumB + seq.bias * 0.2f);
                    seq.momentumA = std::max(0.0f, seq.momentumA - seq.bias * 0.1f);
                }
                return aWins;
            }

            case COMP_REVENGE: {
                // Loser gets boost next time
                bool aWins;
                if (seq.lastWinnerA) {
                    // B has revenge chance
                    aWins = random::uniform() >= seq.bias * 0.7f;
                } else {
                    // A has revenge chance
                    aWins = random::uniform() < (1.0f - seq.bias * 0.7f);
                }
                seq.lastWinnerA = aWins;
                return aWins;
            }

            case COMP_ECHO:
                // Winner fires, loser echoes on next clock
                if (isAClock) {
                    bool aWins = random::uniform() >= seq.bias;
                    if (!aWins) seq.pendingEchoA = true;
                    return aWins;
                } else {
                    bool bWins = random::uniform() < seq.bias;
                    if (!bWins) seq.pendingEchoB = true;
                    return !bWins;
                }

            case COMP_VALUE_THEFT:
                // Winner uses combined CV - handled in output stage
                return random::uniform() >= seq.bias;
        }

        return true;  // Default A wins
    }

    // Update LED display for current sequencer
    void updateSequencerLEDs() {
        if (currentLayout <= 0) return;

        Sequencer& seq = sequencers[currentLayout - 1];

        // Steps and values can have independent single/dual modes
        // Step buttons
        if (seq.isStepSingleMode()) {
            for (int i = 0; i < 16; i++) {
                updateStepLEDSingle(i, seq);
            }
        } else {
            for (int i = 0; i < 8; i++) {
                updateStepLEDDual(i, seq, true);
                updateStepLEDDual(8 + i, seq, false);
            }
        }

        // Value knobs
        if (seq.isValueSingleMode()) {
            updateValueKnobLEDsSingle(seq);
        } else {
            updateValueKnobLEDsDual(seq);
        }

        for (int i = 16; i < 24; i++) {
            updateKnobLED(i);
        }
    }

    void updateStepLEDSingle(int stepIndex, const Sequencer& seq) {
        bool showAmber = shouldShowAmber(2);  // stepLengthA timer
        bool isPlayhead = (stepIndex == seq.currentStepA);
        bool isActive = seq.steps[stepIndex];

        uint8_t color;
        if (stepIndex >= seq.stepLengthA) {
            color = LCXL::LED_OFF;  // Out of range
        } else if (showAmber && stepIndex == seq.stepLengthA - 1) {
            // Boundary marker (only while adjusting)
            color = LCXL::LED_AMBER_FULL;
        } else if (isPlayhead && isActive) {
            color = LCXL::LED_GREEN_FULL;  // Playhead on active step: bright green
        } else if (isPlayhead) {
            color = LCXL::LED_RED_LOW;  // Playhead on inactive step: dim red
        } else if (isActive) {
            color = LCXL::LED_GREEN_LOW;  // Active step: dim green
        } else {
            color = LCXL::LED_OFF;  // Inactive step
        }
        sendButtonLEDSysEx(stepIndex, color);
    }

    void updateStepLEDDual(int buttonIndex, const Sequencer& seq, bool isSeqA) {
        int localStep = isSeqA ? buttonIndex : (buttonIndex - 8);
        int stepLength = isSeqA ? seq.stepLengthA : seq.stepLengthB;
        int currentStep = isSeqA ? seq.currentStepA : seq.currentStepB;
        bool showAmber = shouldShowAmber(isSeqA ? 2 : 3);  // stepLengthA or stepLengthB timer
        bool isPlayhead = (localStep == currentStep);
        bool isActive = seq.steps[buttonIndex];

        uint8_t color;
        if (stepLength == 0 || localStep >= stepLength) {
            color = LCXL::LED_OFF;  // Out of range or seq B disabled
        } else if (showAmber && localStep == stepLength - 1) {
            // Boundary marker (only while adjusting)
            color = LCXL::LED_AMBER_FULL;
        } else if (isPlayhead && isActive) {
            color = LCXL::LED_GREEN_FULL;  // Playhead on active step: bright green
        } else if (isPlayhead) {
            color = LCXL::LED_RED_LOW;  // Playhead on inactive step: dim red
        } else if (isActive) {
            color = LCXL::LED_GREEN_LOW;  // Active step: dim green
        } else {
            color = LCXL::LED_OFF;  // Inactive step
        }
        sendButtonLEDSysEx(buttonIndex, color);
    }

    // Helper to get soft takeover color for a knob (bright = playhead position)
    uint8_t getSoftTakeoverColor(int knobIndex, bool bright = false) {
        if (lastPhysicalKnobPos[knobIndex] < 0) {
            // Unknown physical position - assume picked up until we receive MIDI
            return bright ? LCXL::LED_GREEN_FULL : LCXL::LED_GREEN_LOW;
        }
        int storedValue = knobValues[currentLayout][knobIndex];
        int physicalPos = lastPhysicalKnobPos[knobIndex];

        if (knobPickedUp[knobIndex] || std::abs(physicalPos - storedValue) <= 2) {
            return bright ? LCXL::LED_GREEN_FULL : LCXL::LED_GREEN_LOW;  // Picked up
        } else if (physicalPos < storedValue) {
            return bright ? LCXL::LED_YELLOW_FULL : LCXL::LED_YELLOW_LOW;  // Turn right
        } else {
            return bright ? LCXL::LED_RED_FULL : LCXL::LED_RED_LOW;  // Turn left
        }
    }

    // Check if amber should be shown for a length parameter (within 200ms of last change)
    bool shouldShowAmber(int lengthParamIndex) {
        if (lengthChangeTime[lengthParamIndex] < 0) return false;
        return (currentTime - lengthChangeTime[lengthParamIndex]) < AMBER_DISPLAY_TIME;
    }

    void updateValueKnobLEDsSingle(const Sequencer& seq) {
        // Single mode: all 16 knobs show one sequence
        // valueLengthA = how many knobs are active (1-16)
        bool showAmber = shouldShowAmber(0);  // valueLengthA timer
        for (int i = 0; i < 16; i++) {
            uint8_t color;
            bool isPlayhead = (i == seq.currentValueIndexA);

            if (i >= seq.valueLengthA) {
                // AFTER the length = OFF
                color = LCXL::LED_OFF;
            } else if (showAmber && i == seq.valueLengthA - 1) {
                // Last active position = AMBER (only while adjusting)
                color = LCXL::LED_AMBER_FULL;
            } else {
                // In range = show soft takeover color (bright if playhead)
                color = getSoftTakeoverColor(i, isPlayhead);
            }
            sendKnobLEDSysEx(i, color);
        }
    }

    void updateValueKnobLEDsDual(const Sequencer& seq) {
        bool showAmberA = shouldShowAmber(0);  // valueLengthA timer
        bool showAmberB = shouldShowAmber(1);  // valueLengthB timer

        // Row 1: Seq A values (knobs 0-7)
        for (int i = 0; i < 8; i++) {
            uint8_t color;
            bool isPlayhead = (i == seq.currentValueIndexA);
            if (i >= seq.valueLengthA) {
                color = LCXL::LED_OFF;
            } else if (showAmberA && i == seq.valueLengthA - 1) {
                color = LCXL::LED_AMBER_FULL;
            } else {
                color = getSoftTakeoverColor(i, isPlayhead);
            }
            sendKnobLEDSysEx(i, color);
        }

        // Row 2: Seq B values (knobs 8-15)
        for (int i = 0; i < 8; i++) {
            int knobIndex = 8 + i;
            uint8_t color;
            bool isPlayhead = (i == seq.currentValueIndexB);
            if (seq.valueLengthB == 0 || i >= seq.valueLengthB) {
                color = LCXL::LED_OFF;
            } else if (showAmberB && i == seq.valueLengthB - 1) {
                color = LCXL::LED_AMBER_FULL;
            } else {
                color = getSoftTakeoverColor(knobIndex, isPlayhead);
            }
            sendKnobLEDSysEx(knobIndex, color);
        }
    }

    void updateExpanderMessage() {
        expanderMessage.moduleId = id;
        expanderMessage.currentLayout = currentLayout;

        // Copy fader values
        for (int i = 0; i < 8; i++) {
            expanderMessage.faderValues[i] = faderValues[i];
        }

        // Copy knob values for all layouts
        for (int layout = 0; layout < 9; layout++) {
            for (int i = 0; i < 24; i++) {
                expanderMessage.knobValues[layout][i] = knobValues[layout][i];
            }
        }

        // Copy button states
        for (int i = 0; i < 16; i++) {
            expanderMessage.buttonStates[i] = buttonStates[i];
        }

        // Copy sequencer data
        for (int s = 0; s < 8; s++) {
            auto& dst = expanderMessage.sequencers[s];
            auto& src = sequencers[s];
            for (int i = 0; i < 16; i++) {
                dst.steps[i] = src.steps[i];
            }

            // Sequence A data
            dst.currentStepA = src.currentStepA;
            dst.currentValueIndexA = src.currentValueIndexA;
            dst.stepLengthA = src.stepLengthA;
            dst.valueLengthA = src.valueLengthA;
            dst.triggeredA = seqTriggeredAThisFrame[s];

            // Sequence B data
            dst.currentStepB = src.currentStepB;
            dst.currentValueIndexB = src.currentValueIndexB;
            dst.stepLengthB = src.stepLengthB;
            dst.valueLengthB = src.valueLengthB;
            dst.triggeredB = seqTriggeredBThisFrame[s];

            // Mode flags
            dst.isValueSingleMode = src.isValueSingleMode();
            dst.isStepSingleMode = src.isStepSingleMode();

            // Voltage settings
            dst.voltageRangeA = src.voltageRangeA;
            dst.voltageRangeB = src.voltageRangeB;
            dst.bipolarA = src.bipolarA;
            dst.bipolarB = src.bipolarB;

            // Legacy fields for compatibility
            dst.loopStart = 0;
            dst.loopEnd = src.stepLengthA - 1;
            dst.currentStep = src.currentStepA;
            dst.currentValueIndex = src.currentValueIndexA;
            dst.valueStart = 0;
            dst.valueEnd = src.valueLengthA - 1;
            dst.triggered = seqTriggeredAThisFrame[s];
        }

        // Copy last change info
        expanderMessage.lastChange = lastChange;
    }

    void initializeDevice() {
        // Check if MIDI output is connected
        if (midiOutput.getDeviceId() < 0) {
            return;
        }

        // First, force the Launch Control XL to template 8 (Factory Template 1)
        sendForceTemplate(8);
        // Then reset/clean all LEDs
        sendResetLEDs();
        takenOver = true;
        // Update LEDs with our state
        updateAllLEDs();
    }

    void performTakeover() {
        initializeDevice();
    }

    void sendForceTemplate(uint8_t templateNum) {
        // SysEx: F0 00 20 29 02 11 77 [template] F7
        // This forces the Launch Control XL to switch to the specified template
        midi::Message msg;
        msg.bytes = {
            0xF0,
            0x00, 0x20, 0x29, 0x02, 0x11,
            0x77,  // Template change command (119 decimal)
            templateNum,
            0xF7
        };
        midiOutput.sendMessage(msg);
    }

    void processMidiMessage(const midi::Message& msg) {
        int channel = msg.getChannel();
        int status = msg.getStatus();

        // Only process messages on our expected channel
        if (channel != LCXL::MIDI_CHANNEL) {
            return;
        }

        switch (status) {
            case 0xb: // Control Change
                processCCMessage(msg.getNote(), msg.getValue());
                break;
            case 0x9: // Note On
                processNoteOn(msg.getNote(), msg.getValue());
                break;
            case 0x8: // Note Off
                processNoteOff(msg.getNote());
                break;
        }
    }

    void processCCMessage(int cc, int value) {
        // Check faders
        for (int i = 0; i < 8; i++) {
            if (cc == LCXL::FADERS[i]) {
                faderValues[i] = value;
                return;
            }
        }

        // Check knobs row 1
        for (int i = 0; i < 8; i++) {
            if (cc == LCXL::KNOB_ROW1[i]) {
                processKnobChange(i, value);
                return;
            }
        }

        // Check knobs row 2
        for (int i = 0; i < 8; i++) {
            if (cc == LCXL::KNOB_ROW2[i]) {
                processKnobChange(8 + i, value);
                return;
            }
        }

        // Check knobs row 3
        for (int i = 0; i < 8; i++) {
            if (cc == LCXL::KNOB_ROW3[i]) {
                processKnobChange(16 + i, value);
                return;
            }
        }
    }

    void processKnobChange(int knobIndex, int value) {
        // In sequencer mode, bottom row knobs (16-23) are parameters - bypass soft takeover
        bool isParameterKnob = (currentLayout > 0 && knobIndex >= 16);

        if (isParameterKnob) {
            // Parameter knobs work immediately without soft takeover
            knobValues[currentLayout][knobIndex] = value;
            knobPickedUp[knobIndex] = true;
            int paramIdx = knobIndex - 16;
            processSequencerParameter(paramIdx, value);
        } else {
            // Soft takeover logic for value knobs
            int storedValue = knobValues[currentLayout][knobIndex];

            if (!knobPickedUp[knobIndex]) {
                // Check if we've reached the pickup zone (±2)
                if (std::abs(value - storedValue) <= 2) {
                    knobPickedUp[knobIndex] = true;
                }
            }

            if (knobPickedUp[knobIndex]) {
                knobValues[currentLayout][knobIndex] = value;
            }
        }

        lastPhysicalKnobPos[knobIndex] = value;

        // Update LED - in sequencer mode, value knobs show sequencer state with soft takeover
        if (currentLayout > 0 && knobIndex < 16) {
            Sequencer& seq = sequencers[currentLayout - 1];
            if (seq.isValueSingleMode()) {
                updateValueKnobLEDsSingle(seq);
            } else {
                updateValueKnobLEDsDual(seq);
            }
        } else {
            updateKnobLED(knobIndex);
        }
    }

    void processSequencerParameter(int paramIndex, int value) {
        // paramIndex: 0=VAL-A, 1=VAL-B, 2=STEP-A, 3=STEP-B, 4=PROB-A, 5=PROB-B, 6=BIAS, 7=reserved
        Sequencer& seq = sequencers[currentLayout - 1];

        switch (paramIndex) {
            case 0:  // Value Length A (1-16)
                seq.valueLengthA = 1 + (value * 15 / 127);  // Map 0-127 to 1-16
                if (seq.currentValueIndexA >= seq.valueLengthA) {
                    seq.currentValueIndexA = 0;
                }
                lengthChangeTime[0] = currentTime;  // Record time for amber display
                recordChange(CHANGE_VALUE_LENGTH_A, currentLayout, seq.valueLengthA);
                updateSequencerLEDs();
                break;

            case 1:  // Value Length B (0-8)
                seq.valueLengthB = value * 9 / 128;  // Map 0-127 to 0-8
                if (seq.valueLengthB > 0 && seq.currentValueIndexB >= seq.valueLengthB) {
                    seq.currentValueIndexB = 0;
                }
                lengthChangeTime[1] = currentTime;  // Record time for amber display
                recordChange(CHANGE_VALUE_LENGTH_B, currentLayout, seq.valueLengthB);
                updateSequencerLEDs();
                break;

            case 2:  // Step Length A (1-16)
                seq.stepLengthA = 1 + (value * 15 / 127);  // Map 0-127 to 1-16
                if (seq.currentStepA >= seq.stepLengthA) {
                    seq.currentStepA = 0;
                }
                lengthChangeTime[2] = currentTime;  // Record time for amber display
                recordChange(CHANGE_STEP_LENGTH_A, currentLayout, seq.stepLengthA);
                updateSequencerLEDs();
                break;

            case 3:  // Step Length B (0-8)
                seq.stepLengthB = value * 9 / 128;  // Map 0-127 to 0-8
                if (seq.stepLengthB > 0 && seq.currentStepB >= seq.stepLengthB) {
                    seq.currentStepB = 0;
                }
                lengthChangeTime[3] = currentTime;  // Record time for amber display
                recordChange(CHANGE_STEP_LENGTH_B, currentLayout, seq.stepLengthB);
                updateSequencerLEDs();
                break;

            case 4:  // Probability A (0-100%)
                seq.probA = value / 127.f;
                recordChange(CHANGE_PROB_A, currentLayout, value * 100 / 127);
                break;

            case 5:  // Probability B (0-100%)
                seq.probB = value / 127.f;
                recordChange(CHANGE_PROB_B, currentLayout, value * 100 / 127);
                break;

            case 6:  // Bias/Amount (0-100%)
                seq.bias = value / 127.f;
                recordChange(CHANGE_BIAS, currentLayout, value * 100 / 127);
                break;

            case 7:  // Reserved
                break;
        }
    }

    void processNoteOn(int note, int velocity) {
        if (velocity == 0) {
            processNoteOff(note);
            return;
        }

        // Check for Device button
        if (note == LCXL::BTN_DEVICE) {
            deviceButtonHeld = true;
            showLayoutSelectionLEDs();
            return;
        }

        // Check for Record Arm button
        if (note == LCXL::BTN_REC_ARM) {
            recArmHeld = true;
            // Show current mode on LEDs
            if (currentLayout > 0) {
                showModeSelectionLEDs();
            }
            return;
        }

        // If Record Arm is held in sequencer mode, handle mode selection and voltage/bipolar settings
        if (recArmHeld && currentLayout > 0) {
            Sequencer& seq = sequencers[currentLayout - 1];

            // Track Focus row (top): Mode selection (all 8 buttons for competition/routing modes)
            for (int m = 0; m < 8; m++) {
                if (note == LCXL::TRACK_FOCUS[m]) {
                    if (seq.isStepSingleMode()) {
                        seq.routingMode = m;
                        recordChange(CHANGE_ROUTE_MODE, currentLayout, m);
                    } else {
                        seq.competitionMode = m;
                        recordChange(CHANGE_COMP_MODE, currentLayout, m);
                    }
                    showModeSelectionLEDs();
                    return;
                }
            }

            // Track Control row (bottom): Voltage and polarity settings
            // Button 1: Cycle voltage range A (green=5V, amber=10V, red=1V)
            if (note == LCXL::TRACK_CONTROL[0]) {
                seq.voltageRangeA = (seq.voltageRangeA + 1) % 3;
                recordChange(CHANGE_VOLTAGE_A, currentLayout, seq.voltageRangeA);
                showModeSelectionLEDs();
                return;
            }
            // Button 2: Toggle bipolar A
            if (note == LCXL::TRACK_CONTROL[1]) {
                seq.bipolarA = !seq.bipolarA;
                recordChange(CHANGE_BIPOLAR_A, currentLayout, seq.bipolarA ? 1 : 0);
                showModeSelectionLEDs();
                return;
            }
            // Button 5: Cycle voltage range B
            if (note == LCXL::TRACK_CONTROL[4]) {
                seq.voltageRangeB = (seq.voltageRangeB + 1) % 3;
                recordChange(CHANGE_VOLTAGE_B, currentLayout, seq.voltageRangeB);
                showModeSelectionLEDs();
                return;
            }
            // Button 6: Toggle bipolar B
            if (note == LCXL::TRACK_CONTROL[5]) {
                seq.bipolarB = !seq.bipolarB;
                recordChange(CHANGE_BIPOLAR_B, currentLayout, seq.bipolarB ? 1 : 0);
                showModeSelectionLEDs();
                return;
            }
        }

        // If Device is held, check for layout switching and utilities
        if (deviceButtonHeld) {
            // Track Focus 1 = return to default
            if (note == LCXL::TRACK_FOCUS[0]) {
                switchLayout(0);
                return;
            }

            // Track Focus 2-8 = utilities (only in sequencer mode)
            if (currentLayout > 0) {
                for (int i = 1; i < 8; i++) {
                    if (note == LCXL::TRACK_FOCUS[i]) {
                        executeSequencerUtility(i);
                        return;
                    }
                }
            }

            // Track Control 1-8 = enter sequencer 1-8
            for (int i = 0; i < 8; i++) {
                if (note == LCXL::TRACK_CONTROL[i]) {
                    switchLayout(i + 1);
                    return;
                }
            }
        }

        // Normal button handling
        if (currentLayout == 0) {
            // Default mode: toggle gate outputs
            processDefaultModeButton(note);
        } else {
            // Sequencer mode: toggle steps
            processSequencerModeButton(note);
        }
    }

    void showModeSelectionLEDs() {
        if (currentLayout <= 0) return;
        Sequencer& seq = sequencers[currentLayout - 1];

        // Track Focus row (top, buttons 0-7): Mode selection (all 8 modes)
        int currentMode = seq.isStepSingleMode() ? seq.routingMode : seq.competitionMode;
        for (int m = 0; m < 8; m++) {
            uint8_t color = (m == currentMode) ? LCXL::LED_GREEN_FULL : LCXL::LED_OFF;
            sendButtonLEDSysEx(m, color);
        }

        // Track Control row (bottom, buttons 8-15): Voltage and polarity settings
        // Button 8 (index 0): Voltage range A (green=5V, amber=10V, red=1V)
        uint8_t voltColorA = (seq.voltageRangeA == 0) ? LCXL::LED_GREEN_FULL :
                             (seq.voltageRangeA == 1) ? LCXL::LED_AMBER_FULL : LCXL::LED_RED_FULL;
        sendButtonLEDSysEx(8, voltColorA);

        // Button 9 (index 1): Bipolar A (green=unipolar, red=bipolar)
        sendButtonLEDSysEx(9, seq.bipolarA ? LCXL::LED_RED_FULL : LCXL::LED_GREEN_FULL);

        // Buttons 10, 11: Off
        sendButtonLEDSysEx(10, LCXL::LED_OFF);
        sendButtonLEDSysEx(11, LCXL::LED_OFF);

        // Button 12 (index 4): Voltage range B
        uint8_t voltColorB = (seq.voltageRangeB == 0) ? LCXL::LED_GREEN_FULL :
                             (seq.voltageRangeB == 1) ? LCXL::LED_AMBER_FULL : LCXL::LED_RED_FULL;
        sendButtonLEDSysEx(12, voltColorB);

        // Button 13 (index 5): Bipolar B
        sendButtonLEDSysEx(13, seq.bipolarB ? LCXL::LED_RED_FULL : LCXL::LED_GREEN_FULL);

        // Buttons 14, 15: Off
        sendButtonLEDSysEx(14, LCXL::LED_OFF);
        sendButtonLEDSysEx(15, LCXL::LED_OFF);
    }

    void showLayoutSelectionLEDs() {
        // Show current layout on Track Focus buttons
        // Button 0 = default (layout 0), Buttons 1-7 unused in default mode
        // Track Control 1-8 = sequencer layouts 1-8
        for (int i = 0; i < 8; i++) {
            uint8_t color = LCXL::LED_OFF;
            if (currentLayout == 0 && i == 0) {
                color = LCXL::LED_GREEN_FULL;  // Default layout selected
            }
            sendButtonLEDSysEx(i, color);
        }
        // Track Control buttons show sequencer selection
        for (int i = 0; i < 8; i++) {
            uint8_t color = (currentLayout == i + 1) ? LCXL::LED_GREEN_FULL : LCXL::LED_OFF;
            sendButtonLEDSysEx(8 + i, color);
        }
    }

    void executeSequencerUtility(int utilityIndex) {
        Sequencer& seq = sequencers[currentLayout - 1];

        switch (utilityIndex) {
            case 1:  // Copy current sequencer
                copyBuffer = seq;
                break;

            case 2:  // Paste to current sequencer
                seq.steps[0] = copyBuffer.steps[0];  // Copy step pattern
                for (int i = 0; i < 16; i++) {
                    seq.steps[i] = copyBuffer.steps[i];
                }
                updateSequencerLEDs();
                break;

            case 3:  // Clear all steps
                for (int i = 0; i < 16; i++) {
                    seq.steps[i] = false;
                }
                updateSequencerLEDs();
                break;

            case 4:  // Randomize steps
                for (int i = 0; i < 16; i++) {
                    seq.steps[i] = random::uniform() > 0.5f;
                }
                updateSequencerLEDs();
                break;

            case 5:  // Randomize values
                for (int i = 0; i < 16; i++) {
                    knobValues[currentLayout][i] = static_cast<int>(random::uniform() * 127);
                }
                updateSequencerLEDs();
                break;

            case 6:  // Invert steps
                for (int i = 0; i < 16; i++) {
                    seq.steps[i] = !seq.steps[i];
                }
                updateSequencerLEDs();
                break;

            case 7:  // Reset playheads
                seq.currentStepA = 0;
                seq.currentStepB = 0;
                seq.currentValueIndexA = 0;
                seq.currentValueIndexB = 0;
                seq.alternateCounter = 0;
                updateSequencerLEDs();
                break;
        }
    }

    // Copy buffer for sequencer copy/paste
    Sequencer copyBuffer;

    // Convert knob value (0-127) to voltage based on range and bipolar settings
    // Range: 0=5V, 1=10V, 2=1V
    // Bipolar: false=unipolar (0 to max), true=bipolar (-max/2 to +max/2)
    float knobToVoltage(int knobValue, int voltageRange, bool bipolar) {
        float normalized = knobValue / 127.f;  // 0.0 to 1.0
        float maxVoltage;
        switch (voltageRange) {
            case 0: maxVoltage = 5.f; break;   // Green: 5V
            case 1: maxVoltage = 10.f; break;  // Amber: 10V
            case 2: maxVoltage = 1.f; break;   // Red: 1V
            default: maxVoltage = 5.f; break;
        }
        if (bipolar) {
            return normalized * maxVoltage - (maxVoltage / 2.f);  // -max/2 to +max/2
        } else {
            return normalized * maxVoltage;  // 0 to max
        }
    }

    void processNoteOff(int note) {
        if (note == LCXL::BTN_DEVICE) {
            deviceButtonHeld = false;
            // Restore normal LEDs when releasing Device
            updateAllLEDs();
            return;
        }

        if (note == LCXL::BTN_REC_ARM) {
            recArmHeld = false;
            // Restore normal LEDs when releasing Record Arm
            if (currentLayout > 0) {
                updateSequencerLEDs();
            }
            return;
        }
    }

    void processDefaultModeButton(int note) {
        // Track Focus buttons = gates 1-8
        for (int i = 0; i < 8; i++) {
            if (note == LCXL::TRACK_FOCUS[i]) {
                buttonStates[i] = !buttonStates[i];
                updateButtonLED(i, buttonStates[i]);
                return;
            }
        }

        // Track Control buttons = gates 9-16
        for (int i = 0; i < 8; i++) {
            if (note == LCXL::TRACK_CONTROL[i]) {
                buttonStates[8 + i] = !buttonStates[8 + i];
                updateButtonLED(8 + i, buttonStates[8 + i]);
                return;
            }
        }
    }

    // Helper to get step index from MIDI note (-1 if not a step button)
    int getStepIndexFromNote(int note) {
        for (int i = 0; i < 8; i++) {
            if (note == LCXL::TRACK_FOCUS[i]) return i;
            if (note == LCXL::TRACK_CONTROL[i]) return 8 + i;
        }
        return -1;
    }

    void processSequencerModeButton(int note) {
        int stepIndex = getStepIndexFromNote(note);
        if (stepIndex < 0) return;

        int seqIndex = currentLayout - 1;
        Sequencer& seq = sequencers[seqIndex];

        // Toggle step on/off
        seq.steps[stepIndex] = !seq.steps[stepIndex];
        recordChange(CHANGE_STEP_TOGGLE, currentLayout, seq.steps[stepIndex] ? 1 : 0, stepIndex);

        // Update LED for this step
        if (seq.isStepSingleMode()) {
            updateStepLEDSingle(stepIndex, seq);
        } else {
            bool isSeqA = stepIndex < 8;
            updateStepLEDDual(stepIndex, seq, isSeqA);
        }
    }

    void switchLayout(int newLayout) {
        if (newLayout == currentLayout) {
            return;
        }

        currentLayout = newLayout;
        recordChange(CHANGE_LAYOUT, newLayout, newLayout);

        // Reset pickup state for all knobs
        for (int i = 0; i < 24; i++) {
            knobPickedUp[i] = false;
        }

        // Update LEDs - but if Device is still held, show selection instead
        if (deviceButtonHeld) {
            showLayoutSelectionLEDs();
        } else {
            updateAllLEDs();
        }
    }

    void updateKnobLED(int knobIndex) {
        int storedValue = knobValues[currentLayout][knobIndex];
        int physicalPos = lastPhysicalKnobPos[knobIndex];

        uint8_t color;
        if (physicalPos < 0) {
            // Unknown physical position - assume picked up until we receive MIDI
            color = LCXL::LED_GREEN_FULL;
        } else if (knobPickedUp[knobIndex] || std::abs(physicalPos - storedValue) <= 2) {
            color = LCXL::LED_GREEN_FULL;
        } else if (physicalPos < storedValue) {
            color = LCXL::LED_YELLOW_FULL;
        } else {
            color = LCXL::LED_RED_FULL;
        }

        sendKnobLEDSysEx(knobIndex, color);
    }

    void updateButtonLED(int buttonIndex, bool on) {
        uint8_t color = on ? LCXL::LED_GREEN_FULL : LCXL::LED_OFF;
        sendButtonLEDSysEx(buttonIndex, color);
    }

    void updateAllLEDs() {
        // Update knob LEDs based on current layout
        if (currentLayout == 0) {
            // Default mode: all knobs use standard soft-takeover coloring
            for (int i = 0; i < 24; i++) {
                updateKnobLED(i);
            }
            // Update button LEDs for default mode
            for (int i = 0; i < 16; i++) {
                updateButtonLED(i, buttonStates[i]);
            }
        } else {
            // Sequencer mode: use new LED functions
            updateSequencerLEDs();
        }
    }

    void sendKnobLEDSysEx(int knobIndex, uint8_t color) {
        // SysEx: F0 00 20 29 02 11 78 [template] [index] [color] F7
        // Template 8 = Factory Template 1
        midi::Message msg;
        msg.bytes = {
            0xF0,
            0x00, 0x20, 0x29, 0x02, 0x11,
            0x78,  // LED command (120 decimal)
            0x08,  // Template 8 (Factory Template 1)
            static_cast<uint8_t>(knobIndex),
            color,
            0xF7
        };
        midiOutput.sendMessage(msg);
    }

    void sendButtonLEDSysEx(int buttonIndex, uint8_t color) {
        // Button LED indices:
        // Knobs: 0-7 (row 1), 8-15 (row 2), 16-23 (row 3)
        // Buttons: Track Focus 24-31, Track Control 32-39
        uint8_t ledIndex;
        if (buttonIndex < 8) {
            ledIndex = 24 + buttonIndex;  // Top row (Track Focus) = indices 24-31
        } else {
            ledIndex = 32 + (buttonIndex - 8);  // Bottom row (Track Control) = indices 32-39
        }

        midi::Message msg;
        msg.bytes = {
            0xF0,
            0x00, 0x20, 0x29, 0x02, 0x11,
            0x78,  // LED command (120 decimal)
            0x08,  // Template 8 (Factory Template 1)
            ledIndex,
            color,
            0xF7
        };
        midiOutput.sendMessage(msg);
    }

    void sendResetLEDs() {
        // Reset command: B8 00 00 (176+8, 0, 0)
        // This clears all LEDs on template 8 (Factory Template 1)
        midi::Message msg;
        msg.bytes = {0xB8, 0x00, 0x00};  // CC channel 9, CC 0, value 0
        midiOutput.sendMessage(msg);
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();

        // Save MIDI settings
        json_object_set_new(rootJ, "midiInput", midiInput.toJson());
        json_object_set_new(rootJ, "midiOutput", midiOutput.toJson());

        // Save current layout
        json_object_set_new(rootJ, "currentLayout", json_integer(currentLayout));
        json_object_set_new(rootJ, "outputLayout", json_integer(outputLayout));

        // Save fader values
        json_t* fadersJ = json_array();
        for (int i = 0; i < 8; i++) {
            json_array_append_new(fadersJ, json_integer(faderValues[i]));
        }
        json_object_set_new(rootJ, "faders", fadersJ);

        // Save knob values for all layouts
        json_t* knobsJ = json_array();
        for (int layout = 0; layout < 9; layout++) {
            json_t* layoutJ = json_array();
            for (int i = 0; i < 24; i++) {
                json_array_append_new(layoutJ, json_integer(knobValues[layout][i]));
            }
            json_array_append_new(knobsJ, layoutJ);
        }
        json_object_set_new(rootJ, "knobs", knobsJ);

        // Save button states
        json_t* buttonsJ = json_array();
        for (int i = 0; i < 16; i++) {
            json_array_append_new(buttonsJ, json_boolean(buttonStates[i]));
        }
        json_object_set_new(rootJ, "buttons", buttonsJ);

        // Save sequencer states
        json_t* seqsJ = json_array();
        for (int s = 0; s < 8; s++) {
            json_t* seqJ = json_object();

            // Save steps
            json_t* stepsJ = json_array();
            for (int i = 0; i < 16; i++) {
                json_array_append_new(stepsJ, json_boolean(sequencers[s].steps[i]));
            }
            json_object_set_new(seqJ, "steps", stepsJ);

            // Save lengths
            json_object_set_new(seqJ, "valueLengthA", json_integer(sequencers[s].valueLengthA));
            json_object_set_new(seqJ, "valueLengthB", json_integer(sequencers[s].valueLengthB));
            json_object_set_new(seqJ, "stepLengthA", json_integer(sequencers[s].stepLengthA));
            json_object_set_new(seqJ, "stepLengthB", json_integer(sequencers[s].stepLengthB));

            // Save probability and bias
            json_object_set_new(seqJ, "probA", json_real(sequencers[s].probA));
            json_object_set_new(seqJ, "probB", json_real(sequencers[s].probB));
            json_object_set_new(seqJ, "bias", json_real(sequencers[s].bias));

            // Save modes
            json_object_set_new(seqJ, "competitionMode", json_integer(sequencers[s].competitionMode));
            json_object_set_new(seqJ, "routingMode", json_integer(sequencers[s].routingMode));

            // Save voltage settings
            json_object_set_new(seqJ, "voltageRangeA", json_integer(sequencers[s].voltageRangeA));
            json_object_set_new(seqJ, "voltageRangeB", json_integer(sequencers[s].voltageRangeB));
            json_object_set_new(seqJ, "bipolarA", json_boolean(sequencers[s].bipolarA));
            json_object_set_new(seqJ, "bipolarB", json_boolean(sequencers[s].bipolarB));

            json_array_append_new(seqsJ, seqJ);
        }
        json_object_set_new(rootJ, "sequencers", seqsJ);

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        // Load MIDI settings
        json_t* midiInputJ = json_object_get(rootJ, "midiInput");
        if (midiInputJ) midiInput.fromJson(midiInputJ);

        json_t* midiOutputJ = json_object_get(rootJ, "midiOutput");
        if (midiOutputJ) midiOutput.fromJson(midiOutputJ);

        // Load current layout
        json_t* layoutJ = json_object_get(rootJ, "currentLayout");
        if (layoutJ) currentLayout = json_integer_value(layoutJ);
        json_t* outLayoutJ = json_object_get(rootJ, "outputLayout");
        if (outLayoutJ) outputLayout = json_integer_value(outLayoutJ);

        // Load fader values
        json_t* fadersJ = json_object_get(rootJ, "faders");
        if (fadersJ) {
            for (int i = 0; i < 8; i++) {
                json_t* valJ = json_array_get(fadersJ, i);
                if (valJ) faderValues[i] = json_integer_value(valJ);
            }
        }

        // Load knob values
        json_t* knobsJ = json_object_get(rootJ, "knobs");
        if (knobsJ) {
            for (int layout = 0; layout < 9; layout++) {
                json_t* layoutJ = json_array_get(knobsJ, layout);
                if (layoutJ) {
                    for (int i = 0; i < 24; i++) {
                        json_t* valJ = json_array_get(layoutJ, i);
                        if (valJ) knobValues[layout][i] = json_integer_value(valJ);
                    }
                }
            }
        }

        // Load button states
        json_t* buttonsJ = json_object_get(rootJ, "buttons");
        if (buttonsJ) {
            for (int i = 0; i < 16; i++) {
                json_t* valJ = json_array_get(buttonsJ, i);
                if (valJ) buttonStates[i] = json_boolean_value(valJ);
            }
        }

        // Load sequencer states
        json_t* seqsJ = json_object_get(rootJ, "sequencers");
        if (seqsJ) {
            for (int s = 0; s < 8; s++) {
                json_t* seqJ = json_array_get(seqsJ, s);
                if (seqJ) {
                    // Load steps
                    json_t* stepsJ = json_object_get(seqJ, "steps");
                    if (stepsJ) {
                        for (int i = 0; i < 16; i++) {
                            json_t* valJ = json_array_get(stepsJ, i);
                            if (valJ) sequencers[s].steps[i] = json_boolean_value(valJ);
                        }
                    }

                    // Load lengths
                    json_t* vlA = json_object_get(seqJ, "valueLengthA");
                    if (vlA) sequencers[s].valueLengthA = json_integer_value(vlA);
                    json_t* vlB = json_object_get(seqJ, "valueLengthB");
                    if (vlB) sequencers[s].valueLengthB = json_integer_value(vlB);
                    json_t* slA = json_object_get(seqJ, "stepLengthA");
                    if (slA) sequencers[s].stepLengthA = json_integer_value(slA);
                    json_t* slB = json_object_get(seqJ, "stepLengthB");
                    if (slB) sequencers[s].stepLengthB = json_integer_value(slB);

                    // Load probability and bias
                    json_t* pA = json_object_get(seqJ, "probA");
                    if (pA) sequencers[s].probA = json_real_value(pA);
                    json_t* pB = json_object_get(seqJ, "probB");
                    if (pB) sequencers[s].probB = json_real_value(pB);
                    json_t* biasJ = json_object_get(seqJ, "bias");
                    if (biasJ) sequencers[s].bias = json_real_value(biasJ);

                    // Load modes
                    json_t* compMode = json_object_get(seqJ, "competitionMode");
                    if (compMode) sequencers[s].competitionMode = json_integer_value(compMode);
                    json_t* routMode = json_object_get(seqJ, "routingMode");
                    if (routMode) sequencers[s].routingMode = json_integer_value(routMode);

                    // Load voltage settings
                    json_t* vrA = json_object_get(seqJ, "voltageRangeA");
                    if (vrA) sequencers[s].voltageRangeA = json_integer_value(vrA);
                    json_t* vrB = json_object_get(seqJ, "voltageRangeB");
                    if (vrB) sequencers[s].voltageRangeB = json_integer_value(vrB);
                    json_t* bpA = json_object_get(seqJ, "bipolarA");
                    if (bpA) sequencers[s].bipolarA = json_boolean_value(bpA);
                    json_t* bpB = json_object_get(seqJ, "bipolarB");
                    if (bpB) sequencers[s].bipolarB = json_boolean_value(bpB);
                }
            }
        }
    }
};

// Simple label widget for panel text
struct PanelLabel : widget::Widget {
    std::string text;
    NVGcolor color = nvgRGB(0x99, 0x99, 0x99);
    float fontSize = 8.f;

    void draw(const DrawArgs& args) override {
        nvgFontSize(args.vg, fontSize);
        nvgFillColor(args.vg, color);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(args.vg, box.size.x / 2, box.size.y / 2, text.c_str(), NULL);
    }
};

// Helper to create labels easily
PanelLabel* createLabel(Vec pos, Vec size, std::string text, float fontSize = 8.f) {
    PanelLabel* label = new PanelLabel();
    label->box.pos = pos;
    label->box.size = size;
    label->text = text;
    label->fontSize = fontSize;
    return label;
}

struct CoreWidget : ModuleWidget {
    CoreWidget(Core* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Core.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Connected light (MIDI status) - positioned like expanders
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(5, 10)), module, Core::CONNECTED_LIGHT));

        // Takeover button with light - positioned near connected light
        addParam(createParamCentered<VCVButton>(mm2px(Vec(20, 10)), module, Core::TAKEOVER_PARAM));
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(35, 10)), module, Core::TAKEOVER_LIGHT));

        // Column labels (like expanders)
        addChild(createLabel(mm2px(Vec(0, 14)), mm2px(Vec(20, 4)), "CLK/RST", 7.f));
        addChild(createLabel(mm2px(Vec(20, 14)), mm2px(Vec(20, 4)), "SEQ OUT", 7.f));

        // Clock and Reset inputs (left column) - starting at y=22 like expanders
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 22)), module, Core::CLOCK_A_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 32)), module, Core::CLOCK_B_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 42)), module, Core::RESET_INPUT));

        // Sequencer outputs (right column) - starting at y=22 like expanders
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(30, 22)), module, Core::SEQ_TRIG_A_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(30, 32)), module, Core::SEQ_CV_A_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(30, 42)), module, Core::SEQ_TRIG_B_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(30, 52)), module, Core::SEQ_CV_B_OUTPUT));

        // Row labels for inputs/outputs
        addChild(createLabel(mm2px(Vec(15, 20)), mm2px(Vec(10, 4)), "A", 6.f));
        addChild(createLabel(mm2px(Vec(15, 30)), mm2px(Vec(10, 4)), "B", 6.f));
        addChild(createLabel(mm2px(Vec(15, 40)), mm2px(Vec(10, 4)), "RST", 6.f));
        addChild(createLabel(mm2px(Vec(15, 50)), mm2px(Vec(10, 4)), "CV B", 6.f));

        // Faders section header
        addChild(createLabel(mm2px(Vec(10, 60)), mm2px(Vec(20, 5)), "FADERS", 8.f));

        // Fader outputs (bottom section, two columns)
        // Column 1: Faders 1-4
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 70)), module, Core::FADER_OUTPUT_1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 80)), module, Core::FADER_OUTPUT_1 + 1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 90)), module, Core::FADER_OUTPUT_1 + 2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 100)), module, Core::FADER_OUTPUT_1 + 3));
        // Column 2: Faders 5-8
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(30, 70)), module, Core::FADER_OUTPUT_1 + 4));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(30, 80)), module, Core::FADER_OUTPUT_1 + 5));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(30, 90)), module, Core::FADER_OUTPUT_1 + 6));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(30, 100)), module, Core::FADER_OUTPUT_1 + 7));

        // Fader row numbers (between columns)
        addChild(createLabel(mm2px(Vec(15, 68)), mm2px(Vec(10, 4)), "1    5", 6.f));
        addChild(createLabel(mm2px(Vec(15, 78)), mm2px(Vec(10, 4)), "2    6", 6.f));
        addChild(createLabel(mm2px(Vec(15, 88)), mm2px(Vec(10, 4)), "3    7", 6.f));
        addChild(createLabel(mm2px(Vec(15, 98)), mm2px(Vec(10, 4)), "4    8", 6.f));
    }

    void appendContextMenu(Menu* menu) override {
        Core* module = dynamic_cast<Core*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("MIDI Input"));
        app::appendMidiMenu(menu, &module->midiInput);

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("MIDI Output"));
        app::appendMidiMenu(menu, &module->midiOutput);

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Sequencer Output"));

        // Option to follow current view
        menu->addChild(createCheckMenuItem("Follow view", "",
            [=]() { return module->outputLayout == 0; },
            [=]() { module->outputLayout = 0; }
        ));

        // Options for each sequencer
        for (int i = 1; i <= 8; i++) {
            menu->addChild(createCheckMenuItem(string::f("Sequencer %d", i), "",
                [=]() { return module->outputLayout == i; },
                [=]() { module->outputLayout = i; }
            ));
        }
    }
};

Model* modelCore = createModel<Core, CoreWidget>("Core");
