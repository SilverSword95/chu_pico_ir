/*
 * Chu Pico Air Sensor
 * WHowe <github.com/whowechina>
 * 
 */

#include "air.h"

#include <stdint.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

#include "board_defs.h"

int calibrationCounter = 10;
bool calibrated = false;
uint16_t thresholds[6];
uint16_t maxReadings[6];

static const uint8_t IR_SENSOR_LIST[] = IR_SENSOR_PINS;
static const uint8_t IR_LED_LIST[] = IR_LED_PINS;

// Sets the output pins to switch the charlieplexed array of LEDs.
// 0 is the bottom-most LED and 5 is the top-most
void change_light(int light) {
    switch (light) {
    case 0:
      gpio_set_dir(IR_LED_LIST[0], 1);
      gpio_set_dir(IR_LED_LIST[1], 1);
      gpio_set_dir(IR_LED_LIST[2], 0);

      gpio_put(IR_LED_LIST[0], 1);
      gpio_put(IR_LED_LIST[1], 0);
      gpio_put(IR_LED_LIST[2], 0);
      break;
    case 1:
      gpio_set_dir(IR_LED_LIST[0], 1);
      gpio_set_dir(IR_LED_LIST[1], 1);
      gpio_set_dir(IR_LED_LIST[2], 0);

      gpio_put(IR_LED_LIST[0], 0);
      gpio_put(IR_LED_LIST[1], 1);
      gpio_put(IR_LED_LIST[2], 0);
      break;
    case 2:
      gpio_set_dir(IR_LED_LIST[0], 0);
      gpio_set_dir(IR_LED_LIST[1], 1);
      gpio_set_dir(IR_LED_LIST[2], 1);

      gpio_put(IR_LED_LIST[0], 0);
      gpio_put(IR_LED_LIST[1], 1);
      gpio_put(IR_LED_LIST[2], 0);
      break;
    case 3:
      gpio_set_dir(IR_LED_LIST[0], 0);
      gpio_set_dir(IR_LED_LIST[1], 1);
      gpio_set_dir(IR_LED_LIST[2], 1);

      gpio_put(IR_LED_LIST[0], 0);
      gpio_put(IR_LED_LIST[1], 0);
      gpio_put(IR_LED_LIST[2], 1);
      break;
    case 4:
      gpio_set_dir(IR_LED_LIST[0], 1);
      gpio_set_dir(IR_LED_LIST[1], 0);
      gpio_set_dir(IR_LED_LIST[2], 1);

      gpio_put(IR_LED_LIST[0], 1);
      gpio_put(IR_LED_LIST[1], 0);
      gpio_put(IR_LED_LIST[2], 0);
      break;
    case 5:
      gpio_set_dir(IR_LED_LIST[0], 1);
      gpio_set_dir(IR_LED_LIST[1], 0);
      gpio_set_dir(IR_LED_LIST[2], 1);

      gpio_put(IR_LED_LIST[0], 0);
      gpio_put(IR_LED_LIST[1], 0);
      gpio_put(IR_LED_LIST[2], 1);
      break;
    default:
      turnoff_light();
      break;
  }
}

// Sets all output pins to high-impedance to turn off all LEDs
void turnoff_light() {
    gpio_set_dir(IR_LED_LIST[0], 0);
    gpio_set_dir(IR_LED_LIST[1], 0);
    gpio_set_dir(IR_LED_LIST[2], 0);
}

uint16_t get_value(int sensor) {
  // Turn on light corresponding to read sensor
  change_light(sensor);

  // Delay required because the read may occur faster than the physical light turning on
  sleep_ms(AIR_LED_DELAY);
  
#ifndef IR_SENSOR_ANALOG
  return gpio_get(IR_SENSOR_LIST[sensor]);
#else
  adc_select_input(IR_SENSOR_LIST[sensor]-26);
  return adc_read();
#endif

  // Turn the lights off when we're done reading
  turnoff_light();
}

void air_init(){
    gpio_init(IR_LED_LIST[0]);
    gpio_init(IR_LED_LIST[1]);
    gpio_init(IR_LED_LIST[2]);
	
    for (int i = 0; i < 6; i++) {
        maxReadings[i] = 0;
    }
#ifndef IR_SENSOR_ANALOG
    for (int i = 0; i < 6; i++) {
        gpio_init(IR_SENSOR_LIST[i]);
        gpio_set_dir(IR_SENSOR_LIST[i], 0);
    }
#else
	for (int i = 0; i < 6; i++) {
        adc_gpio_init(IR_SENSOR_LIST[i]);
    }
#endif
}

void analog_calibrate() {
#ifdef IR_SENSOR_ANALOG
  // Skip some samples, for some reason the first few readings tend to give wild values that can skew min/max tracking
  for (int i = 0; i < SKIP_SAMPLES; i++) {
    for (int sensor = 0; sensor < 6; sensor++) {
      get_value(sensor);
    }
  }

  // begin calibration
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    for (int sensor = 0; sensor < 6; sensor++) {
      uint16_t value = get_value(sensor);

      if (value > maxReadings[sensor])
        maxReadings[sensor] = value;
    }

    // after sweeping the LEDs, scan the touchboard to simulate the delay between
    // IR sweeps during actual gameplay so we calibrate accurately
  }

  for (int i = 0; i < 6; i++) {
    thresholds[i] = (AIR_INPUT_DETECTION * maxReadings[i]);
  }
#endif
calibrated = true;
}

bool get_sensor_state(int sensor) {
  uint16_t value = get_value(sensor);

#ifndef IR_SENSOR_ANALOG
  return value == 0 ? true : false;
#else
  return value < thresholds[sensor];
#endif
}

// Using data from air sensors, compute the height of the player's hand, from 0 (not present) to 1 (highest possible position).
float get_hand_position() {
  int highestTriggered = -1;
  
  for (int i = 0; i < 6; i++) {
    if (get_sensor_state(i)) {
      if ((i + 1) > highestTriggered) {
        highestTriggered = i + 1;
      }
    }
  }

  if (calibrated) {
    return highestTriggered == -1 ? 0 : ((float) highestTriggered / 6.0f);
  } else {
    calibrationCounter--;
    
    if (calibrationCounter <= 0) {
	  analog_calibrate();
      calibrated = true;
    }

    return 0;
  }
}

uint8_t get_sensor_readings() {
  uint8_t reading = 0;

  for (int i = 0; i < 6; i++) {
    reading |= ((int) get_sensor_state(i) << i);
  }

  if (calibrated) {
    return reading;
  } else {
    calibrationCounter--;
    
    if (calibrationCounter <= 0) {
	  analog_calibrate();
      calibrated = true;
    }

    return 0;
  }
}