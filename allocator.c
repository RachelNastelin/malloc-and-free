#define _GNU_SOURCE

#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <math.h>

/*****************************************************************************
Soooo this code is garbage. Just FYI. Prepare for seg faults. 

HOW TO RUN THIS STUPID THING:
1. IN TERMINAL: make
2. IN TERMINAL: gdb ls
3. IN GDB: set environment LD_PRELOAD=./myallocator.so
4. IN GDB: run

(cite Pouya for helping with that)
******************************************************************************/

// The minimum size returned by malloc
#define MIN_MALLOC_SIZE 16

// Round a value x up to the next multiple of y
#define ROUND_UP(x,y) ((x) % (y) == 0 ? (x) : (x) + ((y) - (x) % (y)))
#define ROUND_DOWN(x,y) ((x) % (y) == 0 ? (x) : (x) - ((y) + (x) % (y)))

// The size of a single page of memory, in bytes
#define PAGE_SIZE 0x1000

// USE ONLY IN CASE OF EMERGENCY
bool in_malloc = false;           // Set whenever we are inside malloc.
bool use_emergency_block = false; // If set, use the emergency space for allocations
char emergency_block[1024];       // Emergency space for allocating to print errors
typedef struct __attribute__((packed)) slot slot_t;
typedef struct __attribute__((packed)) slot{
  slot_t * next_slot;
}slot_t;

typedef struct __attribute__((packed)) header{
  size_t size;
  void * next_block;
  slot_t * free_list;
}header_t;

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

  /*Storing 8 chunks of memory*/
  header_t * focal_mem[8]; //Holds the variable-sized memory blocks
  header_t header_cur;

  for(int i =4; i < 12; i++){
    //Get a pointer to the next block
    void* p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    //Put that pointer inside of focal_mem
    focal_mem[i-4] = p;
 
    //Make header
    header_cur.size = pow(2, i);
    header_cur.next_block = NULL;
    header_cur.free_list = p + (sizeof(header_t) + (16 - sizeof(header_t)));
  }//for

  /*splitting the block*/
  for(int i = 0; i < 8; i++){ //Loops through focal_mem
    slot_t* cur_slot = focal_mem[i]->free_list;
    slot_t* cur_slot_end = cur_slot + focal_mem[i]->size;

    cur_slot->next_slot = cur_slot_end;

    for(int j = 0; cur_slot_end != NULL; j++){ //Loops through slots in each block
      cur_slot = cur_slot_end;
      cur_slot_end =  cur_slot + focal_mem[i]->size;
      cur_slot->next_slot = cur_slot_end;
    }//for j
    //cur->next_slot = NULL;
  }//for i
  
  /*Rounding*/
  int leading = __builtin_clzll(size); //Number of leading zeros
  int round_up_ready = 64 - leading; /*The number of digits that aren't leading
                                       zeros*/
  int ret_size = pow(2, round_up_ready); //The size of the space you'll need

  if(ret_size <= 16){ //If you need less than 16 bytes
    slot_t * tmp = focal_mem[0]->free_list;
    focal_mem[0]->free_list = focal_mem[0]->free_list->next_slot;
    return &(*tmp);
  }//if
  int index = 1;
  //while the rounded size is greater than cur slot size
  while(ret_size >= focal_mem[index]->size){
    //continue looking
    index++;
  }//while
  /*You've found a slot big enough, so move the free list along and return the
    next thing in the free list*/
  slot_t * tmp = focal_mem[index]->free_list;
  focal_mem[index]->free_list = focal_mem[index]->free_list->next_slot;
  
  // Done with malloc, so clear this flag
  in_malloc = false;
  return &(*tmp);
}

/**
 * Free space occupied by a heap object.
 * \param ptr   A pointer somewhere inside the object that is being freed
 */
void xxfree(void* ptr) {
  /*Get the size of the slot that ptr is in*/
  int slot_size_so_far = 0;
  intptr_t address = (intptr_t)ptr;
  for(int i=4; pow(2,i) < address; i++){
    slot_size_so_far = pow(2,i);
  }//for
  
  
  /*Go to the correct block for the slot size*/
  
  /*Get to the end of free_list in that block*/
  slot_t * tmp;
 
  /*Set the last element's next to the node you're adding on*/
}

/**
 * Get the available size of an allocated object
 * \param ptr   A pointer somewhere inside the allocated object
 * \returns     The number of bytes available for use in this object
 */
size_t xxmalloc_usable_size(void* ptr) {
  // We aren't tracking the size of allocated objects yet, so all we know is that it's at least PAGE_SIZE bytes.
  //int rounded_size = ROUND_DOWN(ptr,PAGE_SIZE);
  //return rounded_size;
  return 16;
}

/**Notes to self (DELETE THIS)
 * Didn't do the last paragraph of part B
 * If you fill a block, add a new block
 * Switch back the thing in the makefile from O0 to O2 (there are two of them)
 */
