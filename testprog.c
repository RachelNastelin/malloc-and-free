#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <math.h> //for pow function

#define PAGE_SIZE 0x1000

typedef struct __attribute__((packed)) header{
  size_t size;
  void * nextBlock;
  void * free_list;
}header_t;

typedef struct __attribute__((packed)) slot{
  void * nextSlot;
}slot_t;

void *
func(size_t size){
  /*Storing 8 chunks of memory*/
  void * arr[8];
  for(int i =4; i < 12; i++){
    void* p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    arr[i-4] = p;
    header_t header_cur;
    //arr[i-4] = var;
    header_cur.size = pow(2, i);
    header_cur.nextBlock = NULL;
    header_cur.free_list= p + sizeof(header_t);
    
  }//for

  /*Rounding*/
  //Numbers are passed as 64bit integers
  if(__builtin_clzll(size) < 59){
    //It's going to be rounded up to the 16 byte block of memory
    void * tmp = header_cur.free_list; //pointing to the first free space after header
    header_cur.free_list = tmp+16;
    return tmp;
  }//if
  if((__builtin_ctzll(size) + __builtin_clzll(size)) == 63){
    //The size is a power of 2
    int power = __builtin_ctz(size);
  }//if
}//func


int
main(void){
  func(10);
}
/*Remember to go through each slot and set its nextSlot when you make the page*/

/*For free:
  If you have the slots:
  _____________________________
  |         |         |         |
  |____A____|____B____|____C____|

  (not including header)
  and you gave away A and B, now free_list should point to C, and 
  C.nextSlot = NULL. Then say you get A back. Now you'll want to: 
  1. Let free_list still point to C
  2. Make C.nextSlot = A
  3. Make A.nextSlot = NULL

  If you give away C and A without getting B back, and then the user wants to use
  more memory of that size (for example, 16 bytes), then you need to use 
  nextBlock, not nextSlot, to make a new page
*/
