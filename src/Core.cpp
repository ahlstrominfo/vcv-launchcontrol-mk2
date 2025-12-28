#include "plugin.hpp"
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

    // LED color values
    constexpr uint8_t LED_OFF = 12;
    constexpr uint8_t LED_RED_LOW = 13;
    constexpr uint8_t LED_RED_FULL = 15;
    constexpr uint8_t LED_GREEN_LOW = 28;
    constexpr uint8_t LED_GREEN_FULL = 60;
    constexpr uint8_t LED_AMBER_LOW = 29;
    constexpr uint8_t LED_AMBER_FULL = 63;
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
        OUTPUTS_LEN
    };
    enum LightId {
        CONNECTED_LIGHT,
        TAKEOVER_LIGHT,
        LIGHTS_LEN
    };

    bool takenOver = false;
    dsp::BooleanTrigger takeoverTrigger;

    midi::InputQueue midiInput;
    midi::Output midiOutput;

    // Current state
    int currentLayout = 0;  // 0 = default, 1-8 = sequencers
    bool deviceButtonHeld = false;

    // Loop point selection state (for sequencer mode)
    int heldStepIndex = -1;           // Which step is currently held (-1 = none)
    bool loopPointSet = false;        // Was a loop point set while holding?

    // Fader values (0-127 MIDI, converted to 0-10V)
    int faderValues[8] = {0};

    // Knob values per layout (0 = default, 1-8 = sequencers)
    int knobValues[9][24] = {{0}};

    // Button toggle states for default layout (16 buttons)
    bool buttonStates[16] = {false};

    // Sequencer states (8 sequencers)
    struct Sequencer {
        bool steps[16] = {false};       // Step on/off
        int loopStart = 0;              // Loop start step (0-15)
        int loopEnd = 15;               // Loop end step (0-15)
        int currentStep = 0;            // Current playback position
        int currentValueIndex = 0;      // Current value knob index
        int valueStart = 0;             // Start value knob (0-15)
        int valueEnd = 15;              // End value knob (0-15)
    };
    Sequencer sequencers[8];

    // Soft takeover state
    int lastPhysicalKnobPos[24] = {};  // Will be initialized to -1 in constructor
    bool knobPickedUp[24] = {};        // Will be initialized to true in constructor

    Core() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Configure takeover button
        configButton(TAKEOVER_PARAM, "Take Over LEDs");

        // Configure fader outputs
        for (int i = 0; i < 8; i++) {
            configOutput(FADER_OUTPUT_1 + i, string::f("Fader %d", i + 1));
        }

        // Initialize soft takeover state
        for (int i = 0; i < 24; i++) {
            lastPhysicalKnobPos[i] = -1;  // Unknown position
            knobPickedUp[i] = true;        // Start as picked up
        }

        INFO("LaunchControl: Core module created");
    }

    void onAdd(const AddEvent& e) override {
        INFO("LaunchControl: Module added to rack");
        Module::onAdd(e);
    }

    void process(const ProcessArgs& args) override {
        // Check takeover button using trigger for edge detection
        if (takeoverTrigger.process(params[TAKEOVER_PARAM].getValue() > 0.f)) {
            INFO("LaunchControl: TAKE button pressed!");
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

        // Set connected light based on MIDI input device
        bool midiConnected = midiInput.getDeviceId() >= 0;
        lights[CONNECTED_LIGHT].setBrightness(midiConnected ? 1.f : 0.f);

        // Takeover light - stays on once takeover happens
        lights[TAKEOVER_LIGHT].setBrightness(takenOver ? 1.f : 0.f);
    }

    void performTakeover() {
        INFO("LaunchControl: performTakeover() called");

        // Check if MIDI output is connected
        if (midiOutput.getDeviceId() < 0) {
            INFO("LaunchControl: No MIDI output device selected!");
            // Still set takenOver so light turns on
            takenOver = true;
            return;
        }

        INFO("LaunchControl: MIDI output device ID = %d", midiOutput.getDeviceId());

        // First, force the Launch Control XL to template 8 (Factory Template 1)
        sendForceTemplate(8);
        // Then reset/clean all LEDs
        sendResetLEDs();
        takenOver = true;
        // Update LEDs with our state
        updateAllLEDs();
        INFO("LaunchControl: performTakeover() complete");
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
        INFO("LaunchControl: Sending force template SysEx, %zu bytes", msg.bytes.size());
        midiOutput.sendMessage(msg);
    }

    void processMidiMessage(const midi::Message& msg) {
        int channel = msg.getChannel();
        int status = msg.getStatus();

        // Uncomment for debugging: INFO("LaunchControl MIDI IN: status=%x channel=%d note=%d value=%d", status, channel, msg.getNote(), msg.getValue());

        // Only process messages on our expected channel
        if (channel != LCXL::MIDI_CHANNEL) {
            // Don't log ignored messages to avoid spam
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
        // Soft takeover logic
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

        lastPhysicalKnobPos[knobIndex] = value;
        updateKnobLED(knobIndex);
    }

    void processNoteOn(int note, int velocity) {
        INFO("LaunchControl: Note ON - note=%d velocity=%d", note, velocity);

        if (velocity == 0) {
            processNoteOff(note);
            return;
        }

        // Check for Device button
        if (note == LCXL::BTN_DEVICE) {
            INFO("LaunchControl: Device button HELD");
            deviceButtonHeld = true;
            return;
        }

        // If Device is held, check for layout switching
        if (deviceButtonHeld) {
            INFO("LaunchControl: Device held, checking for layout switch...");
            // Track Focus 1 = return to default
            if (note == LCXL::TRACK_FOCUS[0]) {
                INFO("LaunchControl: Switching to default layout");
                switchLayout(0);
                return;
            }

            // Track Control 1-8 = enter sequencer 1-8
            for (int i = 0; i < 8; i++) {
                if (note == LCXL::TRACK_CONTROL[i]) {
                    INFO("LaunchControl: Switching to sequencer %d", i + 1);
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

    void processNoteOff(int note) {
        if (note == LCXL::BTN_DEVICE) {
            deviceButtonHeld = false;
            return;
        }

        // Handle step button release in sequencer mode
        if (currentLayout > 0) {
            processSequencerModeButtonRelease(note);
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

        if (heldStepIndex < 0) {
            // No step currently held - this becomes the held step
            heldStepIndex = stepIndex;
            loopPointSet = false;
            INFO("LaunchControl: Holding step %d for loop selection", stepIndex);
        } else {
            // Another step is held - set loop points!
            int startStep = std::min(heldStepIndex, stepIndex);
            int endStep = std::max(heldStepIndex, stepIndex);

            seq.loopStart = startStep;
            seq.loopEnd = endStep;
            loopPointSet = true;

            INFO("LaunchControl: Set loop points: %d to %d", startStep, endStep);

            // Update LEDs to show new loop points
            for (int i = 0; i < 16; i++) {
                updateStepLED(i, seq);
            }
        }
    }

    void processSequencerModeButtonRelease(int note) {
        int stepIndex = getStepIndexFromNote(note);
        if (stepIndex < 0) return;

        // Only process if this is the held step being released
        if (stepIndex == heldStepIndex) {
            if (!loopPointSet) {
                // No loop point was set - toggle the step
                int seqIndex = currentLayout - 1;
                Sequencer& seq = sequencers[seqIndex];
                seq.steps[stepIndex] = !seq.steps[stepIndex];
                updateStepLED(stepIndex, seq);
                INFO("LaunchControl: Toggled step %d", stepIndex);
            }
            // Reset held state
            heldStepIndex = -1;
            loopPointSet = false;
        }
    }

    void switchLayout(int newLayout) {
        if (newLayout == currentLayout) {
            INFO("LaunchControl: Already in layout %d, ignoring", newLayout);
            return;
        }

        INFO("LaunchControl: Switching from layout %d to %d", currentLayout, newLayout);
        currentLayout = newLayout;

        // Reset pickup state for all knobs
        for (int i = 0; i < 24; i++) {
            knobPickedUp[i] = false;
        }

        // Update all LEDs for new layout
        updateAllLEDs();
        INFO("LaunchControl: Layout switch complete, LEDs updated");
    }

    void updateKnobLED(int knobIndex) {
        int storedValue = knobValues[currentLayout][knobIndex];
        int physicalPos = lastPhysicalKnobPos[knobIndex];

        uint8_t color;
        if (physicalPos < 0) {
            color = LCXL::LED_OFF;
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

    void updateStepLED(int stepIndex, const Sequencer& seq) {
        uint8_t color;
        if (stepIndex == seq.currentStep) {
            color = LCXL::LED_RED_FULL;  // Playhead
        } else if (stepIndex == seq.loopStart || stepIndex == seq.loopEnd) {
            color = LCXL::LED_AMBER_FULL;  // Loop markers
        } else if (seq.steps[stepIndex]) {
            color = LCXL::LED_GREEN_FULL;  // Active step
        } else {
            color = LCXL::LED_OFF;  // Inactive step
        }
        sendButtonLEDSysEx(stepIndex, color);
    }

    void updateAllLEDs() {
        // Update all knob LEDs
        for (int i = 0; i < 24; i++) {
            updateKnobLED(i);
        }

        // Update button LEDs based on current layout
        if (currentLayout == 0) {
            for (int i = 0; i < 16; i++) {
                updateButtonLED(i, buttonStates[i]);
            }
        } else {
            Sequencer& seq = sequencers[currentLayout - 1];
            for (int i = 0; i < 16; i++) {
                updateStepLED(i, seq);
            }
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
        INFO("LaunchControl: Sending reset LEDs (B8 00 00)");
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
            json_t* stepsJ = json_array();
            for (int i = 0; i < 16; i++) {
                json_array_append_new(stepsJ, json_boolean(sequencers[s].steps[i]));
            }
            json_object_set_new(seqJ, "steps", stepsJ);
            json_object_set_new(seqJ, "loopStart", json_integer(sequencers[s].loopStart));
            json_object_set_new(seqJ, "loopEnd", json_integer(sequencers[s].loopEnd));
            json_object_set_new(seqJ, "valueStart", json_integer(sequencers[s].valueStart));
            json_object_set_new(seqJ, "valueEnd", json_integer(sequencers[s].valueEnd));
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
                    json_t* stepsJ = json_object_get(seqJ, "steps");
                    if (stepsJ) {
                        for (int i = 0; i < 16; i++) {
                            json_t* valJ = json_array_get(stepsJ, i);
                            if (valJ) sequencers[s].steps[i] = json_boolean_value(valJ);
                        }
                    }
                    json_t* lsJ = json_object_get(seqJ, "loopStart");
                    if (lsJ) sequencers[s].loopStart = json_integer_value(lsJ);
                    json_t* leJ = json_object_get(seqJ, "loopEnd");
                    if (leJ) sequencers[s].loopEnd = json_integer_value(leJ);
                    json_t* vsJ = json_object_get(seqJ, "valueStart");
                    if (vsJ) sequencers[s].valueStart = json_integer_value(vsJ);
                    json_t* veJ = json_object_get(seqJ, "valueEnd");
                    if (veJ) sequencers[s].valueEnd = json_integer_value(veJ);
                }
            }
        }
    }
};

struct CoreWidget : ModuleWidget {
    CoreWidget(Core* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Core.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Connected light (MIDI status)
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(7, 18)), module, Core::CONNECTED_LIGHT));

        // Takeover button with light
        addParam(createParamCentered<VCVButton>(mm2px(Vec(20, 22)), module, Core::TAKEOVER_PARAM));
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(27, 22)), module, Core::TAKEOVER_LIGHT));

        // Fader outputs
        float yStart = 35.0f;
        float ySpacing = 10.0f;
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, yStart + i * ySpacing)), module, Core::FADER_OUTPUT_1 + i));
        }
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
    }
};

Model* modelCore = createModel<Core, CoreWidget>("Core");
