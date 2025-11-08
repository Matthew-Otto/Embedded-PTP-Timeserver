#include "mcu.h"

void suspend(void) {
    __WFI();
}