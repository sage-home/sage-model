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

#ifdef __cplusplus
}
#endif