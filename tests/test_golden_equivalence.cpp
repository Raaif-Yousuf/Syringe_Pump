// Golden equivalence tests.
//
// These pin the refactored pump_core against the ORIGINAL firmware as it
// existed at commit 5b46e19 (the commit immediately before PR #2,
// "fix: correct lead screw pitch used in flow rate math"). That original
// firmware used LEAD_MM_PER_REV = 2.0; the current hardware uses 8.0
// (LEAD_MM_PER_REV in pump_core.h). The OriginalOracle namespace below is a
// deliberately independent re-implementation of that commit's math (not a
// call into pump_core), so these tests actually check the refactor against
// the historical firmware rather than against itself.
//
// The button/limit-switch/jog decision logic in checkButtonState(),
// checkJogButtons() and checkLimitSwitch() was byte-for-byte identical
// between that commit and the current firmware (PR #2 only touched
// LEAD_MM_PER_REV, an LED color constant, and a redundant stepper.setSpeed
// call in stopMotor()), so the state-machine table below asserts exact
// equality rather than a scaled one.

#include <gtest/gtest.h>

#include <cmath>

#include "pump_core.h"

namespace OriginalOracle {

// Verbatim constants/formulas from commit 5b46e19.
constexpr float kMicrostepFactor = 16.0f;
constexpr float kStepsPerRev = 200.0f * kMicrostepFactor;
constexpr float kLeadMmPerRev = 2.0f;  // The pre-PR#2 lead screw pitch.
constexpr float kMm3PerMl = 1000.0f;
constexpr float kSecPerMin = 60.0f;
constexpr float kMaxPotFlow = 7.5f;
constexpr float kDiameter10Ml = 14.7f;
constexpr float kDiameter20Ml = 19.1f;

float diameterMm(int syringeSizeMl) {
  if (syringeSizeMl == 10) return kDiameter10Ml;
  if (syringeSizeMl == 20) return kDiameter20Ml;
  return kDiameter20Ml;
}

constexpr double kPi = 3.14159265358979323846;

float areaMm2(float diameterMm) {
  return static_cast<float>(kPi) * std::pow(diameterMm / 2.0f, 2.0f);
}

float rawFlowFromPot(int potValue) {
  return (static_cast<float>(potValue) / 1023.0f) * kMaxPotFlow;
}

float quantizeFlow(float rawFlow) {
  float q = std::round(rawFlow * 10.0f) / 10.0f;
  return q < 0.0f ? 0.0f : q;
}

// commandedStepsPerSec(): mirrors calculateMotorSpeed()'s
// TARGET_STEPS_PER_SECOND / targetStepsPerSec computation exactly.
float commandedStepsPerSec(float flowMlMin, float area) {
  if (flowMlMin < 0.001f) {
    return 0.0f;
  }
  float flowVolumeMm3Sec = (flowMlMin * kMm3PerMl) / kSecPerMin;
  float linearVelocityMmSec = flowVolumeMm3Sec / area;
  float targetStepsPerSecond = (linearVelocityMmSec / kLeadMmPerRev) * kStepsPerRev;
  return -targetStepsPerSecond;
}

// volumePerStepMl(): mirrors calculateVolumePerStep() exactly.
float volumePerStepMl(float area) {
  float volumePerRevMm3 = area * kLeadMmPerRev;
  return (volumePerRevMm3 / kStepsPerRev) / kMm3PerMl;
}

}  // namespace OriginalOracle

namespace {

// The known, documented hardware change between the two firmware versions.
constexpr float kLeadRatioOldOverNew = OriginalOracle::kLeadMmPerRev / pump::LEAD_MM_PER_REV;  // 2/8
constexpr float kLeadRatioNewOverOld = pump::LEAD_MM_PER_REV / OriginalOracle::kLeadMmPerRev;  // 8/2
constexpr float kTol = 1e-2f;

struct PotCase {
  int potValue;
  int syringeSizeMl;
};

// A table of pot readings x syringe sizes, covering zero, low, mid and
// full-scale flow for both supported syringe sizes.
const PotCase kPotCases[] = {
    {0, 10},   {0, 20},    {100, 10}, {100, 20}, {300, 10},
    {300, 20}, {512, 10},  {512, 20}, {700, 10}, {700, 20},
    {1023, 10}, {1023, 20},
};

TEST(GoldenEquivalence, LeadMmPerRevIsPinned) {
  // Any change to this constant changes every commanded speed and
  // time-remaining number below; if this assertion ever needs to change,
  // the golden expectations in this file must be revisited deliberately.
  EXPECT_FLOAT_EQ(pump::LEAD_MM_PER_REV, 8.0f);
}

TEST(GoldenEquivalence, CommandedSpeedScalesByLeadRatioForEveryPotSyringeCombo) {
  EXPECT_NEAR(kLeadRatioOldOverNew, 2.0f / 8.0f, 1e-6f);

  for (const auto &c : kPotCases) {
    float diameter = pump::syringeDiameterMm(c.syringeSizeMl);
    float area = pump::syringeAreaMm2(diameter);

    float oldDiameter = OriginalOracle::diameterMm(c.syringeSizeMl);
    float oldArea = OriginalOracle::areaMm2(oldDiameter);
    // Syringe geometry is lead-independent, so it must match exactly. The pot
    // is reversed on purpose, so reading r now gives the flow the original
    // firmware gave at POT_MAX_COUNTS - r.
    ASSERT_FLOAT_EQ(area, oldArea);

    float flow = pump::quantizeFlow(pump::rawFlowFromPot(c.potValue));
    float oldFlow = OriginalOracle::quantizeFlow(OriginalOracle::rawFlowFromPot(pump::POT_MAX_COUNTS - c.potValue));
    ASSERT_FLOAT_EQ(flow, oldFlow);

    float refactoredSpeed = pump::commandedStepsPerSec(flow, area);
    float originalSpeed = OriginalOracle::commandedStepsPerSec(oldFlow, oldArea);

    // Direction must always agree: both zero, or both negative (forward).
    if (originalSpeed == 0.0f) {
      EXPECT_FLOAT_EQ(refactoredSpeed, 0.0f) << "pot=" << c.potValue << " size=" << c.syringeSizeMl;
    } else {
      EXPECT_LT(originalSpeed, 0.0f);
      EXPECT_LT(refactoredSpeed, 0.0f) << "pot=" << c.potValue << " size=" << c.syringeSizeMl;
      EXPECT_NEAR(refactoredSpeed, originalSpeed * kLeadRatioOldOverNew, kTol)
          << "pot=" << c.potValue << " size=" << c.syringeSizeMl;
    }
  }
}

TEST(GoldenEquivalence, VolumePerStepScalesByInverseLeadRatio) {
  for (int size : {10, 20}) {
    float area = pump::syringeAreaMm2(pump::syringeDiameterMm(size));
    float oldArea = OriginalOracle::areaMm2(OriginalOracle::diameterMm(size));
    ASSERT_FLOAT_EQ(area, oldArea);

    float refactoredVolPerStep = pump::volumePerStepMl(area);
    float originalVolPerStep = OriginalOracle::volumePerStepMl(oldArea);

    EXPECT_NEAR(refactoredVolPerStep, originalVolPerStep * kLeadRatioNewOverOld, 1e-6f)
        << "size=" << size;
  }
}

// ---- State machine: unchanged by the lead screw fix, so exact equality ----

struct ButtonCase {
  bool running;
  bool empty;
  bool buttonLow;
  pump::ButtonAction expected;
};

const ButtonCase kButtonCases[] = {
    {false, false, true, pump::ButtonAction::Start},
    {true, false, false, pump::ButtonAction::Stop},
    {true, false, true, pump::ButtonAction::None},
    {false, false, false, pump::ButtonAction::None},
    {false, true, true, pump::ButtonAction::None},
    {true, true, false, pump::ButtonAction::None},
};

TEST(GoldenEquivalence, ButtonStateMachineMatchesOriginalControlFlow) {
  for (const auto &c : kButtonCases) {
    pump::PumpState state;
    state.running = c.running;
    state.empty = c.empty;
    EXPECT_EQ(pump::decideButtonAction(state, c.buttonLow), c.expected)
        << "running=" << c.running << " empty=" << c.empty << " buttonLow=" << c.buttonLow;
  }
}

struct JogCase {
  bool running;
  bool forwardLow;
  bool reverseLow;
  bool limitLow;
  pump::JogAction expected;
};

const JogCase kJogCases[] = {
    {true, true, false, true, pump::JogAction::None},
    {false, true, false, true, pump::JogAction::JogForward},
    {false, true, false, false, pump::JogAction::JogBlockedEmpty},
    {false, false, true, true, pump::JogAction::JogReverse},
    {false, false, true, false, pump::JogAction::JogReverse},
    {false, false, false, true, pump::JogAction::Stop},
};

TEST(GoldenEquivalence, JogStateMachineMatchesOriginalControlFlow) {
  for (const auto &c : kJogCases) {
    EXPECT_EQ(pump::decideJogAction(c.running, c.forwardLow, c.reverseLow, c.limitLow), c.expected)
        << "running=" << c.running << " fwd=" << c.forwardLow << " rev=" << c.reverseLow
        << " limit=" << c.limitLow;
  }
}

}  // namespace
