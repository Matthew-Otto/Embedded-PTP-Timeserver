#ifndef FIFO_H
#define FIFO_H

#include <stdint.h>
#include "semaphore.h"

typedef struct {
    semaphore_t full;
    semaphore_t empty;
    semaphore_t mutex;
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint8_t  data[];
} FIFO_t;

FIFO_t* fifo_init(uint32_t size);
void fifo_put(FIFO_t *fifo, uint8_t data);
int fifo_put_nonblock(FIFO_t *fifo, uint8_t data);
void fifo_get(FIFO_t *fifo, uint8_t *data);
int32_t fifo_size(FIFO_t *fifo);

#endif // FIFO_H