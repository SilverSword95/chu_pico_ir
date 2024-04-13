/*
 * Chu Controller Board Definitions
 * WHowe <github.com/whowechina>
 */

#if defined BOARD_CHU_PICO

#define I2C_PORT i2c0
#define I2C_SDA 4
#define I2C_SCL 5
#define I2C_FREQ 733*1000

#define RGB_PIN 0
#define RGB_ORDER GRB // or RGB

//#define IR_SENSOR_ANALOG
#define IR_LED_PINS { 27, 26, 28 }
#define IR_SENSOR_PINS { 16, 17, 18, 19, 20, 21 }
#define AIR_LED_DELAY 125
#define AIR_INPUT_DETECTION 0.90
#define CALIBRATION_SAMPLES 200
#define SKIP_SAMPLES 40

#define NKRO_KEYMAP "azsxdcfv1q2w3e4r5t6y7u8igbhnjmk,/'.;[]"
#else

#endif
