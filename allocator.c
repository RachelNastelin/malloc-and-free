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
    focal_mem[i-4] = p;
    focal_mem[i-4]->size = pow(2, i);
    focal_mem[i-4]->next_block = NULL;
    focal_mem[i-4]->free_list = p + ROUND_UP(sizeof(header_t), focal_mem[i-4]->size);
    //focal_mem[i-4]->free_list->next_slot = NULL;

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
    //Make the last next_slot NULL
  }//for i
  
  /*Rounding*/
  int leading = __builtin_clzll(size); //Number of leading zeros
  int trailing = __builtin_ctzll(size); //Number of leading zeros
  int power = 0;
  if(leading + trailing != 64){//There's more than 1 1 in the binary number
    while(pow(2, power) < size){
      power++;
    }//while
  }//if
  int ret_size = pow(2, power); //The size of the space you'll need
  if(ret_size <= 16){ //If you need less than 16 bytes
    slot_t * tmp = focal_mem[0]->free_list;
    focal_mem[0]->free_list = focal_mem[0]->free_list->next_slot;
    return &(*tmp);
  }//if
  //while the rounded size is greater than cur slot size
 
  slot_t * tmp = focal_mem[power]->free_list;
  focal_mem[power]->free_list = focal_mem[power]->free_list->next_slot;
  
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
  size_t slot_size = xxmalloc_usable_size(ptr);
  /*Get to the correct place in focal_mem*/
  int index = 0;
  while(focal_mem[index]->size < slot_size){
    index++;    
  }
  header_t ptr_freed_from = focal_mem[index];
  /*Go through free_list*/
  slot_t cur_slot = ptr_freed_from->free_list;
  while(cur_slot->next != NULL){
    cur_slot = cur_slot->next;
  }//while
  /*Put new memory at the end*/
  intptr_t ptr_address = (intptr_t)ptr;
  intptr_t address = ROUND_DOWN(ptr_address, slot_size);

}

/**
 * Get the available size of an allocated object
 * \param ptr   A pointer somewhere inside the allocated object
 * \returns     The number of bytes available for use in this object
 */
size_t xxmalloc_usable_size(void* ptr) {
  header_t * tmp = (header_t*)ptr;
  intptr_t ptr_address = (intptr_t)ptr;
  /*Round down to the next multiple of 4096, since that's the size of a block*/
  tmp = ROUND_DOWN(ptr_address,4096);
  /*Get to the header there and return its size field*/
  return tmp->size;
}

/**Notes to self (DELETE THIS)
 * Didn't do the last paragraph of part B
 * If you fill a block, add a new block
 * Switch back the thing in the makefile from O0 to O2 (there are two of them)
 */
