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
#define ROUND_DOWN(x,y) ((x) % (y) == 0 ? (x) : ((x) - (x) % (y)))

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

typedef struct __attribute__((packed)) header header_t;
typedef struct __attribute__((packed)) header{
  size_t size;
  header_t* next_block;
  slot_t * free_list;
}header_t;


header_t * focal_mem[8]; //Holds the variable-sized memory blocks
slot_t* return_ptr;

header_t* make_block(int size){
  header_t* p = (header_t*) mmap(NULL,PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  intptr_t free_list_ptr = (intptr_t) p;
  free_list_ptr += ROUND_UP(sizeof(header_t),size);
  
  p-> size = size;
  p->next_block = NULL;
  p->free_list = (slot_t*) free_list_ptr;
 
  
  /*splitting the block*/
  slot_t* cur_slot = p->free_list;
  slot_t* next_slot = cur_slot + size;

  cur_slot->next_slot = next_slot;

  int end = floor((PAGE_SIZE-sizeof(header_t))/ size);
  
  for(int i = 0; i < end; i++){ //Loops through slots in each block
    cur_slot = next_slot;
    next_slot =  cur_slot + size;
    cur_slot->next_slot = next_slot;
  }

  cur_slot->next_slot = NULL;

  return p;
}


/*
helper_t * make_focal_mem(){
 // Request memory from the operating system in page-sized chunks
  void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  // Check for errors
  if(p == MAP_FAILED) {
    use_emergency_block = true;
    perror("mmap");
    exit(2);
  }

  header_t header_cur;

  for(int i =4; i < 12; i++){
    //Get a pointer to the next block
    void* p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    focal_mem[i-4] = p;
    focal_mem[i-4]->size = pow(2, i);
    focal_mem[i-4]->next_block = NULL;
    focal_mem[i-4]->free_list = p + ROUND_UP(sizeof(header_t), focal_mem[i-4]->size);
  }//for
  
  return focal_mem;
}//xxmalloc_helper
*/

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

  //In the case size is > 2048
  if(size > 2048){
    size = ROUND_UP(size, PAGE_SIZE);
    void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    in_malloc = false;
    return p;
  }
  
 
  /*Rounding*/
  int leading = __builtin_clzll(size); //Number of leading zeros
  int trailing = __builtin_ctzll(size); //Number of leading zeros
  int index = 0;
  
  if(leading + trailing != 64){//There's more than 1 1 in the binary number

    while(pow(2, (index+4)) < size){
      index++;
    }
  }
  
  int ret_size = pow(2, (index+4)); //The size of the space you'll need
  
  if(ret_size <= 16){ //If you need less than 16 bytes

    if(focal_mem[index] == NULL){
      focal_mem[index] = make_block(ret_size);
    }

    if(focal_mem[index]->free_list == NULL){
      header_t* last_block;

      while(focal_mem[index]->next_block != NULL){
        last_block = focal_mem[index]->next_block;
      }
    
      last_block->next_block = make_block(ret_size);
      focal_mem[index]->free_list = last_block-> next_block-> free_list;
    
      return_ptr = focal_mem[0]->free_list;
      focal_mem[0]->free_list = focal_mem[0]->free_list->next_slot;
    
      // Done with malloc, so clear this flag
      in_malloc = false; //ask about this
      return &return_ptr;
    }
  }

  //If you need between 16 and 2048 bytes
  if(focal_mem[index] == NULL){
    focal_mem[index] = make_block(ret_size);
  }

  header_t* cur_block = focal_mem[index];
  if(cur_block->free_list == NULL){

    while(cur_block->next_block != NULL){
      cur_block = cur_block->next_block;

      if(cur_block->free_list != NULL){
        return_ptr =cur_block->free_list;
        cur_block->free_list = cur_block->free_list->next_slot;
        
        in_malloc = false;
        return return_ptr;
      }
    }
    
    cur_block->next_block = make_block(ret_size);
    cur_block->free_list = NULL;
    return_ptr =cur_block->next_block->free_list;
    
    in_malloc = false;
    return return_ptr;
    
  }
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
  tmp = (header_t*) ROUND_DOWN(ptr_address,4096);
  
  /*Get to the header there and return its size field*/
  return tmp->size;
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

  header_t* ptr_freed_from = focal_mem[index];

  /*Go through free_list*/
  slot_t* cur_slot = ptr_freed_from->free_list;
  while(cur_slot->next_slot != NULL){
    cur_slot = cur_slot-> next_slot;
  }//while

  /*Put new memory at the end*/
  intptr_t ptr_address = (intptr_t)ptr;
  intptr_t address = ROUND_DOWN(ptr_address, slot_size);

}

// Switch back the thing in the makefile from O0 to O2 (there are two of them)
