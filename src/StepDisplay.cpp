#include "plugin.hpp"
#include "ExpanderMessage.hpp"

struct StepDisplay : Module {
    enum ParamId {
        PARAMS_LEN
    };
    enum InputId {
        INPUTS_LEN
    };
    enum OutputId {
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(STEP_LIGHTS, 8 * 16 * 2),  // 8 sequencers x 16 steps x 2 (green/red)
        CONNECTED_LIGHT,
        LIGHTS_LEN
    };

    LCXLExpanderMessage leftMessages[2];
    LCXLExpanderMessage expanderMessage;  // For forwarding to right

    StepDisplay() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Setup expander message buffers
        leftExpander.producerMessage = &leftMessages[0];
        leftExpander.consumerMessage = &leftMessages[1];
    }

    bool isValidExpander(Module* m) {
        return m && (m->model == modelCore || m->model == modelKnobExpander ||
                     m->model == modelGateExpander || m->model == modelSeqExpander ||
                     m->model == modelStepDisplay || m->model == modelCVExpander ||
                     m->model == modelInfoDisplay);
    }

    void process(const ProcessArgs& args) override {
        bool connected = false;
        LCXLExpanderMessage* msg = nullptr;

        // Check if connected to Core or another expander on the left
        if (isValidExpander(leftExpander.module)) {
            msg = reinterpret_cast<LCXLExpanderMessage*>(leftExpander.consumerMessage);
            if (msg && msg->moduleId >= 0) {
                connected = true;

                // Update LEDs for all 8 sequencers
                for (int s = 0; s < 8; s++) {
                    auto& seq = msg->sequencers[s];

                    for (int step = 0; step < 16; step++) {
                        int lightIndex = (s * 16 + step) * 2;

                        bool isActive = seq.steps[step];
                        bool isPlayhead = false;
                        bool inRange = false;

                        if (seq.isStepSingleMode) {
                            // Single mode: all 16 steps for sequence A
                            isPlayhead = (step == seq.currentStepA);
                            inRange = (step < seq.stepLengthA);
                        } else {
                            // Dual mode: top 8 for A, bottom 8 for B
                            if (step < 8) {
                                isPlayhead = (step == seq.currentStepA);
                                inRange = (step < seq.stepLengthA);
                            } else {
                                int localStep = step - 8;
                                isPlayhead = (localStep == seq.currentStepB);
                                inRange = (seq.stepLengthB > 0 && localStep < seq.stepLengthB);
                            }
                        }

                        // Set LED colors
                        float green = 0.f, red = 0.f;
                        if (!inRange) {
                            // Out of range - off
                            green = 0.f;
                            red = 0.f;
                        } else if (isPlayhead && isActive) {
                            // Playhead on active step - bright green
                            green = 1.f;
                            red = 0.f;
                        } else if (isPlayhead) {
                            // Playhead on inactive step - dim red
                            green = 0.f;
                            red = 0.3f;
                        } else if (isActive) {
                            // Active step - dim green
                            green = 0.3f;
                            red = 0.f;
                        } else {
                            // Inactive step - off
                            green = 0.f;
                            red = 0.f;
                        }

                        lights[STEP_LIGHTS + lightIndex].setBrightness(green);
                        lights[STEP_LIGHTS + lightIndex + 1].setBrightness(red);
                    }
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

        // If not connected, turn off all LEDs
        if (!connected) {
            for (int i = 0; i < 8 * 16 * 2; i++) {
                lights[STEP_LIGHTS + i].setBrightness(0.f);
            }
        }

        lights[CONNECTED_LIGHT].setBrightness(connected ? 1.f : 0.f);
    }
};

// Simple label widget for panel text
struct StepPanelLabel : widget::Widget {
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

struct StepDisplayWidget : ModuleWidget {
    StepDisplayWidget(StepDisplay* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/StepDisplay.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Connected light
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(5, 10)), module, StepDisplay::CONNECTED_LIGHT));

        // LED grid: 8 rows (sequencers) x 16 columns (steps)
        float startX = 6.f;   // Starting X position in mm
        float startY = 18.f;  // Starting Y position in mm
        float spacingX = 4.f; // Horizontal spacing between LEDs
        float spacingY = 12.f; // Vertical spacing between rows

        for (int seq = 0; seq < 8; seq++) {
            for (int step = 0; step < 16; step++) {
                int lightIndex = (seq * 16 + step) * 2;
                float x = startX + step * spacingX;
                float y = startY + seq * spacingY;

                addChild(createLightCentered<TinyLight<GreenRedLight>>(
                    mm2px(Vec(x, y)), module, StepDisplay::STEP_LIGHTS + lightIndex));
            }
        }

        // Module name at bottom
        StepPanelLabel* label = new StepPanelLabel();
        label->box.pos = mm2px(Vec(30, 110));
        label->box.size = mm2px(Vec(15, 8));
        label->text = "STP";
        label->fontSize = 14.f;
        addChild(label);

        // Brand below line
        StepPanelLabel* brand = new StepPanelLabel();
        brand->box.pos = mm2px(Vec(30, 120));
        brand->box.size = mm2px(Vec(15, 8));
        brand->text = "LCXL";
        brand->fontSize = 14.f;
        addChild(brand);
    }
};

Model* modelStepDisplay = createModel<StepDisplay, StepDisplayWidget>("StepDisplay");
