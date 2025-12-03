#include <stdlib.h>
#include "fifo.h"
#include "heap.h"

FIFO_t *fifo_init(uint32_t size) {
    size_t fifo_size = sizeof(FIFO_t) + size * sizeof(uint8_t);

    FIFO_t *fifo = (FIFO_t*)malloc(fifo_size);
    if (fifo == NULL) return NULL;

    init_semaphore(&fifo->mutex, 0);
    init_semaphore(&fifo->full, size);
    init_semaphore(&fifo->empty, 0);

    fifo->head = 0;
    fifo->tail = 0;
    fifo->size = size;
    return fifo;
}

void fifo_put(FIFO_t *fifo, uint8_t data) {
    c_wait(&fifo->full);

    fifo->data[fifo->tail] = data;
    fifo->tail = (fifo->tail + 1) % fifo->size;
    
    c_signal(&fifo->empty);
}

int fifo_put_nonblock(FIFO_t *fifo, uint8_t data) {
    if (fifo->full.value == 0) return -1;
    fifo_put(fifo, data);
}

void fifo_get(FIFO_t *fifo, uint8_t *data) {
    c_wait(&fifo->empty);

    *data = fifo->data[fifo->head];
    fifo->head = (fifo->head + 1) % fifo->size;

    c_signal(&fifo->full);
}

int32_t fifo_size(FIFO_t *fifo) {
    return fifo->empty.value;
}
