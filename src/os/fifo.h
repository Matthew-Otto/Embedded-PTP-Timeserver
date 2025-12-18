#ifndef FIFO_H
#define FIFO_H

#include <stdint.h>
#include "semaphore.h"

typedef struct {
    semaphore_t full;
    semaphore_t empty;
    semaphore_t mutex;
    uint32_t    head;
    uint32_t    tail;
    uint32_t    size;
    uint32_t    elem_size;
    uint8_t    *data;
} FIFO_t;

FIFO_t *fifo_init(uint32_t size, uint32_t elem_size);
void fifo_put(FIFO_t *fifo, const void *value);
void fifo_get(FIFO_t *fifo, void *value);
uint32_t fifo_size(FIFO_t *fifo);

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
int fifo8_put_nonblock(FIFO8_t *fifo, uint8_t data);
void fifo8_get(FIFO8_t *fifo, uint8_t *data);
int fifo8_get_nonblock(FIFO8_t *fifo, uint8_t *data);
int32_t fifo8_size(FIFO8_t *fifo);
void fifo8_clear(FIFO8_t *fifo);

#endif // FIFO_H