# Corne Num — rev 2

Wireless split Corne with a number row: 31 keys per half, MX hotswap sockets,
SuperMini / nice!nano v2 (nRF52840) controllers, ZMK firmware, optional 0.91" OLED per half.

## Contents

| Folder | What is in it |
|---|---|
| `pcb/left/`, `pcb/right/` | Manufacturing files for each half (Gerbers, BOM, CPL, KiCad, board STEP) |
| `firmware/no_screen/` | Firmware for both halves without screens |
| `firmware/with_screen/` | Firmware with the vertical 0.91" OLED: tea cup (left), Golden Gate tower under the moon (right), animated |
| `firmware/settings_reset_nice_nano_v2.uf2` | Clears pairings and Studio changes (use when the halves won't connect) |
| `firmware/zmk-config/` | The full ZMK config: shields, keymap, screen code |
| `case/` | Base, plate and battery STEP |
| `decor/topo_tiles/` | Topographic decor tiles (print STEP/STL, plus the left tile placed in plate coordinates) |
| `screen/` | Where to wire the screen on each half, plus the OLED STEP model |
| `docs/` | Keymap PDF, diode orientation photo |

## Uploading files

- Left PCB files: https://github.com/Hardsummer/Corne-Num-Rev2/upload/main/pcb/left
- Right PCB files: https://github.com/Hardsummer/Corne-Num-Rev2/upload/main/pcb/right
- Updated case CAD: https://github.com/Hardsummer/Corne-Num-Rev2/upload/main/case

## Flashing

1. Plug a half in by USB and double-tap its reset button. A drive called NICENANO appears.
2. Drag the half's `.uf2` file onto it. It reboots by itself.
3. Do the left half first, then the right.

- **No screens:** `corne_num_rev2_LEFT_no_screen.uf2` and `corne_num_rev2_RIGHT_no_screen.uf2`.
- **With screens** (pin header at the top, USB end): `corne_num_LEFT_tea_animated.uf2` and
  `corne_num_RIGHT_golden_gate_animated.uf2`. If a screen reads upside down, flash that half's
  `*_upside_down_fix.uf2` instead.
- **Halves won't connect:** flash `settings_reset_nice_nano_v2.uf2` on both halves, flash the
  firmware again, then turn both halves on together. Re-pair "Corne Num" with the computer.

The left half is the central: it talks to the computer. The right half only sends its keys to the
left half, even when it is plugged into USB.

## Screen wiring (both halves)

| OLED pin | Controller pin |
|---|---|
| GND | GND (pad 23) |
| VCC | VCC (pad 21) |
| SCK | 010 (pad 14, P0.10) |
| SDA | 009 (pad 13, P0.09) |

On the SuperMini each pin name is printed just below its pin, and each row has an extra pin at the
top (B+ / B−). Go by the printed names. See `screen/rev2_LH_screen_wiring.png` and
`screen/rev2_RH_screen_wiring.png`.

## Firmware facts

- Matrix: COL2ROW. Rows R0–R4 = P0.22, P0.24, P1.00, P0.11, P1.04.
  Columns C0–C7 = P0.20, P0.31, P0.29, P0.02, P1.15, P1.13, P1.11, P1.06. The right half uses col-offset 8.
- Layers: Base, Num, Nav, Adjust. ZMK Studio unlock is on Num and Adjust (see the keymap PDF).
- Sleep: after 30 minutes without typing on battery. Wake with a key on the **left** half.
  The screens turn off after 30 s idle and the animation stops to save battery.

## Build notes

- **Controller:** mounted on the bottom on a 2.5 mm header spacer, chip side facing away from the
  PCB, USB-C at the top edge.
- **Diodes:** the band (cathode) goes to the closed end of the silkscreen outline.
- **Battery polarity:** the 600 mAh JST-PH batteries have their red wire on the opposite pin to J1's
  "+" marking. Swap the two crimps in each battery plug before plugging in.
- **Reset button:** the right-angle reset button is pressed from the board edge. Leave clearance in
  the case wall, or the case can hold it pressed and the half looks dead.
- **Parts per half:** 31 × 1N4148W diodes, 31 × Kailh MX hotswap sockets, SuperMini nRF52840,
  JST-PH battery connector, C&K OS102011MA1QN1 power switch, C&K PTS645VL58-2 LFS reset button,
  600 mAh LiPo, 6 × M2 screws.
