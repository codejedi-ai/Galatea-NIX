#include "syscall.h"
#include "asm.h"
#include "rpi.h"
#include "util.h"
#include "gic.h"
#include "../config.h"
#include "malloc.h"

/* Heap arena in BSS so it is in RAM (0x1000000 is not valid on QEMU virt where RAM is at 0x40000000+). */
static uint8_t heap_arena[HEAP_SIZE_DEFAULT] __attribute__((aligned(8)));

uint64_t freeListRoot;  /* points to first free block; each block is [size (8B)][next (8B)] then payload */

/*
 * Every memory reservation has three properties: Process ID, VRAM address, Physical RAM address.
 * VRAM is per-process: each process has its own virtual range (VRAM_HEAP_BASE + process_id * size).
 * The map stores the triple so that vram under a process is mapped to physical RAM.
 */
typedef struct {
	int process_id;
	uint64_t vram_address;   /* payload address in process's virtual space */
	uint64_t phys_start;     /* block header address in physical RAM (0 = slot free) */
	uint64_t size_bytes;
} MallocMapEntry;

static MallocMapEntry alloc_map[MALLOC_MAP_ENTRIES];
/* Next VRAM payload address per process (within each process's virtual heap range). */
static uint64_t next_vram[NPROCESSES];

static uint64_t vram_base(int process_id)
{
	if (process_id < 0 || process_id >= NPROCESSES)
		return VRAM_HEAP_BASE;
	return VRAM_HEAP_BASE + (uint64_t)(process_id) * VRAM_HEAP_SIZE_PER_PROCESS;
}

static void map_clear(void)
{
	for (unsigned i = 0; i < MALLOC_MAP_ENTRIES; i++)
		alloc_map[i].phys_start = 0;
	for (int i = 0; i < NPROCESSES; i++)
		next_vram[i] = vram_base(i);
}

static int map_add(int process_id, uint64_t vram_payload, uint64_t phys_start, uint64_t size_bytes)
{
	if (process_id < 0 || process_id >= NPROCESSES)
		return -1;
	for (unsigned i = 0; i < MALLOC_MAP_ENTRIES; i++) {
		if (alloc_map[i].phys_start == 0) {
			alloc_map[i].process_id = process_id;
			alloc_map[i].vram_address = vram_payload;
			alloc_map[i].phys_start = phys_start;
			alloc_map[i].size_bytes = size_bytes;
			return 0;
		}
	}
	return -1; /* map full */
}

static void map_remove(uint64_t phys_start)
{
	for (unsigned i = 0; i < MALLOC_MAP_ENTRIES; i++) {
		if (alloc_map[i].phys_start == phys_start) {
			alloc_map[i].phys_start = 0;
			return;
		}
	}
}

/* Resolve process's VRAM payload address to physical payload pointer (for kernel/MMU). */
uint64_t *malloc_vram_to_phys(int process_id, uint64_t vram_payload)
{
	for (unsigned i = 0; i < MALLOC_MAP_ENTRIES; i++) {
		if (alloc_map[i].phys_start != 0 &&
		    alloc_map[i].process_id == process_id &&
		    alloc_map[i].vram_address == vram_payload)
			return (uint64_t *)(alloc_map[i].phys_start) + 1;  /* payload */
	}
	return 0;
}

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
	map_clear();
}

/*
 * Defragmentation (SmallestInfiniteSet-style): free list is kept sorted by ascending
 * address. We always take the first block that fits = smallest address that fits.
 * When we free, we add the block back in sorted order and coalesce.
 */
uint64_t *malloc_for_process(int process_id, uint64_t size)
{
  uint64_t wordNeedAllocateinBytes = 8 * (size + 1); /* bytes */
  uint64_t *freeListHead = &freeListRoot;
  uint64_t *curNode = (uint64_t *)(*freeListHead);
  if (process_id < 0 || process_id >= NPROCESSES)
    return 0;
  uint64_t vram_end = vram_base(process_id) + VRAM_HEAP_SIZE_PER_PROCESS;
  if (next_vram[process_id] + wordNeedAllocateinBytes > vram_end)
    return 0;  /* no room in this process's virtual heap */
  /* First fit in address order = smallest address that fits (defrag). */
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
        if (next_vram[process_id] + wordNeedAllocateinBytes > vram_end)
          return 0;
      }
      break;
    }
    freeListHead = ((uint64_t *)*freeListHead) + 1;
    curNode = (uint64_t *)(*freeListHead);
  }
  if (curNode == 0)
    return 0;
  /* Split or use whole block */
  if (*curNode > wordNeedAllocateinBytes)
  {
    *(curNode + size + 1) = *curNode - wordNeedAllocateinBytes;
    *(curNode + size + 2) = *(curNode + 1);
    *freeListHead = (uint64_t)(curNode + size + 1);
  }
  else
  {
    *freeListHead = *(curNode + 1);
  }
  *curNode = wordNeedAllocateinBytes;
  /* Reserve VRAM in this process's space and record (process_id, vram, phys). */
  uint64_t vram_payload = next_vram[process_id];
  next_vram[process_id] += wordNeedAllocateinBytes;
  if (map_add(process_id, vram_payload, (uint64_t)curNode, wordNeedAllocateinBytes) != 0) {
    next_vram[process_id] -= wordNeedAllocateinBytes;
    free(curNode + 1);  /* put block back (map_remove is no-op here) */
    return 0;
  }
  /* Return physical payload pointer so dereference works without MMU. */
  return curNode + 1;
}

uint64_t *malloc(uint64_t size)
{
  /* Default to process 0 when no process context (e.g. init). */
  return malloc_for_process(0, size);
}

/* Deallocates the memory; ptr must be a payload pointer from malloc (or NULL). */
void free(uint64_t *address)
{
  if (!address)
    return;
  /* Remove from physical↔virtual map (addBack: block re-enters free set). */
  map_remove((uint64_t)(address - 1));
  address = address - 1;  /* block header */
  uint64_t FLH = freeListRoot;
  uint64_t next = ((uint64_t) * ((uint64_t *)FLH + 1));
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