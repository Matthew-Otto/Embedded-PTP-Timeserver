#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "heap.h"

__attribute__((section(".heap")))
static uint8_t heap[HEAP_SIZE];
static uint8_t* heap_ptr = heap;
static void* heap_tree[HEAP_TREE_NODE_CNT] = {0};

static int32_t find_ptr(void*);

void heap_init(void){
    memset(heap_tree, 0, HEAP_TREE_NODE_CNT*sizeof(void*));
}

void *malloc(size_t size){
    if (size > HEAP_SIZE) return NULL;

    // get size of block to allocate
    size = next_pow2(size);
    if (size < HEAP_SIZE_MIN) 
        size = HEAP_SIZE_MIN;

    uint16_t i = 0;
    uint32_t block_size = HEAP_SIZE;
    uint16_t skip = 0;
    while (1) {
        // check if current node is at correct depth in tree
        if (block_size == size) {
            // if node is not free
            if (heap_tree[i] != NULL) {
                skip = i;
                block_size <<= 1;
                i = (i-1) / 2; // traverse up
                continue;
            }

            // allocate node
            uint32_t offset = ((i+1) * block_size) & HEAP_ADDR_MAX;
            void *mem_addr = heap_ptr + offset;
            heap_tree[i] = mem_addr;
            return mem_addr;
        } else {
            heap_tree[i] = BRANCH_NODE;
        }

        uint16_t left = 2*i + 1;
        uint16_t right = 2*i + 2;

        if (right == skip) { // right has been visited
            if (i == 0)
                break;
            skip = i;
            block_size <<= 1;
            i = (i-1) / 2; // traverse up

        } else if ((heap_tree[left] != NULL && heap_tree[left] != BRANCH_NODE) || (left == skip)) { // left is a leaf or has been visited
            if (heap_tree[right] != NULL && heap_tree[right] != BRANCH_NODE) { // right is a leaf
                if (i == 0)
                break;
                skip = i;
                block_size <<= 1;
                i = (i-1) / 2; // traverse up
            } else {
                block_size >>= 1;
                i = right; // traverse down right
            }

        } else {
            block_size >>= 1;
            i = left; // traverse down left
        }
    }
    return NULL;
}

void *calloc(size_t elem_cnt, size_t elem_size){
    size_t size = elem_cnt * elem_size;
    void *mem_ptr = malloc(size);
    if (mem_ptr == NULL) return NULL;
    memset(mem_ptr, 0, size);
    return mem_ptr;
}

void free(void *ptr){
    if (heap_tree[0] == NULL) 
        return; // no memory in heap is allocated

    // recurse tree and find node
    int32_t i = find_ptr(ptr);
    if (!i) { // not found
        return;
    }

    // free node
    heap_tree[i] = NULL;

    // free all available parent nodes
    while (i > 0) {
        i = (i-1) / 2;

        // if either child is allocated, break
        if (heap_tree[2*i + 1] != NULL) break; // left
        if (heap_tree[2*i + 2] != NULL) break; // right
        // else, free this node and traverse up the tree
        heap_tree[i] = NULL;
    }
}

void heap_stats(heap_stats_t *stats){
    // calculate used
    int32_t used = 0;
    int16_t i = 0;
    int32_t block_size = HEAP_SIZE;
    int16_t skip = 0;
    while (1) {
        // if current node is leaf node
        if (heap_tree[i] != NULL && heap_tree[i] != BRANCH_NODE) {
            used += block_size;
            skip = i;
            block_size <<= 1;
            if (i == 0)
                break; // entire tree has been visited
            i = (i-1) / 2; // traverse up
        }
    
        int16_t left = 2*i + 1;
        int16_t right = 2*i + 2;
    
        if (right == skip) { // right has been visited
            if (i == 0)
                break; // entire tree has been visited
            skip = i;
            block_size <<= 1;
            i = (i-1) / 2; // traverse up
    
        } else if (heap_tree[left] == NULL || left == skip) { // left is invalid
            if (heap_tree[right] == NULL) { // right is invalid
                if (i == 0)
                    break; // entire tree has been visited
                skip = i;
                block_size <<= 1;
                i = (i-1) / 2; // traverse up
            } else {
                block_size >>= 1;
                i = right; // traverse down right
            }
    
        } else {
            block_size >>= 1;
            i = left; // traverse down left
        }
    }
  
    stats->size = HEAP_SIZE;
    stats->used = used;
    stats->free = HEAP_SIZE - used;
}

// used to recurse the binary tree that represents the heap
// returns the index that contains pointer
static int32_t find_ptr(void *ptr) {
    int16_t i = 0;
    int16_t skip = 0;
    while (1) {
        // if current node is node to be freed
        if (heap_tree[i] == ptr) {
            break;
        }
    
        uint16_t left = 2*i + 1;
        uint16_t right = 2*i + 2;
    
        if (right == skip) { // right has been visited
            if (i == 0) {
                return 0; // entire tree has been visited, pointer not found
            }
            skip = i;
            i = (i-1) / 2; // traverse up
    
        } else if (left >= HEAP_TREE_NODE_CNT || heap_tree[left] == NULL || left == skip) { // left is invalid
            if (right > HEAP_TREE_NODE_CNT || heap_tree[right] == NULL) { // right is invalid
                if (i == 0) {
                    return 0; // entire tree has been visited, pointer not found
                }
                skip = i;
                i = (i-1) / 2; // traverse up
            } else {
                i = right; // traverse down right
            }
    
        } else {
            i = left; // traverse down left
        }
    }
    return i;
}