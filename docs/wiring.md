# PS2 Controller Cable Wiring

Verified wire-color mapping for the PS2 controller cable used in this project, traced
with a multimeter (continuity/resistance mode) against the documented PS2 controller
port pinout (Task 1.3).

## Connector Pinout

Viewed from the front (the console's controller port opening — the same orientation
you're looking at when plugging a controller in):

```
  ______________________________
 | 1   2   3 | 4   5   6 | 7 . 9 |
 |___________|___________|_______|
        (shield tab on the
         outside of the housing)
```

`.` marks the empty/keyed position where pin 8 would be.

- **Single row, 9 positions grouped in 3s:** 1, 2, 3 | 4, 5, 6 | 7, 8, 9.
- **Pin 8 is absent on this connector** — the housing has no pin in that position (keys
  the plug so it can't be inserted rotated 180°); only 8 physical pins are present.
- **Shield tab:** the connector's metal shell, on the outside of the plastic housing —
  not a numbered pin.

## Pin / Signal / Wire Color

| Pin | Signal | Direction              | Wire color |
|-----|--------|-------------------------|------------|
| 1   | DATA   | controller → console    | Brown      |
| 2   | COMMAND| console → controller    | Orange     |
| 3   | 9V     | unused — do not connect | —          |
| 4   | GND    | —                       | Black      |
| 5   | 3.6V   | unused — do not connect | —          |
| 6   | ATT/CS | console → controller (active low) | Yellow |
| 7   | CLOCK  | console → controller    | Blue       |
| 8   | (no connection) | —              | —          |
| 9   | ACK    | controller → console (open-collector) | Green |

No deviations from common documented wire-color conventions were found.

## Shield / Drain Wire

The cable also carries a separate **bare (uninsulated) drain wire**, connected to the
connector's metal shell tab — not to pin 4. This is an EMI shield ground, electrically
distinct from the pin 4 signal ground even though both are "ground."

**Do not use the bare shield wire as the shared ground for wiring the Pico.** Use the
**pin 4 (black) wire** as the shared GND between the Pico and the PS2 console — that's
the signal-reference ground the DATA/COMMAND/CLOCK/ATT/ACK lines are relative to. The
bare shield wire can be left unconnected for this project; it isn't needed for the
signal-level bring-up described in Task 2 onward.

## Pico-Side Wiring (Task 2 GPIO smoke test)

| Pico GPIO | PS2 signal | Wire color |
|-----------|------------|------------|
| GPIO 2    | ATT/CS     | Yellow     |
| GPIO 3    | CLOCK      | Blue       |
| GPIO 4    | COMMAND    | Orange     |
| any GND pin | GND (pin 4) | Black    |

DATA (pin 1, brown) and ACK (pin 9, green) are not wired yet at this stage — Task 2 is
listen-only. They'll be connected starting in Task 3.

---

*Source: Task 1.3 multimeter trace, recorded in `TASK1-CHECKLIST.md`. This file will be
expanded with a full power-scheme diagram at Task 8.1.*
