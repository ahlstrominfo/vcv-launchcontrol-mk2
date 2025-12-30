#include "plugin.hpp"
#include "ExpanderMessage.hpp"

struct CVExpander : Module {
    enum ParamId {
        PARAMS_LEN
    };
    enum InputId {
        INPUTS_LEN
    };
    enum OutputId {
        ENUMS(CV1_OUTPUT, 8),
        ENUMS(CV2_OUTPUT, 8),
        ENUMS(CV3_OUTPUT, 8),
        OUTPUTS_LEN
    };
    enum LightId {
        CONNECTED_LIGHT,
        LIGHTS_LEN
    };

    LCXLExpanderMessage leftMessages[2];
    LCXLExpanderMessage expanderMessage;  // For forwarding to right

    CVExpander() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Configure CV outputs
        for (int i = 0; i < 8; i++) {
            configOutput(CV1_OUTPUT + i, string::f("Sequencer %d CV 1", i + 1));
            configOutput(CV2_OUTPUT + i, string::f("Sequencer %d CV 2", i + 1));
            configOutput(CV3_OUTPUT + i, string::f("Sequencer %d CV 3", i + 1));
        }

        // Setup expander message buffers
        leftExpander.producerMessage = &leftMessages[0];
        leftExpander.consumerMessage = &leftMessages[1];
    }

    bool isValidExpander(Module* m) {
        return m && (m->model == modelCore || m->model == modelKnobExpander ||
                     m->model == modelGateExpander || m->model == modelSeqExpander ||
                     m->model == modelCVExpander || m->model == modelInfoDisplay ||
                     m->model == modelStepDisplay);
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

                    // Output CV values (0-127 mapped to 0-10V)
                    outputs[CV1_OUTPUT + s].setVoltage(seq.cv1 / 127.f * 10.f);
                    outputs[CV2_OUTPUT + s].setVoltage(seq.cv2 / 127.f * 10.f);
                    outputs[CV3_OUTPUT + s].setVoltage(seq.cv3 / 127.f * 10.f);
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
                outputs[CV1_OUTPUT + s].setVoltage(0.f);
                outputs[CV2_OUTPUT + s].setVoltage(0.f);
                outputs[CV3_OUTPUT + s].setVoltage(0.f);
            }
        }

        lights[CONNECTED_LIGHT].setBrightness(connected ? 1.f : 0.f);
    }
};

// Simple label widget for panel text
struct CVPanelLabel : widget::Widget {
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

struct CVExpanderWidget : ModuleWidget {
    CVExpanderWidget(CVExpander* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/CVExpander.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Connected light
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(5, 10)), module, CVExpander::CONNECTED_LIGHT));

        float y = 22.f;

        // CV1 outputs (8 jacks) - Column 1
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.5, y + i * 10)), module, CVExpander::CV1_OUTPUT + i));
        }

        // CV2 outputs (8 jacks) - Column 2
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.5, y + i * 10)), module, CVExpander::CV2_OUTPUT + i));
        }

        // CV3 outputs (8 jacks) - Column 3
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(27.5, y + i * 10)), module, CVExpander::CV3_OUTPUT + i));
        }

        // Column labels
        auto addLabel = [this](Vec pos, Vec size, std::string text, float fontSize = 7.f) {
            CVPanelLabel* label = new CVPanelLabel();
            label->box.pos = pos;
            label->box.size = size;
            label->text = text;
            label->fontSize = fontSize;
            addChild(label);
        };

        addLabel(mm2px(Vec(0, 14)), mm2px(Vec(15, 4)), "CV 1");
        addLabel(mm2px(Vec(10, 14)), mm2px(Vec(15, 4)), "CV 2");
        addLabel(mm2px(Vec(20, 14)), mm2px(Vec(15, 4)), "CV 3");

        // Row numbers (on the right side)
        for (int i = 0; i < 8; i++) {
            addLabel(mm2px(Vec(31, 20 + i * 10)), mm2px(Vec(5, 4)), std::to_string(i + 1), 6.f);
        }

        // Module name at bottom
        addLabel(mm2px(Vec(10, 110)), mm2px(Vec(15, 8)), "CVE", 14.f);
        // Brand below line
        addLabel(mm2px(Vec(10, 120)), mm2px(Vec(15, 8)), "LCXL", 14.f);
    }
};

Model* modelCVExpander = createModel<CVExpander, CVExpanderWidget>("CVExpander");
