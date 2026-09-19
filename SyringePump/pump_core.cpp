#include "pump_core.h"

// The Arduino AVR toolchain does not ship the C++-style standard library
// headers (<cmath>, <cstdio>, <cstdlib>), only avr-libc's C headers. Using
// those (and the global-namespace functions they declare) keeps this file
// buildable both on the AVR toolchain and on a host compiler.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

namespace pump {

float syringeDiameterMm(int syringeSizeMl) {
  if (syringeSizeMl == 10) {
    return DIAMETER_10ML;
  }
  if (syringeSizeMl == 20) {
    return DIAMETER_20ML;
  }
  return DIAMETER_20ML;  // Default, matches the original firmware.
}

float syringeAreaMm2(float diameterMm) {
  return static_cast<float>(PI) * pow(diameterMm / 2.0f, 2.0f);
}

float rawFlowFromPot(int potValue, float maxFlowMlMin, int potMaxCounts) {
  int counts = POT_REVERSED ? potMaxCounts - potValue : potValue;
  return (static_cast<float>(counts) / static_cast<float>(potMaxCounts)) * maxFlowMlMin;
}

float quantizeFlow(float rawFlow) {
  float quantized = round(rawFlow * 10.0f) / 10.0f;
  if (quantized < 0.0f) {
    quantized = 0.0f;
  }
  return quantized;
}

float commandedStepsPerSec(float flowMlMin, float areaMm2) {
  if (flowMlMin < 0.001f) {
    return 0.0f;
  }

  float flowVolumeMm3Sec = (flowMlMin * MILLIMETER_CUBE_PER_ML) / SECONDS_PER_MINUTE;
  float linearVelocityMmSec = flowVolumeMm3Sec / areaMm2;
  float targetStepsPerSecond = (linearVelocityMmSec / LEAD_MM_PER_REV) * STEPS_PER_REV;

  return -targetStepsPerSecond;
}

float volumePerStepMl(float areaMm2) {
  float volumePerRevMm3 = areaMm2 * LEAD_MM_PER_REV;
  return (volumePerRevMm3 / STEPS_PER_REV) / MILLIMETER_CUBE_PER_ML;
}

TimeRemaining computeTimeRemaining(float flowMlMin, long currentPositionSteps,
                                    float totalVolumeMl, float areaMm2,
                                    bool pumpEmptyFlag) {
  long positionSteps = labs(currentPositionSteps);
  float volumePerStep = volumePerStepMl(areaMm2);
  float volumeDispensedMl = static_cast<float>(positionSteps) * volumePerStep;
  float volumeRemainingMl = totalVolumeMl - volumeDispensedMl;

  float timeRemainingSec;
  if (flowMlMin > 0.001f && volumeRemainingMl > 0.0f) {
    timeRemainingSec = (volumeRemainingMl / flowMlMin) * SECONDS_PER_MINUTE;
  } else {
    timeRemainingSec = 0.0f;
  }

  int totalSeconds = static_cast<int>(timeRemainingSec);
  int minutes = totalSeconds / static_cast<int>(SECONDS_PER_MINUTE);
  int seconds = totalSeconds % static_cast<int>(SECONDS_PER_MINUTE);

  TimeRemaining result;
  result.volumeRemainingMl = volumeRemainingMl;
  result.minutes = minutes;
  result.seconds = seconds;
  result.empty = pumpEmptyFlag || volumeRemainingMl <= 0.0f;
  return result;
}

ButtonAction decideButtonAction(const PumpState &state, bool buttonReadingLow) {
  if (state.empty) {
    return ButtonAction::None;
  }

  if (buttonReadingLow && !state.running) {
    return ButtonAction::Start;
  }
  if (!buttonReadingLow && state.running) {
    return ButtonAction::Stop;
  }
  return ButtonAction::None;
}

JogAction decideJogAction(bool pumpRunning, bool jogForwardLow, bool jogReverseLow,
                           bool limitSwitchLow) {
  if (pumpRunning) {
    return JogAction::None;
  }

  if (jogForwardLow) {
    return limitSwitchLow ? JogAction::JogForward : JogAction::JogBlockedEmpty;
  }
  if (jogReverseLow) {
    return JogAction::JogReverse;
  }
  return JogAction::Stop;
}

}  // namespace pump

// ========================== PotFilter ==========================

PotFilter::PotFilter(float alpha, int deadbandCounts)
    : alpha_(alpha), deadbandCounts_(deadbandCounts), emaValue_(0.0f), lastOutput_(0), initialized_(false) {}

int PotFilter::update(int rawAdc) {
  if (!initialized_) {
    emaValue_ = static_cast<float>(rawAdc);
    lastOutput_ = rawAdc;
    initialized_ = true;
    return lastOutput_;
  }

  emaValue_ = alpha_ * static_cast<float>(rawAdc) + (1.0f - alpha_) * emaValue_;
  int rounded = static_cast<int>(emaValue_ + (emaValue_ >= 0.0f ? 0.5f : -0.5f));

  if (abs(rounded - lastOutput_) >= deadbandCounts_) {
    lastOutput_ = rounded;
  }
  return lastOutput_;
}

void PotFilter::reset() {
  initialized_ = false;
  emaValue_ = 0.0f;
  lastOutput_ = 0;
}

// ========================== PumpRuntime ==========================

PumpRuntime::PumpRuntime(StepperType &stepper, LcdType &lcd, const Pins &pins, int syringeSizeMl)
    : stepper_(stepper),
      lcd_(lcd),
      pins_(pins),
      syringeSizeMl_(syringeSizeMl),
      totalSyringeVolumeMl_(syringeSizeMl == 10 ? 10.0f : 20.0f),
      syringeAreaMm2_(pump::syringeAreaMm2(pump::syringeDiameterMm(syringeSizeMl))) {}

void PumpRuntime::begin() {
  pinMode(pins_.ledGreenPin, OUTPUT);
  pinMode(pins_.ledBluePin, OUTPUT);
  pinMode(pins_.ledRedPin, OUTPUT);

  pinMode(pins_.buttonPin, INPUT_PULLUP);
  pinMode(pins_.limitSwitchPin, INPUT_PULLUP);
  pinMode(pins_.jogForwardPin, INPUT_PULLUP);
  pinMode(pins_.jogReversePin, INPUT_PULLUP);

  lcd_.init();
  lcd_.backlight();
  lcd_.clear();

  lcd_.print("Syringe Pump V2.0");
  lcd_.setCursor(0, 1);
  lcd_.print("Reading Pot...");

  readFlowRatePot();
  calculateMotorSpeed();

  stepper_.setMaxSpeed(1000.0f);
  stepper_.setAcceleration(500.0f);

  stepper_.setCurrentPosition(0);

  stopMotor();
}

void PumpRuntime::loop() {
  readFlowRatePot();

  if (state_.running) {
    calculateMotorSpeed();

    if (stepper_.speed() != targetStepsPerSec_) {
      stepper_.setSpeed(targetStepsPerSec_);
    }
  }

  checkLimitSwitch();
  checkButtonState();
  checkJogButtons();

  stepper_.runSpeed();

  if (millis() - lastDisplayUpdateTime_ >= static_cast<unsigned long>(kDisplayUpdateIntervalMs)) {
    refreshDisplay();
    lastDisplayUpdateTime_ = millis();
  }
}

void PumpRuntime::readFlowRatePot() {
  int potValueRaw = analogRead(pins_.potPin);
  int potValueFiltered = potFilter_.update(potValueRaw);
  float raw = pump::rawFlowFromPot(potValueFiltered);
  commandedFlowRateMlMin_ = pump::quantizeFlow(raw);
}

void PumpRuntime::calculateMotorSpeed() {
  targetStepsPerSec_ = pump::commandedStepsPerSec(commandedFlowRateMlMin_, syringeAreaMm2_);
}

void PumpRuntime::checkLimitSwitch() {
  int limitSwitchState = digitalRead(pins_.limitSwitchPin);
  if (limitSwitchState == HIGH) {
    stopMotorEmpty();
  }
}

void PumpRuntime::checkButtonState() {
  bool buttonLow = digitalRead(pins_.buttonPin) == LOW;
  pump::ButtonAction action = pump::decideButtonAction(state_, buttonLow);

  if (action == pump::ButtonAction::Start) {
    calculateMotorSpeed();
    startMotor(targetStepsPerSec_);
  } else if (action == pump::ButtonAction::Stop) {
    stopMotor();
  }
}

void PumpRuntime::checkJogButtons() {
  bool jogForwardLow = digitalRead(pins_.jogForwardPin) == LOW;
  bool jogReverseLow = digitalRead(pins_.jogReversePin) == LOW;
  bool limitSwitchLow = digitalRead(pins_.limitSwitchPin) == LOW;

  pump::JogAction action = pump::decideJogAction(state_.running, jogForwardLow, jogReverseLow, limitSwitchLow);

  switch (action) {
    case pump::JogAction::None:
      break;
    case pump::JogAction::JogForward:
      applyLed(pump::LedColor::Yellow);
      stepper_.setSpeed(pump::JOG_SPEED_STEPS_PER_SEC);
      break;
    case pump::JogAction::JogBlockedEmpty:
      stopMotorEmpty();
      break;
    case pump::JogAction::JogReverse:
      stepper_.setCurrentPosition(0);
      if (state_.empty) {
        state_.empty = false;
      }
      applyLed(pump::LedColor::Yellow);
      stepper_.setSpeed(-pump::JOG_SPEED_STEPS_PER_SEC);
      break;
    case pump::JogAction::Stop:
      stopMotor();
      break;
  }
}

void PumpRuntime::startMotor(float stepsPerSec) {
  applyLed(pump::LedColor::Green);
  stepper_.setSpeed(stepsPerSec);
  state_.running = true;
}

void PumpRuntime::stopMotor() {
  if (!state_.empty) {
    applyLed(pump::LedColor::Yellow);
  }
  state_.running = false;
}

void PumpRuntime::stopMotorEmpty() {
  applyLed(pump::LedColor::Red);
  stepper_.setSpeed(0.0f);
  state_.running = false;
  state_.empty = true;

  lcd_.setCursor(0, 1);
  lcd_.print("Status: EMPTY!  ");
}

void PumpRuntime::applyLed(pump::LedColor color) {
  switch (color) {
    case pump::LedColor::Green:
      digitalWrite(pins_.ledGreenPin, HIGH);
      digitalWrite(pins_.ledBluePin, LOW);
      digitalWrite(pins_.ledRedPin, LOW);
      break;
    case pump::LedColor::Yellow:
      digitalWrite(pins_.ledGreenPin, HIGH);
      digitalWrite(pins_.ledBluePin, LOW);
      digitalWrite(pins_.ledRedPin, HIGH);
      break;
    case pump::LedColor::Red:
      digitalWrite(pins_.ledGreenPin, LOW);
      digitalWrite(pins_.ledBluePin, LOW);
      digitalWrite(pins_.ledRedPin, HIGH);
      break;
  }
}

void PumpRuntime::refreshDisplay() {
  pump::TimeRemaining tr = pump::computeTimeRemaining(commandedFlowRateMlMin_, stepper_.currentPosition(),
                                                        totalSyringeVolumeMl_, syringeAreaMm2_, state_.empty);

  lcd_.setCursor(0, 0);
  lcd_.print("Flow:");
  lcd_.print(commandedFlowRateMlMin_, 1);
  lcd_.print("mL/min  ");

  lcd_.setCursor(0, 1);
  if (tr.empty) {
    lcd_.print("Status: EMPTY!  ");
  } else {
    char timeBuffer[25];
    snprintf(timeBuffer, sizeof(timeBuffer), "Time: %dm %02ds   ", tr.minutes, tr.seconds);
    lcd_.print(timeBuffer);
  }
}
