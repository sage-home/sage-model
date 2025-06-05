# HDF5 Galaxy Output System

## Overview
The HDF5 Galaxy Output System is responsible for writing SAGE galaxies to HDF5 files, providing a standardized, high-performance, and metadata-rich storage format. It uses a property-based approach to dynamically determine which galaxy properties to output, supporting both core and physics properties through the unified property system.

**Note**: This document covers the internal implementation of the HDF5 output engine. For information on how to *use* the HDF5 output functionality through the I/O interface, see [HDF5 Output Handler Guide](io_hdf5_output_guide.md).

## Purpose
The HDF5 output system serves as the primary interface for saving galaxy data in SAGE, addressing several key requirements:

1. **Scientific Accessibility**: Producing files that are easily readable by scientific analysis tools (Python, MATLAB, IDL)
2. **Performance**: Efficiently writing large galaxy catalogs with optimized buffering and chunking
3. **Metadata Preservation**: The system writes comprehensive metadata including:
   - **File-level metadata**: SAGE version, compilation flags, runtime parameters
   - **Dataset attributes**: Property units, descriptions, and data types
   - **Cosmological parameters**: Stored in Header/Simulation group
   - **Galaxy counts**: Total number of galaxies per snapshot and per tree
   - **Snapshot information**: Redshifts and snapshot numbers
4. **Cross-Platform Compatibility**: Ensuring files are readable across different operating systems and architectures
5. **Modularity Support**: Adapting dynamically to the available properties based on loaded physics modules

This system is a critical component in the modular architecture, as it must work with any combination of physics modules without hardcoded assumptions about available properties.

## Key Concepts
- **Dynamic Property Discovery**: Output fields are determined at runtime based on property definitions
- **Property-Based Serialization**: Galaxy properties are accessed through the unified property system
- **Output Transformers**: Custom functions can transform properties before output (unit conversion, etc.)
- **Buffered Writing**: Galaxies are collected in buffers before writing to improve performance
- **Hierarchical Organization**: Data is organized in HDF5 groups by snapshot with comprehensive metadata

## HDF5 File Structure

The output files use a hierarchical structure with groups for different snapshots and comprehensive header information:

```
/
├── Header/
│   ├── Simulation/          # Simulation parameters
│   │   ├── BoxSize
│   │   ├── PartMass
│   │   └── ...
│   ├── Runtime/             # Runtime parameters
│   │   ├── TreeType
│   │   ├── OutputDir
│   │   └── ...
│   └── Misc/               # Miscellaneous information
│       ├── SAGEVersion
│       ├── CompilerFlags
│       └── ...
├── Snap_063/               # Snapshot group (e.g., snapshot 63)
│   ├── SnapNum             # Dataset: Snapshot numbers
│   ├── Type                # Dataset: Galaxy types
│   ├── StellarMass         # Dataset: Stellar masses
│   ├── BlackHoleMass       # Dataset: Black hole masses
│   └── ...                # Other galaxy properties
├── Snap_042/               # Another snapshot group
│   ├── SnapNum
│   ├── Type
│   └── ...
└── TreeInfo/               # Forest/tree metadata
    ├── Snap_063/
    │   └── NumGalsPerTreePerSnap
    └── Snap_042/
        └── NumGalsPerTreePerSnap
```

**Key Attributes**: Each dataset includes metadata attributes such as:
- Units (e.g., "10^10 Msun/h", "Mpc/h", "km/s")
- Description (human-readable property description)
- SAGE version information
- Cosmological parameters (stored in Header groups)
## Data Structures

### Property Buffer Info
```c
struct property_buffer_info {
    char *name;             // Property name
    char *description;      // Property description
    char *units;            // Property units
    void *data;             // Buffer for the property data
    hid_t h5_dtype;         // HDF5 datatype
    property_id_t prop_id;  // Property ID for lookup
    bool is_core_prop;      // Flag indicating if this is a core property
    int index;              // Index in the original list of fields
};
```

### HDF5 Save Info
```c
struct hdf5_save_info {
    hid_t file_id;                  // HDF5 file ID
    hid_t *group_ids;               // HDF5 group IDs for each snapshot
    int32_t num_output_fields;      // Number of fields to output
    
    /* Buffer management */
    int32_t buffer_size;            // Number of galaxies per buffer
    int32_t *num_gals_in_buffer;    // Current number of galaxies in buffer
    int64_t *tot_ngals;             // Total galaxies written per snapshot
    
    /* Dynamic property information */
    struct property_buffer_info **property_buffers; // [snap_idx][prop_idx]
    int num_properties;            // Total properties to output
    
    /* Property system information */
    property_id_t *prop_ids;       // Array of property IDs
    char **prop_names;             // Array of property names
    char **prop_units;             // Array of property units
    char **prop_descriptions;      // Array of property descriptions
    hid_t *prop_h5types;           // Array of HDF5 datatypes
    bool *is_core_prop;            // Array of flags indicating if properties are core
};
```
## API Reference

### Initialization

#### `initialize_hdf5_galaxy_files()`
```c
int32_t initialize_hdf5_galaxy_files(
    const int filenr,
    struct save_info *save_info,
    const struct params *run_params
);
```
**Purpose**: Initializes HDF5 file(s) for writing galaxy data.  
**Parameters**:
- `filenr`: File number (for multiple-file output)
- `save_info`: Save information structure (must be cast to hdf5_save_info internally)
- `run_params`: Global parameters structure

**Returns**: 0 on success, non-zero on failure.  
**Implementation Details**:
- Creates HDF5 file and initializes its structure
- Creates snapshot groups based on specified redshifts
- Discovers available properties for output
- Allocates buffers for each property
- Writes header information including cosmology, units, and version

**Example**:
```c
struct save_info *save_info = calloc(1, sizeof(struct save_info));
if (save_info == NULL) {
    LOG_ERROR("Failed to allocate save_info");
    return -1;
}

// Initialize specific to HDF5 format
int status = initialize_hdf5_galaxy_files(0, save_info, run_params);
if (status != 0) {
    LOG_ERROR("Failed to initialize HDF5 output files");
    free(save_info);
    return -1;
}
```
### Galaxy Saving

#### `save_hdf5_galaxies()`
```c
int32_t save_hdf5_galaxies(
    const int64_t task_forestnr,
    const int32_t num_gals,
    struct forest_info *forest_info,
    struct halo_data *halos,
    struct halo_aux_data *haloaux,
    struct GALAXY *halogal,
    struct save_info *save_info,
    const struct params *run_params
);
```
**Purpose**: Saves galaxies to HDF5 file(s).  
**Parameters**:
- `task_forestnr`: Forest number for the current task
- `num_gals`: Number of galaxies to save
- `forest_info`: Forest information structure
- `halos`: Array of halo data
- `haloaux`: Array of auxiliary halo data
- `halogal`: Array of galaxies to save
- `save_info`: Save information structure (must be cast to hdf5_save_info internally)
- `run_params`: Global parameters structure

**Returns**: 0 on success, non-zero on failure.  
**Implementation Details**:
- For each galaxy, determines which snapshot it belongs to
- Processes properties in two stages:
  - **Core properties**: Handled directly using `GALAXY_PROP_*` macros or specific logic
  - **Physics properties**: Processed via `dispatch_property_transformer` for custom transformations
- Buffers galaxies to improve I/O performance
- Writes buffers to HDF5 file when full or when all galaxies processed
- Updates galaxy counts and metadata
#### `finalize_hdf5_galaxy_files()`
```c
int32_t finalize_hdf5_galaxy_files(
    const struct forest_info *forest_info,
    struct save_info *save_info,
    const struct params *run_params
);
```
**Purpose**: Finalizes HDF5 files after all galaxies have been written.  
**Parameters**:
- `forest_info`: Forest information structure
- `save_info`: Save information structure (must be cast to hdf5_save_info internally)
- `run_params`: Global parameters structure

**Returns**: 0 on success, non-zero on failure.  
**Implementation Details**:
- Flushes any remaining galaxies in buffers
- Updates metadata with final galaxy counts
- Closes all HDF5 handles (files, groups, datasets)
- Frees allocated memory for buffers and property information
- Writes total number of galaxies for each snapshot

#### `create_hdf5_master_file()`
```c
int32_t create_hdf5_master_file(const struct params *run_params);
```
**Purpose**: Creates a master file for multi-file output.  
**Parameters**:
- `run_params`: Global parameters structure

**Returns**: 0 on success, non-zero on failure.  
**Notes**: Only used in MPI mode with multiple files.
### Helper Functions

#### HDF5 Galaxy Processing

Galaxy data is now processed directly by the HDF5 output functions, eliminating the need for a separate preparation step. The property transformation system handles any necessary conversions automatically through the `dispatch_property_transformer()` function.

**Implementation Details**:
- Properties are processed in the main HDF5 output loop
- **Core properties**: Accessed directly using `GALAXY_PROP_*` macros 
- **Physics properties**: Processed via `dispatch_property_transformer()` 
- Transformations include unit conversions, logarithmic scaling, and derived calculations
- All processing respects core-physics separation principles

**Example**:
```c
// In save_hdf5_galaxies() - simplified processing
for (int i = 0; i < num_gals; i++) {
    struct GALAXY *galaxy = &halogal[i];
    int snap_idx = get_output_snap_idx(galaxy->SnapNum, forest_info);
    
    // Skip galaxies that don't belong to any output snapshot
    if (snap_idx < 0) continue;
    
    // Galaxy data processing is handled directly by HDF5 output functions
    // Property transformations applied automatically during buffer writes
    
    // Write buffer if full
    if (buffer_idx >= hdf5_info->buffer_size - 1) {
        write_galaxy_buffer(hdf5_info, snap_idx);
    }
}
```
#### `generate_field_metadata()`
```c
int generate_field_metadata(
    struct hdf5_save_info *hdf5_info,
    const struct params *run_params
);
```
**Purpose**: Generates metadata for all fields to be output.  
**Parameters**:
- `hdf5_info`: HDF5 save information structure
- `run_params`: Global parameters structure

**Returns**: 0 on success, non-zero on failure.  
**Implementation Details**:
- Scans all properties in the property system
- Identifies properties marked for output (`output: true` in `properties.yaml`)
- Collects property names, descriptions, units, and data types
- Determines HDF5 data types based on property types
- Stores information in the hdf5_info structure for later use
- Allocates buffers for each property based on the buffer size

#### `initialize_hdf5_galaxy_files()`
```c
int32_t initialize_hdf5_galaxy_files(
    const int filenr,
    struct save_info *save_info,
    const struct params *run_params
);
```
**Purpose**: Initializes HDF5 file structure and prepares for galaxy output.  
**Implementation Details**:
- Creates HDF5 file using H5Fcreate
- Creates snapshot groups for each redshift in the simulation
- Initializes HDF5 save info structure
- Allocates buffers for each property and snapshot
- Configures chunking and compression settings
- Writes header information and metadata

## Implementation Details

### Property-Based Dynamic Output

The HDF5 output system dynamically determines which properties to output through these steps:

1. **Property Discovery**: During initialization, the system scans the property registry for properties with the `output: true` flag set in `properties.yaml`:
   ```c
   // Pseudo-code for discovery process
   for (int i = 0; i < property_count; i++) {
       struct property_info *prop = get_property_info(i);
       if (prop->output) {
           // Add to output properties list
           add_output_property(hdf5_info, prop);
       }
   }
   ```

**Note**: All output fields are defined in `properties.yaml` and controlled by the `output: true` flag. Only properties with this flag set will be included in the HDF5 output files.
2. **Buffer Allocation**: For each property to output, a buffer is allocated:
   ```c
   // Allocate buffer for each property
   for (int i = 0; i < hdf5_info->num_properties; i++) {
       size_t type_size = get_property_type_size(hdf5_info->prop_types[i]);
       hdf5_info->property_buffers[snap_idx][i].data = 
           calloc(buffer_size, type_size);
   }
   ```

3. **Output Transformation**: When preparing galaxies for output, each property may have a transformer function:
   ```c
   // For each property to output
   for (int i = 0; i < hdf5_info->num_properties; i++) {
       struct property_buffer_info *buffer = &property_buffer[i];
       
       // Get pointer to the buffer element for this galaxy
       void *buffer_element = (char*)buffer->data + galaxy_idx * get_property_type_size(buffer->h5_dtype);
       
       // Check if property has a transformer function
       if (has_transformer_function(buffer->prop_id)) {
           // Use transformer
           apply_output_transformer(galaxy, buffer->prop_id, buffer_element, run_params);
       } else {
           // Use standard property access
           get_property_value(galaxy, buffer->prop_id, buffer_element);
       }
   }
   ```
### Output Transformer System

The Output Transformer system allows for custom property transformations before output:

1. **Transformer Registration**: Properties can specify a transformer function in their definition:
   ```yaml
   # In properties.yaml
   - name: BlackHoleMass
     type: double
     output: true
     output_transformer_function: "transform_output_BlackHoleMass"
   ```

2. **Transformer Implementation**: The transformer function is implemented in the physics module:
   ```c
   int transform_output_BlackHoleMass(
       const struct GALAXY *galaxy, 
       property_id_t prop_id,
       void *output_buffer_element_ptr, 
       const struct params *run_params
   ) {
       // Get the raw property value
       double raw_value = get_double_property(galaxy, prop_id, 0.0);
       
       // Apply transformation (e.g., unit conversion)
       double transformed = raw_value * run_params->units.UnitMass_in_g / SOLAR_MASS;
       
       // Write to output buffer
       *(double*)output_buffer_element_ptr = transformed;
       
       return 0;
   }
   ```
3. **Transformer Dispatcher**: The system uses a dispatcher function to route properties to their transformers:
   ```c
   int dispatch_property_transformer(
       const struct GALAXY *galaxy,
       property_id_t prop_id,
       void *output_buffer_element_ptr,
       const struct params *run_params
   ) {
       // Get the property name
       const char *prop_name = get_property_name(prop_id);
       
       // Dispatch to the appropriate transformer
       if (strcmp(prop_name, "BlackHoleMass") == 0) {
           return transform_output_BlackHoleMass(galaxy, prop_id, output_buffer_element_ptr, run_params);
       } else if (strcmp(prop_name, "StellarMass") == 0) {
           return transform_output_StellarMass(galaxy, prop_id, output_buffer_element_ptr, run_params);
       }
       // ... other properties
       
       // Default: no transformation
       return 0;
   }
   ```

### Memory Management and Performance

The HDF5 output system uses several strategies for efficient memory usage and performance:

1. **Buffered Writing**: Galaxies are collected in buffers before writing to disk:
   ```c
   // Add galaxy to buffer
   hdf5_info->num_gals_in_buffer[snap_idx]++;
   
   // Write buffer if full
   if (hdf5_info->num_gals_in_buffer[snap_idx] >= hdf5_info->buffer_size) {
       write_galaxy_buffer(hdf5_info, snap_idx);
   }
   ```
2. **HDF5 Chunking**: Data is written in chunks for better compression and access:
   ```c
   // Set chunking properties
   hsize_t chunk_dims[1] = {(hsize_t)CHUNK_SIZE};
   hid_t chunk_plist = H5Pcreate(H5P_DATASET_CREATE);
   H5Pset_chunk(chunk_plist, 1, chunk_dims);
   
   // Set compression if enabled
   if (run_params->output.compression_level > 0) {
       H5Pset_deflate(chunk_plist, run_params->output.compression_level);
   }
   ```

3. **Resource Tracking**: HDF5 handles are tracked to prevent resource leaks:
   ```c
   // Track HDF5 handle
   hdf5_track_handle(dataset_id, HDF5_HANDLE_DATASET);
   
   // Later, during cleanup
   hdf5_close_all_handles();
   ```

4. **Buffer Reuse**: Buffers are reused across multiple buffer writes:
   ```c
   // Reset buffer counter after writing
   hdf5_info->num_gals_in_buffer[snap_idx] = 0;
   
   // No need to reallocate buffers
   ```

### Core-Physics Property Separation

The HDF5 output system respects core-physics separation through these mechanisms:

1. **Core Property Access**: Core properties are accessed directly with `GALAXY_PROP_*` macros
2. **Physics Property Access**: Physics properties use generic `get_property_*` functions
3. **Dynamic Property Discovery**: Output fields are determined at runtime based on available properties
4. **Output Transformers**: Each module can register transformers for its own properties
## Usage Patterns

### Basic HDF5 Output Setup
```c
// In io_hdf5_output.c initialization
struct io_interface hdf5_output_interface = {
    .name = "HDF5",
    .version = "1.0",
    .format_id = 1,
    .capabilities = IO_CAP_WRITE_GALAXIES,
    
    .initialize = io_hdf5_output_init,
    .read_forest = NULL,  // Not implemented for output
    .write_galaxies = io_hdf5_write_galaxies,
    .cleanup = io_hdf5_output_cleanup,
    
    .close_open_handles = io_hdf5_close_handles,
    .get_open_handle_count = io_hdf5_get_handle_count
};

// Register the interface
io_register_interface(&hdf5_output_interface);
```

### Implementing an Output Transformer
```c
// In physics_output_transformers.c
int transform_output_BlackHoleMass(
    const struct GALAXY *galaxy,
    property_id_t prop_id,
    void *output_buffer_element_ptr,
    const struct params *run_params
) {
    // Get the raw property value using core-physics separation principles
    // For physics properties, use generic accessors
    double raw_value = get_double_property(galaxy, prop_id, 0.0);
    
    // Apply logarithmic transformation if value is positive
    if (raw_value > 0.0) {
        *(double*)output_buffer_element_ptr = log10(raw_value);
    } else {
        // Handle zero or negative values
        *(double*)output_buffer_element_ptr = -999.0;  // Common astronomy convention
    }
    
    return 0;
}
```
### Adding a New Output Property
```c
// 1. Add to properties.yaml
/*
- name: SpecificStarFormationRate
  type: float
  initial_value: 0.0
  units: "yr^-1"
  description: "Specific star formation rate (SFR/StellarMass)"
  output: true
  output_transformer_function: "transform_output_SpecificSFR"
*/

// 2. Implement the transformer function
int transform_output_SpecificSFR(
    const struct GALAXY *galaxy,
    property_id_t prop_id,
    void *output_buffer_element_ptr,
    const struct params *run_params
) {
    // Get SFR and stellar mass using property IDs
    property_id_t sfr_id = get_property_id_by_name("StarFormationRate");
    property_id_t sm_id = get_property_id_by_name("StellarMass");
    
    double sfr = get_double_property(galaxy, sfr_id, 0.0);
    double sm = get_double_property(galaxy, sm_id, 0.0);
    
    // Calculate specific SFR with appropriate units
    if (sm > 0.0 && sfr > 0.0) {
        double specific_sfr = sfr / sm;
        *(float*)output_buffer_element_ptr = (float)specific_sfr;
    } else {
        *(float*)output_buffer_element_ptr = 0.0f;
    }
    
    return 0;
}

// 3. Register the transformer in the dispatcher
// (this is done automatically by the property system)
```
## Error Handling

The HDF5 output system uses a layered approach to error handling:

1. **HDF5 Error Stack**: HDF5 errors are captured and logged:
   ```c
   // Check HDF5 function result
   if (dataset_id < 0) {
       LOG_ERROR("Failed to create dataset for %s", property_name);
       H5Eprint(H5E_DEFAULT, stderr);
       return -1;
   }
   ```

2. **Resource Tracking**: HDF5 handles are tracked to prevent resource leaks:
   ```c
   // Register handle for tracking
   hdf5_track_handle(dataset_id, HDF5_HANDLE_DATASET);
   
   // During cleanup or error handling
   hdf5_close_all_handles();
   ```

3. **Buffer Validation**: Buffer states are validated before writing:
   ```c
   // Check if buffer is empty
   if (hdf5_info->num_gals_in_buffer[snap_idx] == 0) {
       LOG_DEBUG("No galaxies in buffer for snapshot %d", snap_idx);
       return 0;
   }
   
   // Check if buffer data is valid
   for (int i = 0; i < hdf5_info->num_properties; i++) {
       if (hdf5_info->property_buffers[snap_idx][i].data == NULL) {
           LOG_ERROR("NULL buffer for property %s", 
                    hdf5_info->property_buffers[snap_idx][i].name);
           return -1;
       }
   }
   ```

## Integration Points
- **Dependencies**: Relies on the property system for property definitions and access
- **Used By**: The I/O interface system for galaxy output
- **Events**: Does not emit or handle events directly
- **Properties**: Reads all properties marked with `output: true`

## See Also
- [HDF5 Output Handler Guide](io_hdf5_output_guide.md) - User guide for the I/O interface
- [IO Interface System](io_interface.md) - Overview of the I/O interface architecture
- [Physics Output Transformers Guide](physics_output_transformers_guide.md) - Comprehensive guide for physics module developers on customizing output transformations
- [Property System](property_system.md) - Unified property system documentation

---

*Last updated: May 26, 2025*  
*Component version: 1.0*