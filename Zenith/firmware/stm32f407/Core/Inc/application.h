#ifndef ZENITH_APPLICATION_H
#define ZENITH_APPLICATION_H

#include "bmp388.h"

/* Globals for SWD Watch; latest probe and cumulative counters since reset. */
extern volatile uint32_t g_bmp388_chip_id;
extern volatile uint32_t g_bmp388_probe_ok;
extern volatile uint32_t g_bmp388_last_result;
extern volatile uint32_t g_bmp388_probe_count;
extern volatile uint32_t g_bmp388_success_count;
extern volatile uint32_t g_bmp388_error_count;
extern volatile uint32_t g_bmp388_error_reg;
extern volatile uint32_t g_bmp388_error_flags_seen;
extern volatile uint32_t g_bmp388_status;

enum bmp388_sample_flags {
    BMP388_SAMPLE_VALID = 1U,
    BMP388_TIME_UNCERTAIN = 2U,
    BMP388_EVENT_BACKLOG = 4U,
    BMP388_LONG_PERIOD = 8U,
    BMP388_SAMPLE_STALE = 16U,
    BMP388_SAMPLE_OUT_OF_RANGE = 32U
};

extern volatile uint32_t g_bmp388_state; /* 0 boot, 1 reset wait, 2 running, 3 retry */
extern volatile uint32_t g_bmp388_calibration_valid;
extern volatile uint32_t g_bmp388_configured;
extern volatile bmp388_registers_t g_bmp388_registers;
extern volatile double g_bmp388_pressure_pa;
extern volatile double g_bmp388_temperature_c;
extern volatile uint32_t g_bmp388_raw_pressure;
extern volatile uint32_t g_bmp388_raw_temperature;
extern volatile uint32_t g_bmp388_sample_valid;
extern volatile uint32_t g_bmp388_sample_flags;
extern volatile uint32_t g_bmp388_sample_count;
extern volatile uint32_t g_bmp388_frame_count;
extern volatile uint32_t g_bmp388_irq_count;
extern volatile uint32_t g_bmp388_irq_time_us;
extern volatile uint32_t g_bmp388_sample_time_us;
extern volatile uint32_t g_bmp388_period_us;
extern volatile uint32_t g_bmp388_service_latency_us;
extern volatile uint32_t g_bmp388_read_duration_us;
extern volatile uint32_t g_bmp388_event_overrun_count;
extern volatile uint32_t g_bmp388_irq_gap_count;
extern volatile uint32_t g_bmp388_no_data_count;
extern volatile uint32_t g_bmp388_data_timeout_count;
extern volatile uint32_t g_bmp388_invalid_sample_count;
extern volatile uint32_t g_bmp388_int_status;
extern volatile double g_bmp388_irq_rate_hz;
extern volatile double g_bmp388_sample_rate_hz;
extern volatile uint32_t g_bmp388_rate_valid;

void Application_Init(bmp388_read_fn read, bmp388_write_fn write, uint32_t tick);
/* Only a timestamp and sequence number are published by the PB0 ISR. */
void Application_DataReadyISR(uint32_t timestamp_us);
/* Call from main, never from an ISR. tick is the existing 1 kHz TIM2 count. */
void Application_Poll(uint32_t tick);

#endif
