#include "plugin.hpp"
#include "ExpanderMessage.hpp"

struct InfoDisplay : Module {
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
        CONNECTED_LIGHT,
        LIGHTS_LEN
    };

    LCXLExpanderMessage leftMessages[2];
    LCXLExpanderMessage expanderMessage;  // For forwarding to right

    // Display text
    std::string line1 = "";
    std::string line2 = "";
    std::string line3 = "";

    InfoDisplay() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Setup expander message buffers
        leftExpander.producerMessage = &leftMessages[0];
        leftExpander.consumerMessage = &leftMessages[1];
    }

    bool isValidExpander(Module* m) {
        return m && (m->model == modelCore || m->model == modelKnobExpander ||
                     m->model == modelGateExpander || m->model == modelSeqExpander ||
                     m->model == modelStepDisplay || m->model == modelInfoDisplay);
    }

    std::string getChangeTypeName(ChangeType type) {
        switch (type) {
            case CHANGE_LAYOUT: return "Layout";
            case CHANGE_VALUE_LENGTH_A: return "Val Len A";
            case CHANGE_VALUE_LENGTH_B: return "Val Len B";
            case CHANGE_STEP_LENGTH_A: return "Step Len A";
            case CHANGE_STEP_LENGTH_B: return "Step Len B";
            case CHANGE_PROB_A: return "Prob A";
            case CHANGE_PROB_B: return "Prob B";
            case CHANGE_BIAS: return "Bias";
            case CHANGE_VOLTAGE_A: return "Voltage A";
            case CHANGE_VOLTAGE_B: return "Voltage B";
            case CHANGE_BIPOLAR_A: return "Bipolar A";
            case CHANGE_BIPOLAR_B: return "Bipolar B";
            case CHANGE_COMP_MODE: return "Comp Mode";
            case CHANGE_ROUTE_MODE: return "Route Mode";
            case CHANGE_STEP_TOGGLE: return "Step";
            case CHANGE_UTILITY: return "Utility";
            default: return "";
        }
    }

    std::string getValueString(ChangeType type, int value, int step) {
        switch (type) {
            case CHANGE_LAYOUT:
                return value == 0 ? "Default" : "Seq " + std::to_string(value);
            case CHANGE_VALUE_LENGTH_A:
            case CHANGE_VALUE_LENGTH_B:
            case CHANGE_STEP_LENGTH_A:
            case CHANGE_STEP_LENGTH_B:
                return std::to_string(value);
            case CHANGE_PROB_A:
            case CHANGE_PROB_B:
            case CHANGE_BIAS:
                return std::to_string(value) + "%";
            case CHANGE_VOLTAGE_A:
            case CHANGE_VOLTAGE_B:
                switch (value) {
                    case 0: return "5V";
                    case 1: return "10V";
                    case 2: return "1V";
                    default: return "?";
                }
            case CHANGE_BIPOLAR_A:
            case CHANGE_BIPOLAR_B:
                return value ? "On" : "Off";
            case CHANGE_COMP_MODE:
                switch (value) {
                    case 0: return "Independent";
                    case 1: return "Steal";
                    case 2: return "A Priority";
                    case 3: return "B Priority";
                    default: return "?";
                }
            case CHANGE_ROUTE_MODE:
                switch (value) {
                    case 0: return "All A";
                    case 1: return "All B";
                    case 2: return "Bernoulli";
                    case 3: return "Alternate";
                    default: return "?";
                }
            case CHANGE_STEP_TOGGLE:
                return "Step " + std::to_string(step + 1) + " " + (value ? "On" : "Off");
            default:
                return std::to_string(value);
        }
    }

    void process(const ProcessArgs& args) override {
        bool connected = false;
        LCXLExpanderMessage* msg = nullptr;

        // Check if connected to Core or another expander on the left
        if (isValidExpander(leftExpander.module)) {
            msg = reinterpret_cast<LCXLExpanderMessage*>(leftExpander.consumerMessage);
            if (msg && msg->moduleId >= 0) {
                connected = true;

                // Update display text based on last change
                auto& change = msg->lastChange;
                if (change.type != CHANGE_NONE) {
                    // Line 1: Sequencer/Layout info
                    if (change.sequencer == 0) {
                        line1 = "Default";
                    } else {
                        line1 = "Seq " + std::to_string(change.sequencer);
                    }

                    // Line 2: Parameter name
                    line2 = getChangeTypeName(change.type);

                    // Line 3: Value
                    line3 = getValueString(change.type, change.value, change.step);
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

        // If not connected, clear display
        if (!connected) {
            line1 = "";
            line2 = "";
            line3 = "";
        }

        lights[CONNECTED_LIGHT].setBrightness(connected ? 1.f : 0.f);
    }
};

// Custom display widget
struct InfoDisplayWidget : widget::Widget {
    InfoDisplay* module = nullptr;
    std::string fontPath;

    InfoDisplayWidget() {
        fontPath = asset::system("res/fonts/ShareTechMono-Regular.ttf");
    }

    void draw(const DrawArgs& args) override {
        // Background
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3.0);
        nvgFillColor(args.vg, nvgRGB(0x10, 0x10, 0x10));
        nvgFill(args.vg);

        // Border
        nvgStrokeColor(args.vg, nvgRGB(0x40, 0x40, 0x40));
        nvgStrokeWidth(args.vg, 1.0);
        nvgStroke(args.vg);

        std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
        if (!font) return;

        nvgFontFaceId(args.vg, font->handle);

        std::string text1 = module ? module->line1 : "---";
        std::string text2 = module ? module->line2 : "---";
        std::string text3 = module ? module->line3 : "---";

        // Line 1 - Sequencer (larger)
        nvgFontSize(args.vg, 14);
        nvgFillColor(args.vg, nvgRGB(0x00, 0xff, 0x00));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgText(args.vg, box.size.x / 2, 4, text1.c_str(), NULL);

        // Line 2 - Parameter name
        nvgFontSize(args.vg, 11);
        nvgFillColor(args.vg, nvgRGB(0xff, 0xcc, 0x00));
        nvgText(args.vg, box.size.x / 2, 22, text2.c_str(), NULL);

        // Line 3 - Value
        nvgFontSize(args.vg, 14);
        nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0xff));
        nvgText(args.vg, box.size.x / 2, 38, text3.c_str(), NULL);
    }
};

struct InfoDisplayModuleWidget : ModuleWidget {
    InfoDisplayModuleWidget(InfoDisplay* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/InfoDisplay.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Connected light
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(5, 10)), module, InfoDisplay::CONNECTED_LIGHT));

        // Info display widget
        InfoDisplayWidget* display = new InfoDisplayWidget();
        display->box.pos = mm2px(Vec(3, 18));
        display->box.size = mm2px(Vec(24, 20));
        display->module = module;
        addChild(display);
    }
};

Model* modelInfoDisplay = createModel<InfoDisplay, InfoDisplayModuleWidget>("InfoDisplay");
