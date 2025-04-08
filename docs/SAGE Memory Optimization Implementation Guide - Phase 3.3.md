# SAGE Memory Optimization Implementation Guide - Phase 3.3

## 1. Alignment with Refactoring Plan

This implementation guide addresses Phase 3.3 of the enhanced SAGE refactoring plan, focusing on Memory Optimization. Key objectives include:
- Configurable buffered I/O.
- Memory mapping options for large files.
- Geometric growth for dynamic array reallocation.
- Memory pooling for galaxy struct allocations.

**Note:** Initial plans included halo caching and prefetching. However, analysis revealed SAGE's depth-first tree traversal processes each halo only once per forest. Therefore, caching halo data would provide no benefit and has been removed from this phase. Optimizations now focus on I/O efficiency and memory allocation patterns.

## 2. Architecture Overview

Phase 3.3 introduces components designed to reduce peak memory usage, minimize I/O bottlenecks, and decrease allocation overhead. These components integrate with the existing I/O Interface system.

```
┌───────────────────────────┐      ┌─────────────────────┐      ┌─────────────────────┐
│ SAGE Tree Traversal       │      │ Array Utils         │      │ Memory Pool Manager │
│ (core_build_model.c)      │      │ (core_array_utils.c)│      │ (core_memory_pool.c)│
│ - construct_galaxies      │◄─────┤ - Geometric growth  │◄────▶│ - Size segregation  │
│ - join_galaxies_of_prog.  │      │ - Galaxy array      │      │ - Galaxy alloc/free │
│ - evolve_galaxies         │      │   handling          │      │ - Stats tracking    │
└───────────────┬───────────┘      └─────────┬───────────┘      └─────────┬───────────┘
                │                            │                            │
                │                            ▼                            ▼
                │              ┌───────────────────────────┐   ┌─────────────────────┐
                │              │ I/O Interface             │   │ Core Memory Alloc.  │
                │              │ (io_interface.c)          │   │ (core_mymalloc.c)   │
                │              │ - Format handlers         │   │ - Tracks allocations│
                │              │ - Resource tracking       │   └──────────▲──────────┘
                │              └─────────────┬─────────────┘              │
                │                            │                            │
                ▼                            ▼                            │
┌───────────────────────────┐      ┌─────────────────────┐                │
│ Buffer Manager            │      │ Memory Mapping      │                │
│ (io_buffer_manager.c)     │      │ (io_memory_map.c)   │                │
│ - Dynamic sizing          │◄─────┤ - mmap/MapViewOfFile│                │
│ - Write/Read buffering    │      │ - Cross-platform    │                │
│ - Flush mechanism         │      │   wrappers          │                │
└───────────────┬───────────┘      └─────────┬───────────┘                │
                │                            │                            │
                ▼                            ▼                            │
┌───────────────────────────┐      ┌─────────────────────┐                │
│ File System               │      │ Operating System    │                │
│ (OS level)                │      │                     │                │
│ - File reads/writes       │◄─────┤                     │◀───────────────┘
│ - File seeking            │      │                     │
└───────────────────────────┘      └─────────────────────┘

```

**Interactions:**
- **Tree Traversal** uses the **Memory Pool** to allocate/free `GALAXY` structs. `join_galaxies_of_progenitors` uses **Array Utils**.
- **I/O Handlers** (via `io_interface`) use the **Buffer Manager** for file I/O.
- **Buffer Manager** can potentially use **Memory Mapping** for read operations as an optimization, falling back to standard reads if mapping fails or is disabled.
- **Memory Pool** relies on the underlying **Core Memory Allocation** (`mymalloc`).

## 3. File Structure and Implementation Details

### 3.1 Array Growth Utilities

*   **New Files:** `src/core/core_array_utils.h`, `src/core/core_array_utils.c`
*   **Purpose:** Replace fixed-increment array reallocation (`*maxgals += 10000`) with geometric growth (e.g., multiplying capacity by 1.5 or 2) to reduce the frequency of expensive `realloc` calls.
*   **Key Functions:**
    ```c
    int array_expand(void** array, size_t element_size, int* current_capacity, int min_new_size, float growth_factor);
    int galaxy_array_expand(struct GALAXY** array, int* current_capacity, int min_new_size); // Handles GALAXY specifics if needed
    ```
*   **Integration:** Modify `join_galaxies_of_progenitors` in `src/core/core_build_model.c` to call `galaxy_array_expand`. Identify and update any other dynamic array resizing points.
*   **Configuration:** `growth_factor` (e.g., 1.5 or 2.0) can be a compile-time constant or added to `run_params->runtime` (likely `run_params->tuning`). Default to 1.5.
*   **Notes:** Use `myrealloc` internally. Handle allocation failures. Ensure correct update of the capacity variable (`*maxgals`).

### 3.2 Buffer Manager

*   **New Files:** `src/io/io_buffer_manager.h`, `src/io/io_buffer_manager.c`
*   **Purpose:** Centralize I/O buffering logic, making it configurable and potentially adaptive, improving performance over direct read/write calls for smaller chunks.
*   **Key Functions/Structs:**
    ```c
    struct io_buffer_config {
        size_t initial_size_bytes; size_t min_size_bytes; size_t max_size_bytes;
        bool auto_resize; // Optional: logic to resize based on usage patterns
    };
    // Callback for flushing buffer to actual storage (file/HDF5 dataset)
    typedef int (*io_flush_fn)(int fd, hid_t hdf5_handle, const void* buffer, size_t size, off_t offset, void* context);
    struct io_buffer* buffer_create(struct io_buffer_config* config, int fd, hid_t hdf5_handle, off_t initial_offset, io_flush_fn flush_callback, void* flush_context);
    int buffer_write(struct io_buffer* buffer, const void* data, size_t size); // Copies data, flushes if full
    int buffer_flush(struct io_buffer* buffer); // Writes remaining data
    // Optional: int buffer_read(...) if read buffering is needed
    void buffer_destroy(struct io_buffer* buffer);
    ```
*   **Integration:**
    *   Modify I/O output handlers (`io_binary_output.c`, `io_hdf5_output.c`) to create and use an `io_buffer` instance instead of their current buffering logic (e.g., the HDF5 buffer struct or the `USE_BUFFERED_WRITE` approach).
    *   The `flush_callback` will wrap the actual file writing (`mypwrite` for binary, HDF5 routines for HDF5).
    *   Remove the existing `buffered_io.c/h` and its usage.
*   **Configuration:** Add `OutputBufferSizeMB` (int, default e.g., 8MB) to `run_params->runtime`. The config struct can derive min/max from this.
*   **Notes:** Handles buffer fullness automatically via `buffer_write`. `buffer_flush` must be called during finalization (`finalize_galaxy_files`).

### 3.3 Memory Mapping Service

*   **New Files:** `src/io/io_memory_map.h`, `src/io/io_memory_map.c`
*   **Purpose:** Optionally map large, read-only tree files directly into memory to avoid repeated `read` calls, especially beneficial for binary formats.
*   **Key Functions/Structs:**
    ```c
    enum mmap_access_mode { MMAP_READ_ONLY }; // Only read-only needed for input trees
    struct mmap_options { enum mmap_access_mode mode; size_t mapping_size; off_t offset; };
    struct mmap_region* mmap_file(const char* filename, int fd, struct mmap_options* options);
    void* mmap_get_pointer(struct mmap_region* region);
    int mmap_unmap(struct mmap_region* region);
    ```
*   **Integration:**
    *   Modify binary *input* handlers (e.g., `io_lhalo_binary.c`) to *optionally* use `mmap_file` if enabled by configuration.
    *   The handler's `initialize` function would attempt mapping the relevant file(s).
    *   If successful, `read_forest` would calculate pointers into the mapped region based on offsets instead of using `pread`.
    *   If mapping fails or is disabled, the handler must fall back gracefully to using `pread`.
    *   The `cleanup` function must call `mmap_unmap` for any active mappings.
*   **Configuration:** Add `EnableMemoryMappingInput` (boolean, default false) to `run_params->runtime`.
*   **Notes:** Needs platform-specific implementations (`mmap` for POSIX, `MapViewOfFile` for Windows). Handle mapping failures. May not be beneficial for all formats or access patterns.

### 3.4 Memory Pooling System

*   **New Files:** `src/core/core_memory_pool.h`, `src/core/core_memory_pool.c`
*   **Purpose:** Manage allocation/deallocation of `GALAXY` structs using pre-allocated blocks to reduce `malloc`/`free` overhead and memory fragmentation.
*   **Key Functions/Structs:**
    ```c
    struct memory_pool* galaxy_pool_create(size_t initial_capacity);
    struct GALAXY* galaxy_pool_alloc(struct memory_pool* pool);
    void galaxy_pool_free(struct memory_pool* pool, struct GALAXY* galaxy);
    void galaxy_pool_destroy(struct memory_pool* pool);
    // Internal: functions to manage blocks and free lists
    ```
*   **Integration:**
    *   Modify `src/core/core_mymalloc.c` or create wrappers/macros: Calls to `mymalloc(sizeof(struct GALAXY))` and `myfree` for `GALAXY` pointers should be redirected to `galaxy_pool_alloc`/`galaxy_pool_free`.
    *   A global or per-task memory pool instance needs initialization (e.g., in `sage.c` or `core_init.c`) and destruction (`finalize_sage`).
    *   **Crucially**: When a `GALAXY` struct is allocated from the pool, its `extension_data` needs separate handling (allocation/freeing), as the pool only manages the base struct size. The `galaxy_extension_initialize` and `galaxy_extension_cleanup` functions will likely need integration with the pool's allocation/free cycle or modification to handle this.
*   **Configuration:** Add `EnableGalaxyMemoryPool` (boolean, default true) to `run_params->runtime`. Pool sizing can be adaptive internally.
*   **Notes:** Pool needs to handle exhaustion (allocate new larger blocks). Consider thread-safety if OpenMP planned later.

**4. Implementation Strategy and Priorities**

Implement components in this order, testing incrementally:

1.  **Array Growth Utilities:** Foundational, immediate benefit.
2.  **Memory Pooling:** Impacts core allocation, needs careful integration with `mymalloc` wrappers and galaxy extension lifecycle.
3.  **Buffer Manager:** Replaces existing buffering, integrates with I/O handlers.
4.  **Memory Mapping:** Optional performance enhancement for specific formats.

**5. Critical Implementation Considerations**

*   **Configuration:** Define new runtime parameters in `struct params` (likely under `runtime` or a new `tuning` substruct). Ensure `read_parameter_file` handles them with defaults.
*   **Error Handling:** Use existing logging and error reporting (`io_set_error`, `LOG_ERROR`, etc.). Ensure resource cleanup on failure.
*   **Galaxy Extensions & Pooling:** The interaction is critical. Allocating from the pool only gives the base `GALAXY` struct memory. `extension_data` pointers and their associated memory blocks must still be managed separately, likely during the `galaxy_extension_initialize` and `galaxy_extension_cleanup` calls, which might need adjustment to work correctly with pooled `GALAXY` structs. Ensure `galaxy_extension_copy` works correctly with potentially pooled source/destination structs.
*   **MPI:** Memory pools and buffers should be task-local. Memory mapping might require MPI-IO for shared files or careful coordination.
*   **Backward Compatibility:** Ensure SAGE runs correctly and produces identical scientific results when optimizations are disabled via configuration.

**6. Testing Strategy**

*   **Unit Tests:** Add new test files for each component.
*   **Integration Tests:** Modify `test_sage.sh` to run with different optimization features enabled/disabled. Use `sagediff.py` to compare outputs against the baseline.
*   **Performance/Memory Tests:** (Qualitative for Phase 3.3, Quantitative for Phase 6) Run on large datasets. Measure peak memory (`/usr/bin/time -v` or `valgrind --tool=massif`) and runtime. Compare configurations. Profile (`gprof`, `perf`) to identify bottlenecks remaining *after* these optimizations.

This revised guide focuses Phase 3.3 on achievable I/O and allocation optimizations, removing the unnecessary halo cache while providing clearer direction for implementing the remaining components.```