#include "barometric_altitude.h"

#include <math.h>

#define BARO_WARMUP_US 10000000U
#define BARO_CAPTURE_US 3000000U
#define BARO_MAX_SAMPLE_GAP_US 10000U
#define BARO_STALE_US 100000U
#define BARO_FILTER_TAU_US 500000.0
#define BARO_MIN_PRESSURE_PA 30000.0
#define BARO_MAX_PRESSURE_PA 125000.0

/* NOAA/NWS pressure-altitude approximation, converted from ft/hPa to m/Pa:
 * H(P) = 145366.45 * 0.3048 * (1 - (P / 101325)^0.190284).
 * Relative height is H(P) - H(reference), NOT a sea-level pressure setting.
 * Standard-atmosphere model; sensor temperature is not ambient-air input.
 */
#define BARO_STANDARD_PRESSURE_PA 101325.0
#define BARO_HEIGHT_SCALE_M 44307.69396
#define BARO_PRESSURE_EXPONENT 0.190284

volatile uint32_t g_baro_state;
volatile uint32_t g_baro_state_elapsed_ms;
volatile uint32_t g_baro_reference_ready;
volatile double g_baro_reference_pa;
volatile uint32_t g_baro_reference_samples;
volatile double g_baro_reference_std_pa;
volatile uint32_t g_baro_reference_restart_count;
volatile double g_baro_altitude_relative_m;
volatile double g_baro_altitude_filtered_m;
volatile double g_baro_altitude_estimated_m;
volatile double g_baro_altitude_estimated_filtered_m;
volatile uint32_t g_baro_estimated_valid;
volatile uint32_t g_baro_valid;
volatile uint32_t g_baro_zero_request;

static uint32_t warmup_complete;
static uint32_t have_good_sample;
static uint32_t last_good_us;
static uint32_t window_start_us;
static uint32_t filter_initialized;
static uint32_t estimated_time_us;
static double reference_mean;
static double reference_m2;
static double reference_height_m;

static double PressureHeight(double pressure_pa)
{
    return BARO_HEIGHT_SCALE_M *
           (1.0 - pow(pressure_pa / BARO_STANDARD_PRESSURE_PA, BARO_PRESSURE_EXPONENT));
}

static void ClearCapture(void)
{
    g_baro_reference_samples = 0U;
    g_baro_state_elapsed_ms = 0U;
    reference_mean = reference_m2 = 0.0;
}

void BarometricAltitude_Init(void)
{
    g_baro_state = BARO_WAITING;
    g_baro_reference_ready = 0U;
    g_baro_reference_pa = g_baro_reference_std_pa = 0.0;
    g_baro_reference_restart_count = 0U;
    g_baro_altitude_relative_m = g_baro_altitude_filtered_m = 0.0;
    g_baro_altitude_estimated_m = g_baro_altitude_estimated_filtered_m = 0.0;
    g_baro_estimated_valid = estimated_time_us = 0U;
    g_baro_valid = g_baro_zero_request = 0U;
    warmup_complete = have_good_sample = filter_initialized = 0U;
    last_good_us = window_start_us = 0U;
    reference_height_m = 0.0;
    ClearCapture();
}

void BarometricAltitude_Invalidate(void)
{
    g_baro_estimated_valid = 0U;
    g_baro_valid = 0U;
    filter_initialized = have_good_sample = 0U;
    if (g_baro_reference_ready == 0U) {
        if (g_baro_reference_samples != 0U) {
            ++g_baro_reference_restart_count;
        }
        ClearCapture();
        g_baro_state = warmup_complete != 0U ? BARO_CAPTURING : BARO_WAITING;
    }
}

void BarometricAltitude_Poll(uint32_t now_us)
{
    if (g_baro_zero_request != 0U) {
        g_baro_zero_request = 0U;
        g_baro_reference_ready = g_baro_valid = 0U;
        g_baro_reference_pa = g_baro_reference_std_pa = 0.0;
        g_baro_altitude_relative_m = g_baro_altitude_filtered_m = 0.0;
        reference_height_m = 0.0;
        filter_initialized = have_good_sample = 0U;
        ClearCapture();
        g_baro_state = warmup_complete != 0U ? BARO_CAPTURING : BARO_WAITING;
    }
    if (g_baro_estimated_valid != 0U && (uint32_t)(now_us - estimated_time_us) >= BARO_STALE_US) {
        BarometricAltitude_Invalidate();
    }
}

void BarometricAltitude_Update(double pressure_pa, uint32_t timestamp_us)
{
    uint32_t dt_us = 0U;
    if (!isfinite(pressure_pa) || pressure_pa < BARO_MIN_PRESSURE_PA ||
        pressure_pa > BARO_MAX_PRESSURE_PA) {
        BarometricAltitude_Invalidate();
        return;
    }
    if (have_good_sample != 0U) {
        dt_us = timestamp_us - last_good_us;
        if (dt_us == 0U || dt_us > BARO_MAX_SAMPLE_GAP_US) {
            BarometricAltitude_Invalidate();
            return;
        }
    } else if (warmup_complete == 0U) {
        window_start_us = timestamp_us;
        g_baro_state = BARO_WARMUP;
    }
    last_good_us = timestamp_us;
    have_good_sample = 1U;

    const double estimated = PressureHeight(pressure_pa);
    if (!isfinite(estimated)) {
        BarometricAltitude_Invalidate();
        return;
    }
    if (g_baro_estimated_valid == 0U) {
        g_baro_altitude_estimated_filtered_m = estimated;
    } else {
        const uint32_t estimated_dt = timestamp_us - estimated_time_us;
        const double alpha = (double)estimated_dt / (BARO_FILTER_TAU_US + (double)estimated_dt);
        g_baro_altitude_estimated_filtered_m += alpha * (estimated - g_baro_altitude_estimated_filtered_m);
    }
    g_baro_altitude_estimated_m = estimated;
    estimated_time_us = timestamp_us;
    g_baro_estimated_valid = 1U;

    if (warmup_complete == 0U) {
        const uint32_t elapsed = timestamp_us - window_start_us;
        g_baro_state_elapsed_ms = elapsed / 1000U;
        if (elapsed < BARO_WARMUP_US) {
            return;
        }
        warmup_complete = 1U;
        ClearCapture();
        g_baro_state = BARO_CAPTURING;
    }

    if (g_baro_reference_ready == 0U) {
        if (g_baro_reference_samples == 0U) {
            window_start_us = timestamp_us;
        }
        /* Welford: bounded memory, stable mean and sample standard deviation.
         * Samples are never clipped, and no stationarity claim is inferred.
         */
        const uint32_t count = g_baro_reference_samples + 1U;
        const double delta = pressure_pa - reference_mean;
        reference_mean += delta / (double)count;
        reference_m2 += delta * (pressure_pa - reference_mean);
        g_baro_reference_samples = count;
        const uint32_t elapsed = timestamp_us - window_start_us;
        g_baro_state_elapsed_ms = elapsed / 1000U;
        if (elapsed < BARO_CAPTURE_US || count < 2U) {
            return;
        }
        g_baro_reference_pa = reference_mean;
        g_baro_reference_std_pa = sqrt(reference_m2 / (double)(count - 1U));
        reference_height_m = PressureHeight(reference_mean);
        g_baro_reference_ready = 1U;
        g_baro_state = BARO_READY;
    }

    const double height = estimated - reference_height_m;
    if (!isfinite(height)) {
        BarometricAltitude_Invalidate();
        return;
    }
    g_baro_altitude_relative_m = height;
    if (filter_initialized == 0U) {
        /* Seed from the first actual height, also after an acquisition fault. */
        g_baro_altitude_filtered_m = height;
        filter_initialized = 1U;
    } else {
        /* First-order low-pass, backward Euler, tau = 0.5 s. Actual dt from
         * clean sample timestamps, not an assumed 200 Hz; no exp in the loop.
         */
        const double alpha = (double)dt_us / (BARO_FILTER_TAU_US + (double)dt_us);
        g_baro_altitude_filtered_m += alpha * (height - g_baro_altitude_filtered_m);
    }
    g_baro_valid = 1U;
}
