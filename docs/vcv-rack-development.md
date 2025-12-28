# VCV Rack Module Development Guide

## Overview

VCV Rack is a virtual Eurorack modular synthesizer. Modules are developed in C++ using the Rack SDK.

## Prerequisites

- Familiarity with C++ (C++11 or later)
- Command-line comfort
- Basic knowledge of modular synthesis concepts
- DSP knowledge only required for sound generators/processors

## Development Setup

### Option 1: Rack SDK (Recommended)

1. Download VCV Rack from https://vcvrack.com/
2. Download the Rack SDK for your platform:
   - Windows x64
   - Mac x64 / Mac ARM64
   - Linux x64
3. Set the `RACK_DIR` environment variable to point to the SDK location
4. Build plugins from any folder

### Option 2: Build from Source

- Clone the Rack repository and build plugins in the `plugins/` folder
- Recommended for advanced developers

### Option 3: Plugin Toolchain

- Build for all architectures with one command
- Available as Native (Linux) or Docker (Linux, Mac, Windows)
- Requires ~15 GB disk space, 8 GB RAM

## Project Structure

Use the `helper.py` script to generate a plugin template:

```bash
# Create new plugin
./helper.py createplugin MyPlugin

# Create new module
./helper.py createmodule MyModule res/MyModule.svg
```

This generates:
- `plugin.json` - Plugin manifest with metadata
- `src/plugin.cpp` - Plugin initialization
- `src/plugin.hpp` - Plugin header with extern declarations
- `src/MyModule.cpp` - Module implementation
- `res/MyModule.svg` - Panel artwork

## Module Architecture

### Core Components

```cpp
struct MyModule : Module {
    enum ParamId {
        PITCH_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        CV_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        AUDIO_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ACTIVE_LIGHT,
        LIGHTS_LEN
    };

    MyModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(PITCH_PARAM, 0.f, 1.f, 0.5f, "Pitch");
        configInput(CV_INPUT, "CV");
        configOutput(AUDIO_OUTPUT, "Audio");
    }

    void process(const ProcessArgs& args) override {
        // Called at sample rate (e.g., 44100 Hz)
        float pitch = params[PITCH_PARAM].getValue();
        float cv = inputs[CV_INPUT].getVoltage();
        outputs[AUDIO_OUTPUT].setVoltage(/* computed value */);
        lights[ACTIVE_LIGHT].setBrightness(1.0f);
    }
};
```

### Reading/Writing Values

| Component | Read | Write |
|-----------|------|-------|
| Params | `params[ID].getValue()` | N/A (user controlled) |
| Inputs | `inputs[ID].getVoltage()` | N/A (from cables) |
| Outputs | N/A | `outputs[ID].setVoltage(v)` |
| Lights | N/A | `lights[ID].setBrightness(b)` |

## Voltage Standards

VCV Rack follows Eurorack conventions:

| Signal Type | Voltage Range |
|-------------|---------------|
| Audio | -5V to +5V (10V peak-to-peak) |
| CV (1V/oct) | 0V = C4, +1V per octave |
| Gates/Triggers | 0V (off) to +10V (on) |
| Modulation | 0V to +10V (unipolar) or -5V to +5V (bipolar) |

## MIDI in VCV Rack

### Core MIDI Modules

- **MIDI-CV**: Converts MIDI notes to V/OCT, Gate, Velocity
- **MIDI-CC**: Maps MIDI CC messages (0-127) to CV (0V-10V)
- **MIDI-Gate**: Outputs gates for specific MIDI notes
- **MIDI-Map**: Maps hardware CC to Rack parameters

### MIDI-CC Behavior

- CC values 0-127 map to 0V-10V
- Click digital display to enter "learn" mode
- Gamepad mode: -128 to 127 maps to -10V to +10V

### Developing MIDI Modules

Key classes:
- `dsp::MidiParser` - Parse MIDI messages
- `midi::InputQueue` - Handle incoming MIDI
- `midi::Output` - Send MIDI out

Example open-source MIDI plugins:
- [Kilpatrick-Toolbox](https://github.com/kilpatrickaudio/Kilpatrick-Toolbox)
- [stoermelder PackOne](https://github.com/stoermelder/vcvrack-packone)
- [StudioSixPlusOne](https://github.com/StudioSixPlusOne/rack-modules)

## Panel Design

### Dimensions

- **Height**: 128.5 mm (fixed, Eurorack standard)
- **Width**: Multiples of 5.08 mm (1 HP)
- **Format**: SVG (Inkscape recommended)
- **Units**: Millimeters (not pixels)

### Component Layer Colors

Use a "components" layer with colored circles to mark positions:

| Color | Hex | Component Type |
|-------|-----|----------------|
| Red | #FF0000 | Parameters (knobs, switches) |
| Green | #00FF00 | Inputs |
| Blue | #0000FF | Outputs |
| Magenta | #FF00FF | Lights |
| Yellow | #FFFF00 | Custom widgets |

### Best Practices

- Leave space between knobs/ports for fingers
- Keep text readable at 100% zoom
- Use inverted backgrounds for output ports
- Convert all text to paths (Path > Object to Path)
- Avoid complex gradients (simple 2-color linear OK)

## Building & Testing

```bash
# Build plugin
make

# Install to VCV Rack user folder
make install

# Create distributable package
make dist
```

Check `log.txt` in VCV Rack's user folder for load errors.

## Publishing

1. Include a `LICENSE.txt` file
2. Review `plugin.json` for accuracy
3. Submit to the [VCV Library](https://library.vcvrack.com/)

## Expander Modules

Modules placed adjacent to each other can communicate via the expander system. This allows a main module to be extended with additional I/O or features.

### Basic Structure

```cpp
// Message struct shared between main and expander
struct MyExpanderMessage {
    float values[8];
    bool gates[16];
};

struct MyModule : Module {
    MyExpanderMessage leftMessages[2][1];
    MyExpanderMessage rightMessages[2][1];

    MyModule() {
        // Set up expander message buffers
        leftExpander.producerMessage = leftMessages[0];
        leftExpander.consumerMessage = leftMessages[1];
        rightExpander.producerMessage = rightMessages[0];
        rightExpander.consumerMessage = rightMessages[1];
    }

    void process(const ProcessArgs& args) override {
        // Check if right expander exists and is correct type
        if (rightExpander.module &&
            rightExpander.module->model == modelMyExpander) {

            // Read from consumer (data FROM expander)
            auto* msg = static_cast<MyExpanderMessage*>(
                rightExpander.consumerMessage);
            float val = msg->values[0];

            // Write to expander's producer (data TO expander)
            auto* toExp = static_cast<MyExpanderMessage*>(
                rightExpander.module->leftExpander.producerMessage);
            toExp->values[0] = someValue;
            rightExpander.module->leftExpander.messageFlipRequested = true;
        }
    }
};
```

### Key Points

- **Detection**: Check `rightExpander.module` and verify `->model`
- **Latency**: 1 sample frame delay for message passing
- **Double-buffer**: Producer (write-only) and Consumer (read-only)
- **Flip request**: Set `messageFlipRequested = true` after writing
- **Cleanup**: Free message buffers in destructor

### Expander Patterns

1. **Output expander**: Main module sends data, expander has extra outputs
2. **Input expander**: Expander has extra inputs, sends to main
3. **Bidirectional**: Both modules read and write

## Resources

- [VCV Rack Manual](https://vcvrack.com/manual/)
- [Plugin Development Tutorial](https://vcvrack.com/manual/PluginDevelopmentTutorial)
- [Module Panel Guide](https://vcvrack.com/manual/Panel)
- [VCV Community Forum](https://community.vcvrack.com/)
- [Rack API Documentation](https://vcvrack.com/docs-v2/)
- [Expander API Reference](https://vcvrack.com/docs-v2/structrack_1_1engine_1_1Module_1_1Expander)
- [Expander Examples Thread](https://community.vcvrack.com/t/module-expanders-sharing-a-minimal-example/23101)
