#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"
#include "core_mymalloc.h"
#include "core_logging.h"

#define MAXBLOCKS 50000

static long Nblocks = 0;
static void *Table[MAXBLOCKS];
static size_t SizeTable[MAXBLOCKS];
static size_t TotMem = 0, HighMarkMem = 0, OldPrintedHighMark = 0;

/* file-local function */
long find_block(const void *p);
size_t get_aligned_memsize(size_t n);
void set_and_print_highwater_mark(void);

void *mymalloc_full(size_t n, const char *desc)
{
    n = get_aligned_memsize(n);
    
    /* Log large allocations for debugging */
    if (n > 1024*1024) { // Allocations larger than 1MB
        LOG_DEBUG("Large allocation requested: %.2f MB (%s)", 
                 n / (1024.0 * 1024.0), desc ? desc : "unknown");
    }

    if(Nblocks >= MAXBLOCKS) {
        LOG_ERROR("Nblocks = %ld. No blocks left in mymalloc().", Nblocks);
        LOG_ERROR("Total memory allocated: %.2f MB", TotMem / (1024.0 * 1024.0));
        ABORT(OUT_OF_MEMBLOCKS);
    }

    SizeTable[Nblocks] = n;
    TotMem += n;

    set_and_print_highwater_mark();

    Table[Nblocks] = malloc(n);
    if(Table[Nblocks] == NULL) {
        LOG_ERROR("Failed to allocate memory for %.2f MB", n / (1024.0 * 1024.0));
        ABORT(MALLOC_FAILURE);
    }
    
    void *ptr = Table[Nblocks];
    Nblocks += 1;
    
    return ptr;
}

/* Standard memory allocation function */
void *mymalloc(size_t n)
{
    return mymalloc_full(n, "unknown");
}

void *mycalloc(const size_t count, const size_t size)
{
    void *p = mymalloc(count * size);
    memset(p, 0, count*size);
    return p;
}

long find_block(const void *p)
{
    /* MS: Added on 6th June 2018
       Previously, the code only allowed re-allocation of the last block
       But really any block could be re-allocated -- we just need to search
       through and find the correct pointer
     */
    long iblock = Nblocks - 1;
    for(;iblock >= 0;iblock--) {
        if(p == Table[iblock]) break;
    }
    if(iblock < 0 || iblock >= Nblocks) {
        return -1;
    }

    return iblock;
}

void *myrealloc(void *p, size_t n)
{
    n = get_aligned_memsize(n);

    long iblock = find_block(p);
    if(iblock < 0) {
        LOG_ERROR("Could not locate ptr address = %p within the allocated blocks", p);
        // MEMORY DEBUG CODE - commented out for now but should revisit later in Phase 5.2.G or 5.3
        // for(int i=0;i<Nblocks;i++) {
        //     LOG_DEBUG("Address = %p size = %zu bytes", Table[i], SizeTable[i]);
        // }
        ABORT(INVALID_PTR_REALLOC_REQ);
    }
    void *newp = realloc(Table[iblock], n);
    if(newp == NULL) {
        LOG_ERROR("Failed to re-allocate memory for %.2f MB (old size = %.2f MB)", n / (1024.0 * 1024.0), SizeTable[iblock] / (1024.0 * 1024.0));
        ABORT(MALLOC_FAILURE);
    }
    Table[iblock] = newp;

    TotMem -= SizeTable[iblock];
    TotMem += n;
    SizeTable[iblock] = n;

    set_and_print_highwater_mark();
    return Table[iblock];
}

void myfree(void *p)
{
    if(p == NULL) return;

    XASSERT(Nblocks > 0, -1,
            "Error: While trying to free the pointer at address = %p, "
            "expected Nblocks = %ld to be larger than 0", p, Nblocks);

    long iblock = find_block(p);
    if(iblock < 0) {
        LOG_ERROR("Could not locate ptr address = %p within the allocated blocks", p);
        // MEMORY DEBUG CODE - commented out for now but should revisit later in Phase 5.2.G or 5.3
        // for(int i=0;i<Nblocks;i++) {
        //     LOG_DEBUG("Address = %p size = %zu bytes", Table[i], SizeTable[i]);
        // }
        ABORT(INVALID_PTR_REALLOC_REQ);
    }

    /* Record freeing of large allocations */
    if(SizeTable[iblock] > 1024*1024) {
        LOG_DEBUG("Freeing large allocation: %p, size: %.2f MB", 
                 p, SizeTable[iblock] / (1024.0 * 1024.0));
    }

    free(p);
    Table[iblock] = NULL;
    TotMem -= SizeTable[iblock];
    SizeTable[iblock] = 0;

    /*
      If not removing the last allocated pointer,
      move the last allocated pointer to this new
      vacant spot (i.e., don't leave a hole in the
      table of allocated pointer addresses)
      MS: 6/6/2018
     */
    if(iblock != Nblocks - 1) {
        Table[iblock] = Table[Nblocks - 1];
        SizeTable[iblock] = SizeTable[Nblocks - 1];
    }
    Nblocks--;
}

size_t get_aligned_memsize(size_t n)
{
    if((n % 8) > 0)
        n = (n / 8 + 1) * 8;

    if(n == 0)
        n = 8;

    return n;
}

void print_allocated(void)
{
#ifdef VERBOSE
    fprintf(stdout, "\nallocated = %g MB\n", TotMem / (1024.0 * 1024.0));
    fflush(stdout);
#endif
    return;
}

void set_and_print_highwater_mark(void)
{
    if(TotMem > HighMarkMem) {
        HighMarkMem = TotMem;
        if(HighMarkMem > OldPrintedHighMark + 10 * 1024.0 * 1024.0) {
            LOG_INFO("MEMORY HIGHWATER: %.2f MB with %ld blocks", 
                   HighMarkMem / (1024.0 * 1024.0), Nblocks);
            OldPrintedHighMark = HighMarkMem;
        }
    }
    return;
}