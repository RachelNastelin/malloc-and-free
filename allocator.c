#define _GNU_SOURCE

#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <math.h>

/*CITATIONS: 
Pouya helped us understand how to run in gdb
Johnathan helped us debug xxmalloc and xxfree 
Medha and Mattori helped us with our general strategy and some segmentation faults
Alissa helped us debug xxfree
*/

// The minimum size returned by malloc
#define MIN_MALLOC_SIZE 16

// Round a value x up to the next multiple of y
#define ROUND_UP(x,y) ((x) % (y) == 0 ? (x) : (x) + ((y) - (x) % (y)))
#define ROUND_DOWN(x,y) ((x) - ((x) % (y)))

// The size of a single page of memory, in bytes. 
#define PAGE_SIZE 0x1000
#define MAGIC_NUMBER 0xD00FCA75

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
  int mag_num;
}header_t;


header_t * focal_mem[8]; //Holds the variable-sized memory blocks
slot_t* return_ptr; //The pointer that malloc returns, a global so that it isn't lost after being returned

/**
 * Make a new block
 * \param ret_size   the size of the slots in the new block
 * \return p         a pointer to the header of the new block
**/
header_t* make_block(size_t ret_size){
  
  header_t* p = (header_t*) mmap(NULL,PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  
  intptr_t free_list_ptr = (intptr_t) p;

  if(ret_size == 0){return NULL;}
  free_list_ptr += ROUND_UP(sizeof(header_t),ret_size);

  p->mag_num = MAGIC_NUMBER;
  p->size = ret_size;
  p->next_block = NULL;
  p->free_list = (slot_t*) free_list_ptr;
 
  
  /*splitting the block*/
  slot_t* cur_slot = p->free_list;
  intptr_t cur_slot_ptr = (intptr_t) cur_slot;
  intptr_t next_slot_ptr = cur_slot_ptr + (intptr_t)ret_size;
  slot_t* next_slot = (slot_t*) next_slot_ptr;



  /*Setting the next_slot field for each slot*/
  cur_slot->next_slot = next_slot;

  //determines number of complete blocks possible
  int end = floor((PAGE_SIZE-sizeof(header_t))/ ret_size);
  
  for(int i = 0; i < end - 1; i++){ 
    cur_slot = next_slot;
    next_slot =  (slot_t*)((intptr_t)cur_slot + (intptr_t)ret_size); 
    
    cur_slot->next_slot = next_slot;
  }
  cur_slot->next_slot = NULL;

  return p;
}

/**
 * Allocate space on the heap.
 * \param size  The minimium number of bytes that must be allocated
 * \returns     A pointer to the beginning of the allocated space.
 *              This function may return NULL when an error occurs.
 */
void* xxmalloc(size_t size) {
  
  if(use_emergency_block) {
    return emergency_block;
  } else if(in_malloc) {
    use_emergency_block = true;
    puts("ERROR! Nested call to malloc. Aborting.\n");
    
    exit(2);
  }

  in_malloc = true;

  //In the case size is > 2048
  if(size > 2048){
    size = ROUND_UP(size, PAGE_SIZE);
    void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    in_malloc = false;
    return p;
  }
 
  int index = 0;
  while((1<<(index+4)) < size){
    index++;
  }
  
  //The size of the space you'll need
  int ret_size = 1<<(index+4);

  //If there aren't any blocks for that size memory yet, make one
  if(focal_mem[index] == NULL){
    focal_mem[index] = make_block(ret_size);
  }

  header_t* cur_block = focal_mem[index];
 
  //If the first free_list is empty
  if(cur_block->free_list == NULL){ 
    
    //Look for a free_list that has memory left
    while(cur_block->next_block != NULL){
      cur_block = cur_block->next_block;
      
      //If you found some memory, give it out
      if(cur_block->free_list != NULL){
        return_ptr =cur_block->free_list;
        cur_block->free_list = cur_block->free_list->next_slot;
        
        in_malloc = false;
        return return_ptr;
      }
    }

    //You hit a null block, so make a new block
    cur_block->next_block = make_block(ret_size);
    cur_block = cur_block->next_block;
    return_ptr =cur_block->free_list;
    cur_block->free_list = cur_block->free_list->next_slot;
    in_malloc = false;
    return return_ptr;
  }//if the free_list was empty

  //If the free list wasn't empty
  return_ptr =cur_block->free_list;
  cur_block->free_list = cur_block->free_list->next_slot;
  in_malloc = false;
  return return_ptr;
}
  
/**
 * Get the available size of an allocated object
 * \param ptr   A pointer somewhere inside the allocated object
 * \returns     The number of bytes available for use in this object
 */
size_t xxmalloc_usable_size(void* ptr) {
  

  intptr_t ptr_address = (intptr_t)ptr;
  
  /*Round down to the next multiple of 4096, since that's the size of a block*/
  header_t * tmp = (header_t*) ROUND_DOWN(ptr_address,4096);
  
  /*Get to the header there and return its size field*/
  return tmp->size;
}

/**
 * Free space occupied by a heap object.
 * \param ptr   A pointer somewhere inside the object that is being freed
 */
void xxfree(void* ptr) {

  if(ptr == NULL){return;}
  
  /*Get the size of the slot that ptr is in*/
  size_t slot_size = xxmalloc_usable_size(ptr);
  
  /*Get to the correct place in focal_mem*/
  int index = 0;

  //If there are any null blocks at the start, skip them
  while(focal_mem[index] == NULL){
    index++;
  }
  
  //Go to the block with the right size memory
  while(focal_mem[index]->size < slot_size){
    index++;
    //If there are any more null blocks, skip them
    while(focal_mem[index] == NULL){
      index++;
    }
  }

  //get to the beginning of the block
  intptr_t ptr_address = (intptr_t)ptr;
  header_t *  tmp = (header_t*) ROUND_DOWN(ptr_address,4096);

  //check if the pointer is a large object or not
  if(tmp->mag_num != MAGIC_NUMBER){
    return;
  }
  slot_t * new_slot = (slot_t *) ROUND_DOWN(ptr_address,tmp->size);
  new_slot->next_slot = NULL;
  

  slot_t * cur_slot = tmp->free_list;
  tmp->free_list = new_slot;
  new_slot->next_slot = cur_slot;
}
