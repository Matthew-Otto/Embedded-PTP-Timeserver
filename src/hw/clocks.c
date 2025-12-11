#include "mcu.h"
#include "clocks.h"

void init_sysclk(void) {
    ////// Configure Oscillators //////
    // Configure the main internal regulator output voltage
    MODIFY_REG(PWR->VOSCR, PWR_VOSCR_VOS, (PWR_VOSCR_VOS));
    // Delay after an RCC peripheral clock enabling
    (void)READ_BIT(PWR->VOSCR, PWR_VOSCR_VOS);
    while (READ_BIT(PWR->VOSSR, PWR_VOSSR_VOSRDY) != PWR_VOSSR_VOSRDY);

    // Set HSE bypass
    //SET_BIT(RCC->CR, RCC_CR_HSEBYP);
    //CLEAR_BIT(RCC->CR, RCC_CR_HSEEXT);
    
    // Disable HSE bypass
    CLEAR_BIT(RCC->CR, RCC_CR_HSEBYP);
    // Enable HSE
    SET_BIT(RCC->CR, RCC_CR_HSEON);
    // Wait for HSE ready
    while (READ_BIT(RCC->CR, RCC_CR_HSERDY) == 0U);


    // Disable PLL1
    CLEAR_BIT(RCC->CR, RCC_CR_PLL1ON);
    while (READ_BIT(RCC->CR, RCC_CR_PLL1RDY) != 0U);

    // Configure the PLL1 clock source, multiplication and division factors.
    uint32_t PLLSource = (RCC_PLL1CFGR_PLL1SRC_0 | RCC_PLL1CFGR_PLL1SRC_1);
    uint32_t PLLM = 1;   // pre-div1
    uint32_t PLLN = 20; // mult
    uint32_t PLLP = 2;   // post-div
    uint32_t PLLQ = 2;   // post-div
    uint32_t PLLR = 2;   // post-div
    MODIFY_REG(RCC->PLL1CFGR, (RCC_PLL1CFGR_PLL1SRC | RCC_PLL1CFGR_PLL1M), (PLLSource << RCC_PLL1CFGR_PLL1SRC_Pos) |
            (PLLM << RCC_PLL1CFGR_PLL1M_Pos));
    WRITE_REG(RCC->PLL1DIVR , (((PLLN - 1U ) & RCC_PLL1DIVR_PLL1N) | (((PLLP - 1U ) << RCC_PLL1DIVR_PLL1P_Pos) & RCC_PLL1DIVR_PLL1P) | 
            (((PLLQ - 1U) << RCC_PLL1DIVR_PLL1Q_Pos) & RCC_PLL1DIVR_PLL1Q) | (((PLLR - 1U) << RCC_PLL1DIVR_PLL1R_Pos) & RCC_PLL1DIVR_PLL1R)));

    // Disable PLL1FRACN
    CLEAR_BIT(RCC->PLL1CFGR, RCC_PLL1CFGR_PLL1FRACEN);
    // Configure PLL PLL1FRACR
    //WRITE_REG(RCC->PLL1FRACR, 1968 << RCC_PLL1FRACR_PLL1FRACN_Pos);
    // Enable PLL1FRACN
    //SET_BIT(RCC->PLL1CFGR, RCC_PLL1CFGR_PLL1FRACEN);
    // Select PLL1 input reference frequency range: VCI
    MODIFY_REG(RCC->PLL1CFGR, RCC_PLL1CFGR_PLL1RGE, RCC_PLL1CFGR_PLL1RGE_0);
    // Select PLL1 output frequency range : VCO
    MODIFY_REG(RCC->PLL1CFGR, RCC_PLL1CFGR_PLL1VCOSEL, 0);
    // Enable PLL1 System Clock output
    SET_BIT(RCC->PLL1CFGR, RCC_PLL1CFGR_PLL1PEN);
    // Enable PLL1 DIVQ divier output (for clk_ptp_ref_i)
    SET_BIT(RCC->PLL1CFGR, RCC_PLL1CFGR_PLL1QEN);

    // Enable PLL1
    SET_BIT(RCC->CR, RCC_CR_PLL1ON);
    while (READ_BIT(RCC->CR, RCC_CR_PLL1RDY) == 0U);


    ////// Initializes the CPU, AHB and APB buses clocks //////

    // Increase the number of wait states because of higher CPU frequency
    MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_5WS);
    // Increase the BUS frequency divider
    // PCLK3
    MODIFY_REG(RCC->CFGR2, RCC_CFGR2_PPRE3, (RCC_HCLK_DIV1 << 8));
    // PCLK2
    MODIFY_REG(RCC->CFGR2, RCC_CFGR2_PPRE2, (RCC_HCLK_DIV1 << 4));
    // PCLK1
    MODIFY_REG(RCC->CFGR2, RCC_CFGR2_PPRE1, RCC_HCLK_DIV1);
    // HCLK
    MODIFY_REG(RCC->CFGR2, RCC_CFGR2_HPRE, RCC_SYSCLK_DIV1);
    // SYSCLK
    MODIFY_REG(RCC->CFGR1, RCC_CFGR1_SW, RCC_SYSCLKSOURCE_PLLCLK);
    while ((uint32_t)(RCC->CFGR1 & RCC_CFGR1_SWS) != (RCC_CFGR1_SWS_0 | RCC_CFGR1_SWS_1));
}
