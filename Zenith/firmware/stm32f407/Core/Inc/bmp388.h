#ifndef ZENITH_BMP388_H
#define ZENITH_BMP388_H

#include <stdint.h>

#define BMP388_CHIP_ID 0x50U

/* Numeric values are also published in g_bmp388_last_result for Watch. */
typedef enum {
    BMP388_NOT_TESTED = 0,
    BMP388_OK = 1,
    BMP388_SPI_TIMEOUT = 2,
    BMP388_SPI_ERROR = 3,
    BMP388_WRONG_CHIP_ID = 4,
    BMP388_SENSOR_ERROR = 5,
    BMP388_NOT_READY = 6,
    BMP388_INVALID_ARGUMENT = 7,
    BMP388_BAD_CALIBRATION = 8,
    BMP388_CONFIG_MISMATCH = 9,
    BMP388_NO_NEW_DATA = 10,
    BMP388_OUT_OF_RANGE = 11,
    BMP388_DATA_TIMEOUT = 12
} bmp388_result_t;

typedef struct {
    uint8_t chip_id;
    uint8_t error_reg;
    uint8_t status;
} bmp388_identity_t;

/* Transport must return register data AFTER removing the SPI dummy byte. */
typedef bmp388_result_t (*bmp388_read_fn)(uint8_t reg, uint8_t *data, uint32_t length);
typedef bmp388_result_t (*bmp388_write_fn)(uint8_t reg, uint8_t value);

typedef struct {
    double t1, t2, t3;
    double p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11;
} bmp388_calibration_t;

typedef struct {
    uint8_t pwr_ctrl, osr, odr, config, int_ctrl, if_conf;
} bmp388_registers_t;

typedef struct {
    bmp388_read_fn read;
    bmp388_write_fn write;
    bmp388_calibration_t calibration;
    bmp388_registers_t registers;
    uint8_t calibration_valid;
    uint8_t configured;
} bmp388_device_t;

typedef struct {
    uint32_t raw_pressure;
    uint32_t raw_temperature;
    double pressure_pa;
    double temperature_c;
    uint8_t status;
    uint8_t int_status;
} bmp388_sample_t;

/* Reads CHIP_ID, ERR_REG and STATUS. Does not reset/configure the sensor.
 * ERR_REG bits cmd_err/conf_err are cleared by the sensor when read.
 */
bmp388_result_t BMP388_Identify(bmp388_read_fn read, bmp388_identity_t *identity);
/* Caller checks command readiness, then waits >=2 ms after this command. */
bmp388_result_t BMP388_Reset(bmp388_write_fn write);
/* Call after reset settling. Leaves sensor in normal mode, P/T x1, IIR off,
 * 200 Hz nominal, INT push-pull active high and non-latched (0x42).
 */
bmp388_result_t BMP388_Configure200Hz(bmp388_device_t *device);
bmp388_result_t BMP388_ReadSample(bmp388_device_t *device, bmp388_sample_t *sample);
/* Public for numerical verification against independent reference vectors. */
bmp388_result_t BMP388_DecodeCalibration(const uint8_t raw[21], bmp388_calibration_t *calibration);
bmp388_result_t BMP388_Compensate(const bmp388_calibration_t *calibration, bmp388_sample_t *sample);

#endif
