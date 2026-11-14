/* Software-only checks: compiled ARM C, mocked register transport, no hardware. */
#include "application.h"
#include "main.h"
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

    Application_DataReadyISR(now_us); Application_Poll(1121U);
    CHECK(g_bmp388_no_data_count == 1U && g_bmp388_sample_count == 220U);
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
    Test_Done();
}
