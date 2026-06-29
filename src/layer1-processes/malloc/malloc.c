#include "syscall.h"
#include "processes.h"
#include "asm.h"
#include "rpi.h"
#include "util.h"
#include "gic.h"
#include "malloc.h"

/* Heap arena in BSS (Pi 4 RAM). */
static uint8_t heap_arena[HEAP_SIZE_DEFAULT] __attribute__((aligned(8)));

uint64_t freeListRoot;  /* points to first free block; each block is [size (8B)][next (8B)] then payload */

/* Initialize heap: one free block at heap_start with [size][next]=[heap_size, 0]. Call once at boot. */
void malloc_init(uint64_t heap_start, uint64_t heap_size)
{
	freeListRoot = heap_start;
	*(uint64_t *)heap_start = heap_size;   /* block size in bytes */
	*(uint64_t *)(heap_start + 8) = 0;     /* next = NULL */
}

/* Initialize heap using BSS arena. Call from InitSys instead of malloc_init(HEAP_ADDR_DEFAULT, ...). */
void malloc_init_default(void)
{
	malloc_init((uint64_t)heap_arena, (uint64_t)sizeof(heap_arena));
}

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

/* ---- Heap statistics (used by the shell `mem` command) ------------------- */

/* Total heap size in bytes. */
uint64_t malloc_total_bytes(void)
{
	return (uint64_t)HEAP_SIZE_DEFAULT;
}

/* Sum of all free blocks (walks the free list). Each block: [size][next]. */
uint64_t malloc_free_bytes(void)
{
	uint64_t total = 0;
	uint64_t node = freeListRoot;
	while (node) {
		total += *(uint64_t *)node;          /* block size in bytes */
		node = *(uint64_t *)(node + 8);      /* next pointer */
	}
	return total;
}

/* Number of free blocks (fragmentation indicator). */
uint64_t malloc_free_blocks(void)
{
	uint64_t count = 0;
	uint64_t node = freeListRoot;
	while (node) {
		count++;
		node = *(uint64_t *)(node + 8);
	}
	return count;
}