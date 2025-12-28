# Launch Control XL VCV Rack Module - Concept Plan

## Overview

A VCV Rack module that deeply integrates with the Novation Launch Control XL MK2, providing:
- Direct CV output from faders
- Multiple "layouts" accessible via button combinations
- 8 step sequencers with loop point selection

## Hardware Mapping

### Always Active (All Layouts)

| Control | Function |
|---------|----------|
| Faders 1-8 | CV outputs (0-10V) - always available |

### Layout Switching

**Modifier:** Hold `Device` button

| Combo | Action |
|-------|--------|
| Device + Track Focus 1 | Return to default layout |
| Device + Track Control 1 | Enter Sequencer 1 |
| Device + Track Control 2 | Enter Sequencer 2 |
| Device + Track Control 3 | Enter Sequencer 3 |
| Device + Track Control 4 | Enter Sequencer 4 |
| Device + Track Control 5 | Enter Sequencer 5 |
| Device + Track Control 6 | Enter Sequencer 6 |
| Device + Track Control 7 | Enter Sequencer 7 |
| Device + Track Control 8 | Enter Sequencer 8 |

## Default Layout

When not in a sequencer mode:

### Knobs → CV Outputs (via Knob Expander)

All 24 knobs output CV (0-10V) through the Knob Expander.

### Buttons → Gate Outputs (via Gate Expander)

The buttons act as a **manual toggle patchbay**:

```
Track Focus:    [1] [2] [3] [4] [5] [6] [7] [8]    → Gate outputs 1-8
Track Control:  [9][10][11][12][13][14][15][16]   → Gate outputs 9-16
```

- **Tap:** Toggle output on/off (0V / 10V)
- LEDs show current state (on = lit, off = dark)

### Default Layout Outputs (Module Panel)

| Output | Function |
|--------|----------|
| FADER 1-8 | CV from faders (always active) |
| GATE 1-16 | Toggle gate outputs from buttons |

## Sequencer Mode (per sequencer, 8 total)

### Step Selection (16 steps)

Using the 16 channel buttons as **toggles** (tap to turn on, tap again to turn off):

```
Track Focus:    [1] [2] [3] [4] [5] [6] [7] [8]   (steps 1-8)
Track Control:  [9] [10][11][12][13][14][15][16]  (steps 9-16)
```

- **Tap:** Toggle step on/off (state persists until tapped again)
- **Hold step A + tap step B:** Set loop points (A = start, B = end)

### Inputs (Sequencer Expander)

| Input | Function |
|-------|----------|
| TRIG 1-8 | Trigger inputs - advances corresponding sequencer |
| RESET 1-8 | Reset inputs - resets corresponding sequencer to step 1 |

**Normalling:** Both TRIG and RESET inputs cascade downward.

```
[TRIG 1]→[TRIG 2]→[TRIG 3]→[TRIG 4]→[TRIG 5]→[TRIG 6]→[TRIG 7]→[TRIG 8]
    ↓        ↓        ↓        ↓        ↓        ↓        ↓        ↓
  Seq 1    Seq 2    Seq 3    Seq 4    Seq 5    Seq 6    Seq 7    Seq 8

[RST 1]→[RST 2]→[RST 3]→[RST 4]→[RST 5]→[RST 6]→[RST 7]→[RST 8]
```

**Example:** Patch into TRIG 1 and RST 1 → all 8 sequencers triggered and reset together

### Outputs (Sequencer Expander)

| Output | Function |
|--------|----------|
| GATE 1-8 | Gate outputs from sequencers (10V when step active) |
| CV 1-8 | CV outputs from sequencers (from value knobs) |

Both GATE and CV fire together on each trigger.

## LED Feedback

### Default Layout
- Knob LEDs: Show current value (green gradient?)
- Button LEDs: Show current layout indicator

### Sequencer Layout
- Track Focus/Control buttons:
  - **Off** = step inactive
  - **Green** = step active
  - **Red** = current playhead position
  - **Amber** = loop start/end markers

## Module Architecture: Main + Expanders

Use VCV Rack's **expander system** to keep the main module simple while allowing flexible I/O expansion.

### Main Module (LaunchControl Core)
- MIDI connection to Launch Control XL
- Handles all MIDI input/output and LED control
- Minimal panel: just MIDI selector + status LEDs
- Sends data to adjacent expanders via message passing

### Expander Modules (optional, placed adjacent)

| Expander | Jacks | Function |
|----------|-------|----------|
| **Fader Expander** | 8 out | CV outputs from faders |
| **Gate Expander** | 16 out | Toggle gate outputs (default mode buttons) |
| **Knob Expander** | 24 out | CV outputs from knobs (default mode) |
| **Seq Trigger Expander** | 8+8 in | TRIG 1-8 + RESET 1-8 inputs (normalled) |
| **Seq Gate Expander** | 8 out | Gate outputs from sequencers |
| **Seq CV Expander** | 8 out | CV outputs from sequencers |

Users only add the expanders they need!

**Possible combinations:**
- Simple CV controller: Main + Fader + Knob
- Toggle patchbay: Main + Gate
- Full sequencer: Main + Seq Trigger + Seq Gate + Seq CV
- Everything: All expanders

### Communication Flow

```
                              ┌─→ [Fader Exp] (8 CV out)
                              ├─→ [Gate Exp] (16 gate out)
[Knob Exp] ←── [Main Module] ─┼─→ [Seq Trig Exp] (8+8 in)
 (24 CV out)    (MIDI+LEDs)   ├─→ [Seq Gate Exp] (8 gate out)
                              └─→ [Seq CV Exp] (8 CV out)
```

Main module passes state to expanders via `leftExpander`/`rightExpander` messages (1 sample latency).

Expanders can be placed on either side of main module and chained.

## Sequencer Knob Layout (Polyrhythmic Design)

In sequencer mode, the 24 knobs have specific roles:

### Top Two Rows: Value Knobs (16 knobs)

```
Row 1: [V1] [V2] [V3] [V4] [V5] [V6] [V7] [V8]   (values 1-8)
Row 2: [V9] [V10][V11][V12][V13][V14][V15][V16]  (values 9-16)
```

These set CV values that the sequencer reads from.

### Bottom Row: Sequencer Settings (8 knobs)

```
Row 3: [START] [END] [PROB] [SCALE] [SLEW] [RANGE] [OCT] [DIR]
```

| Knob | Function |
|------|----------|
| 1 | **Start** - First value knob to read (1-16) |
| 2 | **End** - Last value knob to read (1-16) |
| 3 | **Probability** - Chance each step fires (0-100%) |
| 4 | **Scale** - Quantize to scale (chromatic, major, minor, pent, etc.) |
| 5 | **Slew** - Glide/portamento time between values |
| 6 | **Range** - Output attenuator (0-100%) |
| 7 | **Octave** - Octave shift (-2 to +2) |
| 8 | **Direction** - Value knob read direction (fwd / rev / pendulum / random) |

**Notes:**
- Direction affects only value knob cycling, not trigger steps. Triggers always advance forward.
- If Start > End, values are automatically swapped (e.g., Start=12, End=4 becomes Start=4, End=12)

### Polyrhythmic Magic

The **trigger steps** (16 buttons) and **value knobs** (variable range) cycle independently:

**Example:**
- Triggers: 12 steps (buttons 1-12 active, loop enabled)
- Values: 5 knobs (Start=2, End=6)

```
Trigger step:  1   2   3   4   5   6   7   8   9  10  11  12  | 1   2 ...
Value knob:    V2  V3  V4  V5  V6  V2  V3  V4  V5  V6  V2  V3 | V4  V5 ...
```

Pattern fully repeats after **LCM(12, 5) = 60 steps** - creates evolving, non-repetitive sequences!

### Knob LED Feedback (Soft Takeover / Pickup Mode)

**Applies to:** All 24 knobs in ALL modes (default + all sequencers)

When switching layouts/sequencers, physical knob positions may not match stored values. LEDs indicate mismatch:

| LED Color | Meaning | Action |
|-----------|---------|--------|
| **Green** | Knob within ±2 of stored value | Actively controlling |
| **Yellow** | Knob is BELOW stored value | Turn RIGHT to catch up |
| **Red** | Knob is ABOVE stored value | Turn LEFT to catch up |

**Tolerance:** ±2 counts (out of 0-127) for pickup. Prevents fiddly exact-match requirement.

**Behavior:** Value does NOT change until knob "picks up" (reaches green zone). This prevents sudden jumps when switching between modes.

```
Stored value: 64 (green zone: 62-66)
Knob at 30:   [YELLOW] ----→ turn right
Knob at 63:   [GREEN]  ← picked up! now controlling
Knob at 90:   [RED]    ←---- turn left
```

### Additional LED Feedback

- **Value knobs (row 1-2):** Show active range (Start to End) vs inactive
- **Current value position:** Could flash or be brighter than others

## Open Questions

1. **Save/recall** - Save sequencer patterns? Per-patch (automatic) or global presets?

2. **Scale selection** - How to choose scale with a knob?
   - Discrete steps (1=chromatic, 2=major, 3=minor, etc.)
   - Or separate scale selector via right-click menu?

3. **Value knob range** - What voltage does 0-100% knob represent?
   - 0-10V (standard CV)?
   - 0-1V (1V/oct within octave)?
   - Configurable via Range knob?

4. **Pendulum behavior** - At ends, does it repeat the end value or reverse immediately?
   - `1-2-3-4-5-4-3-2-1-2-3...` (no repeat)
   - `1-2-3-4-5-5-4-3-2-1-1-2...` (repeat ends)

## Implementation Phases

### Phase 1: Foundation
- [ ] Set up VCV Rack plugin project structure
- [ ] Basic MIDI input handling (Launch Control XL)
- [ ] Fader → CV output (8 channels)
- [ ] SysEx communication for LED control

### Phase 2: Layout System
- [ ] Implement Device button modifier detection
- [ ] Layout switching state machine (default + 8 sequencers)
- [ ] LED feedback for current layout

### Phase 3: Sequencer Core
- [ ] Single sequencer implementation (16 steps)
- [ ] Clock input handling
- [ ] Step toggle via buttons
- [ ] Gate output

### Phase 4: Loop Points
- [ ] Hold-to-select loop point gesture
- [ ] Visual feedback for loop markers
- [ ] Sequencer respects loop bounds

### Phase 5: Polyrhythmic Features
- [ ] Value knob Start/End range
- [ ] Independent value cycling vs trigger stepping
- [ ] Direction modes (fwd/rev/pendulum/random)
- [ ] Probability, Scale, Slew, Range, Octave controls

### Phase 6: Soft Takeover
- [ ] Track physical knob positions
- [ ] LED feedback (green/yellow/red)
- [ ] ±2 tolerance pickup logic
- [ ] Per-layout value storage

### Phase 7: Expanders
- [ ] Expander message protocol
- [ ] Fader Expander (8 CV out)
- [ ] Gate Expander (16 gate out)
- [ ] Knob Expander (24 CV out)
- [ ] Seq Trigger Expander (8+8 in, normalled)
- [ ] Seq Gate Expander (8 gate out)
- [ ] Seq CV Expander (8 CV out)

### Phase 8: Polish
- [ ] All 8 sequencers working
- [ ] Pattern storage (per-patch)
- [ ] Edge cases and testing

## Technical Notes

### MIDI Channel Strategy
- Use a dedicated User Template (e.g., template 0, MIDI channel 1)
- Or use Factory Template 1 (MIDI channel 9)

### State Management
- Track current layout (0 = default, 1-8 = sequencers)
- Track Device button held state
- Per-sequencer: 16 step states, loop start/end, current position, 24 knob values
- Default mode: 24 knob values, 16 button toggle states
- Soft takeover state: last physical position per knob per layout

### LED Update Strategy
- Use SysEx for direct LED control (bypasses template restrictions)
- Send reset command on module init to take over LEDs
- Batch LED updates using double-buffering for smooth transitions

---

## REDESIGN: Dual-Row Sequencer with Competition System (Dec 2025)

### Overview

Each sequencer layout can operate in **two modes** based on Value Length A:

1. **Single Sequencer Mode** (Value Length A = 9-16): One sequencer, two outputs with routing
2. **Dual Sequencer Mode** (Value Length A = 1-8): Two sequencers that can compete/interact

### Parameter Knobs (Bottom Row)

```
Row 3: [VAL-A] [VAL-B] [STEP-A] [STEP-B] [PROB-A] [PROB-B] [BIAS] [reserved]
          1       2       3        4        5        6       7        8
```

| Knob | Function | Range |
|------|----------|-------|
| 1 | **Value Length A** - How many value knobs for Seq A | 1-16 (≥9 = single mode) |
| 2 | **Value Length B** - How many value knobs for Seq B | 0-8 (0=disabled) |
| 3 | **Step Length A** - How many step buttons for Seq A | 1-16 |
| 4 | **Step Length B** - How many step buttons for Seq B | 0-8 (0=disabled) |
| 5 | **Probability A** - Chance Seq A fires (dual) / overall prob (single) | 0-100% |
| 6 | **Probability B** - Chance Seq B fires (dual) / CV split (single) | 0-100% |
| 7 | **Bias/Amount** - Competition bias (dual) / routing bias (single) | 0-100% |
| 8 | Reserved for future | - |

### Mode Determination

```
If Value Length A >= 9:
  → SINGLE MODE
  → All 16 buttons for steps (length set by knob 3)
  → All 16 value knobs (length set by knob 1)
  → Two outputs with ROUTING modes (Bernoulli gate style)
  → Knobs 2 and 4 ignored
  → Only Clock A used

If Value Length A < 9:
  → DUAL MODE
  → Top 8 buttons (Track Focus) = Seq A steps (length set by knob 3, max 8)
  → Bottom 8 buttons (Track Control) = Seq B steps (length set by knob 4, 0=disabled)
  → Row 1 knobs = Seq A values (length set by knob 1)
  → Row 2 knobs = Seq B values (length set by knob 2, 0=disabled)
  → Two sequencers with COMPETITION modes
  → Clock A normals to Clock B
```

---

### Competition Modes (Dual Mode)

**Selection:** Hold Record Arm + Press Track Focus 1-8

| Rec Arm + Focus | Mode | Behavior | Knob 7 |
|-----------------|------|----------|--------|
| 1 | **Independent** | No competition, A and B fire freely | (no effect) |
| 2 | **Steal** | Bernoulli decides winner when both want to fire | Who wins (0%=A, 100%=B) |
| 3 | **A Priority** | A always wins ties, B only fires alone | How absolute |
| 4 | **B Priority** | B always wins ties, A only fires alone | How absolute |
| 5 | **Momentum** | Each win boosts winner's next chance | Boost amount |
| 6 | **Revenge** | Loser gets probability boost next time | Boost amount |
| 7 | **Echo** | Loser fires on NEXT clock as echo | Echo probability |
| 8 | **Value Theft** | Winner uses combined CV value | Mix (0%=winner, 100%=sum) |

**Trigger Stealing mechanic:**
- When both A and B want to fire on same clock
- Competition mode determines who "wins"
- **Winner:** Fires trigger, advances value index
- **Loser:** Step advances, but NO trigger, NO value change

---

### Routing Modes (Single Mode)

**Selection:** Hold Record Arm + Press Track Focus 1-8

| Rec Arm + Focus | Mode | Behavior | Knob 7 |
|-----------------|------|----------|--------|
| 1 | **All to A** | All triggers → Output A | (no effect) |
| 2 | **All to B** | All triggers → Output B | (no effect) |
| 3 | **Bernoulli** | Random split per trigger | Split % (0%=A, 100%=B) |
| 4 | **Alternate** | A, B, A, B, A, B... | Phase offset |
| 5 | **2+2** | AA, BB, AA, BB... | Pattern length |
| 6 | **Burst** | Random bursts to each output | Burst probability |
| 7 | **Probability** | Knob-controlled split | Split % |
| 8 | **Pattern** | Odd steps=A, even steps=B | (no effect) |

**Single Mode knob meanings:**
- Knob 5: Overall probability (chance step fires at all)
- Knob 6: CV split (0%=both same, 100%=A gets row1, B gets row2)
- Knob 7: Routing bias (mode-dependent)

---

### Trigger/Value Advancement

**Clock pulse behavior:**
1. Clock advances step position: `step = (step + 1) % STEP_LENGTH`
2. If current step is ACTIVE (button ON):
   - Apply probability check (knob 5/6)
   - Apply competition/routing mode
   - Winners: Fire trigger, advance value index
   - Losers: No trigger, no value advance
3. If current step is INACTIVE (button OFF):
   - No trigger
   - Value index stays the same

**Polyrhythm Example:**
- Step Length = 4, Value Length = 5
- Steps 0, 2 active (buttons on)
```
Clock:  0    1    2    3    0    1    2    3    0    1    2    3 ...
Step:   0    1    2    3    0    1    2    3    0    1    2    3
Trig:   ●         ●         ●         ●         ●         ●
Value:  V0        V1        V2        V3        V4        V0
```
Pattern repeats after LCM(4 steps, 5 values) = 20 clock pulses with 10 triggers

---

### Button Mapping

#### While Holding Device Button
| Combo | Action |
|-------|--------|
| Device + Track Focus 1 | Return to default layout |
| Device + Track Focus 2 | Copy current sequencer |
| Device + Track Focus 3 | Paste to current sequencer |
| Device + Track Focus 4 | Clear all steps |
| Device + Track Focus 5 | Randomize steps |
| Device + Track Focus 6 | Randomize values |
| Device + Track Focus 7 | Invert steps (flip on↔off) |
| Device + Track Focus 8 | Reset playheads to 0 |
| Device + Track Control 1-8 | Enter Sequencer 1-8 |

#### While Holding Record Arm Button
| Combo | Action |
|-------|--------|
| Rec Arm + Track Focus 1-8 | Select routing/competition mode |

#### Side Buttons (in sequencer mode)
| Button | Function |
|--------|----------|
| Record Arm | Modifier (hold to select mode) |
| Mute | (reserved for future) |
| Solo | (reserved for future) |

---

### Inputs/Outputs

**Core Module Inputs:**
- Clock A (normaled to Clock B)
- Clock B (for sequencer B in dual mode)
- Reset (resets both A and B to step 0)

**Core Module Outputs:**
- Trigger A
- CV A
- Trigger B
- CV B

---

### LED Feedback

**Dual Mode:**
- Top 8 buttons: Seq A steps (green=active, red=playhead, off=inactive)
- Bottom 8 buttons: Seq B steps (green=active, red=playhead, off=inactive)
- Row 1 knobs: Seq A values (dim=in range, bright=current, off=out of range)
- Row 2 knobs: Seq B values (dim=in range, bright=current, off=out of range)
- Bottom row knobs: Soft takeover (yellow/red/green)

**Single Mode:**
- All 16 buttons: Step pattern (green=active, red=playhead)
- Row 1+2 knobs: Value range (dim=in range, bright=current)
- Bottom row knobs: Soft takeover

**While Holding Record Arm:**
- Track Focus 1-8: Show current mode selection (lit = selected)

---

### Removed Features
- Hold-to-select loop points (replaced by length knobs)
- START knob for value range (always starts at 0)
- Scale quantization
- Slew, Range, Octave, Direction (replaced by competition system)
