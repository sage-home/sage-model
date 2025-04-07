# Array Utilities Guide

## Overview

The array utilities module (`core_array_utils.c/h`) provides functions for more efficient dynamic array resizing, using geometric growth strategies instead of fixed increments. This reduces the number of expensive reallocation operations, particularly for large arrays that grow incrementally.

## Key Features

- **Geometric Growth**: Arrays grow by a factor (e.g., 1.5x) rather than a fixed increment, reducing the number of reallocations as the array size increases.
- **Configurable Growth Factor**: The growth factor can be adjusted as needed.
- **Galaxy-Specific Convenience Function**: A dedicated function for galaxy arrays simplifies integration.
- **Error Handling**: Comprehensive error detection and logging.

## API Reference

### Functions

#### `array_expand`

```c
int array_expand(void **array, size_t element_size, int *current_capacity, 
                int min_new_size, float growth_factor);
```

Resizes an array to accommodate at least `min_new_size` elements, using a geometric growth strategy.

**Parameters**:
- `array`: Pointer to the array pointer (will be updated if reallocated)
- `element_size`: Size of each array element in bytes
- `current_capacity`: Pointer to the current array capacity (will be updated)
- `min_new_size`: Minimum number of elements needed
- `growth_factor`: Factor to grow by (e.g., 1.5 means increase by 50%)

**Returns**: 0 on success, non-zero on failure

#### `array_expand_default`

```c
int array_expand_default(void **array, size_t element_size, int *current_capacity, int min_new_size);
```

Convenience function that calls `array_expand` with the default growth factor (`ARRAY_DEFAULT_GROWTH_FACTOR`).

**Parameters**:
- `array`: Pointer to the array pointer (will be updated if reallocated)
- `element_size`: Size of each array element in bytes
- `current_capacity`: Pointer to the current array capacity (will be updated)
- `min_new_size`: Minimum number of elements needed

**Returns**: 0 on success, non-zero on failure

#### `galaxy_array_expand`

```c
int galaxy_array_expand(struct GALAXY **array, int *current_capacity, int min_new_size);
```

Convenience function specifically for resizing GALAXY arrays with the default growth factor.

**Parameters**:
- `array`: Pointer to the GALAXY array pointer (will be updated if reallocated)
- `current_capacity`: Pointer to the current array capacity (will be updated)
- `min_new_size`: Minimum number of galaxies needed

**Returns**: 0 on success, non-zero on failure

### Constants

- `ARRAY_DEFAULT_GROWTH_FACTOR`: Default growth factor (1.5)
- `ARRAY_MIN_SIZE`: Minimum initial size for dynamically allocated arrays (16)

## Usage Examples

### Basic Array Expansion

```c
int *numbers = NULL;
int capacity = 0;

// Initial allocation for 10 elements
if (array_expand_default((void **)&numbers, sizeof(int), &capacity, 10) != 0) {
    // Handle error
    return -1;
}

// Fill the array
for (int i = 0; i < capacity; i++) {
    numbers[i] = i;
}

// Expand to accommodate 20 elements
if (array_expand_default((void **)&numbers, sizeof(int), &capacity, 20) != 0) {
    // Handle error
    free(numbers);
    return -1;
}
```

### Using Custom Growth Factor

```c
float *values = NULL;
int capacity = 0;

// Initial allocation with custom growth factor (2.0 = double size each time)
if (array_expand((void **)&values, sizeof(float), &capacity, 10, 2.0f) != 0) {
    // Handle error
    return -1;
}
```

### Galaxy Array Expansion

```c
struct GALAXY *galaxies = NULL;
int maxgals = 0;

// Expand galaxy array to accommodate 100 galaxies
if (galaxy_array_expand(&galaxies, &maxgals, 100) != 0) {
    // Handle error
    return -1;
}
```

## Performance Considerations

### Reallocation Efficiency

With a growth factor of 1.5, the number of reallocations required to grow an array to size N is approximately log(N)/log(1.5), which is significantly less than with fixed increments. For example:

- Growing to 1,000,000 elements with fixed increments of 10,000: ~100 reallocations
- Growing to 1,000,000 elements with 1.5x growth factor: ~35 reallocations

### Memory Usage

Geometric growth may lead to more memory being allocated than immediately needed. This trade-off between memory usage and CPU time is generally worthwhile, as:

1. Memory fragmentation is reduced due to fewer reallocations
2. Total runtime is improved due to fewer expensive copy operations
3. The extra memory is typically a small percentage of the total program memory footprint

## Integration in SAGE

The array utilities have been integrated in the following locations:

- `join_galaxies_of_progenitors`: Replaced fixed increment of 10,000 with geometric growth
- `evolve_galaxies`: Replaced fixed increment with geometric growth for final galaxy arrays

These changes improve performance particularly for large merger trees with many galaxies.

## Testing

Comprehensive tests for the array utilities can be found in `tests/test_array_utils.c`, which verifies:

- Basic array expansion functionality
- Preservation of array contents during resize operations
- Performance benefits for multiple expansions
- Galaxy-specific array expansion

To run the tests:

```bash
cd tests
make -f Makefile.array_utils
./test_array_utils
```