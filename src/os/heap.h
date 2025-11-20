#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

#define HEAP_ADDR_LEN 17
#define HEAP_ADDR_MIN_LEN 7
#define HEAP_SIZE (1<<HEAP_ADDR_LEN)
#define HEAP_SIZE_MIN (1<<HEAP_ADDR_MIN_LEN)
#define HEAP_ADDR_MAX (HEAP_SIZE-1)
#define HEAP_TREE_LEVEL_CNT (HEAP_ADDR_LEN - HEAP_ADDR_MIN_LEN)
#define HEAP_TREE_NODE_CNT ((2<<HEAP_TREE_LEVEL_CNT) - 1)

#define BRANCH_NODE (void *)0xFFFFFFFF

static inline uint32_t next_pow2(uint32_t x) {
    if (x <= 2) {
        return x;
    } else {
        return (1U << 31) >> (__builtin_clz(x - 1)-1);
    }
}

// struct for holding statistics on the state of the heap
typedef struct heap_stats {
    uint32_t size;   // heap size (in bytes)
    uint32_t used;   // number of bytes used/allocated
    uint32_t free;   // number of bytes available to allocate
} heap_stats_t;

typedef struct heap_node heap_node_t;
struct heap_node {
    uint32_t mem_ptr;
};

// initializes the heap (clears all nodes)
void heap_init(void);

// allocates memory on heap
// size will be rounded up to nearest power of two
void *malloc(size_t size);

// allocates memory on heap
// allocated memory will be initialized to zero
// size will be rounded up to nearest power of two
void *calloc(size_t elem_cnt, size_t elem_size);

// frees the block on the heap tracked by ptr
void free(void *ptr);

// returns various heap statistics
void heap_stats(heap_stats_t *stats);

#endif // HEAP_H