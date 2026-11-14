#include "serial_output.h"
#include "application.h"
#include "barometric_altitude.h"
#include "main.h"

#include "stm32f4xx.h"
#include "tusb.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TELEMETRY_PERIOD_US 50000U
#define COMMAND_LIMIT 16U

volatile uint32_t g_usb_ready;
volatile uint32_t g_usb_error;
volatile uint32_t g_usb_mounted;
volatile uint32_t g_usb_connected;
volatile uint32_t g_usb_irq_count;
volatile uint32_t g_usb_frame_count;
volatile uint32_t g_usb_drop_count;
volatile uint32_t g_usb_zero_count;
volatile uint32_t g_usb_command_error_count;

static uint32_t last_tx_us;
static uint32_t sequence;
static uint32_t command_length;
static uint32_t command_overflow;
static char command[COMMAND_LIMIT];

/* Private development VID/PID, not a production USB identity. */
static const tusb_desc_device_t device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200, .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON, .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE, .idVendor = 0xCAFE, .idProduct = 0x4001,
    .bcdDevice = 0x0100, .iManufacturer = 1, .iProduct = 2,
    .iSerialNumber = 3, .bNumConfigurations = 1
};
enum { USB_CONFIG_LENGTH = TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN };
static const uint8_t configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, USB_CONFIG_LENGTH, 0, 100),
    TUD_CDC_DESCRIPTOR(0, 4, 0x81, 8, 0x02, 0x82, 64)
};

uint8_t const *tud_descriptor_device_cb(void) { return (uint8_t const *)&device_descriptor; }
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    return index == 0U ? configuration_descriptor : NULL;
}
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    static uint16_t descriptor[64];
    static char serial[25];
    (void)langid;
    const char *value;
    if (index == 0U) {
        descriptor[0] = (uint16_t)((TUSB_DESC_STRING << 8) | 4U);
        descriptor[1] = 0x0409;
        return descriptor;
    }
    if (index == 1U) value = "ZenithAircraft";
    else if (index == 2U) value = "Zenith Altitudes CDC";
    else if (index == 3U) {
        const volatile uint32_t *uid = (const volatile uint32_t *)UID_BASE;
        (void)snprintf(serial, sizeof(serial), "%08lX%08lX%08lX",
                       (unsigned long)uid[0], (unsigned long)uid[1], (unsigned long)uid[2]);
        value = serial;
    } else if (index == 4U) value = "Altitudes";
    else return NULL;
    size_t length = strlen(value);
    if (length > 63U) length = 63U;
    for (size_t i = 0U; i < length; ++i) descriptor[i + 1U] = (uint8_t)value[i];
    descriptor[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2U * length + 2U));
    return descriptor;
}

uint32_t tusb_time_millis_api(void) { return g_tim2_irq_count; }

void OTG_FS_IRQHandler(void)
{
    ++g_usb_irq_count;
    tud_int_handler(0);
}

void SerialOutput_Init(void)
{
    g_usb_ready = g_usb_error = g_usb_mounted = g_usb_connected = 0U;
    g_usb_irq_count = g_usb_frame_count = g_usb_drop_count = 0U;
    g_usb_zero_count = g_usb_command_error_count = 0U;
    sequence = command_length = command_overflow = 0U;
    last_tx_us = Platform_TimeUs();
    g_usb_error = Platform_USB_Init();
    if (g_usb_error != 0U) return; /* Acquisition still runs if USB clock fails. */
    NVIC_SetPriority(OTG_FS_IRQn, 10U); /* Below sensor EXTI0 (8), above TIM2 (15). */
    if (!tusb_init()) {
        g_usb_error = 4U;
        return;
    }
    g_usb_ready = 1U;
}

static void ReceiveCommands(void)
{
    /* Bounded RX work. Only ZERO is recognized; no reset/flash commands. */
    uint32_t budget = 64U;
    while (budget-- != 0U && tud_cdc_available() != 0U) {
        const char byte = (char)tud_cdc_read_char();
        if (byte == '\r') continue;
        if (byte == '\n') {
            command[command_length] = '\0';
            if (command_overflow == 0U && strcmp(command, "ZERO") == 0) {
                g_baro_zero_request = 1U;
                ++g_usb_zero_count;
            } else {
                ++g_usb_command_error_count;
            }
            command_length = command_overflow = 0U;
        } else if (command_length < COMMAND_LIMIT - 1U && command_overflow == 0U) {
            command[command_length++] = byte;
        } else {
            command_overflow = 1U;
        }
    }
}

void SerialOutput_Poll(void)
{
    if (g_usb_ready == 0U) return;
    tud_task_ext(0U, false);
    g_usb_mounted = tud_mounted() ? 1U : 0U;
    g_usb_connected = tud_cdc_connected() ? 1U : 0U;
    ReceiveCommands();
    const uint32_t now = Platform_TimeUs();
    if ((uint32_t)(now - last_tx_us) < TELEMETRY_PERIOD_US) return;
    last_tx_us = now;
    if (g_usb_connected == 0U) return;
    const uint32_t rel_ok = g_baro_valid != 0U && g_baro_reference_ready != 0U;
    const uint32_t asl_ok = g_baro_estimated_valid;
    char line[512];
    const int length = snprintf(line, sizeof(line),
        "{\"v\":1,\"seq\":%lu,\"ms\":%lu,\"p\":%.2f,\"t\":%.2f,"
        "\"rel\":%.3f,\"asl\":%.3f,\"rel_ok\":%lu,\"asl_ok\":%lu,"
        "\"state\":%lu,\"elapsed\":%lu,\"ref\":%.2f,\"n\":%lu,\"std\":%.2f,"
        "\"hz\":%.1f,\"drops\":%lu,\"err\":%lu,\"overruns\":%lu,\"zero\":%lu}\n",
        (unsigned long)++sequence, (unsigned long)g_tim2_irq_count,
        isfinite(g_bmp388_pressure_pa) ? g_bmp388_pressure_pa : 0.0,
        isfinite(g_bmp388_temperature_c) ? g_bmp388_temperature_c : 0.0,
        rel_ok != 0U ? g_baro_altitude_filtered_m : 0.0,
        asl_ok != 0U ? g_baro_altitude_estimated_filtered_m : 0.0,
        (unsigned long)rel_ok, (unsigned long)asl_ok, (unsigned long)g_baro_state,
        (unsigned long)g_baro_state_elapsed_ms, g_baro_reference_pa,
        (unsigned long)g_baro_reference_samples, g_baro_reference_std_pa,
        g_bmp388_rate_valid != 0U ? g_bmp388_sample_rate_hz : 0.0,
        (unsigned long)g_usb_drop_count, (unsigned long)g_bmp388_error_count,
        (unsigned long)g_bmp388_event_overrun_count, (unsigned long)g_usb_zero_count);
    if (length <= 0 || (size_t)length >= sizeof(line) ||
        tud_cdc_write_available() < (uint32_t)length) {
        ++g_usb_drop_count;
        return;
    }
    if (tud_cdc_write(line, (uint32_t)length) != (uint32_t)length) {
        ++g_usb_drop_count;
        return;
    }
    (void)tud_cdc_write_flush();
    ++g_usb_frame_count;
}
