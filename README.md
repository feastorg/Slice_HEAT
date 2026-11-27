
# DC heating Element Controller Slice

DC heating Element Controller Slice for BREAD.

## Introduction

Two-channel controller for resistive DC heating elements, with a
MAX31855K thermocouple input per channel. Each channel has a high-side
P-channel MOSFET (AOD4185) that is on only while the bus E-stop and the
Nano's enable are both high, and a low-side N-channel MOSFET (AOD4184A)
switched by the Nano's PWM. A B340A freewheel diode sits across each
output.

## Ratings

- Heater supply: 12 V only, on its own terminal block J6 (VHTR+ and
  VHTR- = PGND), not from the bus. Fit an external 8 A slow-blow fuse
  in the + lead.
- Up to 3.5 A per channel, 7 A for both together.
- Resistive heaters: up to 3.5 A per channel. An inductive load (solenoid,
  fan, pump) relies on the on-board freewheel diodes D3/D4 alone, and they
  are B340A parts rated 3 A average: keep inductive loads to 3 A per
  channel.
- PGND meets the logic ground at one point on the board (NT1).
- FET heat: the drain tabs of Q2-Q4 sit on 1.74-1.93 cm2 of 1 oz
  copper each (F.Cu and B.Cu, joined by thermal vias; Q1 4.95 cm2), less
  than the 1 in2 of 2 oz behind the datasheets' RθJA. At 3.5 A and
  100 % duty, with RDS(on) at TJ 125 C, the AOD4184A low side dissipates
  0.20 W and the AOD4185 high side 0.38 W, so TJ at 50 C ambient is at
  most about 70 C and 88 C. These are estimates: RθJA is assumed to be
  100 C/W, not measured. Bench check before relying on 3.5 A: run
  both channels at 3.5 A for 30 minutes with a thermocouple on each FET
  tab (Q1-Q4).

## Connections

Wires enter all four terminal blocks from the right-hand board edge;
each position is marked on the silkscreen.

| Terminal | Pin | Signal | Use |
|---|---|---|---|
| J4 | 2 / 1 | HTR1+ / HTR1- | heater 1 |
| J5 | 2 / 1 | HTR2+ / HTR2- | heater 2 |
| J6 | 2 / 1 | VHTR+ / VHTR- (PGND) | 12 V heater supply in |
| J3 | 5 / 6 | TC1+ / TC1- | type K thermocouple, channel 1 (U2) |
| J3 | 7 / 8 | TC2+ / TC2- | type K thermocouple, channel 2 (U3) |
| J3 | 1 / 2, 3 / 4 | TMSTR1 (TH1) / GND, TMSTR2 (TH2) / GND | thermistor inputs (R1/R2 pull-ups to +5V) |

J4-J6 are Phoenix Contact SPT 2,5/ 2-H-5,0 (1990973): 2-position
push-in, 5 mm pitch, 24 A, 0.2-4 mm2 rigid or 0.2-2.5 mm2 flexible
wire, 10 mm strip length. J3 is a Phoenix Contact PT 1,5/ 8-3,5-H
(8-position push-in, 3.5 mm pitch).

E-stop: the bus E_STOP line (J2.6) must be high for the heaters to run.
Unwired, a 1 M pull-down holds it low and the heaters stay off.
JP1 (BENCH, open by default) pulls the line high through R18 (10 k) for use off
the loaf; leave it open in a loaf.

## Warnings

- Thermocouples: use insulated (sheathed or ungrounded-junction) probes.
  A junction touching a live heater conductor puts up to 12 V on T+/T-,
  far past the MAX31855's absolute maximum of VCC + 0.3 V (3.6 V on its
  3.3 V supply), and can destroy it.
- Heater supply polarity: J6 pin 2 is + (VHTR+), pin 1 is - (VHTR-).
  Reversed, the TVS D2 and
  the MOSFET body diodes with the freewheel diodes conduct straight
  across the supply until the external fuse opens, and C7 (polarised)
  is reverse biased. There is no reverse-polarity protection on the
  board: check the polarity before connecting the supply.

## Pin map (Arduino Nano, A1)

| Nano pin | Signal | Function |
|---|---|---|
| D11 | PWM1 | heater 1 low-side gate |
| D10 | PWM2 | heater 2 low-side gate |
| D12 | HTR_EN | high = high-side switches may turn on |
| D2 | ESTOP_SNS | buffered E-stop, input (high = run) |
| D9 | CLK | MAX31855 clock (software SPI, 3.3 V shifted) |
| D7 | DATA | MAX31855 SO |
| D8 | SS1 | thermocouple 1 chip select |
| D6 | SS2 | thermocouple 2 chip select |
| D5 | LED | WS2812B status LED |
| A0 / A1 | TMSTR1 / TMSTR2 | thermistor inputs (unused by the sketch) |
| A4 / A5 | I2C | bus I2C (slice address 6) |

## Usage

> **Draft firmware.** `firmware/HEAT.ino` is a minimal first sketch: it
> compiles but has not run on hardware, and it needs review and revision.
> Still to come: per-channel I2C reads (a read now returns channel 1
> only, and an I2C write sets both channels), a command watchdog, and the
> port to bread-slice-core.

Build `firmware/HEAT.ino` for an Arduino Nano (`arduino:avr:nano`). Over
serial at 115200 baud send `<ch>,<target C>,<ramp C/min>` (ch 1, 2 or 0
for both; ramp 0 steps straight to the target). The sketch prints
setpoint, temperature and duty for both channels once a second. It drops
HTR_EN and both PWMs on a thermocouple fault or above 100 C; that fault
holds until the next command.

The E-stop latches. E_STOP low (polled every 250 ms, and caught between
polls by a falling edge on D2/INT0) turns both heaters off and sets both
targets to 0, and the sketch ignores commands while latched. Releasing
the E-stop does not restart anything: clear the latch with `C` on serial
or the single byte 0x43 over I2C (it clears only with E_STOP high), or
reset the Nano, then send a new target. The hardware cut through the
high-side P-FETs works without the sketch.

## Documentation

Please refer to the [docs](/docs) for more info.

---

<div align="center">
  <p>
    <img src="https://img.shields.io/badge/Developed_by-UWO--FAST--et--al-orange" alt="Developed by UWO-FAST et al.">
    <img src="https://img.shields.io/badge/Powered_by-Open_Source-blue" alt="Powered by Open Source">
  </p>
</div>
