# VCV Rack LaunchControl XL Plugin

## Build & Deploy
```bash
make                    # Build plugin
# Copy to your VCV Rack plugins folder (see README.md for platform-specific paths)
```
Restart VCV Rack after deploying to load new plugin.

## Project Structure
- `src/Core.cpp` - Main module (LaunchControl Core)
- `src/KnobExpander.cpp` - Outputs 24 knob CV values
- `src/GateExpander.cpp` - Outputs 16 button gate states
- `src/SeqExpander.cpp` - Outputs triggers/CV from 8 sequencers
- `plans/module-concept.md` - Design documentation

## Sequencer System (Dual Mode)
Layout 1-8 are sequencer layouts. Each has a `Sequencer` struct with:

**Row 3 Parameter Knobs (indices 16-23):**
- Knob 1 (idx 16, param 0): `valueLengthA` (1-16), >=9 = single mode
- Knob 2 (idx 17, param 1): `valueLengthB` (0-8), row 2 only
- Knob 3 (idx 18, param 2): `stepLengthA` (1-16)
- Knob 4 (idx 19, param 3): `stepLengthB` (0-8)
- Knobs 5-8: probA, probB, bias, competition/routing mode

**LED Feedback:**
- Row 1 knobs (0-7): Seq A values, soft takeover colors before amber, OFF after
- Row 2 knobs (8-15): Seq B values, same pattern
- Amber = length boundary marker

**Modes:**
- Single mode (valueLengthA >= 9): One sequencer uses all 16 values/steps
- Dual mode (valueLengthA < 9): Two sequencers, A uses row 1, B uses row 2

## LED Color Constants
```cpp
LED_OFF = 12
LED_RED_FULL = 15
LED_GREEN_FULL = 60
LED_YELLOW_FULL = 62
LED_AMBER_FULL = 63
```

## MIDI SysEx
Uses Novation Launch Control XL MK2 SysEx for LED control.
Template 8 is used for the plugin.
