#ifndef ZENITH_BAROMETRIC_ALTITUDE_H
#define ZENITH_BAROMETRIC_ALTITUDE_H

#include <stdint.h>

enum barometric_altitude_state {
    BARO_WAITING = 0,
    BARO_WARMUP = 1,
    BARO_CAPTURING = 2,
    BARO_READY = 3
};

extern volatile uint32_t g_baro_state;
extern volatile uint32_t g_baro_state_elapsed_ms;
extern volatile uint32_t g_baro_reference_ready;
extern volatile double g_baro_reference_pa;
extern volatile uint32_t g_baro_reference_samples;
extern volatile double g_baro_reference_std_pa;
extern volatile uint32_t g_baro_reference_restart_count;
extern volatile double g_baro_altitude_relative_m;
extern volatile double g_baro_altitude_filtered_m;
/* Standard-pressure altitude, not surveyed elevation or corrected QNH. */
extern volatile double g_baro_altitude_estimated_m;
extern volatile double g_baro_altitude_estimated_filtered_m;
extern volatile uint32_t g_baro_estimated_valid;
extern volatile uint32_t g_baro_valid;
/* Write 1 in Watch. Main consumes it; no sensor/MCU reset is performed. */
extern volatile uint32_t g_baro_zero_request;

/* All functions run in main, never in an ISR. Times are modular TIM5 us. */
void BarometricAltitude_Init(void);
void BarometricAltitude_Poll(uint32_t now_us);
/* Exactly once per fresh, numerically valid, temporally clean pressure sample.
 * The caller rejects backlog, uncertain timestamps and long-period flags.
 */
void BarometricAltitude_Update(double pressure_pa, uint32_t timestamp_us);
/* Invalidates the current height and unfinished capture, preserving a fixed
 * committed reference. The next clean sample reinitializes the filter.
 */
void BarometricAltitude_Invalidate(void);

#endif
