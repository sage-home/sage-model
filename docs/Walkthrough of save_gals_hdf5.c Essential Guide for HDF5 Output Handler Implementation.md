# Walkthrough of `save_gals_hdf5.c`: Essential Guide for HDF5 Output Handler Implementation

## 1. Overall Architecture and Key Concepts

Looking at `save_gals_hdf5.c`, I can see that implementing the HDF5 Output Handler requires understanding several key architectural decisions:

### File Structure
```
[HDF5 File]
├── Header/                  (Group with metadata)
│   ├── Simulation/          (Simulation parameters)
│   ├── Runtime/             (Runtime parameters)
│   └── Misc/                (Version info, etc.)
├── Snap_X/                  (Group for each output snapshot)
│   ├── Property1            (Dataset for galaxy property)
│   ├── Property2            (Each dataset has attributes for units/description)
│   └── ...
└── TreeInfo/                (Tree structure metadata)
    └── Snap_X/              (Per-snapshot tree data)
        └── NumGalsPerTreePerSnap (Dataset)
```

### Memory Model
The HDF5 implementation uses a **structure of arrays** approach rather than an array of structures:
- Each galaxy property is stored in its own array
- This matches how HDF5 datasets are structured
- Contrast with binary output which uses array of structs

### Buffering Strategy
```
                 ┌───────────┐
                 │ Galaxy 1  │
                 │ Galaxy 2  │
                 │ Galaxy 3  │    When buffer is full:
Galaxies ───────►│    ...    │────────────────────┐
                 │ Galaxy N  │                    │
                 └───────────┘                    ▼
                     Buffer                 ┌──────────┐
                                            │   HDF5   │
                                            │   File   │
                                            └──────────┘
```

## 2. Critical Function Flow

The HDF5 output handler will need to implement three main functions that map to the existing code:

1. **Initialize** (maps to `initialize_hdf5_galaxy_files()`)
   - Creates HDF5 file structure
   - Sets up groups and datasets with metadata
   - Allocates memory for buffers

2. **Write Galaxies** (maps to `save_hdf5_galaxies()`)
   - Adds galaxies to memory buffer
   - Triggers writing when buffer is full
   - Manages galaxy count tracking

3. **Finalize** (maps to `finalize_hdf5_galaxy_files()`)
   - Writes remaining galaxies in buffer
   - Adds final metadata and attributes
   - Closes file handles and frees resources

## 3. Resource Management - The Most Critical Aspect

This is where many implementations fail! HDF5 handle tracking is complex:

```c
// Example of proper resource tracking
hid_t dataset_id = H5Dopen2(file_id, field_name, H5P_DEFAULT);
// Always check status
if (dataset_id < 0) { return error; }

// Use the resource...

// Always close in reverse order of creation
status = H5Dclose(dataset_id);
if (status < 0) { return error; }
```

Notice how every operation in `save_gals_hdf5.c` follows this pattern with comprehensive error checking.

## 4. Critical Macros to Understand

The HDF5 implementation uses several macros that abstract complex operations:

### Attribute Creation
```c
CREATE_SINGLE_ATTRIBUTE(group_id, "name", value, type)
CREATE_STRING_ATTRIBUTE(group_id, "name", value, length)
```

### Memory Management
```c
MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, field_name)
FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, field_name)
```

### Dataset Writing
```c
EXTEND_AND_WRITE_GALAXY_DATASET(field_name)
```
This is the most complex macro - it handles dataset extension, hyperslab selection, and writing.

## 5. Integration with Extension Properties

For the refactoring, you'll need to add support for extension properties. This will require:

1. Creating additional datasets for extension properties
2. Adding metadata attributes for extensions
3. Extending the buffer structure to include extensions
4. Modifying the prepare_galaxy_for_output function to handle extensions

## 6. Implementation Strategy

To successfully implement the HDF5 output handler:

1. **Start with Interface Definition**
   - Define the handler struct with function pointers
   - Include state variables for buffers and file handles

2. **Implement Core Functions in Stages**
   - First implement initialization with minimum fields
   - Next add galaxy writing with basic properties
   - Then add finalization with basic cleanup
   - Only then add extension property support

3. **Focus on Resource Management**
   - Track all HDF5 handles explicitly
   - Implement proper cleanup routines
   - Add error handling with descriptive messages

4. **Test Incrementally**
   - Verify file creation and structure
   - Test with a single galaxy first
   - Validate extension property handling separately

## 7. Common Pitfalls to Avoid

Based on the code and likely previous issues:

1. **Resource Leaks**
   - Failing to close HDF5 handles
   - Not freeing memory allocations

2. **Hyperslab Errors**
   - Incorrect dimensions or offsets
   - Missing validation checks

3. **Type Mismatches**
   - HDF5 type not matching C type
   - Endianness issues with binary data

4. **Extension Integration**
   - Forgetting to register extension properties
   - Not handling optional extensions correctly

5. **Error Propagation**
   - Ignoring return values
   - Not providing descriptive error messages

By carefully studying this walkthrough and the existing implementation, you should be able to create a robust HDF5 output handler that integrates with the new IO interface system and supports extension properties.​​​​​​​​​​​​​​​​