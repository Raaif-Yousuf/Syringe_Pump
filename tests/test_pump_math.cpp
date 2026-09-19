#include <gtest/gtest.h>

#include "pump_core.h"

namespace {

constexpr float kTol = 1e-3f;

TEST(SyringeDiameter, TenMl) {
  EXPECT_FLOAT_EQ(pump::syringeDiameterMm(10), pump::DIAMETER_10ML);
}

TEST(SyringeDiameter, TwentyMl) {
  EXPECT_FLOAT_EQ(pump::syringeDiameterMm(20), pump::DIAMETER_20ML);
}

TEST(SyringeDiameter, UnknownSizeDefaultsTo20) {
  EXPECT_FLOAT_EQ(pump::syringeDiameterMm(5), pump::DIAMETER_20ML);
}

TEST(SyringeArea, MatchesCircleArea) {
  float area = pump::syringeAreaMm2(20.0f);
  EXPECT_NEAR(area, 314.159265f, 0.01f);
}

TEST(PotToFlow, ZeroReadingIsZeroFlow) {
  EXPECT_FLOAT_EQ(pump::rawFlowFromPot(0), 0.0f);
}

TEST(PotToFlow, FullScaleReadingIsMaxFlow) {
  EXPECT_NEAR(pump::rawFlowFromPot(1023), pump::MAX_POT_FLOW_ML_MIN, kTol);
}

TEST(PotToFlow, MidScaleIsHalfMaxFlow) {
  EXPECT_NEAR(pump::rawFlowFromPot(512), pump::MAX_POT_FLOW_ML_MIN * (512.0f / 1023.0f), kTol);
}

TEST(QuantizeFlow, RoundsToNearestTenth) {
  EXPECT_FLOAT_EQ(pump::quantizeFlow(1.23f), 1.2f);
  EXPECT_FLOAT_EQ(pump::quantizeFlow(1.26f), 1.3f);
}

TEST(QuantizeFlow, ClampsNegativeToZero) {
  EXPECT_FLOAT_EQ(pump::quantizeFlow(-0.5f), 0.0f);
}

TEST(CommandedSpeed, ZeroFlowGivesZeroSpeed) {
  float area = pump::syringeAreaMm2(pump::DIAMETER_20ML);
  EXPECT_FLOAT_EQ(pump::commandedStepsPerSec(0.0f, area), 0.0f);
}

TEST(CommandedSpeed, PositiveFlowGivesNegativeSpeed) {
  float area = pump::syringeAreaMm2(pump::DIAMETER_20ML);
  float speed = pump::commandedStepsPerSec(3.0f, area);
  EXPECT_LT(speed, 0.0f);
}

TEST(CommandedSpeed, HigherFlowMeansFasterMagnitude) {
  float area = pump::syringeAreaMm2(pump::DIAMETER_20ML);
  float slow = pump::commandedStepsPerSec(1.0f, area);
  float fast = pump::commandedStepsPerSec(5.0f, area);
  EXPECT_GT(-fast, -slow);
}

TEST(VolumePerStep, ProportionalToArea) {
  float smallArea = pump::syringeAreaMm2(pump::DIAMETER_10ML);
  float bigArea = pump::syringeAreaMm2(pump::DIAMETER_20ML);
  EXPECT_GT(pump::volumePerStepMl(bigArea), pump::volumePerStepMl(smallArea));
}

TEST(TimeRemaining, FullSyringeAtZeroFlowReportsNotEmpty) {
  float area = pump::syringeAreaMm2(pump::DIAMETER_20ML);
  pump::TimeRemaining tr = pump::computeTimeRemaining(0.0f, 0, 20.0f, area, false);
  EXPECT_FALSE(tr.empty);
  EXPECT_NEAR(tr.volumeRemainingMl, 20.0f, kTol);
  EXPECT_EQ(tr.minutes, 0);
  EXPECT_EQ(tr.seconds, 0);
}

TEST(TimeRemaining, PumpEmptyFlagForcesEmptyReport) {
  float area = pump::syringeAreaMm2(pump::DIAMETER_20ML);
  pump::TimeRemaining tr = pump::computeTimeRemaining(2.0f, 0, 20.0f, area, true);
  EXPECT_TRUE(tr.empty);
}

TEST(TimeRemaining, VolumeExhaustedReportsEmptyEvenIfFlagIsFalse) {
  float area = pump::syringeAreaMm2(pump::DIAMETER_20ML);
  // Enough steps to fully dispense a 20 mL syringe.
  float volumePerStep = pump::volumePerStepMl(area);
  long stepsToEmpty = static_cast<long>(20.0f / volumePerStep) + 1;
  pump::TimeRemaining tr = pump::computeTimeRemaining(2.0f, stepsToEmpty, 20.0f, area, false);
  EXPECT_TRUE(tr.empty);
}

TEST(TimeRemaining, HigherFlowMeansLessTimeRemaining) {
  float area = pump::syringeAreaMm2(pump::DIAMETER_20ML);
  pump::TimeRemaining slow = pump::computeTimeRemaining(1.0f, 0, 20.0f, area, false);
  pump::TimeRemaining fast = pump::computeTimeRemaining(5.0f, 0, 20.0f, area, false);
  int slowTotalSec = slow.minutes * 60 + slow.seconds;
  int fastTotalSec = fast.minutes * 60 + fast.seconds;
  EXPECT_GT(slowTotalSec, fastTotalSec);
}

}  // namespace
