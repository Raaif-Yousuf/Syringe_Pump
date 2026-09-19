// Tests for the potentiometer smoothing filter (deadband + EMA). This is an
// intentional behavior change from the original firmware, which fed
// analogRead() straight into the flow-rate calculation with no smoothing.

#include <gtest/gtest.h>

#include <cstdlib>

#include "pump_core.h"

namespace {

TEST(PotFilter, FirstSampleIsPassedThroughUnfiltered) {
  PotFilter filter;
  EXPECT_EQ(filter.update(512), 512);
}

TEST(PotFilter, SteadyInputWithJitterKeepsSetpointConstant) {
  PotFilter filter;
  const int base = 512;
  const int jitter[] = {2, -3, 1, -2, 3, -1, 0, 2, -3, 1, -1, 3, -2, 2, 0};

  int settled = filter.update(base);
  // Let the EMA settle at the steady value first.
  for (int i = 0; i < 20; ++i) {
    settled = filter.update(base);
  }

  for (int j : jitter) {
    int result = filter.update(base + j);
    EXPECT_EQ(result, settled) << "jitter=" << j;
  }
}

TEST(PotFilter, StepChangeIsFollowedWithinBoundedSamples) {
  PotFilter filter;
  const int startValue = 200;
  const int targetValue = 600;

  int settled = filter.update(startValue);
  for (int i = 0; i < 20; ++i) {
    settled = filter.update(startValue);
  }
  EXPECT_EQ(settled, startValue);

  constexpr int kMaxSamples = 40;
  int sampleReachedAt = -1;
  int result = settled;
  for (int i = 0; i < kMaxSamples; ++i) {
    result = filter.update(targetValue);
    if (std::abs(result - targetValue) <= pump::POT_DEADBAND_COUNTS) {
      sampleReachedAt = i + 1;
      break;
    }
  }

  EXPECT_GT(sampleReachedAt, 0) << "step change was never picked up within " << kMaxSamples
                                 << " samples";
  EXPECT_LE(sampleReachedAt, kMaxSamples);
}

TEST(PotFilter, SmallChangeBelowDeadbandDoesNotMoveSetpoint) {
  PotFilter filter;
  int settled = filter.update(500);
  for (int i = 0; i < 20; ++i) {
    settled = filter.update(500);
  }

  // A single sample nudged by less than the deadband should not move the
  // reported setpoint.
  int result = filter.update(500 + (pump::POT_DEADBAND_COUNTS - 1));
  EXPECT_EQ(result, settled);
}

TEST(PotFilter, ResetForgetsPreviousHistory) {
  PotFilter filter;
  filter.update(1000);
  filter.reset();
  // After reset, the next sample should be taken as-is, just like the very
  // first sample ever seen.
  EXPECT_EQ(filter.update(50), 50);
}

}  // namespace
