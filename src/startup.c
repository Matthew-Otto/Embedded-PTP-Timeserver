#include <stdint.h>

#include "mcu.h"
#include "schedule.h"
#include "heap.h"
#include "clocks.h"
#include "gpio.h"
#include "ethernet.h"

extern uint32_t __stack_top;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern void main(void);

void init_fpu(void) {
    SCB_Type *scb = SCB;

    // CP10 and CP11 Full Access
    uint32_t cfg = 0;
    cfg |= (0x3 << 20);
    cfg |= (0x3 << 22);
    WRITE_REG(scb->CPACR, cfg);
}

void reset_handler(void) {
    __disable_irq();
    // Copy .data section from FLASH to SRAM
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;

    // zero initialize .bss
    dst = &_sbss;
    while (dst < &_ebss) *dst++ = 0;

    // initialize HW
    init_sysclk();
    GPIO_init();
    ETH_init();

    // init OS functions
    heap_init();

    // jump to main
    main();
}


void hardfault_handler(void){
    typedef enum {UNKNOWN, STACK_OVERFLOW, STACK_OOB} ERROR;
    volatile ERROR e = UNKNOWN;
    uint32_t *stack_ptr;
    uint32_t lr_value;

    /* Read active stack pointer and LR (EXC_RETURN) */
    __asm volatile(
        "TST lr, #4\n"
        "ITE EQ\n"
        "MRSEQ %[sp], MSP\n"
        "MRSNE %[sp], PSP\n"
        "MOV %[lr], lr\n"
        : [sp]"=r"(stack_ptr), [lr]"=r"(lr_value)
        :
        : "memory"
    );

    volatile uint32_t r0  = stack_ptr[0];
    volatile uint32_t r1  = stack_ptr[1];
    volatile uint32_t r2  = stack_ptr[2];
    volatile uint32_t r3  = stack_ptr[3];
    volatile uint32_t r12 = stack_ptr[4];
    volatile uint32_t lr  = stack_ptr[5];
    volatile uint32_t pc  = stack_ptr[6];
    volatile uint32_t psr = stack_ptr[7];
    volatile uint32_t sp  = (uint32_t)stack_ptr;
    volatile uint32_t exc_return = lr_value;

    // TODO programatically determine error that caused hardfault
    //if (!(stack_pointer < 0x20000000 || stack_pointer > 0x20007FFF)) e = STACK_OOB;

    //if (*sp != 0xdeadbeef) e = STACK_OVERFLOW;

    // hardware breakpoint
    __asm volatile("BKPT #0");
    while (1);
}

void default_fault_isr(void) {
    while (1);
}

void default_isr(void) {
    while (1);
}

// forward declaration of ISRs
void NMI_handler(void) __attribute__((weak, alias ("default_fault_isr")));
void memmanage_handler(void) __attribute__((weak, alias ("default_fault_isr")));
void busfault_handler(void) __attribute__((weak, alias ("default_fault_isr")));
void usagefault_handler(void) __attribute__((weak, alias ("default_fault_isr")));
void securefault_handler(void) __attribute__((weak, alias ("default_fault_isr")));

extern void SVC_handler(void) __attribute__((weak, alias ("default_isr")));
extern void pendSV_handler(void) __attribute__((weak, alias ("default_isr")));
extern void systick_handler(void) __attribute__((weak, alias ("default_isr")));
extern void WWDG_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void PVD_AVD_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void RTC_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void RTC_S_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TAMP_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void RAMCFG_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void FLASH_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void FLASH_S_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GTZC_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void RCC_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void RCC_S_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI0_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI2_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI3_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI4_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI5_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI6_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI7_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI8_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI9_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI10_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI11_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI12_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI13_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI14_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void EXTI15_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA1_Channel0_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA1_Channel1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA1_Channel2_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA1_Channel3_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA1_Channel4_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA1_Channel5_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA1_Channel6_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA1_Channel7_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void IWDG_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void ADC1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void DAC1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void FDCAN1_IT0_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void FDCAN1_IT1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM1_BRK_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM1_UP_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM1_TRG_COM_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM1_CC_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM2_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM3_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM4_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM5_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM6_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM7_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void I2C1_EV_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void I2C1_ER_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void I2C2_EV_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void I2C2_ER_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void SPI1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void SPI2_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void SPI3_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void USART1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void USART2_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void USART3_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void UART4_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void UART5_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void LPUART1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void LPTIM1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM8_BRK_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM8_UP_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM8_TRG_COM_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM8_CC_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void ADC2_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void LPTIM2_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM15_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM16_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM17_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void USB_DRD_FS_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void CRS_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void UCPD1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void FMC_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void OCTOSPI1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void SDMMC1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void I2C3_EV_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void I2C3_ER_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void SPI4_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void SPI5_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void SPI6_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void USART6_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void USART10_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void USART11_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void SAI1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void SAI2_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA2_Channel0_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA2_Channel1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA2_Channel2_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA2_Channel3_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA2_Channel4_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA2_Channel5_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA2_Channel6_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void GPDMA2_Channel7_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void UART7_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void UART8_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void UART9_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void UART12_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void SDMMC2_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void FPU_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void ICACHE_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void DCACHE1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void ETH_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void ETH_WKUP_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void DCMI_PSSI_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void FDCAN2_IT0_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void FDCAN2_IT1_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void CORDIC_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void FMAC_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void DTS_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void RNG_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void HASH_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void CEC_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM12_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM13_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void TIM14_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void I3C1_EV_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void I3C1_ER_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void I2C4_EV_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void I2C4_ER_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void LPTIM3_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void LPTIM4_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void LPTIM5_IRQHandler(void) __attribute__((weak, alias ("default_isr")));
extern void LPTIM6_IRQHandler(void) __attribute__((weak, alias ("default_isr")));

/* Vector table */
__attribute__((section(".isr_vector")))
void (* const vector_table[])(void) = {
    ((void (*)(void))(&__stack_top)),
    reset_handler,
    NMI_handler,
    hardfault_handler,
    memmanage_handler,
    busfault_handler,
    usagefault_handler,
    securefault_handler,
    0,
    0,
    0,
    SVC_handler,
    0,
    0,
    pendSV_handler,
    systick_handler,
    WWDG_IRQHandler,
    PVD_AVD_IRQHandler,
    RTC_IRQHandler,
    RTC_S_IRQHandler,
    TAMP_IRQHandler,
    RAMCFG_IRQHandler,
    FLASH_IRQHandler,
    FLASH_S_IRQHandler,
    GTZC_IRQHandler,
    RCC_IRQHandler,
    RCC_S_IRQHandler,
    EXTI0_IRQHandler,
    EXTI1_IRQHandler,
    EXTI2_IRQHandler,
    EXTI3_IRQHandler,
    EXTI4_IRQHandler,
    EXTI5_IRQHandler,
    EXTI6_IRQHandler,
    EXTI7_IRQHandler,
    EXTI8_IRQHandler,
    EXTI9_IRQHandler,
    EXTI10_IRQHandler,
    EXTI11_IRQHandler,
    EXTI12_IRQHandler,
    EXTI13_IRQHandler,
    EXTI14_IRQHandler,
    EXTI15_IRQHandler,
    GPDMA1_Channel0_IRQHandler,
    GPDMA1_Channel1_IRQHandler,
    GPDMA1_Channel2_IRQHandler,
    GPDMA1_Channel3_IRQHandler,
    GPDMA1_Channel4_IRQHandler,
    GPDMA1_Channel5_IRQHandler,
    GPDMA1_Channel6_IRQHandler,
    GPDMA1_Channel7_IRQHandler,
    IWDG_IRQHandler,
    0,
    ADC1_IRQHandler,
    DAC1_IRQHandler,
    FDCAN1_IT0_IRQHandler,
    FDCAN1_IT1_IRQHandler,
    TIM1_BRK_IRQHandler,
    TIM1_UP_IRQHandler,
    TIM1_TRG_COM_IRQHandler,
    TIM1_CC_IRQHandler,
    TIM2_IRQHandler,
    TIM3_IRQHandler,
    TIM4_IRQHandler,
    TIM5_IRQHandler,
    TIM6_IRQHandler,
    TIM7_IRQHandler,
    I2C1_EV_IRQHandler,
    I2C1_ER_IRQHandler,
    I2C2_EV_IRQHandler,
    I2C2_ER_IRQHandler,
    SPI1_IRQHandler,
    SPI2_IRQHandler,
    SPI3_IRQHandler,
    USART1_IRQHandler,
    USART2_IRQHandler,
    USART3_IRQHandler,
    UART4_IRQHandler,
    UART5_IRQHandler,
    LPUART1_IRQHandler,
    LPTIM1_IRQHandler,
    TIM8_BRK_IRQHandler,
    TIM8_UP_IRQHandler,
    TIM8_TRG_COM_IRQHandler,
    TIM8_CC_IRQHandler,
    ADC2_IRQHandler,
    LPTIM2_IRQHandler,
    TIM15_IRQHandler,
    TIM16_IRQHandler,
    TIM17_IRQHandler,
    USB_DRD_FS_IRQHandler,
    CRS_IRQHandler,
    UCPD1_IRQHandler,
    FMC_IRQHandler,
    OCTOSPI1_IRQHandler,
    SDMMC1_IRQHandler,
    I2C3_EV_IRQHandler,
    I2C3_ER_IRQHandler,
    SPI4_IRQHandler,
    SPI5_IRQHandler,
    SPI6_IRQHandler,
    USART6_IRQHandler,
    USART10_IRQHandler,
    USART11_IRQHandler,
    SAI1_IRQHandler,
    SAI2_IRQHandler,
    GPDMA2_Channel0_IRQHandler,
    GPDMA2_Channel1_IRQHandler,
    GPDMA2_Channel2_IRQHandler,
    GPDMA2_Channel3_IRQHandler,
    GPDMA2_Channel4_IRQHandler,
    GPDMA2_Channel5_IRQHandler,
    GPDMA2_Channel6_IRQHandler,
    GPDMA2_Channel7_IRQHandler,
    UART7_IRQHandler,
    UART8_IRQHandler,
    UART9_IRQHandler,
    UART12_IRQHandler,
    SDMMC2_IRQHandler,
    FPU_IRQHandler,
    ICACHE_IRQHandler,
    DCACHE1_IRQHandler,
    ETH_IRQHandler,
    ETH_WKUP_IRQHandler,
    DCMI_PSSI_IRQHandler,
    FDCAN2_IT0_IRQHandler,
    FDCAN2_IT1_IRQHandler,
    CORDIC_IRQHandler,
 	FMAC_IRQHandler,
    DTS_IRQHandler,
    RNG_IRQHandler,
    0,
    0,
    HASH_IRQHandler,
    0,
    CEC_IRQHandler,
    TIM12_IRQHandler,
    TIM13_IRQHandler,
    TIM14_IRQHandler,
    I3C1_EV_IRQHandler,
    I3C1_ER_IRQHandler,
    I2C4_EV_IRQHandler,
    I2C4_ER_IRQHandler,
    LPTIM3_IRQHandler,
    LPTIM4_IRQHandler,
    LPTIM5_IRQHandler,
    LPTIM6_IRQHandler
};
