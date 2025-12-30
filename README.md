# LaunchControl XL for VCV Rack

A VCV Rack plugin for the **Novation Launch Control XL MK2** MIDI controller, featuring 8 independent dual sequencers with advanced competition and routing modes.

## Modules Overview

| Module | Color | Description |
|--------|-------|-------------|
| **Core** | Red | Main module - MIDI communication, clocks, faders |
| **ClockExpander** | Orange | Per-sequencer clock inputs (LEFT of Core) |
| **SeqExpander** | Purple | All 8 sequencers' triggers and CV outputs |
| **KnobExpander** | Blue | All 24 knob CV outputs |
| **GateExpander** | Green | 16 button gate outputs |
| **CVExpander** | Yellow | Per-sequencer CV outputs (3 per sequencer) |
| **StepDisplay** | Turquoise | Visual LED grid of all sequencer steps |
| **InfoDisplay** | Pink | Shows last parameter change |

## Module Details

### Core (Red)
The main module that communicates with your Launch Control XL.

**Inputs:**
- **CLK A** - Clock input for Sequence A
- **CLK B** - Clock input for Sequence B (normaled to CLK A)
- **RST** - Reset all sequencers to step 0

**Outputs:**
- **TRG A/B** - Trigger outputs for the selected sequencer
- **CV A/B** - CV outputs for the selected sequencer
- **Faders 1-8** - Direct CV outputs from the 8 faders (0-10V)

### ClockExpander (Orange) - LEFT of Core
Provides individual clock inputs for each of the 8 sequencers.

- **CLK A 1-8** - Clock A inputs with chaining (patching input 3 feeds sequencers 3-8)
- **CLK B 1-8** - Clock B inputs (normaled to own CLK A only, not chained)

### SeqExpander (Purple)
Outputs triggers and CV from ALL 8 sequencers simultaneously.

- **TRG A 1-8** - Trigger A outputs for each sequencer
- **TRG B 1-8** - Trigger B outputs for each sequencer
- **CV A 1-8** - CV A outputs for each sequencer
- **CV B 1-8** - CV B outputs for each sequencer

### KnobExpander (Blue)
Outputs CV from all 24 knobs of the current layout (0-10V).

- **ROW 1** - Knobs 1-8 (top row)
- **ROW 2** - Knobs 9-16 (middle row)
- **ROW 3** - Knobs 17-24 (bottom row)

### GateExpander (Green)
Outputs gate signals from the 16 buttons in default mode.

- **FOCUS 1-8** - Track Focus button states (10V when on)
- **CTRL 1-8** - Track Control button states (10V when on)

### CVExpander (Yellow)
Outputs 3 per-sequencer CV values (from knobs 6-8 in sequencer mode).

- **CV1 1-8** - CV 1 for each sequencer (0-10V)
- **CV2 1-8** - CV 2 for each sequencer (0-10V)
- **CV3 1-8** - CV 3 for each sequencer (0-10V)

### StepDisplay (Turquoise)
Visual display showing the step states of all 8 sequencers as an LED grid.

- 8 rows (sequencers) × 16 columns (steps)
- Green = active step, Red = playhead position

### InfoDisplay (Pink)
Shows the most recent parameter change for easy feedback.

- Displays: Sequencer number, parameter name, and value

## Getting Started

1. Add the **Core** module to your patch
2. Right-click and select your Launch Control XL for both MIDI Input and Output
3. Press the **TAKE** button (or it auto-initializes when MIDI connects)
4. The controller LEDs should now reflect the module state

## Layouts

### Default Layout (Layout 0)
- **Knobs**: 24 CV outputs via KnobExpander
- **Faders**: 8 CV outputs directly from Core
- **Buttons**: 16 toggle/momentary gates via GateExpander

**Button Modes:**
- Hold **Record Arm** + press a button to toggle between Toggle and Momentary mode
- **Toggle mode** (default): Press to turn on, press again to turn off
- **Momentary mode**: Gate is only on while button is held, shown with dim green LED

### Sequencer Layouts (1-8)
Hold **Device** + press **Track Control 1-8** to enter a sequencer.

Each sequencer has:
- **Top 8 buttons**: Steps for Sequence A
- **Bottom 8 buttons**: Steps for Sequence B
- **Knob rows 1-2**: Value knobs (CV values for each step)
- **Knob row 3**: Parameters (see below)

Hold **Device** + **Track Focus 1** to return to default layout.

## Sequencer Parameters (Bottom Knob Row)

| Knob | Parameter | Range | Description |
|------|-----------|-------|-------------|
| 1 | Value Length A | 1-16 | Active value knobs for Seq A. Set to 9+ for single mode |
| 2 | Value Length B | 0-8 | Active value knobs for Seq B. Set to 0 to disable |
| 3 | Step Length A | 1-16 | Active steps for Seq A. Set to 9+ for single mode |
| 4 | Step Length B | 0-8 | Active steps for Seq B. Set to 0 to disable |
| 5 | Bias | 0-100% | Controls competition/routing behavior |
| 6 | CV 1 | 0-127 | Per-sequencer CV value (via CVExpander) |
| 7 | CV 2 | 0-127 | Per-sequencer CV value (via CVExpander) |
| 8 | CV 3 | 0-127 | Per-sequencer CV value (via CVExpander) |

## Dual vs Single Mode

### Dual Mode (Default)
Two independent sequences running from separate clocks.

**Setup:** Value Length A < 9

**Signal Flow:**
```
CLK A ──► Seq A steps (buttons 1-8) ──► TRG A output
                                    └──► CV A output (from knobs 1-8)

CLK B ──► Seq B steps (buttons 9-16) ──► TRG B output
                                     └──► CV B output (from knobs 9-16)
```

**Example - Dual Melody + Bass:**
```
┌─────────────────────────────────────────────────────────────┐
│ SEQUENCER 1 (Dual Mode)                                     │
├─────────────────────────────────────────────────────────────┤
│ Buttons 1-8:  [●][●][○][●][○][○][●][○]  ← Seq A steps      │
│ Buttons 9-16: [●][○][○][○][●][○][○][○]  ← Seq B steps      │
│                                                             │
│ Knobs 1-8:   Melody notes (CV A)                           │
│ Knobs 9-16:  Bass notes (CV B)                             │
├─────────────────────────────────────────────────────────────┤
│ OUTPUTS:                                                    │
│   TRG A → Melody envelope trigger                          │
│   CV A  → Melody V/Oct to oscillator                       │
│   TRG B → Bass envelope trigger                            │
│   CV B  → Bass V/Oct to oscillator                         │
└─────────────────────────────────────────────────────────────┘
```

### Single Mode
One sequence using all 16 steps/values, routed to A or B outputs.

**Setup:** Value Length A ≥ 9 (enables single value mode)
          Step Length A ≥ 9 (enables single step mode)

**Signal Flow:**
```
CLK A ──► All 16 steps (buttons 1-16) ──┬──► TRG A output (based on routing)
                                        └──► TRG B output (based on routing)

          All 16 knobs ─────────────────┬──► CV A output
                                        └──► CV B output (same value)
```

**Example - Single Sequence with Bernoulli Routing:**
```
┌─────────────────────────────────────────────────────────────┐
│ SEQUENCER 1 (Single Mode + Bernoulli Routing)               │
├─────────────────────────────────────────────────────────────┤
│ All 16 buttons: [●][●][○][●][●][○][●][○][●][○][●][●][○][●][○][●] │
│                                                             │
│ All 16 knobs:   Note values (same CV to both outputs)      │
│ Bias knob:      50% = equal chance A or B                  │
├─────────────────────────────────────────────────────────────┤
│ OUTPUTS (randomly routed per step):                        │
│   TRG A → ~50% of triggers                                 │
│   TRG B → ~50% of triggers                                 │
│   CV A  → Current step's knob value                        │
│   CV B  → Current step's knob value (same)                 │
└─────────────────────────────────────────────────────────────┘
```

## Output Summary Table

| Output | Dual Mode | Single Mode |
|--------|-----------|-------------|
| **TRG A** | Fires when Seq A step is active | Fires based on routing mode |
| **TRG B** | Fires when Seq B step is active | Fires based on routing mode |
| **CV A** | Value from knobs 1-8 (Seq A position) | Value from knobs 1-16 |
| **CV B** | Value from knobs 9-16 (Seq B position) | Same as CV A |

## Voltage Settings

Hold **Record Arm** to access voltage settings:

**Track Control 1-4** (Seq A voltage):
- 1: Green = 5V range
- 2: Amber = 10V range
- 3: Red = 1V range

**Track Control 2** (Seq A polarity):
- Toggle bipolar mode (-2.5V to +2.5V for 5V range, etc.)

**Track Control 5-8** (Seq B voltage/polarity):
- Same as above but for Sequence B

## Competition Modes (Dual Mode)

When both A and B want to fire at the same time:

Hold **Record Arm** + press **Track Focus 1-8**:

| # | Mode | Description |
|---|------|-------------|
| 1 | Independent | Both sequences fire freely |
| 2 | Steal | Bernoulli gate decides winner |
| 3 | A Priority | A wins more often |
| 4 | B Priority | B wins more often |
| 5 | Momentum | Winner gets boost for next round |
| 6 | Revenge | Loser gets boost for next round |
| 7 | Echo | Loser echoes on next clock |
| 8 | Value Theft | Winner uses combined CV |

## Routing Modes (Single Mode)

How triggers are distributed to A/B outputs:

Hold **Record Arm** + press **Track Focus 1-8**:

| # | Mode | Description |
|---|------|-------------|
| 1 | All A | All triggers to output A |
| 2 | All B | All triggers to output B |
| 3 | Bernoulli | Random routing (bias = probability) |
| 4 | Alternate | A, B, A, B... |
| 5 | Two-Two | A, A, B, B... |
| 6 | Burst | Random bursts to each output |
| 7 | Probability | Per-step random routing |
| 8 | Pattern | Odd steps to A, even to B |

## Sequencer Utilities

While in a sequencer layout, hold **Device** + press **Track Focus**:

| Button | Function |
|--------|----------|
| 1 | Return to default layout |
| 2 | Copy sequencer |
| 3 | Paste sequencer |
| 4 | Clear all steps |
| 5 | Randomize steps |
| 6 | Randomize values |
| 7 | Invert steps |
| 8 | Reset playheads |

## LED Feedback

### Soft Takeover (Knobs)
- **Green** - Knob is synced
- **Yellow** - Turn knob RIGHT to sync
- **Red** - Turn knob LEFT to sync

### Step Display
- **Bright Green** - Playhead on active step
- **Dim Green** - Active step
- **Dim Red** - Playhead on inactive step
- **Off** - Inactive step
- **Amber** - Length boundary

### Button Modes (Default Layout)
- **Off** - Toggle mode, gate off
- **Bright Green** - Toggle mode, gate on
- **Dim Green** - Momentary mode, gate off (indicates button is momentary)
- **Bright Green (when held)** - Momentary mode, gate on

## Expander Chaining

```
[ClockExpander] → [Core] → [InfoDisplay] → [SeqExpander] → [CVExpander] → ...
     (LEFT)                              (RIGHT - any order)
```

All expanders (except ClockExpander) can be placed in any order to the right of Core.

## Requirements

- VCV Rack 2.x
- Novation Launch Control XL MK2

## Building from Source

### Prerequisites

1. **VCV Rack SDK** - Download from [VCV Rack SDK](https://vcvrack.com/manual/PluginDevelopmentTutorial)
2. **Build tools**:
   - **macOS**: Xcode Command Line Tools (`xcode-select --install`)
   - **Windows**: MSYS2 with MinGW-w64
   - **Linux**: `build-essential`, `cmake`, `curl`, `git`

### Build Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/ahlstrominfo/vcv-launchcontrol.git
   cd vcv-launchcontrol
   ```

2. Get the Rack SDK - either:
   - **Option A**: Place the `Rack-SDK` folder in the plugin directory (recommended)
   - **Option B**: Set the path to your SDK:
     ```bash
     export RACK_DIR=/path/to/Rack-SDK
     ```

3. Build:
   ```bash
   make
   ```

### Installation

Copy the plugin files to your VCV Rack plugins folder. The folder must be named `LaunchControlXL`.

**macOS:**
```bash
mkdir -p ~/Documents/Rack2/plugins/LaunchControlXL
cp plugin.dylib res/*.svg ~/Documents/Rack2/plugins/LaunchControlXL/
cp plugin.json ~/Documents/Rack2/plugins/LaunchControlXL/
```

**Windows:**
```bash
mkdir -p ~/Documents/Rack2/plugins/LaunchControlXL
cp plugin.dll res/*.svg ~/Documents/Rack2/plugins/LaunchControlXL/
cp plugin.json ~/Documents/Rack2/plugins/LaunchControlXL/
```

**Linux:**
```bash
mkdir -p ~/.Rack2/plugins/LaunchControlXL
cp plugin.so res/*.svg ~/.Rack2/plugins/LaunchControlXL/
cp plugin.json ~/.Rack2/plugins/LaunchControlXL/
```

Restart VCV Rack to load the plugin.

## License

GPL-3.0-or-later

## Credits

Ideas and design by [@ahlstrominfo](https://github.com/ahlstrominfo)

Code generated by [Claude Code](https://claude.com/claude-code) by Anthropic
