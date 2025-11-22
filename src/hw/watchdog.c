#include "mcu.h"

void init_watchdog(void) {
    //WRITE_BIT(DBGMCU->APB1FZR1, DBGMCU_APB1FZR1_DBG_IWDG_STOP); // disable watchdog in debug
    //CLEAR_BIT(DBGMCU->APB1FZR1, DBGMCU_APB1FZR1_DBG_IWDG_STOP); // disable watchdog in debug
    WRITE_REG(IWDG->KR, 0x5555);
    while (IWDG->SR & 0xF); // wait for pending updates to complete
    WRITE_REG(IWDG->KR, 0xCCCC);
}

void kick_watchdog(void) {
    WRITE_REG(IWDG->KR, 0xAAAA);
}
