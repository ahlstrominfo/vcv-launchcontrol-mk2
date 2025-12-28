#include "plugin.hpp"
#include "ExpanderMessage.hpp"

// Helper to convert knob value to voltage based on range and bipolar settings
inline float knobToVoltage(int knobValue, int voltageRange, bool bipolar) {
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

struct SeqExpander : Module {
    enum ParamId {
        PARAMS_LEN
    };
    enum InputId {
        INPUTS_LEN
    };
    enum OutputId {
        ENUMS(TRIG_A_OUTPUT, 8),
        ENUMS(TRIG_B_OUTPUT, 8),
        ENUMS(CV_A_OUTPUT, 8),
        ENUMS(CV_B_OUTPUT, 8),
        OUTPUTS_LEN
    };
    enum LightId {
        CONNECTED_LIGHT,
        LIGHTS_LEN
    };

    LCXLExpanderMessage leftMessages[2];
    LCXLExpanderMessage expanderMessage;  // For forwarding to right
    dsp::PulseGenerator triggerPulsesA[8];
    dsp::PulseGenerator triggerPulsesB[8];

    SeqExpander() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Configure trigger outputs
        for (int i = 0; i < 8; i++) {
            configOutput(TRIG_A_OUTPUT + i, string::f("Sequencer %d Trigger A", i + 1));
            configOutput(TRIG_B_OUTPUT + i, string::f("Sequencer %d Trigger B", i + 1));
        }

        // Configure CV outputs
        for (int i = 0; i < 8; i++) {
            configOutput(CV_A_OUTPUT + i, string::f("Sequencer %d CV A", i + 1));
            configOutput(CV_B_OUTPUT + i, string::f("Sequencer %d CV B", i + 1));
        }

        // Setup expander message buffers
        leftExpander.producerMessage = &leftMessages[0];
        leftExpander.consumerMessage = &leftMessages[1];
    }

    bool isValidExpander(Module* m) {
        return m && (m->model == modelCore || m->model == modelKnobExpander ||
                     m->model == modelGateExpander || m->model == modelSeqExpander);
    }

    void process(const ProcessArgs& args) override {
        bool connected = false;
        LCXLExpanderMessage* msg = nullptr;

        // Check if connected to Core or another expander on the left
        if (isValidExpander(leftExpander.module)) {
            msg = reinterpret_cast<LCXLExpanderMessage*>(leftExpander.consumerMessage);
            if (msg && msg->moduleId >= 0) {
                connected = true;

                for (int s = 0; s < 8; s++) {
                    auto& seq = msg->sequencers[s];
                    int layout = s + 1;  // Sequencers use layouts 1-8

                    // Fire trigger A if sequencer triggered this frame
                    if (seq.triggeredA) {
                        triggerPulsesA[s].trigger(1e-3f);  // 1ms pulse
                    }

                    // Fire trigger B if sequencer triggered this frame
                    if (seq.triggeredB) {
                        triggerPulsesB[s].trigger(1e-3f);  // 1ms pulse
                    }

                    // Output triggers
                    outputs[TRIG_A_OUTPUT + s].setVoltage(triggerPulsesA[s].process(args.sampleTime) ? 10.f : 0.f);
                    outputs[TRIG_B_OUTPUT + s].setVoltage(triggerPulsesB[s].process(args.sampleTime) ? 10.f : 0.f);

                    // Output CV A from value knob with voltage range/bipolar settings
                    int knobIndexA = seq.currentValueIndexA;
                    float cvA = knobToVoltage(msg->knobValues[layout][knobIndexA], seq.voltageRangeA, seq.bipolarA);
                    outputs[CV_A_OUTPUT + s].setVoltage(cvA);

                    // Output CV B from value knob with voltage range/bipolar settings
                    int knobIndexB = seq.isValueSingleMode ? seq.currentValueIndexA : (8 + seq.currentValueIndexB);
                    float cvB = knobToVoltage(msg->knobValues[layout][knobIndexB], seq.voltageRangeB, seq.bipolarB);
                    outputs[CV_B_OUTPUT + s].setVoltage(cvB);
                }

                // Store for forwarding
                expanderMessage = *msg;
            }
        }

        // Forward message to right expander
        if (rightExpander.module && connected) {
            LCXLExpanderMessage* rightMsg = reinterpret_cast<LCXLExpanderMessage*>(rightExpander.module->leftExpander.producerMessage);
            if (rightMsg) {
                *rightMsg = expanderMessage;
                rightExpander.module->leftExpander.messageFlipRequested = true;
            }
        }

        // If not connected, output zeros
        if (!connected) {
            for (int s = 0; s < 8; s++) {
                outputs[TRIG_A_OUTPUT + s].setVoltage(0.f);
                outputs[TRIG_B_OUTPUT + s].setVoltage(0.f);
                outputs[CV_A_OUTPUT + s].setVoltage(0.f);
                outputs[CV_B_OUTPUT + s].setVoltage(0.f);
            }
        }

        lights[CONNECTED_LIGHT].setBrightness(connected ? 1.f : 0.f);
    }
};

// Simple label widget for panel text
struct SeqPanelLabel : widget::Widget {
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

struct SeqExpanderWidget : ModuleWidget {
    SeqExpanderWidget(SeqExpander* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/SeqExpander.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Connected light
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(5, 10)), module, SeqExpander::CONNECTED_LIGHT));

        float y = 22.f;

        // Trigger A outputs (8 jacks) - Column 1
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.5, y + i * 10)), module, SeqExpander::TRIG_A_OUTPUT + i));
        }

        // Trigger B outputs (8 jacks) - Column 2
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.5, y + i * 10)), module, SeqExpander::TRIG_B_OUTPUT + i));
        }

        // CV A outputs (8 jacks) - Column 3
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(27.5, y + i * 10)), module, SeqExpander::CV_A_OUTPUT + i));
        }

        // CV B outputs (8 jacks) - Column 4
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(37.5, y + i * 10)), module, SeqExpander::CV_B_OUTPUT + i));
        }

        // Column labels
        auto addLabel = [this](Vec pos, Vec size, std::string text, float fontSize = 7.f) {
            SeqPanelLabel* label = new SeqPanelLabel();
            label->box.pos = pos;
            label->box.size = size;
            label->text = text;
            label->fontSize = fontSize;
            addChild(label);
        };

        addLabel(mm2px(Vec(0, 14)), mm2px(Vec(15, 4)), "TRG A");
        addLabel(mm2px(Vec(10, 14)), mm2px(Vec(15, 4)), "TRG B");
        addLabel(mm2px(Vec(20, 14)), mm2px(Vec(15, 4)), "CV A");
        addLabel(mm2px(Vec(30, 14)), mm2px(Vec(15, 4)), "CV B");

        // Row numbers (on the right side)
        for (int i = 0; i < 8; i++) {
            addLabel(mm2px(Vec(41, 20 + i * 10)), mm2px(Vec(5, 4)), std::to_string(i + 1), 6.f);
        }
    }
};

Model* modelSeqExpander = createModel<SeqExpander, SeqExpanderWidget>("SeqExpander");
