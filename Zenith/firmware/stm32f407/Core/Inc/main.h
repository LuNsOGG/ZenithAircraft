#ifndef ZENITH_MAIN_H
#define ZENITH_MAIN_H

#include <stdint.h>

#define PLATFORM_HSI_HZ 16000000U
#define PLATFORM_TIMER_HZ 1000U

enum platform_error {
    PLATFORM_ERROR_NONE = 0,
    PLATFORM_ERROR_HSI_TIMEOUT = 1,
    PLATFORM_ERROR_CLOCK_SWITCH_TIMEOUT = 2,
    PLATFORM_ERROR_HARDFAULT = 3
};

/* SWD watch expressions. Values survive optimization, not a reset. */
extern uint32_t SystemCoreClock;
extern volatile uint32_t g_platform_ready;
extern volatile uint32_t g_platform_error;
extern volatile uint32_t g_tim2_irq_count;
extern volatile uint32_t g_main_wake_count;

void SystemInit(void);
void TIM2_IRQHandler(void);
void EXTI0_IRQHandler(void);
void HardFault_Handler(void);
/* Free-running TIM5 at 1 MHz nominal; wraps every ~71.6 min and freezes on halt. */
uint32_t Platform_TimeUs(void);

#endif
