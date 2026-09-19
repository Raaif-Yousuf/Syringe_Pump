#include <gtest/gtest.h>

#include "pump_core.h"

namespace {

using pump::ButtonAction;
using pump::decideButtonAction;
using pump::decideJogAction;
using pump::JogAction;
using pump::PumpState;

TEST(ButtonAction, PressWhileIdleStarts) {
  PumpState state;
  state.running = false;
  state.empty = false;
  EXPECT_EQ(decideButtonAction(state, /*buttonReadingLow=*/true), ButtonAction::Start);
}

TEST(ButtonAction, ReleaseWhileRunningStops) {
  PumpState state;
  state.running = true;
  state.empty = false;
  EXPECT_EQ(decideButtonAction(state, /*buttonReadingLow=*/false), ButtonAction::Stop);
}

TEST(ButtonAction, PressWhileAlreadyRunningIsNoOp) {
  PumpState state;
  state.running = true;
  state.empty = false;
  EXPECT_EQ(decideButtonAction(state, /*buttonReadingLow=*/true), ButtonAction::None);
}

TEST(ButtonAction, ReleaseWhileAlreadyIdleIsNoOp) {
  PumpState state;
  state.running = false;
  state.empty = false;
  EXPECT_EQ(decideButtonAction(state, /*buttonReadingLow=*/false), ButtonAction::None);
}

TEST(ButtonAction, EmptyStateIgnoresButtonEntirely) {
  PumpState state;
  state.running = false;
  state.empty = true;
  EXPECT_EQ(decideButtonAction(state, /*buttonReadingLow=*/true), ButtonAction::None);

  state.running = true;
  EXPECT_EQ(decideButtonAction(state, /*buttonReadingLow=*/false), ButtonAction::None);
}

TEST(JogAction, IgnoredWhilePumpRunning) {
  EXPECT_EQ(decideJogAction(/*pumpRunning=*/true, /*jogForwardLow=*/true, /*jogReverseLow=*/false,
                             /*limitSwitchLow=*/true),
            JogAction::None);
}

TEST(JogAction, ForwardWithClearLimitJogsForward) {
  EXPECT_EQ(decideJogAction(false, /*jogForwardLow=*/true, false, /*limitSwitchLow=*/true),
            JogAction::JogForward);
}

TEST(JogAction, ForwardWithLimitTrippedIsBlocked) {
  EXPECT_EQ(decideJogAction(false, /*jogForwardLow=*/true, false, /*limitSwitchLow=*/false),
            JogAction::JogBlockedEmpty);
}

TEST(JogAction, ReverseAlwaysAllowed) {
  EXPECT_EQ(decideJogAction(false, false, /*jogReverseLow=*/true, /*limitSwitchLow=*/false),
            JogAction::JogReverse);
}

TEST(JogAction, NeitherButtonStopsMotor) {
  EXPECT_EQ(decideJogAction(false, false, false, true), JogAction::Stop);
}

TEST(JogAction, ForwardTakesPriorityOverReverse) {
  EXPECT_EQ(decideJogAction(false, /*jogForwardLow=*/true, /*jogReverseLow=*/true, /*limitSwitchLow=*/true),
            JogAction::JogForward);
}

}  // namespace
