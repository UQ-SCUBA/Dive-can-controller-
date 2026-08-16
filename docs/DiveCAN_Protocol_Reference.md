# DiveCAN Protocol Reference

Compiled from [QuickRecon/DiveCAN](https://github.com/QuickRecon/DiveCAN) —
a reverse-engineered spec for Shearwater's proprietary rebreather CAN bus.
The upstream repo's own caveat applies directly here: **this is based on
incomplete reverse-engineering; validate before relying on it in a
life-support system.**

## Physical layer

- 125 kbps, ~3.0V high / 2.0V idle / 0.0V low
- 560Ω termination (non-standard — typical CAN is 120Ω)
- Max 9 devices per bus

## Extended CAN ID layout (29-bit)

```
Bits 28-24: Channel      (5 bits, fixed at 0x0D for this bus)
Bits 23-16: Message type (8 bits)
Bits 15-8:  Params / destination device ID (8 bits)
Bits 7-0:   Source device ID (8 bits)
```

So a full ID like `0xD040004` reads as: channel `0x0D`, msg type `0x04`
(PPO2), dest/params `0x00`, source device ID `0x04`.

## Device ID assignments

| ID | Device |
|----|--------|
| 1 | Shearwater controller |
| 2 | JJ OBOE, ISC Pathfinder head |
| 3 | JJ HUD |
| 4 | JJ SOLO, Optima head |
| 5 | rEvo Battery Box |

**Open question:** our handset needs its own device ID not in this list —
not resolved yet. Worth checking the actual DiveCANHead firmware source
(github.com/QuickRecon/DiveCANHead, per the original design summary) for
what it expects/assigns to a generic third-party handset.

## Message type IDs

| Hex | Function | Detail below? |
|-----|----------|----------------|
| 0x00 | Device ID | ✓ |
| 0x01 | Device Name | ✓ |
| 0x02 | Unknown metadata | — |
| 0x03 | Shutdown | — |
| 0x04 | PPO2 value | ✓ |
| 0x07 | HUD Status | — |
| 0x08 | Ambient Pressure | ✓ |
| 0x0A | Unified Diagnostic Services (UDS) | — |
| 0x0B | Tank Pressure | ✓ |
| 0x10 | NOP (ignored) | — |
| 0x11 | Millivolt readings | ✓ |
| 0x12 | Calibration response | ✓ |
| 0x13 | Calibration init | ✓ |
| 0x20-0x23 | CO2 enabled/value/cal response/cal request | — (out of scope, see design summary) |
| 0x30 | Menu system | ✓ (partial, see below) |
| 0x37 | Bus initialization | — |
| 0xC1 | Temperature | — |
| 0xC3 | Unknown (handset) | — |
| 0xC4 | Temperature probes enabled | — |
| 0xC9 | Setpoint | ✓ |
| 0xCA | Cell status | ✓ |
| 0xCB | General status | ✓ |
| 0xCC | Diving state | — |
| 0xD2 | Serial number | ✓ |

## Messages relevant to the handset (v1 scope)

### PPO2 — `0xD040004`, 4 bytes (receive)
- Byte 0: always `0x0`
- Bytes 1-3: per-cell PPO2, 8-bit unsigned, value × 100 (e.g. `0x84` = 1.32 bar)
- `0xFF` = sensor failure; all three `0xFF` = "Needs Cal"

Matches the design summary's existing PPO2 handling exactly — this just
confirms the byte offsets (cells are bytes 1-3, not 0-2; byte 0 is a fixed
zero pad).

### Cell Status — `0xDCA0004`, 2 bytes (receive)
- Byte 0: 3-bit status mask — bit0=cell1, bit1=cell2, bit2=cell3;
  `1`=trusted/voted-in, `0`=excluded/outlier (shown yellow). `0b111` = all
  nominal.
- Byte 1: consensus PPO2, same ×100 encoding as PPO2. **This is the value
  the deco engine should consume on loop — not a client-side average of
  the three cells** (which is what the current firmware/mockup does as a
  stand-in, clearly flagged as such).

### Setpoint — `0xDC90000`, 1 byte (send, handset → head)
- Byte 0: target setpoint, 8-bit unsigned ×100 PPO2

### Millivolts — `0xD110004`, 7 bytes (receive, optional/nice-to-have)
- Bytes 0-1, 2-3, 4-5: per-cell millivolts, big-endian 16-bit, ×100
- Byte 6: always `0x0`
- Design summary already says not to implement consumption of this —
  included here only for completeness / the future Sense screen.

### Status — `0xDCB0004`, 8 bytes (receive)
- Byte 0: battery voltage
- Bytes 1-2: solenoid current
- Bytes 3-4: injection duration
- Byte 5: setpoint PPO2 (echo)
- Byte 6: consensus
- Byte 7: error code

### Ambient Pressure — `0xD080000` (NOT sent by this handset, receive-only reference)
- Bytes 0-1: surface ambient pressure, big-endian int16, millibar
- Bytes 2-3: current ambient pressure, big-endian int16, millibar
- Byte 5: solenoid depth-compensation flag (1 = enabled)

Design summary explicitly excludes sending this (no depth compensation on
the head by design) — documented here only so the format is on record if
that decision is ever revisited.

### Tank Pressure — `0xD0B0004`, receive (nice-to-have, bailout gas planning)
- Byte 0: cylinder designator (`0x00`=O2, `0x10`=Diluent)
- Bytes 1-2: pressure, decibar (e.g. `0x0203` = 51.5 bar, displayed as 52 bar)

### Calibration handshake — `0xD130201` init / `0xD120004` response
1. Handset → `0xD130201` (3 bytes): byte0 = cal gas FO2 (`0x64`=100%), bytes1-2 = atmospheric pressure, big-endian int16 mbar
2. Head → `0xD120004` (8 bytes), status `0x05` = ack, triggers calibration screen
3. Millivolt updates continue over standard messaging; ~1s minimum delay required
4. Head → `0xD120004` again, status `0x01` = success, containing: byte0=status, bytes1-3=per-cell mV, byte4=FO2 result, bytes5-6=pressure result (big-endian), byte7=reserved (`0x07`)

### Device ID / Name / Serial (receive, for a future bus-devices/diagnostics screen)
- Device ID — `0xD000001`..`0xD000004`-style (last hex digit = device ID), 3 bytes: byte0=manufacturer ID (`0x1`=Shearwater), byte1=reserved, byte2=firmware version
- Device Name — `0xD010004`, 8 bytes ASCII, no null terminator required
- Serial Number — `0xDD20003`, 8 bytes ASCII

### Bus init — `0xD370401`, 3 bytes
Largely undocumented upstream; noted as having a firmware quirk affecting
byte values. Not needed for v1.

### Menu system — `0x30` (handset TX `0xd0a0401` / head TX `0xd0a0104`)
Partially reverse-engineered dialogue for remote menu configuration
(INIT handshake, field name/flags/text exchange, save/pause state). Complex
and incomplete upstream — **not in scope**, noted only so it isn't
mistaken for something simpler later.

## Explicitly out of scope (per the design summary)

- CO2 (`0x20`-`0x23`)
- UDS (`0x0A`)
- Millivolts consumption (`0x11` — received but unused)
- Ambient Pressure send (`0x08` — head runs setpoint blind to depth by design)

## Important caveat from upstream

> There is no relationship between PPO2, millivolts, and the consensus
> value on the handset side... Values are used and displayed verbatim as
> they come over the bus with no form of consistency checking between
> messages.

This matters for the deco engine specifically: it should read the **Cell
Status consensus byte**, not re-derive its own average — the current
firmware port does the latter only because there's no bus yet to source
the real value from.
