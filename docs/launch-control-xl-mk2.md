# Novation Launch Control XL MK2 - MIDI Specification

## Overview

The Launch Control XL MK2 is a USB MIDI controller designed primarily for Ableton Live but fully programmable for any MIDI application.

### Physical Controls

- **24 Rotary Knobs** (pots) with 300-degree motion, each with LED indicator
- **8 Faders** (60mm travel)
- **16 Assignable Buttons** (multi-color backlit) in 2 rows
- **8 Additional Buttons** (Device, Mute, Solo, Record Arm, Up, Down, Left, Right)
- **2 Template Selection Buttons** (User/Factory)

### Connectivity

- USB 2.0 Type B (bus-powered)
- Appears as "Launch Control XL" MIDI device
- Class-compliant (no drivers needed)

### Dimensions

240mm (W) x 240mm (D) x 39mm (H)

## Templates

The device has 16 templates:

| Slots | Type | Editable | Default MIDI Channel |
|-------|------|----------|---------------------|
| 0-7 (00h-07h) | User | Yes | 1-8 |
| 8-15 (08h-0Fh) | Factory | No | 9-16 |

Templates are edited using [Novation Components](https://components.novationmusic.com/).

## Factory Template 1 MIDI Mapping

### Knobs (CC Messages) - MIDI Channel 9

| Row | Position | Control | CC Numbers (L to R) |
|-----|----------|---------|---------------------|
| 1 | Top | Send A | 13, 14, 15, 16, 17, 18, 19, 20 |
| 2 | Middle | Send B | 29, 30, 31, 32, 33, 34, 35, 36 |
| 3 | Bottom | Pan/Device | 49, 50, 51, 52, 53, 54, 55, 56 |

### Faders (CC Messages) - MIDI Channel 9

| Position | CC Number |
|----------|-----------|
| Fader 1 | 77 |
| Fader 2 | 78 |
| Fader 3 | 79 |
| Fader 4 | 80 |
| Fader 5 | 81 |
| Fader 6 | 82 |
| Fader 7 | 83 |
| Fader 8 | 84 |

### Channel Buttons (Note Messages) - MIDI Channel 9

| Row | Function | Note Numbers (L to R) |
|-----|----------|----------------------|
| Top | Track Focus | 41, 42, 43, 44, 57, 58, 59, 60 |
| Bottom | Track Control | 73, 74, 75, 76, 89, 90, 91, 92 |

### Navigation Buttons (CC Messages) - MIDI Channel 9

| Button | CC Number |
|--------|-----------|
| Up | 104 |
| Down | 105 |
| Left | 106 |
| Right | 107 |

## LED Control

### LED Types

The Launch Control XL has **bi-color LEDs** (NOT RGB):

- **Channel buttons (16)**: Red + Green LEDs that can mix to form Amber/Yellow
- **Direction buttons (4)**: Single Red LED only
- **Function buttons (4)**: Device, Mute, Solo, Record Arm - Single Yellow LED only
- **Knob LEDs (24)**: Red + Green LEDs

### Two Protocols Available

1. **Launchpad Protocol** (Note/CC messages) - Only works if currently selected template has matching note/CC and channel
2. **SysEx Protocol** - Updates any LED on any template, regardless of current template

### Reset / "Takeover" Command

To take full control of the device's LEDs, send a reset command:

```
Hex:     Bn 00 00
Decimal: (176+n) 0 0
```

Where `n` is the template number (0-7 user, 8-15 factory). This:
- Turns off ALL LEDs
- Resets buffer settings to defaults
- Resets duty cycle to default

After reset, you have full control to set LEDs via SysEx.

### LED Color Byte Calculation

**Formula:** `Velocity = (16 × Green) + Red + Flags`

Where:
- **Green**: 0-3 (off, low, medium, full brightness)
- **Red**: 0-3 (off, low, medium, full brightness)
- **Flags**:
  - `12` (0x0C) = Normal display
  - `8` (0x08) = Flashing mode
  - `0` = Double-buffering mode

### Pre-calculated Color Values

| Decimal | Hex | Color |
|---------|-----|-------|
| 12 | 0x0C | Off |
| 13 | 0x0D | Red Low |
| 15 | 0x0F | Red Full |
| 28 | 0x1C | Green Low |
| 60 | 0x3C | Green Full |
| 29 | 0x1D | Amber Low |
| 63 | 0x3F | Amber Full |
| 62 | 0x3E | Yellow Full |

### Flashing Colors

| Decimal | Color |
|---------|-------|
| 11 | Red Full Flash |
| 56 | Green Full Flash |
| 58 | Yellow Full Flash |
| 59 | Amber Full Flash |

### SysEx Message Format - Set Single LED

```
F0 00 20 29 02 11 78 [Template] [Index] [Color] F7
```

| Byte | Description |
|------|-------------|
| F0 | SysEx Start |
| 00 20 29 | Novation Manufacturer ID |
| 02 | Product Type |
| 11 | Launch Control XL Device ID |
| 78 | LED Single Message Command |
| Template | 00-07 (user), 08-0F (factory) |
| Index | LED position (see below) |
| Color | Color/brightness value |
| F7 | SysEx End |

You can set multiple LEDs in one message by adding more Index/Color pairs before F7.

### SysEx - Change Template

```
F0 00 20 29 02 11 77 [Template] F7
```

Switches the device to the specified template (0-15).

### SysEx - Toggle Button State

```
F0 00 20 29 02 11 7B [Template] [Index] [Value] F7
```

Value: 00 (off) or 7F (on). Only affects buttons configured as "Toggle" mode.

### LED Index Map

| Index (hex) | Index (dec) | LED Position |
|-------------|-------------|--------------|
| 00-07 | 0-7 | Top knob row (L-R) |
| 08-0F | 8-15 | Middle knob row (L-R) |
| 10-17 | 16-23 | Bottom knob row (L-R) |
| 18-1F | 24-31 | Top channel buttons (L-R) |
| 20-27 | 32-39 | Bottom channel buttons (L-R) |
| 28-2B | 40-43 | Device, Mute, Solo, Record Arm |
| 2C-2F | 44-47 | Up, Down, Left, Right |

### Double-Buffering

For flicker-free LED updates, use double-buffering:

```
Bn 00 31  - Display buffer 1, update buffer 0
[Send LED updates with Flags=0]
Bn 00 34  - Swap to display buffer 0
```

### Automatic Flashing

Enable device-controlled LED flashing:

```
Bn 00 28  - Enable auto-flash at internal rate
```

### Launchpad Protocol (Note-based)

For templates with matching note assignments:

```
Note On: 9n [Note] [Velocity]
Note Off: 8n [Note] 00
CC: Bn [CC] [Value]
```

The velocity/value determines the LED color using the same color byte formula.

## User Template Programming

Each control in a user template can be configured:

### Knobs/Faders
- CC number (1-127)
- MIDI channel (1-16)
- Min/Max values
- LED color
- LED MIDI channel

### Buttons
- Type: Note or CC
- Note/CC number
- Press type: Momentary or Toggle
- On/Off values
- LED behavior

## VCV Rack Integration Tips

### Using with MIDI-CC Module

1. Connect Launch Control XL via USB
2. In VCV Rack, add Core MIDI-CC module
3. Select "Launch Control XL" as MIDI device
4. Click output ports to enter learn mode
5. Move controls on Launch Control XL to map

### LED Feedback in VCV Rack

1. Use a module that outputs MIDI (like CV-CC)
2. Connect CV signals to the module
3. Configure to send CC/Note on same channel as Launch Control XL template
4. Use velocity mode for color control

### Recommended VCV Modules

- **Core MIDI-CC**: Map CCs to CV outputs
- **Core CV-CC**: Send CV back as CC for LED feedback
- **stoermelder MIDI-CAT**: Advanced MIDI mapping with LED support

## References

- [Novation Launch Control XL Product Page](https://novationmusic.com/products/launch-control-xl-mk2)
- [Novation Components](https://components.novationmusic.com/) - Template editor
- [Programmer's Reference Guide (PDF)](https://fael-downloads-prod.focusrite.com/customer/prod/downloads/launch_control_xl_programmer_s_reference_guide.pdf)
- [Novation Support](https://support.novationmusic.com/)
