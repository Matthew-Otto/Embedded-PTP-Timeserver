#ifndef DEBUG_H
#define DEBUG_H

typedef struct {
    GPIO_TypeDef *bank;
    uint16_t pin;
} pindef_t;

void init_debug_pins(void);
void debug_toggle(int idx);

#endif // DEBUG_H