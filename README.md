# Syringe Pump

![Syringe pump](docs/pump-photo-1.jpg)

I designed and built this syringe pump from scratch: a stepper-driven lead
screw pushes a syringe plunger at a flow rate you dial in on a potentiometer,
with an LCD showing flow rate and time remaining, and limit-switch/LED
interlocks handled entirely in Arduino firmware I wrote. Everything besides
the raw hardware (motor, driver, extrusion, fasteners) is custom: the 3D
printed frame, the wiring harness, and the C++ firmware.

Demo clip: [docs/pump-demo.mp4](docs/pump-demo.mp4)

## How it works

- A NEMA 17 stepper turns a lead screw through a flexible coupling; a carriage
  on the screw pushes the syringe plunger, so the plunger speed (and the
  resulting flow rate) is set directly by the motor's step rate.
- An Arduino Uno computes the required steps/sec from the commanded flow rate
  (mL/min) and the syringe's cross-sectional area, then drives an A4988
  stepper driver at 1/16 microstepping (3200 steps/rev) via the AccelStepper
  library.
- A potentiometer sets the flow rate live in 0.1 mL/min steps; an I2C LCD
  shows the current flow rate and estimated time to empty.
- A limit switch (wired normally-closed) stops the motor and lights a red LED
  when the plunger reaches the end of travel; two jog buttons let you reposition
  the carriage between runs. A tri-color LED shows running/paused/empty state.

## Specs (calculated, not bench-measured)

| Parameter | Value |
|---|---|
| Motor / driver | NEMA 17 + A4988 @ 1/16 microstepping (3200 steps/rev) |
| Lead screw | 250 mm, 8 mm lead |
| Syringe sizes | 10 mL and 20 mL |
| Flow rate range | 0-7.5 mL/min (pot-limited), 0.1 mL/min resolution |
| Control board | Arduino Uno |

## Wiring summary

| Signal | Pin |
|---|---|
| Stepper STEP / DIR | D2 / D3 |
| Start/pause button | D7 |
| Limit switch (NC) | D8 |
| Jog forward / reverse | D4 / D5 |
| Potentiometer | A0 |
| Status LED (G/B/R) | D9 / D10 / D11 |
| LCD | I2C (SDA/SCL), address 0x27 |

Logic runs on 5 V from the Arduino; the motor is powered separately at 24 V
through the A4988. Full parts list: NEMA 17 stepper, A4988 driver, Arduino
Uno, 24 V power supply, 8 mm-lead lead screw with flexible coupling, 8 mm
linear rods + LM8UU bearings, 2020/2040 aluminum extrusion, 16x2 I2C LCD,
10k potentiometer, latching push button, 2 momentary jog buttons, limit
switch, common-cathode RGB LED.

## Build / flash

```
arduino-cli core install arduino:avr
arduino-cli lib install AccelStepper "LiquidCrystal I2C"
arduino-cli compile --fqbn arduino:avr:uno SyringePump
arduino-cli upload -p <PORT> --fqbn arduino:avr:uno SyringePump
```

## Testing

I don't have the hardware anymore, so the pump math and state machine live
in `pump_core.h/.cpp` and are unit tested on a PC with GoogleTest, checked
against the original firmware's numbers. CI runs this alongside `arduino-cli
compile` on every pull request.

```
cmake -S . -B build && cmake --build build && ctest --test-dir build
```
