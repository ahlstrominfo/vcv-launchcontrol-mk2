# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Deploy

```bash
make                    # Build plugin (uses RACK_DIR or ./Rack-SDK)
```

**Install to VCV Rack:**
- macOS: `~/Documents/Rack2/plugins/LaunchControlXL/`
- Windows: `~/Documents/Rack2/plugins/LaunchControlXL/`
- Linux: `~/.Rack2/plugins/LaunchControlXL/`

Copy `plugin.dylib` (or `.dll`/`.so`), `plugin.json`, and `res/*.svg` to the plugin folder. Restart VCV Rack to load.

## Architecture

### Module System
This is a VCV Rack plugin for Novation Launch Control XL MK2. Uses a **Core + Expanders** pattern:

- **Core** (`Core.cpp`) - Central hub: MIDI I/O, sequencer logic, state management
- **Expanders** - Satellite modules that receive state via VCV's expander message system

**Expander chain:** `[ClockExpander] ← [Core] → [InfoDisplay/SeqExpander/KnobExpander/GateExpander/CVExpander/StepDisplay]`

ClockExpander must be LEFT of Core. All others go RIGHT in any order.

### Communication
- `ExpanderMessage.hpp` defines `LCXLExpanderMessage` (Core → right expanders) and `ClockExpanderMessage` (ClockExpander → Core)
- Messages are double-buffered and updated each `process()` call
- Expanders check `leftExpander.module` chain to find valid message source

### Sequencer System

**9 Layouts:** Layout 0 = default (knobs/buttons as CV/gates), Layouts 1-8 = sequencers

**Dual Mode vs Single Mode** (per sequencer):
- `valueLengthA < 9` → Dual mode: Two independent sequences (A uses row 1, B uses row 2)
- `valueLengthA >= 9` → Single mode: One sequence using all 16 values/steps

**Row 3 Parameter Knobs (indices 16-23):**
| Knob | Param | Description |
|------|-------|-------------|
| 1 (idx 16) | valueLengthA | 1-16, ≥9 triggers single mode |
| 2 (idx 17) | valueLengthB | 0-8 (row 2 only, dual mode) |
| 3 (idx 18) | stepLengthA | 1-16 |
| 4 (idx 19) | stepLengthB | 0-8 (dual mode) |
| 5-8 | bias, cv1-3 | Competition/routing, per-seq CV |

**Competition Modes (dual):** Independent, Steal, A/B Priority, Momentum, Revenge, Echo, Value Theft
**Routing Modes (single):** All A, All B, Bernoulli, Alternate, Two-Two, Burst, Probability, Pattern

### MIDI / SysEx

Uses Novation Launch Control XL MK2 Factory Template 1 (MIDI channel 9, index 8).

**LED Control via SysEx:**
```cpp
// Header: 0x00, 0x20, 0x29, 0x02, 0x11
LED_OFF = 12
LED_RED_FULL = 15
LED_GREEN_FULL = 60
LED_YELLOW_FULL = 62
LED_AMBER_FULL = 63
```

**Soft Takeover:** Knobs use pickup mode with ±2 tolerance. LEDs show direction: green=synced, yellow=turn right, red=turn left.

### Key Data Structures

**Core.cpp:**
- `Sequencer` struct - 16 steps, lengths, bias, CV values, voltage settings, playback state, glide
- `knobValues[9][24]` - All knob values per layout
- `buttonStates[16]`, `buttonMomentary[16]` - Default layout button states

**ExpanderMessage.hpp:**
- `LCXLExpanderMessage::SequencerData` - Per-sequencer state passed to expanders
- `LastChangeInfo` - Tracks parameter changes for InfoDisplay

## Files

| File | Purpose |
|------|---------|
| `Core.cpp` | Main module, MIDI, sequencer logic (~1800 lines) |
| `SeqExpander.cpp` | Outputs 8x TRG A/B + CV A/B |
| `KnobExpander.cpp` | Outputs 24 knob CVs |
| `GateExpander.cpp` | Outputs 16 button gates |
| `CVExpander.cpp` | Outputs per-sequencer CV 1-3 |
| `ClockExpander.cpp` | Per-sequencer clock inputs |
| `StepDisplay.cpp` | Visual LED grid |
| `InfoDisplay.cpp` | Shows last parameter change |
| `ExpanderMessage.hpp` | Shared message structs |
| `plugin.hpp/cpp` | Module registration |
