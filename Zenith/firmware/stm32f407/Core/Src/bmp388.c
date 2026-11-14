#include "bmp388.h"

#include <stddef.h>
#include <math.h>

/* Bosch BST-BMP388-DS001-07, sections 4.3.1 .. 4.3.3. */
#define BMP388_REG_CHIP_ID 0x00U
#define BMP388_REG_ERROR   0x02U
#define BMP388_ERROR_MASK  0x07U
#define BMP388_CMD_READY   0x10U
#define BMP388_REG_STATUS  0x03U
#define BMP388_REG_DATA    0x04U
#define BMP388_REG_INT     0x11U
#define BMP388_DATA_READY  0x60U
#define BMP388_INT_DRDY    0x08U

bmp388_result_t BMP388_Identify(bmp388_read_fn read, bmp388_identity_t *identity)
{
    uint8_t state[2] = {0U, 0U};
    bmp388_result_t result;

    if (identity == NULL) {
        return BMP388_INVALID_ARGUMENT;
    }
    /* Never leave a previous successful response in a failed snapshot. */
    *identity = (bmp388_identity_t){0U, 0U, 0U};
    if (read == NULL) {
        return BMP388_INVALID_ARGUMENT;
    }

    result = read(BMP388_REG_CHIP_ID, &identity->chip_id, 1U);
    if (result != BMP388_OK) {
        identity->chip_id = 0U;
        return result;
    }
    if (identity->chip_id != BMP388_CHIP_ID) {
        return BMP388_WRONG_CHIP_ID;
    }

    /* Read adjacent ERR_REG/STATUS together; skip reserved register 0x01. */
    result = read(BMP388_REG_ERROR, state, 2U);
    if (result != BMP388_OK) {
        return result;
    }
    identity->error_reg = state[0];
    identity->status = state[1];
    if ((identity->error_reg & BMP388_ERROR_MASK) != 0U) {
        return BMP388_SENSOR_ERROR;
    }
    if ((identity->status & BMP388_CMD_READY) == 0U) {
        return BMP388_NOT_READY;
    }
    return BMP388_OK;
}

static uint16_t DecodeU16(const uint8_t *raw)
{
    return (uint16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
}

/* Explicit sign extension avoids implementation-defined unsigned casts. */
static int32_t DecodeS16(const uint8_t *raw)
{
    const uint32_t value = DecodeU16(raw);
    return value < 32768U ? (int32_t)value : (int32_t)value - 65536;
}

static int32_t DecodeS8(uint8_t raw)
{
    return raw < 128U ? (int32_t)raw : (int32_t)raw - 256;
}

bmp388_result_t BMP388_DecodeCalibration(const uint8_t raw[21], bmp388_calibration_t *calibration)
{
    uint8_t any_set = 0U;
    uint8_t all_set = 0xFFU;
    if (raw == NULL || calibration == NULL) {
        return BMP388_INVALID_ARGUMENT;
    }
    *calibration = (bmp388_calibration_t){0};
    for (uint32_t i = 0U; i < 21U; ++i) {
        any_set |= raw[i];
        all_set &= raw[i];
    }
    if (any_set == 0U || all_set == 0xFFU || DecodeU16(&raw[2]) == 0U) {
        return BMP388_BAD_CALIBRATION;
    }
    /* Bosch datasheet rev 1.7, section 9.1: exact binary scale factors.
     * Independent implementation of the published calibration equations.
     */
    calibration->t1 = (double)DecodeU16(&raw[0]) * 0x1p8;
    calibration->t2 = (double)DecodeU16(&raw[2]) * 0x1p-30;
    calibration->t3 = (double)DecodeS8(raw[4]) * 0x1p-48;
    calibration->p1 = (double)(DecodeS16(&raw[5]) - 16384) * 0x1p-20;
    calibration->p2 = (double)(DecodeS16(&raw[7]) - 16384) * 0x1p-29;
    calibration->p3 = (double)DecodeS8(raw[9]) * 0x1p-32;
    calibration->p4 = (double)DecodeS8(raw[10]) * 0x1p-37;
    calibration->p5 = (double)DecodeU16(&raw[11]) * 0x1p3;
    calibration->p6 = (double)DecodeU16(&raw[13]) * 0x1p-6;
    calibration->p7 = (double)DecodeS8(raw[15]) * 0x1p-8;
    calibration->p8 = (double)DecodeS8(raw[16]) * 0x1p-15;
    calibration->p9 = (double)DecodeS16(&raw[17]) * 0x1p-48;
    calibration->p10 = (double)DecodeS8(raw[19]) * 0x1p-48;
    calibration->p11 = (double)DecodeS8(raw[20]) * 0x1p-65;
    return BMP388_OK;
}

bmp388_result_t BMP388_Compensate(const bmp388_calibration_t *calibration, bmp388_sample_t *sample)
{
    if (calibration == NULL || sample == NULL) {
        return BMP388_INVALID_ARGUMENT;
    }
    /* Datasheet sections 9.2/9.3. Temperature from this same conversion is
     * required for pressure; double precision avoids unnecessary cancellation.
     * Out-of-range values are flagged, never clamped into plausible readings.
     */
    const double dt = (double)sample->raw_temperature - calibration->t1;
    const double t = dt * (calibration->t2 + dt * calibration->t3);
    const double p = (double)sample->raw_pressure;
    const double offset = calibration->p5 + t * (calibration->p6 + t *
                          (calibration->p7 + t * calibration->p8));
    const double gain = calibration->p1 + t * (calibration->p2 + t *
                        (calibration->p3 + t * calibration->p4));
    sample->temperature_c = t;
    sample->pressure_pa = offset + p * gain + p * p *
                          (calibration->p9 + t * calibration->p10 + p * calibration->p11);
    if (!isfinite(t) || !isfinite(sample->pressure_pa) || t < -40.0 || t > 85.0 ||
        sample->pressure_pa < 30000.0 || sample->pressure_pa > 125000.0) {
        return BMP388_OUT_OF_RANGE;
    }
    return BMP388_OK;
}

bmp388_result_t BMP388_Reset(bmp388_write_fn write)
{
    return write != NULL ? write(0x7EU, 0xB6U) : BMP388_INVALID_ARGUMENT;
}

bmp388_result_t BMP388_Configure200Hz(bmp388_device_t *device)
{
    static const uint8_t addresses[6] = {0x1BU, 0x1CU, 0x1DU, 0x1FU, 0x19U, 0x1AU};
    static const uint8_t expected[6] = {0x33U, 0x00U, 0x00U, 0x00U, 0x42U, 0x00U};
    static const uint8_t masks[6] = {0x33U, 0x3FU, 0x1FU, 0x0EU, 0x5FU, 0x07U};
    uint8_t raw[21];
    uint8_t observed[6] = {0};
    bmp388_result_t result;
    if (device == NULL || device->read == NULL || device->write == NULL) {
        return BMP388_INVALID_ARGUMENT;
    }
    device->calibration_valid = 0U;
    device->configured = 0U;
    device->registers = (bmp388_registers_t){0};
    result = device->read(0x31U, raw, 21U);
    if (result != BMP388_OK) {
        return result;
    }
    result = BMP388_DecodeCalibration(raw, &device->calibration);
    if (result != BMP388_OK) {
        return result;
    }
    device->calibration_valid = 1U;
    /* Reset has left the device asleep. Configure everything before starting
     * normal mode. Single-register writes follow BMP388's address/data pairs.
     */
    for (uint32_t i = 1U; i < 6U; ++i) {
        result = device->write(addresses[i], expected[i]);
        if (result != BMP388_OK) {
            return result;
        }
    }
    result = device->write(addresses[0], expected[0]);
    if (result != BMP388_OK) {
        return result;
    }
    for (uint32_t i = 0U; i < 6U; ++i) {
        result = device->read(addresses[i], &observed[i], 1U);
        if (result != BMP388_OK) {
            return result;
        }
    }
    device->registers = (bmp388_registers_t){observed[0], observed[1], observed[2],
                                            observed[3], observed[4], observed[5]};
    for (uint32_t i = 0U; i < 6U; ++i) {
        if ((observed[i] & masks[i]) != expected[i]) {
            return BMP388_CONFIG_MISMATCH;
        }
    }
    device->configured = 1U;
    return BMP388_OK;
}

bmp388_result_t BMP388_ReadSample(bmp388_device_t *device, bmp388_sample_t *sample)
{
    uint8_t raw[6];
    bmp388_result_t result;
    if (device == NULL || sample == NULL || device->read == NULL) {
        return BMP388_INVALID_ARGUMENT;
    }
    *sample = (bmp388_sample_t){0};
    if (device->configured == 0U || device->calibration_valid == 0U) {
        return BMP388_NOT_READY;
    }
    /* Acknowledge DRDY, then require fresh pressure AND temperature flags.
     * A periodic timer wakeup alone never produces a sensor sample.
     */
    result = device->read(BMP388_REG_INT, &sample->int_status, 1U);
    if (result != BMP388_OK) {
        return result;
    }
    result = device->read(BMP388_REG_STATUS, &sample->status, 1U);
    if (result != BMP388_OK) {
        return result;
    }
    if ((sample->int_status & BMP388_INT_DRDY) == 0U ||
        (sample->status & BMP388_DATA_READY) != BMP388_DATA_READY) {
        return BMP388_NO_NEW_DATA;
    }
    /* One burst preserves pressure/temperature register shadowing. */
    result = device->read(BMP388_REG_DATA, raw, 6U);
    if (result != BMP388_OK) {
        return result;
    }
    sample->raw_pressure = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8) | ((uint32_t)raw[2] << 16);
    sample->raw_temperature = (uint32_t)raw[3] | ((uint32_t)raw[4] << 8) | ((uint32_t)raw[5] << 16);
    return BMP388_Compensate(&device->calibration, sample);
}
