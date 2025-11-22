#include <stdlib.h>
#include "fifo.h"
#include "heap.h"

FIFO8_t *fifo8_init(uint32_t size) {
    size_t fifo_size = sizeof(FIFO8_t) + size * sizeof(uint8_t);

    FIFO8_t *fifo = (FIFO8_t*)malloc(fifo_size);
    if (fifo == NULL) return NULL;

    init_semaphore(&fifo->full, fifo_size);
    init_semaphore(&fifo->empty, 0);

    fifo->head = 0;
    fifo->tail = 0;
    fifo->size = size;
    return fifo;
}

void fifo8_put(FIFO8_t *fifo, uint8_t data) {
    c_wait(&fifo->full);

    fifo->data[fifo->tail] = data;
    fifo->tail = (fifo->tail + 1) % fifo->size;
    
    c_signal(&fifo->empty);
}

void fifo8_get(FIFO8_t *fifo, uint8_t *data) {
    c_wait(&fifo->empty);

    *data = fifo->data[fifo->head];
    fifo->head = (fifo->head + 1) % fifo->size;

    c_signal(&fifo->full);
}

uint16_t fifo8_size(FIFO8_t *fifo) {
    if (fifo->tail >= fifo->head)
        return fifo->tail - fifo->head;
    else
        return fifo->size - (fifo->head - fifo->tail);
}
