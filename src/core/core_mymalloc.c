#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "core_allvars.h"
#include "core_mymalloc.h"

#define MAXBLOCKS 2048

static long Nblocks = 0;
static void *Table[MAXBLOCKS];
static size_t SizeTable[MAXBLOCKS];
static size_t TotMem = 0, HighMarkMem = 0, OldPrintedHighMark = 0;

/* file-local function */
long find_block(const void *p);
size_t get_aligned_memsize(size_t n);
void set_and_print_highwater_mark(void);

void *mymalloc(size_t n)
{
    n = get_aligned_memsize(n);

    if(Nblocks >= MAXBLOCKS) {
        fprintf(stderr, "Nblocks = %ld No blocks left in mymalloc().\n", Nblocks);
        ABORT(OUT_OF_MEMBLOCKS);
    }

    SizeTable[Nblocks] = n;
    TotMem += n;

    set_and_print_highwater_mark();

    Table[Nblocks] = malloc(n);
    if(Table[Nblocks] == NULL) {
        fprintf(stderr, "Failed to allocate memory for %g MB\n",  n / (1024.0 * 1024.0) );
        ABORT(MALLOC_FAILURE);
    }

    Nblocks += 1;

    return Table[Nblocks - 1];
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
        fprintf(stderr,"Error: Could not locate ptr address = %p within the allocated blocks\n", p);
        for(int i=0;i<Nblocks;i++) {
            fprintf(stderr,"Address = %p size = %zu bytes\n", Table[i], SizeTable[i]);
        }
        ABORT(INVALID_PTR_REALLOC_REQ);
    }
    void *newp = realloc(Table[iblock], n);
    if(newp == NULL) {
        fprintf(stderr, "Error: Failed to re-allocate memory for %g MB (old size = %g MB)\n",  n / (1024.0 * 1024.0), SizeTable[Nblocks-1]/ (1024.0 * 1024.0) );
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
        fprintf(stderr,"Error: Could not locate ptr address = %p within the allocated blocks\n", p);
        for(int i=0;i<Nblocks;i++) {
            fprintf(stderr,"Address = %p size = %zu bytes\n", Table[i], SizeTable[i]);
        }
        ABORT(INVALID_PTR_REALLOC_REQ);
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
#ifdef VERBOSE
            fprintf(stderr, "\nnew high mark = %g MB\n", HighMarkMem / (1024.0 * 1024.0));
#endif
            OldPrintedHighMark = HighMarkMem;
        }
    }
    return;
}
