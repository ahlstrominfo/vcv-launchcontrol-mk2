#include "plugin.hpp"
#include "ExpanderMessage.hpp"

struct SeqExpander : Module {
    enum ParamId {
        PARAMS_LEN
    };
    enum InputId {
        INPUTS_LEN
    };
    enum OutputId {
        ENUMS(TRIG_OUTPUT, 8),
        ENUMS(CV_OUTPUT, 8),
        OUTPUTS_LEN
    };
    enum LightId {
        CONNECTED_LIGHT,
        LIGHTS_LEN
    };

    LCXLExpanderMessage leftMessages[2];
    dsp::PulseGenerator triggerPulses[8];

    SeqExpander() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Configure trigger outputs
        for (int i = 0; i < 8; i++) {
            configOutput(TRIG_OUTPUT + i, string::f("Sequencer %d Trigger", i + 1));
        }

        // Configure CV outputs
        for (int i = 0; i < 8; i++) {
            configOutput(CV_OUTPUT + i, string::f("Sequencer %d CV", i + 1));
        }

        // Setup expander message buffers
        leftExpander.producerMessage = &leftMessages[0];
        leftExpander.consumerMessage = &leftMessages[1];
    }

    void process(const ProcessArgs& args) override {
        bool connected = false;

        // Check if connected to Core module on the left
        if (leftExpander.module && leftExpander.module->model == modelCore) {
            LCXLExpanderMessage* msg = reinterpret_cast<LCXLExpanderMessage*>(leftExpander.consumerMessage);
            if (msg && msg->moduleId >= 0) {
                connected = true;

                for (int s = 0; s < 8; s++) {
                    auto& seq = msg->sequencers[s];

                    // Fire trigger if sequencer triggered this frame
                    if (seq.triggered) {
                        triggerPulses[s].trigger(1e-3f);  // 1ms pulse
                    }

                    // Output trigger
                    outputs[TRIG_OUTPUT + s].setVoltage(triggerPulses[s].process(args.sampleTime) ? 10.f : 0.f);

                    // Output CV from value knob (0-5V at 1V/oct)
                    int knobIndex = seq.currentValueIndex;
                    int layout = s + 1;  // Sequencers use layouts 1-8
                    float cv = msg->knobValues[layout][knobIndex] / 127.f * 5.f;
                    outputs[CV_OUTPUT + s].setVoltage(cv);
                }
            }
        }

        // If not connected, output zeros
        if (!connected) {
            for (int s = 0; s < 8; s++) {
                outputs[TRIG_OUTPUT + s].setVoltage(0.f);
                outputs[CV_OUTPUT + s].setVoltage(0.f);
            }
        }

        lights[CONNECTED_LIGHT].setBrightness(connected ? 1.f : 0.f);
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

        // Trigger outputs (8 jacks)
        float y = 20.f;
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.5, y + i * 10)), module, SeqExpander::TRIG_OUTPUT + i));
        }

        // CV outputs (8 jacks)
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.5, y + i * 10)), module, SeqExpander::CV_OUTPUT + i));
        }
    }
};

Model* modelSeqExpander = createModel<SeqExpander, SeqExpanderWidget>("SeqExpander");
