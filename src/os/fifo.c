#include <stdlib.h>
#include <string.h>
#include "fifo.h"
#include "heap.h"


// Multithread safe general purpose mutex
FIFO_t *fifo_init(uint32_t size, uint32_t elem_size) {
    FIFO_t *fifo = (FIFO_t*)malloc(sizeof(FIFO_t));
    if (fifo == NULL) return NULL;
    fifo->data = (uint8_t *)malloc(size * elem_size);
    if (fifo->data == NULL) return NULL;

    init_semaphore(&fifo->mutex, 0);
    init_semaphore(&fifo->full, size);
    init_semaphore(&fifo->empty, 0);

    fifo->head = 0;
    fifo->tail = 0;
    fifo->size = size;
    fifo->elem_size = elem_size;
    return fifo;
}

void fifo_put(FIFO_t *fifo, const void *value) {
    c_wait(&fifo->full);
    b_wait(&fifo->mutex);

    void *addr = fifo->data + (fifo->tail * fifo->elem_size);
    memcpy(addr, value, fifo->elem_size);
    fifo->tail = (fifo->tail + 1) % fifo->size;
    
    b_signal(&fifo->mutex);
    c_signal(&fifo->empty);
}

void fifo_get(FIFO_t *fifo, void *value) {
    c_wait(&fifo->empty);
    b_wait(&fifo->mutex);

    void *addr = fifo->data + (fifo->head * fifo->elem_size);
    memcpy(value, addr, fifo->elem_size);
    fifo->head = (fifo->head + 1) % fifo->size;
    
    b_signal(&fifo->mutex);
    c_signal(&fifo->full);
}

uint32_t fifo_size(FIFO_t *fifo) {
    return fifo->empty.value;
}



// byte fifo (not safe for multi-producer, multi-consumer)
FIFO8_t *fifo8_init(uint32_t size) {
    size_t fifo_size = sizeof(FIFO8_t) + size * sizeof(uint8_t);

    FIFO8_t *fifo = (FIFO8_t*)malloc(fifo_size);
    if (fifo == NULL) return NULL;

    init_semaphore(&fifo->full, size);
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

int fifo8_put_nonblock(FIFO8_t *fifo, uint8_t data) {
    if (fifo->full.value == 0) return -1;
    fifo8_put(fifo, data);
    return 0;
}

void fifo8_get(FIFO8_t *fifo, uint8_t *data) {
    c_wait(&fifo->empty);

    *data = fifo->data[fifo->head];
    fifo->head = (fifo->head + 1) % fifo->size;

    c_signal(&fifo->full);
}

int fifo8_get_nonblock(FIFO8_t *fifo, uint8_t *data) {
    if (fifo->empty.value == 0) return -1;
    fifo8_get(fifo, data);
    return 0;
}

int32_t fifo8_size(FIFO8_t *fifo) {
    return fifo->empty.value;
}

void fifo8_clear(FIFO8_t *fifo) {
    reset_semaphore(&fifo->full, fifo->size);
    reset_semaphore(&fifo->empty, 0);

    fifo->head = 0;
    fifo->tail = 0;
}
