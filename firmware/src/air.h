/*
 * Chu Pico Air Sensor
 * WHowe <github.com/whowechina>
 */

#ifndef AIR_H
#define AIR_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

void change_light(int light);
void turnoff_light();
uint16_t get_value(int sensor);
void air_init();
bool get_sensor_state(int sensor);
float get_hand_position();
uint8_t get_sensor_readings();

#endif
