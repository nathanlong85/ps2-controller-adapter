# PS2 Guitar Adapter — Task 1: Hardware Prep & Toolchain Bring-Up

Change: `ps2-guitar-adapter` | Branch: `feature/20260711/ps2-guitar-adapter`
Plan: `docs/superpowers/plans/2026-07-11-ps2-guitar-adapter.md`

All three steps below are hardware-only — no code yet.

## Step 1: Retrieve hardware

- [ ] Raspberry Pi Pico 2W
- [ ] CRKD Gibson Les Paul controller
- [ ] PS2 console (or test harness)
- [ ] Powered USB hub
- [ ] PS2 controller cable (for splicing/breakout)
- [ ] Multimeter

## Step 2: Verify toolchain with blink example

- [ ] Build the standard Pico SDK "blink" example (`blink/blink.c` from the official `pico-examples` repo) using VS Code + the Raspberry Pi Pico extension
- [ ] **Target the Pico 2W explicitly** — `-DPICO_BOARD=pico2_w`. The SDK defaults to the original Pico 1 board; forgetting this flag produces a binary that won't run on the 2W.
- [ ] Flash it and confirm the onboard LED blinks

If it fails: note the specific error before continuing — toolchain setup needs to be resolved first.

## Step 3: Verify PS2 cable pinout with a multimeter

Using continuity/resistance mode, trace each wire in the PS2 controller cable back to its pin on the connector and record the wire color:

| Pin | Signal | Direction | Wire color (fill in) |
|-----|--------|-----------|----------------------|
| 1 | DATA | controller → console | |
| 2 | COMMAND | console → controller | |
| 4 | GND | — | |
| 6 | ATT/CS | console → controller (active low) | |
| 7 | CLOCK | console → controller | |
| 9 | ACK | controller → console (open-collector) | |

Pins 3 (9V), 5 (3.6V), and 8 (no connection) are unused — do **not** wire these.

- [ ] Flag any deviation from common documented wire-color conventions

---

## When you're ready to resume

Just run `/comet` in the project. It will:
1. Detect the active change (`ps2-guitar-adapter`) and that it's mid-build.
2. Reload the `subagent-driven-development` skill and Comet's dispatch extensions.
3. Read the checkpoint at `openspec/changes/ps2-guitar-adapter/.comet/subagent-progress.md`, which records we're paused at Task 1 (hardware relay, no subagent dispatched yet).
4. Ask you for the results of Steps 1-3 above, then continue automatically into Task 2 and beyond — dispatching implementer/reviewer subagents for `[CODE]` steps and pausing to relay `[HW]` steps to you, per the mode you already chose.

No other setup needed — the branch, plan, and state file are already committed.
