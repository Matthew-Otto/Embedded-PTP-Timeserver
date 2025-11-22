#ifndef FIFO_H
#define FIFO_H

#include <stdint.h>
#include "semaphore.h"

typedef struct {
    semaphore_t full;
    semaphore_t empty;
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint8_t  data[];
} FIFO8_t;

FIFO8_t* fifo8_init(uint32_t size);
void fifo8_put(FIFO8_t *fifo, uint8_t data);
void fifo8_get(FIFO8_t *fifo, uint8_t *data);
uint16_t fifo8_size(FIFO8_t *fifo);

#endif // FIFO_H