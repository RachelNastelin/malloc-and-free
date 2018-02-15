#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <math.h> //for pow function

#define PAGE_SIZE 0x1000

typedef struct __attribute__((packed)) slot slot_t;


typedef struct __attribute__((packed)) header{
  size_t size;
  void * next_block;
  slot_t * free_list;
}header_t;

typedef struct __attribute__((packed)) slot{
  slot_t * next_slot;
}slot_t;

void *
func(size_t size){
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

  
  //splitting the block
  for(int i = 0; i < 8; i++){ //Loops through focal_mem

    int num_slots = floor(PAGE_SIZE/(focal_mem[i]->size));
    slot_t* cur = focal_mem[i]->free_list;
    slot_t* temp = cur + focal_mem[i]->size;

    cur->next_slot = temp;

    for(int j = 0; j < num_slots; j++){ //Loops through slots in each block
      cur = temp;
      temp =  cur + focal_mem[i]->size;
    }//for j

    cur->next_slot = NULL;
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
  return &(*tmp);
}//func

int
main(void){
  func(10);
}
/*Remember to go through each slot and set its nextSlot when you make the page*/

/*For free:
  If you have the slots:
  ______________________________
  |         |         |         |
  |____A____|____B____|____C____|

  (not including header)
  and you gave away A and B, now free_list should point to C, and 
  C.nextSlot = NULL. Then say you get A back. Now you'll want to: 
  1. Let free_list still point to C
  2. Make C.nextSlot = A
  3. Make A.nextSlot = NULL

  If you give away C and A without getting B back, and then the user wants to 
  use more memory of that size (for example, 16 bytes), then you need to use 
  nextBlock, not nextSlot, to make a new page
*/
