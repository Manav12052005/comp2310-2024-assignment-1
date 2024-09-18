#include "mymalloc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>

// Alignment stuff
const size_t kAlignment = sizeof(size_t);
const size_t kMinAllocationSize = kAlignment;
const size_t kMetadataSize = sizeof(Block);
const size_t kFooterSize = sizeof(size_t);
const size_t kBlockOverhead = kMetadataSize + kFooterSize;
const size_t kMaxAllocationSize = (128ull << 20) - kMetadataSize; // 128 MB
const size_t kMemorySize = (64ull << 20); // 64 MB

// Single free list
static Block *free_list = NULL;
static void *heap_start = NULL;

// Removed: Track mmaped blocks
// static Block *mmaped_blocks = NULL;

// Flags
#define ALLOCATED_FLAG 0x1
#define FENCEPOST_FLAG 0x2
#define MMAPED_FLAG    0x4
#define SIZE_MASK      ~(ALLOCATED_FLAG | FENCEPOST_FLAG | MMAPED_FLAG)

// For stats
static size_t current_memory_usage = 0;
static size_t peak_memory_usage = 0;
static size_t heap_size = 0;

// Logging macro (define as needed)
#define LOG(msg, ...) fprintf(stderr, msg, ##__VA_ARGS__)

// Set block size
static void set_block_size(Block *block, size_t size) {
    block->size = (block->size & ~SIZE_MASK) | (size & SIZE_MASK);
}

// Get block size
static size_t get_block_size(Block *block) {
    return block->size & SIZE_MASK;
}

// Mark allocated
static void set_allocated(Block *block, bool allocated) {
    if (allocated) {
        block->size |= ALLOCATED_FLAG;
    } else {
        block->size &= ~ALLOCATED_FLAG;
    }
}

// Check if allocated
static bool is_allocated(Block *block) {
    return block->size & ALLOCATED_FLAG;
}

// Mark fencepost
static void set_fencepost(Block *block, bool fencepost) {
    if (fencepost) {
        block->size |= FENCEPOST_FLAG;
    } else {
        block->size &= ~FENCEPOST_FLAG;
    }
}

// Check fencepost
static bool is_fencepost(Block *block) {
    return block->size & FENCEPOST_FLAG;
}

// Mark mmaped
static void set_mmaped(Block *block, bool mmaped) {
    if (mmaped) {
        block->size |= MMAPED_FLAG;
    } else {
        block->size &= ~MMAPED_FLAG;
    }
}

// Check mmaped
static bool is_mmaped(Block *block) {
    return block->size & MMAPED_FLAG;
}

// Round up size
static size_t round_up(size_t size) {
    return (size + kAlignment - 1) & ~(kAlignment - 1);
}

// Get footer
static size_t *get_footer(Block *block) {
    return (size_t *)((char *)block + get_block_size(block) - kFooterSize);
}

// Get previous block
Block *get_prev_block(Block *block) {
    if (block == NULL || block == heap_start) return NULL;

    size_t *footer = (size_t *)((char *)block - kFooterSize);
    size_t prev_size = *footer & SIZE_MASK;
    Block *prev_block = (Block *)((char *)block - prev_size);

    if (is_fencepost(prev_block) || prev_block == block || prev_size == 0) return NULL;

    return prev_block;
}

// Add to free list
void add_to_free_list(Block *block) {
    block->next = free_list;
    if (free_list != NULL) {
        free_list->prev = block;
    }
    block->prev = NULL;
    free_list = block;
}

// Remove from free list
void remove_from_free_list(Block *block) {
    if (block->prev != NULL) {
        block->prev->next = block->next;
    } else {
        free_list = block->next;
    }
    if (block->next != NULL) {
        block->next->prev = block->prev;
    }
    block->next = NULL;
    block->prev = NULL;
}

// Initialize heap
static void init_heap() {
    if (heap_start == NULL) {
        void *mem = mmap(NULL, kMemorySize, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) {
            LOG("Failed to init heap\n");
            return;
        }

        heap_start = (char *)mem + kMetadataSize;
        heap_size += kMemorySize;

        // Start fencepost
        Block *start_fencepost = (Block *)mem;
        start_fencepost->size = 0;
        set_allocated(start_fencepost, true);
        set_fencepost(start_fencepost, true);
        set_mmaped(start_fencepost, false);

        // End fencepost
        Block *end_fencepost = (Block *)((char *)mem + kMemorySize - kMetadataSize);
        end_fencepost->size = 0;
        set_allocated(end_fencepost, true);
        set_fencepost(end_fencepost, true);
        set_mmaped(end_fencepost, false);

        // Free block
        Block *initial_block = (Block *)heap_start;
        size_t initial_block_size = kMemorySize - 2 * kMetadataSize;
        set_block_size(initial_block, initial_block_size);
        set_allocated(initial_block, false);
        set_fencepost(initial_block, false);
        set_mmaped(initial_block, false);
        initial_block->next = NULL;
        initial_block->prev = NULL;

        // Footer
        size_t *footer = get_footer(initial_block);
        *footer = initial_block->size;

        add_to_free_list(initial_block);
    }
}

// Split block
static void split_block(Block *block, size_t size) {
    size_t blockSize = get_block_size(block);
    if (blockSize >= size + kBlockOverhead + kMinAllocationSize) {
        Block *new_block = (Block *)((char *)block + size);
        size_t new_block_size = blockSize - size;
        set_block_size(new_block, new_block_size);
        set_allocated(new_block, false);
        set_fencepost(new_block, false);
        set_mmaped(new_block, is_mmaped(block));
        new_block->next = NULL;
        new_block->prev = NULL;

        // Footer
        size_t *new_footer = get_footer(new_block);
        *new_footer = new_block->size;

        set_block_size(block, size);
        size_t *block_footer = get_footer(block);
        *block_footer = block->size;

        add_to_free_list(new_block);
    }
}

// Validate pointer
static int is_valid_pointer(void *p) {
    if (p == NULL) return 0;

    Block *block = ptr_to_block(p);
    if (((uintptr_t)block) % kAlignment != 0) return 0;

    // Check if within heap
    if (heap_start != NULL) {
        void *heap_end = (char *)heap_start + kMemorySize - kMetadataSize;
        if ((void *)block >= heap_start && (void *)block < heap_end) return 1;
    }

    // For mmaped allocations, we can perform a minimal check:
    // Ensure that the block has the MMAPED_FLAG set and has a reasonable size.
    // Note: Without a separate list, full validation isn't possible.
    if (is_mmaped(block)) {
        // Optionally, add additional checks here if possible
        return 1;
    }

    return 0;
}

// Malloc implementation
void *my_malloc(size_t size) {
    if (size == 0 || size > kMaxAllocationSize) return NULL;

    init_heap();
    size_t block_size = round_up(size + kBlockOverhead);
    Block *best_fit = NULL;

    // Large allocs via mmap
    if (block_size > kMemorySize - 2 * kMetadataSize) {
        size_t mmap_size = block_size + 2 * kMetadataSize;
        void *mem = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) {
            LOG("Failed to mmap\n");
            return NULL;
        }

        heap_size += mmap_size;

        // Fenceposts
        Block *start_fencepost = (Block *)mem;
        start_fencepost->size = 0;
        set_allocated(start_fencepost, true);
        set_fencepost(start_fencepost, true);
        set_mmaped(start_fencepost, true);

        Block *end_fencepost = (Block *)((char *)mem + mmap_size - kMetadataSize);
        end_fencepost->size = 0;
        set_allocated(end_fencepost, true);
        set_fencepost(end_fencepost, true);
        set_mmaped(end_fencepost, true);

        // Alloc block
        Block *new_block = (Block *)((char *)mem + kMetadataSize);
        set_block_size(new_block, mmap_size - 2 * kMetadataSize);
        set_allocated(new_block, true);
        set_fencepost(new_block, false);
        set_mmaped(new_block, true);
        size_t *footer = get_footer(new_block);
        *footer = new_block->size;

        // Removed: Track mmaped
        // new_block->next = mmaped_blocks;
        // if (mmaped_blocks != NULL) {
        //     mmaped_blocks->prev = new_block;
        // }
        // new_block->prev = NULL;
        // mmaped_blocks = new_block;

        // Update stats
        size_t payload_size = get_block_size(new_block) - kBlockOverhead;
        current_memory_usage += payload_size;
        if (current_memory_usage > peak_memory_usage) {
            peak_memory_usage = current_memory_usage;
        }

        return (char *)new_block + kMetadataSize;
    }

    // Find best fit
    Block *block = free_list;
    while (block != NULL) {
        size_t bsize = get_block_size(block);
        if (bsize >= block_size) {
            if (best_fit == NULL || bsize < get_block_size(best_fit)) {
                best_fit = block;
                if (bsize == block_size) break; // Exact fit
            }
        }
        block = block->next;
    }

    if (best_fit != NULL) {
        remove_from_free_list(best_fit);
        size_t bsize = get_block_size(best_fit);
        if (bsize - block_size >= kBlockOverhead + kMinAllocationSize) {
            split_block(best_fit, block_size);
        }

        set_allocated(best_fit, true);
        size_t *footer = get_footer(best_fit);
        *footer = best_fit->size;

        // Update stats
        size_t payload_size = get_block_size(best_fit) - kBlockOverhead;
        current_memory_usage += payload_size;
        if (current_memory_usage > peak_memory_usage) {
            peak_memory_usage = current_memory_usage;
        }

        return (char *)best_fit + kMetadataSize;
    }

    // Couldn't find block
    return NULL;
}

// Free implementation
void my_free(void *p) {
    if (p == NULL) return;
    if (!is_valid_pointer(p)) {
        LOG("Attempted to free invalid pointer: %p\n", p);
        return;
    }

    Block *block = ptr_to_block(p);
    if (!is_allocated(block)) {
        LOG("Attempted to free an already free block: %p\n", p);
        return;
    }

    set_allocated(block, false);
    size_t *footer = get_footer(block);
    *footer = block->size;
    size_t payload_size = get_block_size(block) - kBlockOverhead;
    current_memory_usage -= payload_size;

    if (is_mmaped(block)) {
        // Unmap mmaped
        size_t mmap_size = get_block_size(block) + 2 * kMetadataSize;
        void *mem = (char *)block - kMetadataSize;
        if (munmap(mem, mmap_size) == -1) {
            LOG("Failed to munmap: %s\n", strerror(errno));
        }
        // No need to add to free list as it's unmapped
    } else {
        // Coalesce
        Block *next = get_next_block(block);
        if (next && !is_allocated(next) && !is_fencepost(next)) {
            remove_from_free_list(next);
            size_t new_size = get_block_size(block) + get_block_size(next);
            set_block_size(block, new_size);
            footer = get_footer(block);
            *footer = block->size;
        }

        Block *prev = get_prev_block(block);
        if (prev && !is_allocated(prev) && !is_fencepost(prev)) {
            remove_from_free_list(prev);
            size_t new_size = get_block_size(prev) + get_block_size(block);
            set_block_size(prev, new_size);
            footer = get_footer(prev);
            *footer = prev->size;
            block = prev;
        }

        add_to_free_list(block);
    }
}

/* Helper functions */

// Check if free
int is_free(Block *block) {
    return !is_allocated(block);
}

// Get block size
size_t block_size(Block *block) {
    return get_block_size(block);
}

// Start block
Block *get_start_block(void) {
    return (Block *)heap_start;
}

// Next block
Block *get_next_block(Block *block) {
    if (block == NULL || is_fencepost(block)) return NULL;
    Block *next_block = (Block *)((char *)block + get_block_size(block));
    if (is_fencepost(next_block) || get_block_size(next_block) == 0) return NULL;
    return next_block;
}

// Ptr to block
Block *ptr_to_block(void *ptr) {
    if (ptr == NULL) return NULL;
    return (Block *)((char *)ptr - kMetadataSize);
}

// Stats
size_t get_peak_memory_usage() {
    return peak_memory_usage;
}

size_t get_heap_size() {
    return heap_size;
}
