#include "internal-tests.h"
#include <stdlib.h> /* Defines rand, srand */
#include <time.h>   /* Defines time */
#include <stdio.h>  /* Defines printf */

/** Starting code for writing tests that measure memory fragmentation.
 *  Note that the CI will not run this test intentionally.
 */

// You can modify these values to be larger or smaller as needed
// By default, they are quite small to help you test your code.
#define REPTS 100000
#define NUM_PTRS 10000
#define MAX_ALLOC_SIZE 4096

char *ptrs[NUM_PTRS];
size_t sizes[NUM_PTRS];       // Added to store sizes of allocations

size_t current_payload = 0;   // Added to track current aggregate payload (Pk)
size_t max_payload = 0;       // Added to track maximum aggregate payload (max Pi)

/* Forward declarations of functions from mymalloc.c */
void *my_malloc(size_t size);
void my_free(void *ptr);
size_t get_heap_size();        // Function to get current heap size (Hk)

/* Returns a random number between min and max (inclusive) */
int random_in_range(int min, int max) {
    return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

/* Performs REPTS number of calls to my_malloc/my_free. */
void random_allocations() {
    for (int i = 0; i < REPTS; i++) {
        int idx = random_in_range(0, NUM_PTRS - 1);
        if (ptrs[idx] == NULL) {
            size_t random_size = (size_t)random_in_range(1, MAX_ALLOC_SIZE); // Avoid size 0 allocations
            ptrs[idx] = my_malloc(random_size);
            sizes[idx] = random_size; // Store size of allocation
            current_payload += random_size;
            if (current_payload > max_payload) {
                max_payload = current_payload;
            }
        } else {
            my_free(ptrs[idx]);
            current_payload -= sizes[idx];
            sizes[idx] = 0;
            ptrs[idx] = NULL;
        }
    }
}

/* Usage: passing an unsigned integer as the first argument will use that value
 * to seed the pRNG. This will allow you to re-run the same sequence of calls to
 * my_malloc and my_free for the purposes of debugging or measuring
 * fragmentation.
 * If a seed is not given to the program, it will use the current time instead.
 */
int main(int argc, char const *argv[]) {
    unsigned int seed;
    if (argc < 2) {
        seed = (unsigned int)time(NULL);
    } else {
        sscanf(argv[1], "%u", &seed);
    }
    fprintf(stderr, "Running fragmentation test with random seed: %u\n", seed);
    srand(seed);

    // Initialize sizes array
    for (int i = 0; i < NUM_PTRS; i++) {
        sizes[i] = 0;
    }

    random_allocations();

    /* Measure and report peak memory utilization */
    size_t Hk = get_heap_size();  // Get current heap size from allocator
    size_t max_Pi = max_payload;  // Maximum aggregate payload observed

    double Uk = ((double)max_Pi) / ((double)Hk);

    printf("Maximum aggregate payload (max Pi): %zu bytes\n", max_Pi);
    printf("Current heap size (Hk): %zu bytes\n", Hk);
    printf("Peak memory utilization (Uk): %.4f%%\n", Uk * 100.0);

    return 0;
}
