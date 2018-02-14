#define _GNU_SOURCE

#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

// The minimum size returned by malloc
#define MIN_MALLOC_SIZE 16

// Round a value x up to the next multiple of y
#define ROUND_UP(x,y) ((x) % (y) == 0 ? (x) : (x) + ((y) - (x) % (y)))

// The size of a single page of memory, in bytes
#define PAGE_SIZE 0x1000

// USE ONLY IN CASE OF EMERGENCY
bool in_malloc = false;           // Set whenever we are inside malloc.
bool use_emergency_block = false; // If set, use the emergency space for allocations
char emergency_block[1024];       // Emergency space for allocating to print errors

/**
 * Allocate space on the heap.
 * \param size  The minimium number of bytes that must be allocated
 * \returns     A pointer to the beginning of the allocated space.
 *              This function may return NULL when an error occurs.
 */
void* xxmalloc(size_t size) {
  // Before we try to allocate anything, check if we are trying to print an error or if
  // malloc called itself. This doesn't always work, but sometimes it helps.
  if(use_emergency_block) {
    return emergency_block;
  } else if(in_malloc) {
    use_emergency_block = true;
    puts("ERROR! Nested call to malloc. Aborting.\n");
    exit(2);
  }
  
  // If we call malloc again while this is true, bad things will happen.
  in_malloc = true;
  
  // Round the size up to the next multiple of the page size
  size = ROUND_UP(size, PAGE_SIZE);
  
  // Request memory from the operating system in page-sized chunks
  void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  // Check for errors
  if(p == MAP_FAILED) {
    use_emergency_block = true;
    perror("mmap");
    exit(2);
  }

  

  
  // Done with malloc, so clear this flag
  in_malloc = false;
  
  return p;
}

/**
 * Free space occupied by a heap object.
 * \param ptr   A pointer somewhere inside the object that is being freed
 */
void xxfree(void* ptr) {
  // Not yet implemented!
}

/**
 * Get the available size of an allocated object
 * \param ptr   A pointer somewhere inside the allocated object
 * \returns     The number of bytes available for use in this object
 */
size_t xxmalloc_usable_size(void* ptr) {
  // We aren't tracking the size of allocated objects yet, so all we know is that it's at least PAGE_SIZE bytes.
  return PAGE_SIZE;
}

