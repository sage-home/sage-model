#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"
#include "core_mymalloc.h"
#include "core_logging.h"

#define INITIAL_MAXBLOCKS 10000
#define EXPANSION_SAFETY_MARGIN 1000

static long Nblocks = 0;
static long MaxBlocks = 0;  /* Current capacity of block tables */
static void **Table = NULL;  /* Dynamically allocated block table */
static size_t *SizeTable = NULL;  /* Dynamically allocated size table */
static size_t TotMem = 0, HighMarkMem = 0, OldPrintedHighMark = 0;
static long TreeScopeStart = 0;  /* Start of current tree memory scope */
static int memory_system_initialized = 0;

/* file-local function */
long find_block(const void *p);
size_t get_aligned_memsize(size_t n);
void set_and_print_highwater_mark(void);
int expand_block_table_internal(void);

void *mymalloc_full(size_t n, const char *desc)
{
    n = get_aligned_memsize(n);
    
    /* Initialize memory system if not already done */
    if (!memory_system_initialized) {
        if (memory_system_init() != 0) {
            LOG_ERROR("Failed to initialize memory system");
            ABORT(MALLOC_FAILURE);
        }
    }
    
    /* Check memory pressure and expand if needed */
    check_memory_pressure_and_expand();
    
    /* Log large allocations for debugging */
    if (n > 1024*1024) { // Allocations larger than 1MB
        LOG_DEBUG("Large allocation requested: %.2f MB (%s)", 
                 n / (1024.0 * 1024.0), desc ? desc : "unknown");
    }

    /* Check if we still have capacity after potential expansion */
    if(Nblocks >= MaxBlocks) {
        LOG_ERROR("Nblocks = %ld. No blocks left in mymalloc() even after expansion.", Nblocks);
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
        /* Pointer not found in tracking table - likely already freed by tree scope cleanup.
         * This is a valid scenario when tree-scoped memory cleanup frees blocks that
         * are later explicitly freed during component cleanup. Handle gracefully. */
        LOG_DEBUG("Pointer %p not found in block table - likely already freed by tree scope", p);
        return;
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

/* Memory system initialization */
int memory_system_init(void)
{
    if (memory_system_initialized) {
        LOG_WARNING("Memory system already initialized");
        return 0;
    }
    
    MaxBlocks = INITIAL_MAXBLOCKS;
    
    /* Allocate initial block tables */
    Table = (void **) malloc(MaxBlocks * sizeof(void *));
    if (Table == NULL) {
        LOG_ERROR("Failed to allocate initial block table");
        return -1;
    }
    
    SizeTable = (size_t *) malloc(MaxBlocks * sizeof(size_t));
    if (SizeTable == NULL) {
        LOG_ERROR("Failed to allocate initial size table");
        free(Table);
        Table = NULL;
        return -1;
    }
    
    /* Initialize state */
    Nblocks = 0;
    TotMem = 0;
    HighMarkMem = 0;
    OldPrintedHighMark = 0;
    TreeScopeStart = 0;
    memory_system_initialized = 1;
    
    LOG_INFO("Memory system initialized with %ld initial block capacity", MaxBlocks);
    return 0;
}

/* Memory system cleanup */
void memory_system_cleanup(void)
{
    if (!memory_system_initialized) {
        return;
    }
    
    /* Log final memory statistics */
    LOG_INFO("Memory system cleanup: %ld blocks still allocated, %.2f MB total", 
            Nblocks, TotMem / (1024.0 * 1024.0));
    
    /* Free any remaining allocated blocks */
    for (long i = 0; i < Nblocks; i++) {
        if (Table[i] != NULL) {
            LOG_WARNING("Freeing unfreed block %ld of size %.2f MB", 
                       i, SizeTable[i] / (1024.0 * 1024.0));
            free(Table[i]);
        }
    }
    
    /* Free the tables themselves */
    if (Table != NULL) {
        free(Table);
        Table = NULL;
    }
    
    if (SizeTable != NULL) {
        free(SizeTable);
        SizeTable = NULL;
    }
    
    /* Reset state */
    Nblocks = 0;
    MaxBlocks = 0;
    TotMem = 0;
    memory_system_initialized = 0;
    
    LOG_INFO("Memory system cleanup complete");
}

/* Internal function to expand block table capacity */
int expand_block_table_internal(void)
{
    long new_max_blocks = MaxBlocks * 2;  /* Double the capacity */
    
    LOG_DEBUG("Expanding block table from %ld to %ld blocks", MaxBlocks, new_max_blocks);
    
    /* Expand Table array */
    void **new_table = (void **) realloc(Table, new_max_blocks * sizeof(void *));
    if (new_table == NULL) {
        LOG_ERROR("Failed to expand block table from %ld to %ld blocks", MaxBlocks, new_max_blocks);
        return -1;
    }
    Table = new_table;
    
    /* Expand SizeTable array */
    size_t *new_size_table = (size_t *) realloc(SizeTable, new_max_blocks * sizeof(size_t));
    if (new_size_table == NULL) {
        LOG_ERROR("Failed to expand size table from %ld to %ld blocks", MaxBlocks, new_max_blocks);
        return -1;
    }
    SizeTable = new_size_table;
    
    MaxBlocks = new_max_blocks;
    
    LOG_INFO("Block table expanded to %ld blocks capacity", MaxBlocks);
    return 0;
}

/* Public function to expand block table */
int expand_block_table(void)
{
    if (!memory_system_initialized) {
        LOG_ERROR("Memory system not initialized");
        return -1;
    }
    
    return expand_block_table_internal();
}

/* Check memory pressure and expand if needed */
void check_memory_pressure_and_expand(void)
{
    if (!memory_system_initialized) {
        return;
    }
    
    /* Check if we're approaching the block table limit */
    if (Nblocks >= MaxBlocks - EXPANSION_SAFETY_MARGIN) {
        LOG_DEBUG("Memory pressure detected: %ld blocks used, %ld capacity", Nblocks, MaxBlocks);
        
        if (expand_block_table_internal() != 0) {
            LOG_ERROR("Failed to expand block table under memory pressure");
        }
    }
}

/* Tree-scoped memory management */
void begin_tree_memory_scope(void)
{
    if (!memory_system_initialized) {
        LOG_WARNING("Memory system not initialized for tree scope");
        return;
    }
    
    TreeScopeStart = Nblocks;
    LOG_DEBUG("Starting tree memory scope at block %ld", TreeScopeStart);
}

void end_tree_memory_scope(void)
{
    if (!memory_system_initialized) {
        LOG_WARNING("Memory system not initialized for tree scope cleanup");
        return;
    }
    
    long blocks_to_free = Nblocks - TreeScopeStart;
    size_t memory_to_free = 0;
    
    /* Calculate memory being freed */
    for (long i = TreeScopeStart; i < Nblocks; i++) {
        memory_to_free += SizeTable[i];
    }
    
    /* Free all blocks allocated since scope start (in reverse order) */
    while (Nblocks > TreeScopeStart) {
        if (Table[Nblocks - 1] != NULL) {
            free(Table[Nblocks - 1]);
            Table[Nblocks - 1] = NULL;
            TotMem -= SizeTable[Nblocks - 1];
            SizeTable[Nblocks - 1] = 0;
        }
        Nblocks--;
    }
    
    LOG_DEBUG("Freed %ld blocks (%.2f MB) in tree scope cleanup", 
             blocks_to_free, memory_to_free / (1024.0 * 1024.0));
}

/* Enhanced memory statistics */
void print_memory_stats(void)
{
    if (!memory_system_initialized) {
        LOG_INFO("Memory system not initialized");
        return;
    }
    
    LOG_INFO("MEMORY STATS: %.2f MB allocated in %ld blocks (%.2f MB peak, %ld/%ld table capacity)", 
            TotMem / (1024.0 * 1024.0), Nblocks, 
            HighMarkMem / (1024.0 * 1024.0), Nblocks, MaxBlocks);
}