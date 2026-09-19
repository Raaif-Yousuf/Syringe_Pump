#pragma once

#include "pump_hal.h"

/**
 * @file pump_core.h
 * @brief Pump math, potentiometer smoothing and the control state machine,
 * pulled out of the .ino so it can be unit tested on a PC as well as run on
 * the Arduino.
 */

namespace pump {

// ========================== MOTOR & FLOW CONSTANTS ==========================

/** @brief Microstep multiplier of the stepper driver */
constexpr float MICROSTEP_FACTOR = 16.0f;

/** @brief Total steps per motor revolution (with microstepping) */
constexpr float STEPS_PER_REV = 200.0f * MICROSTEP_FACTOR;

/**
 * @brief Lead screw travel per revolution (mm).
 *
 * This is the single source of truth for the lead screw pitch. It was 2.0 mm
 * before the lead screw hardware change (PR #2 on GitHub); it is 8.0 mm for
 * the current hardware. Changing this value changes every commanded speed
 * and time-remaining estimate, so do not change it without re-measuring the
 * physical lead screw.
 */
constexpr float LEAD_MM_PER_REV = 8.0f;

/** @brief Conversion factor: cubic millimeters per mL */
constexpr float MILLIMETER_CUBE_PER_ML = 1000.0f;

/** @brief Seconds in a minute */
constexpr float SECONDS_PER_MINUTE = 60.0f;

/** @brief Maximum flow rate allowed by potentiometer */
constexpr float MAX_POT_FLOW_ML_MIN = 7.5f;

/** @brief Full-scale ADC reading of the potentiometer input */
constexpr int POT_MAX_COUNTS = 1023;

/**
 * @brief The pot is wired so its ADC reading falls as the knob turns toward
 * "faster". With this set, a reading of 0 maps to max flow and full scale to
 * zero, so turning the knob up speeds the pump up.
 */
constexpr bool POT_REVERSED = true;

/** @brief Manual jogging speed (steps/sec, negative = forward convention) */
constexpr float JOG_SPEED_STEPS_PER_SEC = -1000.0f;

// ========================== POTENTIOMETER SMOOTHING ==========================

/**
 * @brief Exponential-moving-average weight applied to each new raw ADC
 * sample (0 < alpha <= 1; smaller means smoother but slower to respond).
 */
constexpr float POT_EMA_ALPHA = 0.2f;

/**
 * @brief Deadband, in ADC counts, that the smoothed reading must move past
 * before the reported setpoint is allowed to change. Rejects potentiometer
 * wiper/ADC jitter that would otherwise flicker the flow-rate setpoint.
 */
constexpr int POT_DEADBAND_COUNTS = 4;

// ========================== SYRINGE DIMENSIONS ==========================

/** @brief Plunger diameter (mm) for 10 mL syringe */
constexpr float DIAMETER_10ML = 14.7f;

/** @brief Plunger diameter (mm) for 20 mL syringe */
constexpr float DIAMETER_20ML = 19.1f;

// ========================== PURE MATH ==========================

/** @brief Selects the plunger diameter (mm) for a syringe size (10 or 20 mL). */
float syringeDiameterMm(int syringeSizeMl);

/** @brief Plunger cross-sectional area (mm^2) for a given diameter (mm). */
float syringeAreaMm2(float diameterMm);

/** @brief Linear map of a raw ADC reading to a flow rate (mL/min), unquantized. */
float rawFlowFromPot(int potValue, float maxFlowMlMin = MAX_POT_FLOW_ML_MIN,
                      int potMaxCounts = POT_MAX_COUNTS);

/** @brief Rounds a flow rate to the nearest 0.1 mL/min and clamps to >= 0. */
float quantizeFlow(float rawFlow);

/**
 * @brief Commanded motor speed (steps/sec) for a flow rate and syringe area.
 *
 * Matches the sign convention of the original firmware: forward (dispensing)
 * motion is negative.
 */
float commandedStepsPerSec(float flowMlMin, float areaMm2);

/** @brief Volume (mL) dispensed per motor step for a given syringe area. */
float volumePerStepMl(float areaMm2);

/** @brief Result of the time-remaining calculation, decoupled from LCD I/O. */
struct TimeRemaining {
  float volumeRemainingMl;
  int minutes;
  int seconds;
  bool empty;
};

TimeRemaining computeTimeRemaining(float flowMlMin, long currentPositionSteps,
                                    float totalVolumeMl, float areaMm2,
                                    bool pumpEmptyFlag);

// ========================== STATE MACHINE ==========================

enum class LedColor { Green, Yellow, Red };

struct PumpState {
  bool running = false;
  bool empty = false;
};

enum class ButtonAction { None, Start, Stop };

/**
 * @brief Decides what the start/stop button should do given the current
 * state and a raw button reading (buttonReadingLow == pressed, since the
 * button is wired with a pull-up).
 */
ButtonAction decideButtonAction(const PumpState &state, bool buttonReadingLow);

enum class JogAction { None, JogForward, JogBlockedEmpty, JogReverse, Stop };

/**
 * @brief Decides the jog action given the raw jog button and limit-switch
 * readings (all *Low params are true when the corresponding pin reads LOW).
 */
JogAction decideJogAction(bool pumpRunning, bool jogForwardLow, bool jogReverseLow,
                           bool limitSwitchLow);

}  // namespace pump

// ========================== POTENTIOMETER FILTER ==========================

/**
 * @brief Exponential-moving-average smoothing with a deadband, applied to a
 * potentiometer's raw ADC reading.
 *
 * This is an intentional behavior change from the original firmware, which
 * fed analogRead() straight into the flow-rate calculation: a steady input
 * with small jitter now keeps a constant filtered value, and a real step
 * change is picked up within a bounded number of samples. See
 * pump::POT_EMA_ALPHA and pump::POT_DEADBAND_COUNTS.
 */
class PotFilter {
 public:
  explicit PotFilter(float alpha = pump::POT_EMA_ALPHA,
                      int deadbandCounts = pump::POT_DEADBAND_COUNTS);

  /** @brief Feeds one raw ADC sample and returns the filtered ADC value. */
  int update(int rawAdc);

  /** @brief Resets the filter so the next sample is taken as-is. */
  void reset();

 private:
  float alpha_;
  int deadbandCounts_;
  float emaValue_;
  int lastOutput_;
  bool initialized_;
};

// ========================== RUNTIME (hardware-facing) ==========================

/**
 * @brief Ties the pure math and state machine above to real (or mocked)
 * hardware. This is what the .ino sketch instantiates and drives.
 */
class PumpRuntime {
 public:
  struct Pins {
    int stepPin;
    int dirPin;
    int buttonPin;
    int limitSwitchPin;
    int jogForwardPin;
    int jogReversePin;
    int potPin;
    int ledGreenPin;
    int ledBluePin;
    int ledRedPin;
  };

  PumpRuntime(StepperType &stepper, LcdType &lcd, const Pins &pins, int syringeSizeMl);

  /** @brief One-time hardware setup; call from the sketch's setup(). */
  void begin();

  /** @brief One control-loop iteration; call from the sketch's loop(). */
  void loop();

  float commandedFlowRateMlMin() const { return commandedFlowRateMlMin_; }
  bool isRunning() const { return state_.running; }
  bool isEmpty() const { return state_.empty; }
  long currentPositionSteps() const { return stepper_.currentPosition(); }

 private:
  void readFlowRatePot();
  void calculateMotorSpeed();
  void checkLimitSwitch();
  void checkButtonState();
  void checkJogButtons();
  void startMotor(float stepsPerSec);
  void stopMotor();
  void stopMotorEmpty();
  void applyLed(pump::LedColor color);
  void refreshDisplay();

  StepperType &stepper_;
  LcdType &lcd_;
  PotFilter potFilter_;

  Pins pins_;
  int syringeSizeMl_;
  float totalSyringeVolumeMl_;
  float syringeAreaMm2_;

  pump::PumpState state_;
  float commandedFlowRateMlMin_ = 0.0f;
  float targetStepsPerSec_ = 0.0f;

  unsigned long lastDisplayUpdateTime_ = 0;
  static constexpr long kDisplayUpdateIntervalMs = 500;
};
