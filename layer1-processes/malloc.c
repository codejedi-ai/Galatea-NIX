#include "syscall.h"
#include "processes.h"
#include "nameserver.h"
#include "asm.h"
#include "rpi.h"
#include "util.h"
#include "custstr.h"
#include "gic.h"
#include "malloc.h"
#define HEAP_ADDR 0x1000000
// === Insert any helper functions here
// one word is 8 bytes
int setmemsize = 1024;
// uint64_t freeListRoot = (uint64_t)(uint64_t *)HEAP_ADDR;
uint64_t freeListRoot = HEAP_ADDR; // this variable is the pointer to the root of the list
uint64_t *mymalloc(uint64_t size)
{
  uint64_t wordNeedAllocateinBytes = 8 * (size + 1); // bytes
  uint64_t *freeListHead = &freeListRoot;
  // dereference to get the actual value which contains the previouse node
  // pointer reference to anouther pointer
  // the current node is the one it points to
  uint64_t *curNode = (uint64_t *)(*freeListHead);
  // this part of the code is looking for a block of continuous memory of size bytes
  while (curNode != 0)
  {
    curNode = (uint64_t *)(*freeListHead);
    uint64_t avalSize = *curNode;
    if (avalSize >= wordNeedAllocateinBytes)
    {
      if (avalSize - wordNeedAllocateinBytes == 8)
      {
        size++;
        wordNeedAllocateinBytes += 8;
      }
      break;
    }
    freeListHead = ((uint64_t *)*freeListHead) + 1;
    curNode = (uint64_t *)(*freeListHead);
  }
  if (curNode == 0)
    return 0;
  // curNode is the adress we keep the size value
  // splitting
  uint64_t avalSize = *curNode;
  if (*curNode > wordNeedAllocateinBytes)
  {
    // this bar initializes the new node
    *(curNode + size + 1) = *curNode - wordNeedAllocateinBytes; // making the size of the new segment
    *(curNode + size + 2) = *(curNode + 1);                     // pointing to the next node in the list
    *freeListHead = (uint64_t)(curNode + size + 1);
  }
  else
  {
    // do not make a new node just reallocate the next pointer
    *freeListHead = *(curNode + 1);
  }
  *curNode = wordNeedAllocateinBytes;
  return curNode + 1;
}

// deallocates the memory stored at address.
// assumes that address contains either an address allocated by mymalloc, in which case it deallocates that memory,
// or the value 0 (NULL), in which case myfree does nothing.
void myfree(uint64_t *address)
{
  if (!address)
    return;
  // cout << "Freeing: " << p << endl;
  //  need to use a reference freelisthead is the reference to the head
  //  need to store the value and address of the free list head
  uint64_t prev = freeListRoot;                       // *listHead is the head of the list
  uint64_t FLH = freeListRoot;                        // *listHead is the head of the list
  uint64_t next = ((uint64_t) * ((uint64_t *)FLH + 1)); // next is a lvalue
  address = address - 1;
  if ((uint64_t *)freeListRoot > address)
  {
    // add front
    // we also need to merge
    *(address + 1) = FLH;
    freeListRoot = (uint64_t)address;
    // need to have past, present and future nodes
    if ((uint64_t)address + *address == *(address + 1))
    {
      // The address is equal to the one $size bits over implieing the next block of addresses is also empty
      *address = *address + *(uint64_t *)*(address + 1);
      *(address + 1) = *(1 + (uint64_t *)FLH);
    }
    return;
  }
  // add to the middle
  // this checks wether or not the next value is smaller than the address. if the next value is then the
  // current address FLH is definatelly smaller than the address and the nect value is larger
  while (next != 0 && next < (uint64_t)address)
  {
    next = (*((uint64_t *)FLH + 1));
    // cout << "FLH: " << (uint64_t *)FLH << " next: " << (uint64_t *)next << endl;
    if (FLH == next)
    {
      //  cout << "Infinite Cycle detected\n";
      return;
    }
    FLH = next; // *listHead is the head of the list
    next = (*((uint64_t *)FLH + 1));
  }
  // next is the next pointer
  // address is the current
  // FLH is the previouse
  // cout << "FLH: " << (uint64_t *)FLH << " p: " << p << " next: " << (uint64_t *)next << endl;
  *(address + 1) = next; // set the address pointing from address to next
  if ((uint64_t)address + *address == *(address + 1))
  {
    // merge with the right
    // The address is equal to the one $size bits over implieing the next block of addresses is also empty
    *address = *address + *(uint64_t *)next;
    *(address + 1) = *(1 + (uint64_t *)next);
  }
  *((uint64_t *)FLH + 1) = (uint64_t)address; // set the next value of FLH to p

  if (*(uint64_t *)FLH + FLH == (uint64_t)address)
  {
    // merge with the left
    // The address is equal to the one $size bits over implieing the next block of addresses is also empty
    *(uint64_t *)FLH = *(uint64_t *)FLH + *address;
    *((uint64_t *)FLH + 1) = *(1 + address); // need to know th evalue of the next
  }
}
/*
void printMemory()
{
  cout << "===== Printing Memory =====" << endl;
  for (int i = 0; i <= setmemsize / 8 + 1; i++)
  {
    uint64_t *ptr = (TheArena + i);

    cout << ptr << " in dec: " << *ptr << " in hex: " << (uint64_t *)*ptr << "\n";
  }
}

void printFreeList()
{
  while (freeListRoot != 0)
  {
    cout << (uint64_t *)freeListRoot << endl;
    freeListRoot = *((uint64_t *)freeListRoot + 1);
  }
}
*/
int malloctest() {
    uint64_t result = 0;
    uint64_t* one = mymalloc(241);
    uint64_t* two = mymalloc(241);
    myfree(one);

    uint64_t* evil = mymalloc(240);
    uint64_t* good = mymalloc(1);
    evil[3] = 240; 
    good[0] = evil[3]+1;
    myfree(evil);

    uint64_t* cs[241];
    for(int i=0; i<241; i++) {
        cs[i] = mymalloc(1);
        cs[i][0] = 1;
    }
    uint64_t* big = mymalloc(1000);
    for(int i=0; i<1000; i++) {
        big[i] = 1000;
    }
    for(int i=0; i<241; i++) {
        result += cs[i][0];
        myfree(cs[i]);
    }

    result += good[0];
    myfree(good);

    // Returns nonzero exit code if the result is wrong
    return result != 241*2;
}
/*
int main2()
{
  uint64_t *addr = mymalloc(4);
  uint64_t *addr_1 = mymalloc(4);
  uint64_t *addr_2 = mymalloc(4);
  uint64_t *addr_3 = mymalloc(4);

  uint64_t *addr_4 = mymalloc(4);
  printMemory();
  myfree(addr_1);
  printMemory();
  myfree(addr_2);
  printMemory();

  uint64_t *addr_5 = mymalloc(8);
  cout << "Special case " << endl;
  for(int i = 0; i < 8; i ++){
    addr_5[i] = -1;
  }
  printMemory();
  printFreeList();
  // Returns nonzero exit code if the result is wrong
  return addr == 0;

 main2();
}
  */