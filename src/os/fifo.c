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



 // TODO organize these

/************* MPMC_FIFO *************/
// blocking fifo with multicore mutex

mFIFO_t *mfifo_init(uint32_t fifo_size, uint8_t element_size) {
    size_t size = sizeof(mFIFO_t) + fifo_size * element_size;

    mFIFO_t *fifo = (mFIFO_t*)malloc(size);
    if (fifo == NULL) return NULL;

    init_semaphore(&fifo->mutex, 0);
    init_semaphore(&fifo->full, fifo_size);
    init_semaphore(&fifo->empty, 0);

    fifo->head = 0;
    fifo->tail = 0;
    fifo->size = fifo_size;
    fifo->element_size = element_size;
    return fifo;
}


void mfifo_put(mFIFO_t *fifo, const void *value) {
    c_wait(&fifo->full);
    b_wait(&fifo->mutex);

    void *addr = (uint8_t *)fifo->data + (fifo->tail * fifo->element_size);
    switch (fifo->element_size) {
        case 1: *(uint8_t *)addr = *(const uint8_t *)value; break;
        case 2: *(uint16_t *)addr = *(const uint16_t *)value; break;
        case 4: *(uint32_t *)addr = *(const uint32_t *)value; break;
        case 8: *(uint64_t *)addr = *(const uint64_t *)value; break;
    }

    fifo->tail = (fifo->tail + 1) % fifo->size;

    b_signal(&fifo->mutex);
    c_signal(&fifo->empty);
}

bool mfifo_put_nonblock(mFIFO_t *fifo, const void *value) {
    if ((fifo->tail + 1) % fifo->size == fifo->head) {
        return false; // FIFO full
    }
    void *addr = (uint8_t *)fifo->data + (fifo->tail * fifo->element_size);
    switch (fifo->element_size) {
        case 1: *(uint8_t *)addr = *(const uint8_t *)value; break;
        case 2: *(uint16_t *)addr = *(const uint16_t *)value; break;
        case 4: *(uint32_t *)addr = *(const uint32_t *)value; break;
        case 8: *(uint64_t *)addr = *(const uint64_t *)value; break;
    }
    fifo->tail = (fifo->tail + 1) % fifo->size;
    return true;
}

void mfifo_get(mFIFO_t *fifo, void *value) {
    c_wait(&fifo->empty);
    b_wait(&fifo->mutex);

    const void *addr = (uint8_t *)fifo->data + (fifo->head * fifo->element_size);
    switch (fifo->element_size) {
        case 1: *(uint8_t *)value = *(const uint8_t *)addr; break;
        case 2: *(uint16_t *)value = *(const uint16_t *)addr; break;
        case 4: *(uint32_t *)value = *(const uint32_t *)addr; break;
        case 8: *(uint64_t *)value = *(const uint64_t *)addr; break;
    }
    fifo->head = (fifo->head + 1) % fifo->size;

    b_signal(&fifo->mutex);
    c_signal(&fifo->full);
}

bool mfifo_get_nonblocking(mFIFO_t *fifo, void *value) {
    if (fifo->head == fifo->tail) return false; // empty

    const void *addr = (uint8_t *)fifo->data + (fifo->head * fifo->element_size);
    switch (fifo->element_size) {
        case 1: *(uint8_t *)value = *(const uint8_t *)addr; break;
        case 2: *(uint16_t *)value = *(const uint16_t *)addr; break;
        case 4: *(uint32_t *)value = *(const uint32_t *)addr; break;
        case 8: *(uint64_t *)value = *(const uint64_t *)addr; break;
    }
    fifo->head = (fifo->head + 1) % fifo->size;

    return true;
}


void mfifo_get_spinlock(mFIFO_t *fifo, void *value) {
    c_wait(&fifo->empty);
    b_wait(&fifo->mutex);

    const void *addr = (uint8_t *)fifo->data + (fifo->head * fifo->element_size);
    switch (fifo->element_size) {
        case 1: *(uint8_t *)value = *(const uint8_t *)addr; break;
        case 2: *(uint16_t *)value = *(const uint16_t *)addr; break;
        case 4: *(uint32_t *)value = *(const uint32_t *)addr; break;
        case 8: *(uint64_t *)value = *(const uint64_t *)addr; break;
    }
    fifo->head = (fifo->head + 1) % fifo->size;

    b_signal(&fifo->mutex);
    c_signal(&fifo->full);
}

// returns the number of elements in the fifo
uint32_t mfifo_size(mFIFO_t *fifo) {
    return fifo->empty.value;
}

uint32_t mfifo_size_nonblocking(mFIFO_t *fifo) {
    if (fifo->tail >= fifo->head)
        return fifo->tail - fifo->head;
    else
        return fifo->size - (fifo->head - fifo->tail);
}