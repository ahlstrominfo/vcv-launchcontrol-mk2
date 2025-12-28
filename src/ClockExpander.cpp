#include "plugin.hpp"
#include "ExpanderMessage.hpp"

struct ClockExpander : Module {
    enum ParamId {
        PARAMS_LEN
    };
    enum InputId {
        ENUMS(CLK_A_INPUT, 8),
        ENUMS(CLK_B_INPUT, 8),
        INPUTS_LEN
    };
    enum OutputId {
        OUTPUTS_LEN
    };
    enum LightId {
        CONNECTED_LIGHT,
        LIGHTS_LEN
    };

    ClockExpanderMessage rightMessages[2];

    ClockExpander() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Configure clock inputs
        for (int i = 0; i < 8; i++) {
            configInput(CLK_A_INPUT + i, string::f("Sequencer %d Clock A", i + 1));
            configInput(CLK_B_INPUT + i, string::f("Sequencer %d Clock B (normaled to A)", i + 1));
        }

        // Setup expander message buffers (sending TO the right, i.e., to Core)
        rightExpander.producerMessage = &rightMessages[0];
        rightExpander.consumerMessage = &rightMessages[1];
    }

    void process(const ProcessArgs& args) override {
        bool connected = false;

        // Check if connected to Core on the right
        if (rightExpander.module && rightExpander.module->model == modelCore) {
            // Write to Core's left expander buffer (same pattern as Core -> right expanders)
            ClockExpanderMessage* msg = reinterpret_cast<ClockExpanderMessage*>(rightExpander.module->leftExpander.producerMessage);
            if (msg) {
                connected = true;
                msg->moduleId = id;

                // Process each sequencer's clocks
                for (int s = 0; s < 8; s++) {
                    // Clock A with chaining: check this input, then previous ones in chain
                    float clockAVoltage = 0.f;
                    bool hasClockA = false;

                    // Check from this sequencer backwards to find connected clock
                    for (int i = s; i >= 0; i--) {
                        if (inputs[CLK_A_INPUT + i].isConnected()) {
                            clockAVoltage = inputs[CLK_A_INPUT + i].getVoltage();
                            hasClockA = true;
                            break;
                        }
                    }

                    msg->clockA[s] = clockAVoltage;
                    msg->hasClockA[s] = hasClockA;

                    // Clock B: only normaled to own Clock A, not chained
                    if (inputs[CLK_B_INPUT + s].isConnected()) {
                        msg->clockB[s] = inputs[CLK_B_INPUT + s].getVoltage();
                        msg->hasClockB[s] = true;
                    } else {
                        // Normal to this sequencer's clock A
                        msg->clockB[s] = clockAVoltage;
                        msg->hasClockB[s] = hasClockA;  // Only has B if A is connected
                    }
                }

                rightExpander.module->leftExpander.messageFlipRequested = true;
            }
        }

        lights[CONNECTED_LIGHT].setBrightness(connected ? 1.f : 0.f);
    }
};

// Simple label widget for panel text
struct ClockPanelLabel : widget::Widget {
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

struct ClockExpanderWidget : ModuleWidget {
    ClockExpanderWidget(ClockExpander* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/ClockExpander.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Connected light
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(20, 10)), module, ClockExpander::CONNECTED_LIGHT));

        float y = 22.f;

        // Clock A inputs (8 jacks) - Column 1
        for (int i = 0; i < 8; i++) {
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.5, y + i * 10)), module, ClockExpander::CLK_A_INPUT + i));
        }

        // Clock B inputs (8 jacks) - Column 2
        for (int i = 0; i < 8; i++) {
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.5, y + i * 10)), module, ClockExpander::CLK_B_INPUT + i));
        }

        // Column labels
        auto addLabel = [this](Vec pos, Vec size, std::string text, float fontSize = 7.f) {
            ClockPanelLabel* label = new ClockPanelLabel();
            label->box.pos = pos;
            label->box.size = size;
            label->text = text;
            label->fontSize = fontSize;
            addChild(label);
        };

        addLabel(mm2px(Vec(0, 14)), mm2px(Vec(15, 4)), "CLK A");
        addLabel(mm2px(Vec(10, 14)), mm2px(Vec(15, 4)), "CLK B");

        // Row numbers (on the right side)
        for (int i = 0; i < 8; i++) {
            addLabel(mm2px(Vec(21, 20 + i * 10)), mm2px(Vec(5, 4)), std::to_string(i + 1), 6.f);
        }
    }
};

Model* modelClockExpander = createModel<ClockExpander, ClockExpanderWidget>("ClockExpander");
