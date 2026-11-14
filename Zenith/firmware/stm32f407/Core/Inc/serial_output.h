#ifndef ZENITH_SERIAL_OUTPUT_H
#define ZENITH_SERIAL_OUTPUT_H

#include <stdint.h>

extern volatile uint32_t g_usb_ready;
extern volatile uint32_t g_usb_error;
extern volatile uint32_t g_usb_mounted;
extern volatile uint32_t g_usb_connected;
extern volatile uint32_t g_usb_irq_count;
extern volatile uint32_t g_usb_frame_count;
extern volatile uint32_t g_usb_drop_count;
extern volatile uint32_t g_usb_zero_count;
extern volatile uint32_t g_usb_command_error_count;

void SerialOutput_Init(void);
void SerialOutput_Poll(void);
void OTG_FS_IRQHandler(void);

#endif
