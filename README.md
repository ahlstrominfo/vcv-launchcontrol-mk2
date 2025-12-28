# LaunchControl XL for VCV Rack

A VCV Rack plugin for the **Novation Launch Control XL MK2** MIDI controller, featuring 8 independent dual sequencers with advanced competition and routing modes.

## Modules

### Core
The main module that communicates with your Launch Control XL.

**Inputs:**
- **CLK A** - Clock input for Sequence A (shared across all sequencers, or use ClockExpander for per-sequencer clocks)
- **CLK B** - Clock input for Sequence B (normaled to CLK A)
- **RST** - Reset all sequencers to step 0

**Outputs:**
- **TRG A/B** - Trigger outputs for the selected sequencer
- **CV A/B** - CV outputs for the selected sequencer (0-5V)
- **Faders 1-8** - Direct CV outputs from the 8 faders (0-10V)

**Right-click menu:**
- **MIDI Input/Output** - Select your Launch Control XL
- **Sequencer Output** - Choose "Follow view" or lock to a specific sequencer (1-8)

### ClockExpander (Left of Core)
Provides individual clock inputs for each of the 8 sequencers.

- **CLK A 1-8** - Clock A inputs with chaining (patching input 3 feeds sequencers 3-8)
- **CLK B 1-8** - Clock B inputs (normaled to own CLK A only, not chained)

### SeqExpander (Right of Core)
Outputs triggers and CV from ALL 8 sequencers simultaneously.

- **TRG A/B 1-8** - Trigger outputs for each sequencer
- **CV A/B 1-8** - CV outputs for each sequencer (0-5V)

### KnobExpander (Right of Core)
Outputs CV from all 24 knobs of the current layout (0-10V).

- **ROW 1-3** - 8 outputs each for the three knob rows

### GateExpander (Right of Core)
Outputs gate signals from the 16 buttons in default mode.

- **FOCUS 1-8** - Top row button states (10V when on)
- **CTRL 1-8** - Bottom row button states (10V when on)

## Getting Started

1. Add the **Core** module to your patch
2. Right-click and select your Launch Control XL for both MIDI Input and Output
3. Press the **TAKE** button (or it auto-initializes when MIDI connects)
4. The controller LEDs should now reflect the module state

## Layouts

### Default Layout (Layout 0)
- **Knobs**: 24 CV outputs via KnobExpander
- **Faders**: 8 CV outputs directly from Core
- **Buttons**: 16 toggle gates via GateExpander

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
| 1 | Value Length A | 1-16 | Active value knobs for Seq A. Set to 9+ for single mode (all 16 knobs) |
| 2 | Value Length B | 0-8 | Active value knobs for Seq B. Set to 0 to disable |
| 3 | Step Length A | 1-16 | Active steps for Seq A. Set to 9+ for single mode (all 16 buttons) |
| 4 | Step Length B | 0-8 | Active steps for Seq B. Set to 0 to disable |
| 5 | Probability A | 0-100% | Chance Seq A fires on each step |
| 6 | Probability B | 0-100% | Chance Seq B fires on each step |
| 7 | Bias | 0-100% | Controls competition/routing behavior |
| 8 | (Reserved) | - | Not currently used |

## Dual vs Single Mode

**Dual Mode** (default): Two independent sequences
- Steps 1-8 (top buttons) for Sequence A
- Steps 9-16 (bottom buttons) for Sequence B
- Values 1-8 (top knobs) for Sequence A
- Values 9-16 (bottom knobs) for Sequence B

**Single Mode**: One sequence using all 16 steps/values
- Set Value Length A to 9+ for single value mode
- Set Step Length A to 9+ for single step mode

## Competition Modes (Dual Mode)

Hold **Record Arm** + press **Track Focus 1-8** to select a competition mode:

1. **Independent** - Both sequences fire freely
2. **Steal** - Bernoulli gate decides winner (bias controls probability)
3. **A Priority** - A wins more often (bias increases A's advantage)
4. **B Priority** - B wins more often (bias increases B's advantage)
5. **Momentum** - Winner gets boost for next round
6. **Revenge** - Loser gets boost for next round
7. **Echo** - Loser echoes on next clock
8. **Value Theft** - Winner uses combined CV

## Routing Modes (Single Mode)

When in single step mode, hold **Record Arm** + press **Track Focus 1-8**:

1. **All A** - All triggers to output A
2. **All B** - All triggers to output B
3. **Bernoulli** - Random routing (bias controls probability)
4. **Alternate** - A, B, A, B...
5. **Two-Two** - A, A, B, B...
6. **Burst** - Random bursts to each output
7. **Probability** - Per-step random routing
8. **Pattern** - Odd steps to A, even to B

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
When switching layouts, knobs use soft takeover to prevent jumps:
- **Green** - Knob is synced (value matches physical position)
- **Yellow** - Turn knob RIGHT to sync
- **Red** - Turn knob LEFT to sync

### Step Display
- **Bright Green** - Playhead on active step
- **Dim Green** - Active step
- **Dim Red** - Playhead on inactive step
- **Off** - Inactive step
- **Amber** - Length boundary (shown briefly when adjusting length)

### Value Knobs (Sequencer Mode)
- **Bright Green** - Current playhead position (synced)
- **Dim Green** - Within sequence length (synced)
- **Off** - Outside sequence length

## Expander Chaining

Expanders can be chained in any order to the right of Core:
```
[ClockExpander] → [Core] → [SeqExpander] → [KnobExpander] → [GateExpander]
```

The ClockExpander must be placed directly to the LEFT of Core. All other expanders go to the right and can be in any order.

## Requirements

- VCV Rack 2.x
- Novation Launch Control XL MK2 (Factory Template 1 / Template 8)

## Building

```bash
make install
```

## License

GPL-3.0-or-later
