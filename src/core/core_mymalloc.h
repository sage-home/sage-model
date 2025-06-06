#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    /* Standard memory management functions */
    extern void *mymalloc(size_t n);
    extern void *mycalloc(const size_t count, const size_t size);
    extern void *myrealloc(void *p, size_t n);
    extern void myfree(void *p);
    extern void print_allocated(void);
    
    /* Simplified memory allocation with description */
    extern void *mymalloc_full(size_t n, const char *desc);
    
    /* Memory system lifecycle */
    extern int memory_system_init(void);
    extern void memory_system_cleanup(void);
    
    /* Tree-scoped memory management */
    extern void begin_tree_memory_scope(void);
    extern void end_tree_memory_scope(void);
    
    /* Dynamic expansion and monitoring */
    extern int expand_block_table(void);
    extern void check_memory_pressure_and_expand(void);
    extern void print_memory_stats(void);

#ifdef __cplusplus
}
#endif