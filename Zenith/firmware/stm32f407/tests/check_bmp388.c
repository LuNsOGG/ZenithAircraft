/* Software-only checks: compiled ARM C, mocked register transport, no hardware. */
#include "application.h"
#include "main.h"
#include "barometric_altitude.h"
#include <math.h>
#include <string.h>
#include "golden_vectors.h"

volatile uint32_t test_count, test_failures, test_first_failure;
static uint8_t registers[128];
static uint8_t trim[21];
static uint32_t now_us;
static uint32_t fail_read;
static uint32_t inject_irq;
static uint32_t reject_int_write;
static uint32_t data_reads;

#define CHECK(expression) do { ++test_count; if (!(expression)) { \
    ++test_failures; if (test_first_failure == 0U) test_first_failure = __LINE__; } } while (0)

uint32_t Platform_TimeUs(void) { return now_us; }

static void Put16(uint8_t *data, int32_t value)
{
    const uint16_t bits = (uint16_t)value;
    data[0] = (uint8_t)bits;
    data[1] = (uint8_t)(bits >> 8);
}

static void Fixture(void)
{
    Put16(&trim[0], 25000); Put16(&trim[2], 20000); trim[4] = (uint8_t)-5;
    Put16(&trim[5], 16500); Put16(&trim[7], 16300); trim[9] = 7; trim[10] = (uint8_t)-3;
    Put16(&trim[11], 12500); Put16(&trim[13], 500); trim[15] = (uint8_t)-12;
    trim[16] = 6; Put16(&trim[17], -900); trim[19] = 7; trim[20] = (uint8_t)-4;
}

static void SensorReset(void)
{
    memset(registers, 0, sizeof(registers));
    registers[0] = 0x50; registers[3] = 0x10;
    memcpy(&registers[0x31], trim, 21);
}

static bmp388_result_t Read(uint8_t reg, uint8_t *data, uint32_t length)
{
    now_us += 20U;
    if (fail_read != 0U) { fail_read = 0U; return BMP388_SPI_TIMEOUT; }
    memcpy(data, &registers[reg], length);
    if (reg == 0x11U) registers[0x11] = 0;
    if (reg == 0x02U) registers[2] &= 1U;
    if (reg == 4U && length == 6U) {
        ++data_reads; registers[3] &= 0x10U;
        if (inject_irq != 0U) { inject_irq = 0U; Application_DataReadyISR(now_us); }
    }
    return BMP388_OK;
}

static bmp388_result_t Write(uint8_t reg, uint8_t value)
{
    now_us += 20U;
    if (reg == 0x7EU && value == 0xB6U) SensorReset();
    else if (reg != 0x19U || reject_int_write == 0U) registers[reg] = value;
    return BMP388_OK;
}

static void Start(uint32_t tick)
{
    const uint32_t time_origin = now_us;
    Fixture(); SensorReset(); fail_read = inject_irq = reject_int_write = data_reads = 0U;
    Application_Init(Read, Write, tick);
    now_us = time_origin + 9000U; Application_Poll(tick + 9U); CHECK(g_bmp388_state == 0U);
    now_us = time_origin + 10000U; Application_Poll(tick + 10U); CHECK(g_bmp388_state == 1U);
    now_us = time_origin + 19000U; Application_Poll(tick + 19U); CHECK(g_bmp388_state == 1U);
    now_us = time_origin + 20000U; Application_Poll(tick + 20U); CHECK(g_bmp388_state == 2U);
    CHECK(g_bmp388_configured == 1U && g_bmp388_calibration_valid == 1U);
    CHECK(registers[0x1B] == 0x33 && registers[0x19] == 0x42);
    CHECK(registers[0x1C] == 0 && registers[0x1D] == 0 && registers[0x1F] == 0);
}

static void NewData(uint32_t timestamp)
{
    now_us = timestamp;
    const uint32_t p = 8000000U, t = 7700000U;
    for (uint32_t i = 0; i < 3U; ++i) {
        registers[4U+i] = (uint8_t)(p >> (8U*i));
        registers[7U+i] = (uint8_t)(t >> (8U*i));
    }
    registers[3] |= 0x60U; registers[0x11] = 8U;
    Application_DataReadyISR(timestamp);
}

void Test_Done(void) { for (;;) { __asm volatile ("nop"); } }

static void CheckAltitude(void)
{
    /* TIM5 wraps during the capture window. All times are unsigned modular. */
    const uint32_t origin = UINT32_MAX - 11002000U;
    uint32_t timestamp = origin;
    BarometricAltitude_Init();
    CHECK(g_baro_state == BARO_WAITING && g_baro_valid == 0U);
    BarometricAltitude_Poll(origin);
    CHECK(g_baro_reference_ready == 0U && g_baro_reference_samples == 0U);
    for (uint32_t i = 0U; i <= 2600U; ++i) {
        timestamp = origin + i * 5000U;
        BarometricAltitude_Poll(timestamp);
        BarometricAltitude_Update(84000.0, timestamp);
        if (i == 1999U) {
            CHECK(g_baro_state == BARO_WARMUP && g_baro_reference_samples == 0U);
        }
        if (i == 2000U) {
            CHECK(g_baro_state == BARO_CAPTURING && g_baro_reference_samples == 1U);
        }
        if (i == 2599U) CHECK(g_baro_reference_ready == 0U);
    }
    CHECK(g_baro_reference_ready == 1U && g_baro_state == BARO_READY);
    CHECK(g_baro_reference_samples == 601U && g_baro_state_elapsed_ms == 3000U);
    CHECK(g_baro_reference_pa == 84000.0 && g_baro_reference_std_pa == 0.0);
    CHECK(g_baro_valid == 1U && g_baro_altitude_relative_m == 0.0);
    CHECK(g_baro_altitude_filtered_m == 0.0);

    timestamp += 5000U;
    BarometricAltitude_Update(83990.0, timestamp);
    const double first_filter = altitude_golden[1].relative * (5000.0 / 505000.0);
    CHECK(g_baro_altitude_relative_m > 0.0);
    CHECK(fabs(g_baro_altitude_filtered_m - first_filter) < 1e-9);
    timestamp += 7000U;
    BarometricAltitude_Update(84010.0, timestamp);
    const double second_filter = first_filter + (7000.0 / 507000.0) *
                                (altitude_golden[2].relative - first_filter);
    CHECK(g_baro_altitude_relative_m < 0.0);
    CHECK(fabs(g_baro_altitude_filtered_m - second_filter) < 1e-9);
    for (uint32_t i = 0U; i < sizeof(altitude_golden)/sizeof(altitude_golden[0]); ++i) {
        timestamp += 5000U;
        BarometricAltitude_Update(altitude_golden[i].pressure, timestamp);
        CHECK(g_baro_valid == 1U);
        CHECK(fabs(g_baro_altitude_relative_m - altitude_golden[i].relative) < 1e-8);
    }
    CHECK(g_baro_reference_pa == 84000.0 && g_baro_reference_samples == 601U);
    BarometricAltitude_Invalidate();
    CHECK(g_baro_valid == 0U && g_baro_reference_ready == 1U);
    timestamp += 5000U;
    BarometricAltitude_Update(83990.0, timestamp);
    CHECK(g_baro_altitude_filtered_m == g_baro_altitude_relative_m);
    BarometricAltitude_Poll(timestamp + 99999U);
    CHECK(g_baro_valid == 1U);
    BarometricAltitude_Poll(timestamp + 100000U);
    CHECK(g_baro_valid == 0U && g_baro_reference_pa == 84000.0);
    BarometricAltitude_Update(NAN, timestamp + 105000U);
    CHECK(g_baro_valid == 0U);
    BarometricAltitude_Update(INFINITY, timestamp + 110000U);
    CHECK(g_baro_valid == 0U);
    BarometricAltitude_Update(29999.0, timestamp + 115000U);
    CHECK(g_baro_valid == 0U);
    BarometricAltitude_Update(125001.0, timestamp + 120000U);
    CHECK(g_baro_valid == 0U && g_baro_reference_ready == 1U);

    timestamp += 125000U;
    g_baro_zero_request = 1U;
    BarometricAltitude_Poll(timestamp);
    CHECK(g_baro_zero_request == 0U && g_baro_reference_ready == 0U);
    CHECK(g_baro_state == BARO_CAPTURING && g_baro_valid == 0U);
    CHECK(g_baro_reference_samples == 0U);
    BarometricAltitude_Update(84000.0, timestamp);
    BarometricAltitude_Update(84000.0, timestamp); /* Duplicate cannot count twice. */
    CHECK(g_baro_reference_samples == 0U && g_baro_reference_restart_count == 1U);
    timestamp += 5000U;
    BarometricAltitude_Update(84000.0, timestamp);
    timestamp += 10001U;
    BarometricAltitude_Update(84000.0, timestamp); /* Incomplete window restarts. */
    CHECK(g_baro_reference_samples == 0U && g_baro_reference_restart_count == 2U);
    timestamp += 5000U;
    const uint32_t capture_origin = timestamp;
    for (uint32_t i = 0U; i <= 600U; ++i) {
        timestamp = capture_origin + i * 5000U;
        BarometricAltitude_Update(84000.0 + (i % 2U != 0U ? 2.0 : -2.0), timestamp);
    }
    CHECK(g_baro_reference_ready == 1U && g_baro_reference_samples == 601U);
    CHECK(fabs(g_baro_reference_pa - noisy_mean) < 1e-8);
    CHECK(fabs(g_baro_reference_std_pa - noisy_std) < 1e-8);
    timestamp += 5000U;
    BarometricAltitude_Update(noisy_mean, timestamp);
    CHECK(fabs(g_baro_altitude_relative_m) < 1e-8);
    /* A constant-pressure filter step approaches 63% after one tau. */
    BarometricAltitude_Invalidate();
    timestamp += 5000U;
    BarometricAltitude_Update(noisy_mean, timestamp);
    for (uint32_t i = 0U; i < 100U; ++i) {
        timestamp += 5000U;
        BarometricAltitude_Update(83990.0, timestamp);
    }
    CHECK(g_baro_altitude_filtered_m / g_baro_altitude_relative_m > 0.62);
    CHECK(g_baro_altitude_filtered_m / g_baro_altitude_relative_m < 0.64);

    /* An interruption before ten seconds requires another continuous warmup. */
    BarometricAltitude_Init();
    BarometricAltitude_Update(84000.0, 1000U);
    BarometricAltitude_Invalidate();
    BarometricAltitude_Update(84000.0, 12000000U);
    CHECK(g_baro_state == BARO_WARMUP && g_baro_state_elapsed_ms == 0U);
    CHECK(g_baro_reference_ready == 0U && g_baro_reference_samples == 0U);

    /* Near the observed 196 Hz: capture is time-based, not fixed at 600 frames.
     * Here the 32-bit counter wraps during the ten-second warmup.
     */
    BarometricAltitude_Init();
    const uint32_t slow_origin = UINT32_MAX - 5000000U;
    for (uint32_t i = 0U; i <= 2550U; ++i) {
        timestamp = slow_origin + i * 5102U;
        BarometricAltitude_Poll(timestamp);
        BarometricAltitude_Update(85000.0, timestamp);
        if (i == 1960U) CHECK(g_baro_state == BARO_WARMUP);
        if (i == 2549U) CHECK(g_baro_reference_ready == 0U);
    }
    CHECK(g_baro_reference_ready == 1U && g_baro_valid == 1U);
    CHECK(g_baro_reference_samples == 590U && g_baro_reference_pa == 85000.0);
    CHECK(g_baro_altitude_relative_m == 0.0 && g_baro_reference_std_pa == 0.0);
    CHECK(g_baro_estimated_valid == 1U);
    CHECK(g_baro_altitude_estimated_m > 1400.0 && g_baro_altitude_estimated_m < 1600.0);
    const double absolute_before_zero = g_baro_altitude_estimated_m;
    const double absolute_filtered_before_zero = g_baro_altitude_estimated_filtered_m;
    g_baro_zero_request = 1U;
    BarometricAltitude_Poll(timestamp);
    CHECK(g_baro_valid == 0U && g_baro_estimated_valid == 1U);
    CHECK(g_baro_altitude_estimated_m == absolute_before_zero);
    CHECK(g_baro_altitude_estimated_filtered_m == absolute_filtered_before_zero);
    BarometricAltitude_Poll(timestamp + 100000U);
    CHECK(g_baro_estimated_valid == 0U); /* Manual zero cannot hide stale data. */
    BarometricAltitude_Update(101325.0, timestamp + 105000U);
    CHECK(g_baro_estimated_valid == 1U && g_baro_altitude_estimated_m == 0.0);
    CHECK(g_baro_altitude_estimated_filtered_m == 0.0);
    CHECK(g_baro_valid == 0U); /* Absolute altitude works before reference capture. */
}

void Test_Run(void)
{
    bmp388_calibration_t calibration;
    bmp388_sample_t sample = {0};
    Fixture();
    CHECK(BMP388_DecodeCalibration(trim, &calibration) == BMP388_OK);
    for (uint32_t i = 0; i < sizeof(golden)/sizeof(golden[0]); ++i) {
        sample.raw_pressure = golden[i].p; sample.raw_temperature = golden[i].t;
        CHECK(BMP388_Compensate(&calibration, &sample) == BMP388_OK);
        CHECK(fabs(sample.pressure_pa - golden[i].pressure) < 1e-7);
        CHECK(fabs(sample.temperature_c - golden[i].temperature) < 1e-10);
    }
    sample.raw_temperature = 0U;
    CHECK(BMP388_Compensate(&calibration, &sample) == BMP388_OUT_OF_RANGE);
    CHECK(sample.temperature_c < -40.0); /* No clamping of invalid values. */
    CHECK(BMP388_DecodeCalibration(NULL, &calibration) == BMP388_INVALID_ARGUMENT);
    memset(trim, 0, 21); CHECK(BMP388_DecodeCalibration(trim, &calibration) == BMP388_BAD_CALIBRATION);
    memset(trim, 0xFF, 21); CHECK(BMP388_DecodeCalibration(trim, &calibration) == BMP388_BAD_CALIBRATION);

    now_us = 0U; Start(0U);
    CHECK(g_bmp388_sample_count == 0U); /* Timer wakeups cannot invent samples. */
    for (uint32_t i = 1U; i <= 220U; ++i) {
        NewData(20000U + i * 5000U);
        Application_Poll(20U + i * 5U);
    }
    CHECK(g_bmp388_sample_count == 220U && data_reads == 220U);
    CHECK(g_bmp388_period_us == 5000U && g_bmp388_sample_valid == 1U);
    CHECK(g_bmp388_rate_valid == 1U);
    CHECK(g_bmp388_sample_rate_hz > 199.0 && g_bmp388_sample_rate_hz < 201.0);
    CHECK(g_bmp388_irq_rate_hz > 199.0 && g_bmp388_irq_rate_hz < 201.0);
    CHECK(g_bmp388_event_overrun_count == 0U && g_bmp388_error_count == 0U);
    CHECK(fabs(g_bmp388_pressure_pa - golden[10].pressure) < 1e-7);
    CHECK(fabs(g_bmp388_temperature_c - golden[10].temperature) < 1e-10);
    CHECK(g_bmp388_read_duration_us == 60U && g_bmp388_sample_time_us == 1120000U);
    CHECK(g_baro_state == BARO_WARMUP && g_baro_reference_ready == 0U);

    Application_DataReadyISR(now_us); Application_Poll(1121U);
    CHECK(g_bmp388_no_data_count == 1U && g_bmp388_sample_count == 220U);
    CHECK(g_baro_state == BARO_WAITING && g_baro_valid == 0U);
    NewData(1130000U); Application_DataReadyISR(1130001U); now_us = 1130010U;
    Application_Poll(1130U);
    CHECK(g_bmp388_event_overrun_count == 1U);
    CHECK((g_bmp388_sample_flags & (BMP388_EVENT_BACKLOG | BMP388_TIME_UNCERTAIN)) == 6U);

    NewData(1135000U); inject_irq = 1U; Application_Poll(1135U);
    CHECK((g_bmp388_sample_flags & BMP388_TIME_UNCERTAIN) != 0U);
    NewData(1140000U); fail_read = 1U; Application_Poll(1140U);
    CHECK(g_bmp388_last_result == BMP388_SPI_TIMEOUT && g_bmp388_sample_valid == 0U);
    NewData(1145000U); Application_Poll(1145U);
    CHECK(g_bmp388_last_result == BMP388_OK && g_bmp388_sample_valid == 1U);

    now_us = 0U; Start(0U); now_us += 100001U; Application_Poll(120U);
    CHECK(g_bmp388_last_result == BMP388_DATA_TIMEOUT && g_bmp388_data_timeout_count == 1U);
    CHECK((g_bmp388_sample_flags & BMP388_SAMPLE_STALE) != 0U);

    NewData(now_us + 5000U); registers[7] = registers[8] = registers[9] = 0U;
    Application_Poll(125U);
    CHECK(g_bmp388_invalid_sample_count == 1U && g_bmp388_sample_valid == 0U);
    CHECK(g_bmp388_frame_count == 1U && g_bmp388_sample_count == 0U);
    CHECK((g_bmp388_sample_flags & BMP388_SAMPLE_OUT_OF_RANGE) != 0U);

    registers[2] = 1U; now_us = 1020000U; Application_Poll(1020U);
    CHECK(g_bmp388_state == 3U && g_bmp388_last_result == BMP388_SENSOR_ERROR);
    CHECK(g_bmp388_calibration_valid == 0U && (g_bmp388_sample_flags & BMP388_SAMPLE_VALID) == 0U);
    registers[2] = 0U; now_us = 2020000U; Application_Poll(2020U);
    CHECK(g_bmp388_state == 1U);
    now_us = 2030000U; Application_Poll(2030U);
    CHECK(g_bmp388_state == 2U && g_bmp388_configured == 1U);

    now_us = 0xFFFFF000U; Start(UINT32_MAX - 5U);
    const uint32_t base = now_us;
    NewData(base + 5000U); Application_Poll(20U);
    NewData(base + 10000U); Application_Poll(25U);
    CHECK(g_bmp388_period_us == 5000U && g_bmp388_sample_count == 2U);

    Fixture(); SensorReset(); reject_int_write = 1U;
    bmp388_device_t device = {.read=Read, .write=Write};
    CHECK(BMP388_Configure200Hz(&device) == BMP388_CONFIG_MISMATCH);
    CHECK(device.configured == 0U && device.registers.int_ctrl == 0U);
    /* End-to-end main application: capture, health fault/recovery, manual zero. */
    now_us = 0U; Start(0U);
    for (uint32_t i = 1U; i <= 2601U; ++i) {
        NewData(20000U + i * 5000U);
        Application_Poll(20U + i * 5U);
    }
    CHECK(g_baro_reference_ready == 1U && g_baro_valid == 1U);
    CHECK(fabs(g_baro_reference_pa - golden[10].pressure) < 1e-7);
    CHECK(fabs(g_baro_altitude_relative_m) < 1e-8);
    const double fixed_reference = g_baro_reference_pa;
    NewData(14025000U); registers[2] = 1U; Application_Poll(14025U);
    CHECK(g_bmp388_state == 3U && g_baro_valid == 0U);
    CHECK(g_baro_reference_ready == 1U && g_baro_reference_pa == fixed_reference);
    registers[2] = 0U; now_us = 15025000U; Application_Poll(15025U);
    now_us = 15035000U; Application_Poll(15035U);
    NewData(15040000U); Application_Poll(15040U);
    CHECK(g_baro_valid == 1U && g_baro_reference_pa == fixed_reference);
    now_us += 100001U; Application_Poll(15140U);
    CHECK(g_baro_valid == 0U && g_baro_reference_ready == 1U);
    g_baro_zero_request = 1U; Application_Poll(15141U);
    CHECK(g_baro_zero_request == 0U && g_baro_reference_ready == 0U);
    CHECK(g_baro_state == BARO_CAPTURING && g_baro_reference_samples == 0U);
    CheckAltitude();
    Test_Done();
}
