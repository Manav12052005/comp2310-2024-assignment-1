#ifndef MYMALLOC_HEADER
#define MYMALLOC_HEADER

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/mman.h>

#ifdef ENABLE_LOG
#define LOG(...) fprintf(stderr, "[malloc] " __VA_ARGS__);
#else
#define LOG(...)
#endif

#define N_LISTS 59

#define ADD_BYTES(ptr, n) ((void *) (((char *) (ptr)) + (n)))

// Block structure with a unified header and footer, utilizing boundary tags
typedef struct Block Block;

struct Block {
    // Size of the block, including meta-data size and flags
    size_t size;
    // Next and Prev blocks in the free list (only used when the block is free)
    Block *next;
    Block *prev;
    // The footer is not explicitly stored as a separate field; it uses the last 8 bytes of the block.
};

// Word alignment
extern const size_t kAlignment;
// Minimum allocation size (1 word)
extern const size_t kMinAllocationSize;
// Size of meta-data per Block
extern const size_t kMetadataSize;
// Maximum allocation size (128 MB)
extern const size_t kMaxAllocationSize;
// Memory size that is mmapped (64 MB)
extern const size_t kMemorySize;

void *my_malloc(size_t size);
void my_free(void *p);

/* Helper functions you are required to implement for internal testing. */
int is_free(Block *block);
size_t block_size(Block *block);

Block *get_start_block(void); 
Block *get_next_block(Block *block);
Block *get_prev_block(Block *block);

Block *ptr_to_block(void *ptr);
size_t get_peak_memory_usage();

#endif
