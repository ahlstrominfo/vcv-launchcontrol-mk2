#include "plugin.hpp"
#include "ExpanderMessage.hpp"

struct KnobExpander : Module {
    enum ParamId {
        PARAMS_LEN
    };
    enum InputId {
        INPUTS_LEN
    };
    enum OutputId {
        ENUMS(KNOB_OUTPUT, 24),
        OUTPUTS_LEN
    };
    enum LightId {
        CONNECTED_LIGHT,
        LIGHTS_LEN
    };

    LCXLExpanderMessage leftMessages[2];
    LCXLExpanderMessage expanderMessage;  // For forwarding to right

    KnobExpander() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Configure knob outputs
        for (int i = 0; i < 8; i++) {
            configOutput(KNOB_OUTPUT + i, string::f("Knob Row 1-%d", i + 1));
        }
        for (int i = 0; i < 8; i++) {
            configOutput(KNOB_OUTPUT + 8 + i, string::f("Knob Row 2-%d", i + 1));
        }
        for (int i = 0; i < 8; i++) {
            configOutput(KNOB_OUTPUT + 16 + i, string::f("Knob Row 3-%d", i + 1));
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
                int layout = msg->currentLayout;

                // Output knob values for current layout (0-10V)
                for (int i = 0; i < 24; i++) {
                    float voltage = msg->knobValues[layout][i] / 127.f * 10.f;
                    outputs[KNOB_OUTPUT + i].setVoltage(voltage);
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
            for (int i = 0; i < 24; i++) {
                outputs[KNOB_OUTPUT + i].setVoltage(0.f);
            }
        }

        lights[CONNECTED_LIGHT].setBrightness(connected ? 1.f : 0.f);
    }
};

// Simple label widget for panel text
struct KnobPanelLabel : widget::Widget {
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

struct KnobExpanderWidget : ModuleWidget {
    KnobExpanderWidget(KnobExpander* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/KnobExpander.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Connected light
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(5, 10)), module, KnobExpander::CONNECTED_LIGHT));

        // Row 1 outputs (8 jacks)
        float y = 22.f;
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.5, y + i * 10)), module, KnobExpander::KNOB_OUTPUT + i));
        }

        // Row 2 outputs (8 jacks)
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.5, y + i * 10)), module, KnobExpander::KNOB_OUTPUT + 8 + i));
        }

        // Row 3 outputs (8 jacks)
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(27.5, y + i * 10)), module, KnobExpander::KNOB_OUTPUT + 16 + i));
        }

        // Column labels
        auto addLabel = [this](Vec pos, Vec size, std::string text, float fontSize = 7.f) {
            KnobPanelLabel* label = new KnobPanelLabel();
            label->box.pos = pos;
            label->box.size = size;
            label->text = text;
            label->fontSize = fontSize;
            addChild(label);
        };

        addLabel(mm2px(Vec(0, 14)), mm2px(Vec(15, 4)), "ROW 1");
        addLabel(mm2px(Vec(10, 14)), mm2px(Vec(15, 4)), "ROW 2");
        addLabel(mm2px(Vec(20, 14)), mm2px(Vec(15, 4)), "ROW 3");

        // Row numbers (on the right side)
        for (int i = 0; i < 8; i++) {
            addLabel(mm2px(Vec(31, 20 + i * 10)), mm2px(Vec(5, 4)), std::to_string(i + 1), 6.f);
        }
    }
};

Model* modelKnobExpander = createModel<KnobExpander, KnobExpanderWidget>("KnobExpander");
