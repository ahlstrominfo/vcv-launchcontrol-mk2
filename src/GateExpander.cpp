#include "plugin.hpp"
#include "ExpanderMessage.hpp"

struct GateExpander : Module {
    enum ParamId {
        PARAMS_LEN
    };
    enum InputId {
        INPUTS_LEN
    };
    enum OutputId {
        ENUMS(GATE_OUTPUT, 16),
        OUTPUTS_LEN
    };
    enum LightId {
        CONNECTED_LIGHT,
        LIGHTS_LEN
    };

    LCXLExpanderMessage leftMessages[2];

    GateExpander() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Configure gate outputs
        for (int i = 0; i < 8; i++) {
            configOutput(GATE_OUTPUT + i, string::f("Gate %d (Focus)", i + 1));
        }
        for (int i = 0; i < 8; i++) {
            configOutput(GATE_OUTPUT + 8 + i, string::f("Gate %d (Control)", i + 1));
        }

        // Setup expander message buffers
        leftExpander.producerMessage = &leftMessages[0];
        leftExpander.consumerMessage = &leftMessages[1];
    }

    void process(const ProcessArgs& args) override {
        bool connected = false;

        // Check if connected to Core module (or another expander) on the left
        if (leftExpander.module && leftExpander.module->model == modelCore) {
            LCXLExpanderMessage* msg = reinterpret_cast<LCXLExpanderMessage*>(leftExpander.consumerMessage);
            if (msg && msg->moduleId >= 0) {
                connected = true;

                // Output button states as gates (10V when on, 0V when off)
                for (int i = 0; i < 16; i++) {
                    outputs[GATE_OUTPUT + i].setVoltage(msg->buttonStates[i] ? 10.f : 0.f);
                }
            }
        }

        // If not connected, output zeros
        if (!connected) {
            for (int i = 0; i < 16; i++) {
                outputs[GATE_OUTPUT + i].setVoltage(0.f);
            }
        }

        lights[CONNECTED_LIGHT].setBrightness(connected ? 1.f : 0.f);
    }
};

struct GateExpanderWidget : ModuleWidget {
    GateExpanderWidget(GateExpander* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/GateExpander.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Connected light
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(5, 10)), module, GateExpander::CONNECTED_LIGHT));

        // Focus row outputs (gates 1-8)
        float y = 20.f;
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.5, y + i * 10)), module, GateExpander::GATE_OUTPUT + i));
        }

        // Control row outputs (gates 9-16)
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.5, y + i * 10)), module, GateExpander::GATE_OUTPUT + 8 + i));
        }
    }
};

Model* modelGateExpander = createModel<GateExpander, GateExpanderWidget>("GateExpander");
