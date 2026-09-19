#pragma once

/**
 * @file pump_hal.h
 * @brief Selects the hardware types/functions pump_core.cpp is built against.
 *
 * On the Arduino build this pulls in the real Arduino core, AccelStepper and
 * LiquidCrystal_I2C. On a host (PC) build (PUMP_HOST_BUILD defined by the
 * CMake test build) it pulls in lightweight mocks instead, so the exact same
 * pump_core.cpp compiles and runs on a development machine.
 */

#if defined(PUMP_HOST_BUILD)

#include "host_mocks.h"
using StepperType = MockStepper;
using LcdType = MockLcd;

#else

#include <Arduino.h>
#include <AccelStepper.h>
#include <LiquidCrystal_I2C.h>
using StepperType = AccelStepper;
using LcdType = LiquidCrystal_I2C;

#endif
