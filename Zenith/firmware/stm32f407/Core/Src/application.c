#include "application.h"
#include "main.h"
#include "barometric_altitude.h"

/* Cooperative startup waits and one health probe/second while streaming.
 * Samples are requested only by data-ready notifications, never by this tick.
 */
#define FIRST_PROBE_TICKS ((PLATFORM_TIMER_HZ + 99U) / 100U)
#define PROBE_PERIOD_TICKS PLATFORM_TIMER_HZ
#define SAMPLE_TIMEOUT_US 100000U
#define LONG_IRQ_PERIOD_US 10000U

volatile uint32_t g_bmp388_chip_id;
volatile uint32_t g_bmp388_probe_ok;
volatile uint32_t g_bmp388_last_result;
volatile uint32_t g_bmp388_probe_count;
volatile uint32_t g_bmp388_success_count;
volatile uint32_t g_bmp388_error_count;
volatile uint32_t g_bmp388_error_reg;
volatile uint32_t g_bmp388_error_flags_seen;
volatile uint32_t g_bmp388_status;
volatile uint32_t g_bmp388_state;
volatile uint32_t g_bmp388_calibration_valid;
volatile uint32_t g_bmp388_configured;
volatile bmp388_registers_t g_bmp388_registers;
volatile double g_bmp388_pressure_pa;
volatile double g_bmp388_temperature_c;
volatile uint32_t g_bmp388_raw_pressure;
volatile uint32_t g_bmp388_raw_temperature;
volatile uint32_t g_bmp388_sample_valid;
volatile uint32_t g_bmp388_sample_flags;
volatile uint32_t g_bmp388_sample_count;
volatile uint32_t g_bmp388_frame_count;
volatile uint32_t g_bmp388_irq_count;
volatile uint32_t g_bmp388_irq_time_us;
volatile uint32_t g_bmp388_sample_time_us;
volatile uint32_t g_bmp388_period_us;
volatile uint32_t g_bmp388_service_latency_us;
volatile uint32_t g_bmp388_read_duration_us;
volatile uint32_t g_bmp388_event_overrun_count;
volatile uint32_t g_bmp388_irq_gap_count;
volatile uint32_t g_bmp388_no_data_count;
volatile uint32_t g_bmp388_data_timeout_count;
volatile uint32_t g_bmp388_invalid_sample_count;
volatile uint32_t g_bmp388_int_status;
volatile double g_bmp388_irq_rate_hz;
volatile double g_bmp388_sample_rate_hz;
volatile uint32_t g_bmp388_rate_valid;

static bmp388_device_t sensor;
static uint32_t last_probe_tick;
static uint32_t state_tick;
static uint32_t seen_irq_count;
static uint32_t previous_irq_us;
static uint32_t previous_irq_valid;
static uint32_t last_frame_us;
static uint32_t last_timeout_us;
static uint32_t rate_start_us;
static uint32_t rate_start_samples;
static uint32_t rate_start_irqs;

void Application_Init(bmp388_read_fn read, bmp388_write_fn write, uint32_t tick)
{
    BarometricAltitude_Init();
    sensor = (bmp388_device_t){.read = read, .write = write};
    last_probe_tick = tick;
    state_tick = tick;
    seen_irq_count = previous_irq_us = previous_irq_valid = 0U;
    last_frame_us = last_timeout_us = rate_start_us = 0U;
    rate_start_samples = rate_start_irqs = 0U;
    g_bmp388_chip_id = 0U;
    g_bmp388_probe_ok = 0U;
    g_bmp388_last_result = BMP388_NOT_TESTED;
    g_bmp388_probe_count = 0U;
    g_bmp388_success_count = 0U;
    g_bmp388_error_count = 0U;
    g_bmp388_error_reg = 0U;
    g_bmp388_error_flags_seen = 0U;
    g_bmp388_status = 0U;
    g_bmp388_state = g_bmp388_calibration_valid = g_bmp388_configured = 0U;
    g_bmp388_registers = (bmp388_registers_t){0};
    g_bmp388_pressure_pa = g_bmp388_temperature_c = 0.0;
    g_bmp388_raw_pressure = g_bmp388_raw_temperature = 0U;
    g_bmp388_sample_valid = g_bmp388_sample_flags = 0U;
    g_bmp388_sample_count = g_bmp388_frame_count = 0U;
    g_bmp388_irq_count = g_bmp388_irq_time_us = 0U;
    g_bmp388_sample_time_us = g_bmp388_period_us = 0U;
    g_bmp388_service_latency_us = g_bmp388_read_duration_us = 0U;
    g_bmp388_event_overrun_count = g_bmp388_irq_gap_count = 0U;
    g_bmp388_no_data_count = g_bmp388_data_timeout_count = 0U;
    g_bmp388_invalid_sample_count = g_bmp388_int_status = 0U;
    g_bmp388_irq_rate_hz = g_bmp388_sample_rate_hz = 0.0;
    g_bmp388_rate_valid = 0U;
}

void Application_DataReadyISR(uint32_t timestamp_us)
{
    /* Single writer ISR: publish timestamp before committing sequence. */
    g_bmp388_irq_time_us = timestamp_us;
    ++g_bmp388_irq_count;
}

static bmp388_result_t Probe(uint32_t tick)
{
    bmp388_identity_t identity;
    bmp388_result_t result;
    last_probe_tick = tick;
    g_bmp388_probe_ok = 0U;
    result = BMP388_Identify(sensor.read, &identity);
    g_bmp388_chip_id = identity.chip_id;
    g_bmp388_error_reg = identity.error_reg;
    g_bmp388_error_flags_seen |= identity.error_reg & 0x07U;
    g_bmp388_status = identity.status;
    ++g_bmp388_probe_count;
    if (result == BMP388_OK) {
        ++g_bmp388_success_count;
        g_bmp388_probe_ok = 1U;
    } else {
        ++g_bmp388_error_count;
    }
    return result;
}

static void Retry(bmp388_result_t result, uint32_t tick)
{
    BarometricAltitude_Invalidate();
    g_bmp388_last_result = (uint32_t)result;
    g_bmp388_configured = g_bmp388_sample_valid = g_bmp388_rate_valid = 0U;
    g_bmp388_calibration_valid = 0U;
    g_bmp388_sample_flags = (g_bmp388_sample_flags & ~(uint32_t)BMP388_SAMPLE_VALID) | BMP388_SAMPLE_STALE;
    sensor.configured = 0U;
    g_bmp388_state = 3U;
    state_tick = tick;
}

static void ServiceDataReady(void)
{
    uint32_t count;
    uint32_t timestamp;
    uint32_t flags = 0U;
    bmp388_sample_t sample;
    /* Stable single-core ISR snapshot without masking interrupts. */
    do {
        count = g_bmp388_irq_count;
        timestamp = g_bmp388_irq_time_us;
    } while (count != g_bmp388_irq_count);
    if (count == seen_irq_count) {
        return;
    }
    const uint32_t pending = count - seen_irq_count;
    seen_irq_count = count;
    if (pending > 1U) {
        g_bmp388_event_overrun_count += pending - 1U;
        flags |= BMP388_EVENT_BACKLOG | BMP388_TIME_UNCERTAIN;
    }
    if (previous_irq_valid != 0U) {
        g_bmp388_period_us = timestamp - previous_irq_us;
        if (g_bmp388_period_us > LONG_IRQ_PERIOD_US) {
            ++g_bmp388_irq_gap_count;
            flags |= BMP388_LONG_PERIOD;
        }
    }
    previous_irq_us = timestamp;
    previous_irq_valid = 1U;

    const uint32_t start = Platform_TimeUs();
    g_bmp388_service_latency_us = start - timestamp;
    const bmp388_result_t result = BMP388_ReadSample(&sensor, &sample);
    const uint32_t finish = Platform_TimeUs();
    g_bmp388_read_duration_us = finish - start;
    if (count != g_bmp388_irq_count) {
        flags |= BMP388_TIME_UNCERTAIN;
    }
    g_bmp388_status = sample.status;
    g_bmp388_int_status = sample.int_status;
    g_bmp388_last_result = (uint32_t)result;
    g_bmp388_sample_valid = 0U;
    if (result == BMP388_OK || result == BMP388_OUT_OF_RANGE) {
        g_bmp388_raw_pressure = sample.raw_pressure;
        g_bmp388_raw_temperature = sample.raw_temperature;
        g_bmp388_pressure_pa = sample.pressure_pa;
        g_bmp388_temperature_c = sample.temperature_c;
        g_bmp388_sample_time_us = timestamp;
        last_frame_us = finish;
        ++g_bmp388_frame_count;
        if (result == BMP388_OK) {
            ++g_bmp388_sample_count;
            g_bmp388_sample_valid = 1U;
            flags |= BMP388_SAMPLE_VALID;
        } else {
            ++g_bmp388_invalid_sample_count;
            flags |= BMP388_SAMPLE_OUT_OF_RANGE;
        }
    } else if (result == BMP388_NO_NEW_DATA) {
        ++g_bmp388_no_data_count;
    } else {
        ++g_bmp388_error_count;
    }
    g_bmp388_sample_flags = flags;
    if (flags == BMP388_SAMPLE_VALID) {
        BarometricAltitude_Update(sample.pressure_pa, timestamp);
    } else {
        BarometricAltitude_Invalidate();
    }
}

void Application_Poll(uint32_t tick)
{
    bmp388_result_t result;
    BarometricAltitude_Poll(Platform_TimeUs());
    if (g_bmp388_state == 0U || g_bmp388_state == 3U) {
        const uint32_t delay = g_bmp388_state == 0U ? FIRST_PROBE_TICKS : PROBE_PERIOD_TICKS;
        if ((uint32_t)(tick - state_tick) < delay) {
            return;
        }
        result = Probe(tick);
        if (result != BMP388_OK) {
            Retry(result, tick);
            return;
        }
        result = BMP388_Reset(sensor.write);
        if (result != BMP388_OK) {
            ++g_bmp388_error_count;
            Retry(result, tick);
            return;
        }
        g_bmp388_calibration_valid = 0U;
        g_bmp388_state = 1U;
        g_bmp388_last_result = BMP388_NOT_READY;
        state_tick = tick;
        return;
    }
    if (g_bmp388_state == 1U) {
        if ((uint32_t)(tick - state_tick) < FIRST_PROBE_TICKS) {
            return;
        }
        result = Probe(tick);
        if (result != BMP388_OK) {
            Retry(result, tick);
            return;
        }
        seen_irq_count = g_bmp388_irq_count;
        result = BMP388_Configure200Hz(&sensor);
        g_bmp388_calibration_valid = sensor.calibration_valid;
        g_bmp388_registers = sensor.registers;
        if (result != BMP388_OK) {
            ++g_bmp388_error_count;
            Retry(result, tick);
            return;
        }
        g_bmp388_configured = 1U;
        g_bmp388_state = 2U;
        g_bmp388_last_result = BMP388_OK;
        previous_irq_valid = 0U;
        last_frame_us = last_timeout_us = rate_start_us = Platform_TimeUs();
        rate_start_samples = g_bmp388_sample_count;
        rate_start_irqs = g_bmp388_irq_count;
        return;
    }

    ServiceDataReady();
    if ((uint32_t)(tick - last_probe_tick) >= PROBE_PERIOD_TICKS) {
        result = Probe(tick);
        if (result != BMP388_OK) {
            Retry(result, tick);
            return;
        }
    }
    const uint32_t now = Platform_TimeUs();
    if ((uint32_t)(now - last_frame_us) >= SAMPLE_TIMEOUT_US) {
        BarometricAltitude_Invalidate();
        g_bmp388_sample_valid = 0U;
        g_bmp388_sample_flags = (g_bmp388_sample_flags & ~(uint32_t)BMP388_SAMPLE_VALID) | BMP388_SAMPLE_STALE;
        g_bmp388_last_result = BMP388_DATA_TIMEOUT;
        if ((uint32_t)(now - last_timeout_us) >= SAMPLE_TIMEOUT_US) {
            ++g_bmp388_data_timeout_count;
            last_timeout_us = now;
        }
    }
    const uint32_t elapsed = now - rate_start_us;
    if (elapsed >= 1000000U) {
        g_bmp388_sample_rate_hz = (double)(g_bmp388_sample_count - rate_start_samples) * 1000000.0 / (double)elapsed;
        g_bmp388_irq_rate_hz = (double)(g_bmp388_irq_count - rate_start_irqs) * 1000000.0 / (double)elapsed;
        g_bmp388_rate_valid = 1U;
        rate_start_us = now;
        rate_start_samples = g_bmp388_sample_count;
        rate_start_irqs = g_bmp388_irq_count;
    }
}
