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

typedef struct {
    semaphore_t full;
    semaphore_t empty;
    semaphore_t mutex;
    uint32_t size;
    uint8_t element_size;
    uint32_t head;
    uint32_t tail;
    uint32_t data[];
} mFIFO_t;

FIFO_t* fifo_init(uint32_t size);
void fifo_put(FIFO_t *fifo, uint8_t data);
int fifo_put_nonblock(FIFO_t *fifo, uint8_t data);
void fifo_get(FIFO_t *fifo, uint8_t *data);
int32_t fifo_size(FIFO_t *fifo);


// TODO organize these
mFIFO_t *mfifo_init(uint32_t fifo_size, uint8_t element_size);
void mfifo_put(mFIFO_t *fifo, const void *value);

// return 1 if fifo full
bool mfifo_put_nonblock(mFIFO_t *fifo, const void *value);
void mfifo_get(mFIFO_t *fifo, void *value);

// return 1 if fifo empty
bool mfifo_get_nonblocking(mFIFO_t *fifo, void *value); 
uint32_t mfifo_size(mFIFO_t *fifo);
// gets size without using semaphore value
uint32_t mfifo_size_nonblocking(mFIFO_t *fifo);

#endif // FIFO_H