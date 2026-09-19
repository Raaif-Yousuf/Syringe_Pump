#include <AccelStepper.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "pump_core.h"

/**
 * @file SyringePump.ino
 * @brief Arduino firmware for a syringe pump with potentiometer-controlled flow rate,
 * LCD feedback, limit switch protection, and manual jogging.
 *
 * The flow rate / step-rate math, potentiometer smoothing and the pump's
 * control state machine live in pump_core.h/.cpp so they can also be
 * compiled and unit tested on a PC. This file only wires that logic up to
 * the real hardware.
 */

// ========================== PIN DEFINITIONS ==========================

/** @brief Step pulse output pin for stepper driver */
const int STEP_PIN = 2;

/** @brief Direction control pin for stepper driver */
const int DIR_PIN = 3;

/** @brief Start/Stop button input pin */
const int BUTTON_PIN = 7;

/** @brief Limit switch input pin (detects syringe empty) */
const int LIMIT_SWITCH_PIN = 8;

/** @brief Manual jog forward button pin */
const int BUTTON_JOG_FORWARD_PIN = 4;

/** @brief Manual jog reverse button pin */
const int BUTTON_JOG_REVERSE_PIN = 5;

/** @brief Potentiometer analog input pin (flow rate control) */
const int POT_PIN = A0;

/** @brief Green status LED pin (running) */
const int LED_GREEN_PIN = 9;

/** @brief Blue LED pin (unused currently) */
const int LED_BLUE_PIN = 10;

/** @brief Red status LED pin (empty/error) */
const int LED_RED_PIN = 11;

// ========================== USER SETTINGS ==========================

/** @brief Selected syringe size (10 or 20 mL) */
const int SYRINGE_SIZE_ML = 20;

// ========================== HARDWARE & RUNTIME ==========================

/** @brief 16x2 I2C LCD instance at address 0x27 */
LiquidCrystal_I2C lcd(0x27, 16, 2);

/** @brief Stepper driver object */
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

/** @brief Pump control logic, driving the hardware above */
PumpRuntime pumpRuntime(stepper, lcd,
                  {STEP_PIN, DIR_PIN, BUTTON_PIN, LIMIT_SWITCH_PIN, BUTTON_JOG_FORWARD_PIN,
                   BUTTON_JOG_REVERSE_PIN, POT_PIN, LED_GREEN_PIN, LED_BLUE_PIN, LED_RED_PIN},
                  SYRINGE_SIZE_ML);

// ========================== TIMING VARIABLES ==========================

/** @brief Last serial print timestamp */
unsigned long lastSerialUpdateTime = 0;

/** @brief Serial refresh interval (ms) */
const long serialUpdateInterval = 200;

// ========================== SETUP ==========================

/**
 * @brief Arduino setup routine
 */
void setup() {
  Serial.begin(9600);
  Serial.println("Starting Pump with Potentiometer Control...");

  pumpRuntime.begin();
}

// ========================== MAIN LOOP ==========================

/**
 * @brief Main program loop
 */
void loop() {
  pumpRuntime.loop();

  // Serial debug output
  if (millis() - lastSerialUpdateTime >= serialUpdateInterval) {
    Serial.print("Flow Rate: ");
    Serial.print(pumpRuntime.commandedFlowRateMlMin(), 1);
    Serial.println(" mL/min");
    lastSerialUpdateTime = millis();
  }
}
