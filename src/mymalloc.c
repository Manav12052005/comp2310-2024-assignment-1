#include "mymalloc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>

// Word alignment
const size_t kAlignment = sizeof(size_t);
// Minimum allocation size (1 word)
const size_t kMinAllocationSize = kAlignment;
// Size of meta-data per Block
const size_t kMetadataSize = sizeof(Block);
// Footer size
const size_t kFooterSize = sizeof(size_t);
// Total block overhead (header + footer)
const size_t kBlockOverhead = kMetadataSize + kFooterSize;
// Maximum allocation size (128 MB minus fenceposts)
const size_t kMaxAllocationSize = (128ull << 20) - kMetadataSize;
// Initial memory size that is mmapped (64 MB)
const size_t kMemorySize = (64ull << 20);

// Array of free lists
static Block *free_lists[N_LISTS] = {NULL};
static void *heap_start = NULL;

// Global pointer to track mmaped blocks
static Block *mmaped_blocks = NULL;

// Define flags using the least significant bits of the size field
#define ALLOCATED_FLAG 0x1
#define FENCEPOST_FLAG 0x2
#define MMAPED_FLAG    0x4
#define SIZE_MASK      ~(ALLOCATED_FLAG | FENCEPOST_FLAG | MMAPED_FLAG)

// For Performance Analysis
static size_t current_memory_usage = 0;  // Current aggregate payload (Pk)
static size_t peak_memory_usage = 0;     // Maximum aggregate payload (max_Pi)
static size_t heap_size = 0;             // Current heap size (Hk)

// Helper functions to set and get block size and flags
static void set_block_size(Block *block, size_t size) {
    block->size = (block->size & ~SIZE_MASK) | (size & SIZE_MASK);
}

static size_t get_block_size(Block *block) {
    return block->size & SIZE_MASK;
}

static void set_allocated(Block *block, bool allocated) {
    if (allocated) {
        block->size |= ALLOCATED_FLAG;
    } else {
        block->size &= ~ALLOCATED_FLAG;
    }
}

static bool is_allocated(Block *block) {
    return block->size & ALLOCATED_FLAG;
}

static void set_fencepost(Block *block, bool fencepost) {
    if (fencepost) {
        block->size |= FENCEPOST_FLAG;
    } else {
        block->size &= ~FENCEPOST_FLAG;
    }
}

static bool is_fencepost(Block *block) {
    return block->size & FENCEPOST_FLAG;
}

static void set_mmaped(Block *block, bool mmaped) {
    if (mmaped) {
        block->size |= MMAPED_FLAG;
    } else {
        block->size &= ~MMAPED_FLAG;
    }
}

static bool is_mmaped(Block *block) {
    return block->size & MMAPED_FLAG;
}

// Helper function to round up to the nearest multiple of kAlignment
static size_t round_up(size_t size) {
    return (size + kAlignment - 1) & ~(kAlignment - 1);
}

// Helper function to get the appropriate free list index
static int get_list_index(size_t size) {
    int index = 0;
    size = (size < kMinAllocationSize) ? kMinAllocationSize : size;
    while (size > 1 && index < N_LISTS - 1) {
        size >>= 1;
        index++;
    }
    return index;
}

// Returns a pointer to the footer of the given block
static size_t *get_footer(Block *block) {
    return (size_t *)((char *)block + get_block_size(block) - kFooterSize);
}

// Returns a pointer to the previous block based on the footer
Block *get_prev_block(Block *block) {
    if (block == NULL || block == heap_start) {
        return NULL;
    }

    size_t *footer = (size_t *)((char *)block - kFooterSize);
    size_t prev_size = *footer & SIZE_MASK;
    Block *prev_block = (Block *)((char *)block - prev_size);

    if (is_fencepost(prev_block) || prev_block == block || prev_size == 0) {
        return NULL;
    }

    return prev_block;
}

void add_to_free_list(Block *block) {
    int index = get_list_index(get_block_size(block));
    block->next = free_lists[index];
    if (free_lists[index] != NULL) {
        free_lists[index]->prev = block;
    }
    block->prev = NULL;
    free_lists[index] = block;
}

void remove_from_free_list(Block *block) {
    int index = get_list_index(get_block_size(block));
    if (block->prev != NULL) {
        block->prev->next = block->next;
    } else {
        free_lists[index] = block->next;
    }
    if (block->next != NULL) {
        block->next->prev = block->prev;
    }
    block->next = NULL;
    block->prev = NULL;
}

// Initialize the heap with fenceposts
static void init_heap() {
    if (heap_start == NULL) {
        void *mem = mmap(NULL, kMemorySize, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) {
            LOG("Failed to initialize heap\n");
            return;
        }

        heap_start = ADD_BYTES(mem, kMetadataSize);

        // Update heap size (Hk)
        heap_size += kMemorySize;

        // Start fencepost
        Block *start_fencepost = (Block *)mem;
        start_fencepost->size = 0;
        set_allocated(start_fencepost, true);
        set_fencepost(start_fencepost, true);
        set_mmaped(start_fencepost, false);

        // End fencepost
        Block *end_fencepost = (Block *)ADD_BYTES(mem, kMemorySize - kMetadataSize);
        end_fencepost->size = 0;
        set_allocated(end_fencepost, true);
        set_fencepost(end_fencepost, true);
        set_mmaped(end_fencepost, false);

        // Initial free block
        Block *initial_block = (Block *)heap_start;
        size_t initial_block_size = kMemorySize - 2 * kMetadataSize;
        set_block_size(initial_block, initial_block_size);
        set_allocated(initial_block, false);
        set_fencepost(initial_block, false);
        set_mmaped(initial_block, false);
        initial_block->next = NULL;
        initial_block->prev = NULL;

        // Set footer
        size_t *footer = get_footer(initial_block);
        *footer = initial_block->size;

        add_to_free_list(initial_block);
    }
}

// Split a block if it's too large
static void split_block(Block *block, size_t size) {
    size_t blockSize = get_block_size(block);
    if (blockSize >= size + kBlockOverhead + kMinAllocationSize) {
        Block *new_block = (Block *)ADD_BYTES(block, size);
        size_t new_block_size = blockSize - size;
        set_block_size(new_block, new_block_size);
        set_allocated(new_block, false);
        set_fencepost(new_block, false);
        set_mmaped(new_block, is_mmaped(block));
        new_block->next = NULL;
        new_block->prev = NULL;

        // Update footers
        size_t *new_footer = get_footer(new_block);
        *new_footer = new_block->size;

        set_block_size(block, size);

        size_t *block_footer = get_footer(block);
        *block_footer = block->size;

        add_to_free_list(new_block);
    }
}

static int is_valid_pointer(void *p) {
    if (p == NULL) {
        return 0;
    }

    // Compute the block address
    Block *block = ptr_to_block(p);

    // Check if block is properly aligned
    if (((uintptr_t)block) % kAlignment != 0) {
        return 0;
    }

    // Check if block is within the initial heap
    void *heap_end = ADD_BYTES(heap_start, kMemorySize - kMetadataSize);
    if (block >= (Block *)heap_start && block < (Block *)heap_end) {
        return 1;
    }

    // Now check if block is in mmaped_blocks list
    Block *current = mmaped_blocks;
    while (current != NULL) {
        if (block == current) {
            return 1;
        }
        current = current->next;
    }

    return 0;
}

void *my_malloc(size_t size) {
    if (size == 0 || size > kMaxAllocationSize) {
        return NULL;
    }

    init_heap();

    size_t block_size = round_up(size + kBlockOverhead);
    Block *best_fit = NULL;

    // If the requested size is larger than a threshold, use mmap
    if (block_size > kMemorySize - 2 * kMetadataSize) {
        size_t mmap_size = block_size + 2 * kMetadataSize;
        void *mem = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) {
            LOG("Failed to mmap additional memory\n");
            return NULL;
        }

        // Update heap size (Hk)
        heap_size += mmap_size;

        // Start fencepost
        Block *start_fencepost = (Block *)mem;
        start_fencepost->size = 0;
        set_allocated(start_fencepost, true);
        set_fencepost(start_fencepost, true);
        set_mmaped(start_fencepost, true);

        // End fencepost
        Block *end_fencepost = (Block *)ADD_BYTES(mem, mmap_size - kMetadataSize);
        end_fencepost->size = 0;
        set_allocated(end_fencepost, true);
        set_fencepost(end_fencepost, true);
        set_mmaped(end_fencepost, true);

        // Allocated block
        Block *new_block = (Block *)ADD_BYTES(mem, kMetadataSize);
        set_block_size(new_block, mmap_size - 2 * kMetadataSize);
        set_allocated(new_block, true);
        set_fencepost(new_block, false);
        set_mmaped(new_block, true);

        // Update footer
        size_t *footer = get_footer(new_block);
        *footer = new_block->size;

        // Add to mmaped_blocks list
        new_block->next = mmaped_blocks;
        if (mmaped_blocks != NULL) {
            mmaped_blocks->prev = new_block;
        }
        new_block->prev = NULL;
        mmaped_blocks = new_block;

        // Update current and peak memory usage
        size_t payload_size = get_block_size(new_block) - kBlockOverhead;
        current_memory_usage += payload_size;
        if (current_memory_usage > peak_memory_usage) {
            peak_memory_usage = current_memory_usage;
        }

        // Return the allocated memory
        return ADD_BYTES(new_block, kMetadataSize);
    }

    // Iterate over the free lists to find the best fitting block
    int index = get_list_index(block_size);
    for (int i = index; i < N_LISTS; i++) {
        Block *block = free_lists[i];
        while (block != NULL) {
            size_t bsize = get_block_size(block);
            if (bsize >= block_size) {
                if (best_fit == NULL || bsize < get_block_size(best_fit)) {
                    best_fit = block;
                    // If it's an exact fit, break early
                    if (bsize == block_size) {
                        break;
                    }
                }
            }
            block = block->next;
        }
        // If an exact fit is found, no need to check further
        if (best_fit != NULL && get_block_size(best_fit) == block_size) {
            break;
        }
    }

    // If a best fit block was found, allocate from it
    if (best_fit != NULL) {
        remove_from_free_list(best_fit);

        size_t bsize = get_block_size(best_fit);
        if (bsize - block_size >= kBlockOverhead + kMinAllocationSize) {
            split_block(best_fit, block_size);
        }

        set_allocated(best_fit, true);

        // Update footer
        size_t *footer = get_footer(best_fit);
        *footer = best_fit->size;

        best_fit->next = NULL;
        best_fit->prev = NULL;

        // Update current and peak memory usage
        size_t payload_size = get_block_size(best_fit) - kBlockOverhead;
        current_memory_usage += payload_size;
        if (current_memory_usage > peak_memory_usage) {
            peak_memory_usage = current_memory_usage;
        }

        return ADD_BYTES(best_fit, kMetadataSize);
    }

    // If no suitable block is found and no large allocation, cannot fulfill the request
    return NULL;
}

void my_free(void *p) {
    if (p == NULL) {
        return;
    }

    if (!is_valid_pointer(p)) {
        // Invalid pointer; ignore
        return;
    }

    Block *block = ptr_to_block(p);

    if (!is_allocated(block)) {
        // Block is already free; ignore
        return;
    }

    set_allocated(block, false);

    // Update footer
    size_t *footer = get_footer(block);
    *footer = block->size;

    // Update current memory usage
    size_t payload_size = get_block_size(block) - kBlockOverhead;
    current_memory_usage -= payload_size;

    if (is_mmaped(block)) {
        // Block is mmaped
        // Remove from mmaped_blocks list
        if (block->prev != NULL) {
            block->prev->next = block->next;
        } else {
            mmaped_blocks = block->next;
        }
        if (block->next != NULL) {
            block->next->prev = block->prev;
        }

        // Do not decrease heap_size since Hk is monotonically non-decreasing
        // Unmap the memory
        size_t mmap_size = get_block_size(block) + 2 * kMetadataSize;
        void *mem = ADD_BYTES(block, -((ssize_t)kMetadataSize));
        munmap(mem, mmap_size);
    } else {
        // Block is within the initial heap
        // Coalesce with next block if possible
        Block *next = get_next_block(block);
        if (next && !is_allocated(next) && !is_fencepost(next)) {
            remove_from_free_list(next);
            size_t new_size = get_block_size(block) + get_block_size(next);
            set_block_size(block, new_size);

            // Update footer
            footer = get_footer(block);
            *footer = block->size;
        }

        // Coalesce with previous block if possible
        Block *prev = get_prev_block(block);
        if (prev && !is_allocated(prev) && !is_fencepost(prev)) {
            remove_from_free_list(prev);
            size_t new_size = get_block_size(prev) + get_block_size(block);
            set_block_size(prev, new_size);

            // Update footer
            footer = get_footer(prev);
            *footer = prev->size;

            block = prev;
        }

        add_to_free_list(block);
    }
}

/** Helper functions **/

/* Returns 1 if the given block is free, 0 if not. */
int is_free(Block *block) {
    return !is_allocated(block);
}

/* Returns the size of the given block */
size_t block_size(Block *block) {
    return get_block_size(block);
}

/* Returns the first block in memory (excluding fenceposts) */
Block *get_start_block(void) {
    return (Block *)heap_start;
}

Block *get_next_block(Block *block) {
    if (block == NULL || is_fencepost(block)) {
        return NULL;
    }
    Block *next_block = (Block *)ADD_BYTES(block, get_block_size(block));
    if (is_fencepost(next_block) || get_block_size(next_block) == 0) {
        return NULL;
    }
    return next_block;
}

/* Given a ptr assumed to be returned from a previous call to `my_malloc`, return a pointer to the start of the metadata block. */
Block *ptr_to_block(void *ptr) {
    if (ptr == NULL) {
        return NULL;
    }
    return (Block *)((char *)ptr - kMetadataSize);
}

/* Performance Analysis Functions */
size_t get_peak_memory_usage() {
    return peak_memory_usage;
}

size_t get_heap_size() {
    return heap_size;
}
