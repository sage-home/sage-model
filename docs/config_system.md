# Configuration System

## Overview
The SAGE Configuration System provides a flexible, JSON-based approach to configuring all aspects of the model. It enables runtime configuration of parameters, modules, and pipeline components without requiring recompilation, supporting both file-based configuration and command-line overrides.

## Purpose
The configuration system serves as the central coordination point for SAGE's runtime behavior, allowing users to:
- Define which physics modules are active in a simulation
- Set simulation parameters (cosmology, resolution, etc.)
- Configure individual module behaviors
- Override specific settings via command line without modifying config files
- Support hierarchical configuration with inheritance

This system is particularly critical for the modular architecture, as it enables runtime determination of which modules to load and how they interact without hardcoding these relationships.

## Key Concepts
- **Configuration Object**: A hierarchical tree of typed values (boolean, integer, double, string, object, array)
- **Path-Based Access**: Values are accessed via dot-notation paths (e.g., "modules.cooling.enabled")
- **Type Safety**: Values are stored with type information and converted appropriately
- **Configuration Overrides**: Command-line options can override file-based configuration
- **Default Values**: Functions provide default values when configuration entries are missing
- **Module Configuration**: Specialized handling for module-specific settings

## Data Structures

### Config Value
```c
struct config_value {
    enum config_value_type type;  /* Type of value (boolean, integer, etc.) */
    union {
        bool boolean;             /* Boolean value */
        int64_t integer;          /* Integer value */
        double floating;          /* Floating-point value */
        char *string;             /* String value */
        struct config_object *object; /* Object value (key-value pairs) */
        struct {
            struct config_value **items; /* Array items */
            int count;            /* Number of items */
            int capacity;         /* Allocated capacity */
        } array;
    } u;
};
```

### Config Object
```c
struct config_object {
    struct {
        char *key;                /* Key string */
        struct config_value value; /* Value for this key */
    } *entries;
    int count;                    /* Number of entries */
    int capacity;                 /* Allocated capacity */
};
```

### Config System
```c
struct config_system {
    struct config_object *root;   /* Root configuration object */
    char *filename;               /* Path to the loaded config file */
    bool initialized;             /* Whether the system is initialized */
    
    /* Override arguments */
    struct {
        char path[MAX_CONFIG_PATH]; /* Path to override */
        char value[MAX_CONFIG_VALUE]; /* Override value */
    } overrides[MAX_CONFIG_OVERRIDE_ARGS]; /* Command-line overrides */
    int num_overrides;            /* Number of overrides */
};
```

## API Reference

### Initialization Functions

#### `config_system_initialize()`
```c
int config_system_initialize(void);
```
**Purpose**: Initializes the configuration system, creating an empty root configuration object.  
**Returns**: 0 on success, non-zero on failure.  
**Notes**: Must be called before any other configuration functions.

#### `config_system_cleanup()`
```c
int config_system_cleanup(void);
```
**Purpose**: Releases all resources used by the configuration system.  
**Returns**: 0 on success, non-zero on failure.  
**Notes**: Should be called at program termination to avoid memory leaks.

### File Operations

#### `config_load_file()`
```c
int config_load_file(const char *filename);
```
**Purpose**: Loads and parses a JSON configuration file.  
**Parameters**:
- `filename`: Path to the configuration file

**Returns**: 0 on success, non-zero on failure.  
**Notes**: 
- File size is limited to MAX_CONFIG_FILE_SIZE (1 MB)
- JSON parsing errors are reported with line numbers for easier debugging
- Previous configuration is replaced entirely

#### `config_save_file()`
```c
int config_save_file(const char *filename, bool pretty);
```
**Purpose**: Saves current configuration to a JSON file.  
**Parameters**:
- `filename`: Path where the file should be saved
- `pretty`: Whether to format with indentation (true) or compact (false)

**Returns**: 0 on success, non-zero on failure.  

### Value Access Functions

#### `config_get_boolean()`
```c
bool config_get_boolean(const char *path, bool default_value);
```
**Purpose**: Retrieves a boolean value from the configuration.  
**Parameters**:
- `path`: Dot-notation path to the value (e.g., "modules.cooling.enabled")
- `default_value`: Value to return if path doesn't exist or isn't a boolean

**Returns**: The boolean value at the specified path, or the default value.  
**Example**:
```c
bool cooling_enabled = config_get_boolean("modules.cooling.enabled", false);
if (cooling_enabled) {
    // Initialize cooling module
}
```

#### `config_get_integer()`
```c
int config_get_integer(const char *path, int default_value);
```
**Purpose**: Retrieves an integer value from the configuration.  
**Parameters**:
- `path`: Dot-notation path to the value
- `default_value`: Value to return if path doesn't exist or isn't an integer

**Returns**: The integer value at the specified path, or the default value.  

#### `config_get_double()`
```c
double config_get_double(const char *path, double default_value);
```
**Purpose**: Retrieves a floating-point value from the configuration.  
**Parameters**:
- `path`: Dot-notation path to the value
- `default_value`: Value to return if path doesn't exist or isn't a double

**Returns**: The double value at the specified path, or the default value.  

#### `config_get_string()`
```c
const char *config_get_string(const char *path, const char *default_value);
```
**Purpose**: Retrieves a string value from the configuration.  
**Parameters**:
- `path`: Dot-notation path to the value
- `default_value`: Value to return if path doesn't exist or isn't a string

**Returns**: The string value at the specified path, or the default value.  
**Notes**: The returned string is owned by the configuration system and should not be freed.

#### `config_get_value()`
```c
const struct config_value *config_get_value(const char *path);
```
**Purpose**: Retrieves a typed value structure from the configuration.  
**Parameters**:
- `path`: Dot-notation path to the value

**Returns**: Pointer to the value structure, or NULL if not found.  
**Notes**: This function is useful when you need to determine the type of a value or access complex structures like objects and arrays.

### Array Access Functions

#### `config_get_array_size()`
```c
int config_get_array_size(const char *path);
```
**Purpose**: Gets the number of elements in an array.  
**Parameters**:
- `path`: Dot-notation path to the array

**Returns**: Number of elements in the array, or -1 if not found or not an array.  

#### `config_get_array_element()`
```c
const struct config_value *config_get_array_element(const char *path, int index);
```
**Purpose**: Retrieves a specific element from an array.  
**Parameters**:
- `path`: Dot-notation path to the array
- `index`: Zero-based index of the element to retrieve

**Returns**: Pointer to the value, or NULL if not found.  
**Example**:
```c
// Get the number of physics modules
int num_modules = config_get_array_size("physics.modules");
for (int i = 0; i < num_modules; i++) {
    // Get each module configuration
    const struct config_value *module = config_get_array_element("physics.modules", i);
    // Process module configuration
}
```

### Object Access Functions

#### `config_get_boolean_from_object()`
```c
bool config_get_boolean_from_object(const struct config_object *obj, const char *key, bool default_value);
```
**Purpose**: Retrieves a boolean value from a specific key in a config object.  
**Parameters**:
- `obj`: The configuration object
- `key`: Key to retrieve
- `default_value`: Value to return if key not found or invalid type

**Returns**: The boolean value or default_value if not found.  

#### `config_get_string_from_object()`
```c
const char *config_get_string_from_object(const struct config_object *obj, const char *key, const char *default_value);
```
**Purpose**: Retrieves a string value from a specific key in a config object.  
**Parameters**:
- `obj`: The configuration object
- `key`: Key to retrieve
- `default_value`: Value to return if key not found or invalid type

**Returns**: The string value or default_value if not found.  

### Value Modification Functions

#### `config_set_boolean()`
```c
int config_set_boolean(const char *path, bool value);
```
**Purpose**: Sets a boolean value in the configuration.  
**Parameters**:
- `path`: Path where to set the value
- `value`: Boolean value to set

**Returns**: 0 on success, non-zero on failure.  
**Notes**: Creates intermediate objects if they don't exist.

#### `config_set_integer()`
```c
int config_set_integer(const char *path, int value);
```
**Purpose**: Sets an integer value in the configuration.  
**Parameters**:
- `path`: Path where to set the value
- `value`: Integer value to set

**Returns**: 0 on success, non-zero on failure.  

#### `config_set_double()`
```c
int config_set_double(const char *path, double value);
```
**Purpose**: Sets a floating-point value in the configuration.  
**Parameters**:
- `path`: Path where to set the value
- `value`: Double value to set

**Returns**: 0 on success, non-zero on failure.  

#### `config_set_string()`
```c
int config_set_string(const char *path, const char *value);
```
**Purpose**: Sets a string value in the configuration.  
**Parameters**:
- `path`: Path where to set the value
- `value`: String value to set

**Returns**: 0 on success, non-zero on failure.  
**Notes**: The string is copied, so the original can be freed afterward.

### Override Functions

#### `config_add_override()`
```c
int config_add_override(const char *path, const char *value);
```
**Purpose**: Adds a command-line override for a configuration value.  
**Parameters**:
- `path`: Path to the configuration value
- `value`: String representation of the value

**Returns**: 0 on success, non-zero on failure.  
**Notes**: Overrides are not applied immediately; call `config_apply_overrides()` to apply them.

#### `config_apply_overrides()`
```c
int config_apply_overrides(void);
```
**Purpose**: Applies all registered overrides to the configuration.  
**Returns**: 0 on success, non-zero on failure.  
**Notes**: 
- Converts string values to appropriate types based on automatic type inference
- Boolean detection: "true", "yes" → true; "false", "no" → false
- String detection: Values wrapped in double quotes are treated as strings
- Numeric detection: Other values are parsed as integers or floating-point numbers

### SAGE-Specific Configuration Functions

#### `config_configure_modules()`
```c
int config_configure_modules(struct params *params);
```
**Purpose**: Sets up modules based on the current configuration.  
**Parameters**:
- `params`: Global parameters structure

**Returns**: 0 on success, non-zero on failure.  
**Notes**: Iterates through module configurations and applies settings.

#### `config_configure_pipeline()`
```c
int config_configure_pipeline(void);
```
**Purpose**: Sets up the pipeline based on the current configuration.  
**Returns**: 0 on success, non-zero on failure.  
**Notes**: Determines which modules to include in the pipeline and their order.

#### `config_configure_params()`
```c
int config_configure_params(struct params *params);
```
**Purpose**: Updates the params structure based on the current configuration.  
**Parameters**:
- `params`: Global parameters structure

**Returns**: 0 on success, non-zero on failure.  
**Notes**: Overrides parameter file settings with JSON configuration values.

#### `config_generate_default()`
```c
struct config_object *config_generate_default(void);
```
**Purpose**: Creates a default configuration structure.  
**Returns**: Root configuration object with default settings.  
**Notes**: 
- Creates default sections for modules, pipeline, and debug settings
- Used when no configuration file is provided
- Modules section includes discovery settings and search paths
- Pipeline section provides basic pipeline configuration
- Debug section sets default logging levels

#### `config_create_schema()`
```c
int config_create_schema(void);
```
**Purpose**: Generates a JSON schema for the configuration.  
**Returns**: 0 on success, non-zero on failure.  
**Notes**: **This function is not yet implemented** and currently returns -1. It is planned for future versions to provide schema validation and documentation generation.

## Usage Patterns

### Basic Configuration Loading
```c
// Initialize the configuration system
config_system_initialize();

// Load configuration from a file
if (config_load_file("input/config.json") != 0) {
    LOG_ERROR("Failed to load configuration");
    return -1;
}

// Access configuration values
int steps = config_get_integer("simulation.steps", 100);
double dt = config_get_double("simulation.timestep", 0.01);
bool verbose = config_get_boolean("logging.verbose", false);

// Configure the simulation
config_configure_modules(params);
config_configure_pipeline();
config_configure_params(params);

// Run the simulation
// ...

// Clean up
config_system_cleanup();
```

### Command-Line Overrides
```c
// Parse command-line arguments
for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
        config_load_file(argv[++i]);
    } else if (strncmp(argv[i], "--set:", 6) == 0) {
        // Format: --set:path=value
        char *arg = argv[i] + 6;
        char *equals = strchr(arg, '=');
        if (equals) {
            *equals = '\0';
            config_add_override(arg, equals + 1);
        }
    }
}

// Apply all overrides
config_apply_overrides();
```

### Dynamic Module Configuration
```c
// Check if a module is enabled
bool cooling_enabled = config_get_boolean("modules.cooling.enabled", true);
if (cooling_enabled) {
    // Get module-specific configuration
    double efficiency = config_get_double("modules.cooling.efficiency", 1.0);
    const char *table_file = config_get_string("modules.cooling.table_file", "cooling_table.dat");
    
    // Initialize module with configuration
    cooling_module_init(efficiency, table_file);
}
```

### Working with Array Configurations
```c
// Iterate through module instances
int num_instances = config_get_array_size("modules.instances");
for (int i = 0; i < num_instances; i++) {
    char path[256];
    
    // Build path to this instance
    snprintf(path, sizeof(path), "modules.instances[%d]", i);
    
    // Get the instance value
    const struct config_value *instance = config_get_value(path);
    if (instance && instance->type == CONFIG_VALUE_OBJECT) {
        // Get properties from this instance
        const char *name = config_get_string_from_object(instance->u.object, "name", "");
        bool enabled = config_get_boolean_from_object(instance->u.object, "enabled", false);
        
        if (enabled) {
            LOG_INFO("Module %s is enabled", name);
            // Process module configuration
        }
    }
}
```

## JSON Configuration Schema

The SAGE configuration follows this general structure. **Note**: This is an example structure rather than a formal JSON schema. A formal schema validation system is planned for future implementation via the `config_create_schema()` function.

```json
{
    "modules": {
        "discovery_enabled": true,
        "search_paths": [
            "./src/physics",
            "./modules"
        ],
        "instances": [
            {
                "name": "cooling_module",
                "enabled": true,
                "efficiency": 1.5,
                "parameters": {
                    "cooling_rate": 0.75,
                    "use_tables": true
                }
            },
            {
                "name": "infall_module",
                "enabled": true,
                "parameters": {
                    "infall_rate": 1.2
                }
            }
        ]
    },
    "simulation": {
        "cosmology": {
            "h0": 0.73,
            "omega_m": 0.25,
            "omega_lambda": 0.75
        },
        "snapshots": [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100],
        "output_dir": "./output",
        "debug_level": 1
    },
    "pipeline": {
        "name": "standard_pipeline",
        "steps": [
            {
                "type": "infall",
                "module": "infall_module",
                "enabled": true
            },
            {
                "type": "cooling",
                "module": "cooling_module",
                "enabled": true
            }
        ]
    }
}
```

## Integration Points

### Module System Integration
The configuration system directly feeds the module system by:
1. Providing module discovery settings (`modules.discovery_enabled`, `modules.search_paths`)
2. Determining which modules to activate (`modules.instances[].enabled`)
3. Providing module-specific parameters (`modules.instances[].parameters`)

### Pipeline Integration
The pipeline configuration determines execution order and activated modules:
1. Pipeline name and global settings (`pipeline.name`)
2. Pipeline steps with module references (`pipeline.steps[]`)
3. Step-specific settings (`pipeline.steps[].enabled`)

### Parameter System Integration
The configuration overrides parameters from the parameter file:
1. Runtime parameter updates (`simulation.*`)
2. Cosmology settings (`simulation.cosmology.*`)
3. Output configuration (`simulation.output_*`)

### Command-Line Integration
The override system enables command-line arguments to modify configuration:
1. Register overrides with `config_add_override()`
2. Apply with `config_apply_overrides()`
3. Override syntax: `--set:modules.cooling.enabled=false`

#### Override Type Inference
When applying overrides, the system automatically infers the appropriate data type:

- **Boolean Values**: 
  - `"true"`, `"yes"` → `true`
  - `"false"`, `"no"` → `false`
- **String Values**: Values wrapped in double quotes (`"string value"`) are stored as strings
- **Numeric Values**: 
  - Values containing only digits are parsed as integers
  - Values containing decimal points are parsed as floating-point numbers
- **Fallback**: If type inference fails, values are stored as strings

## Error Handling

The configuration system uses return codes to indicate success or failure:

- `0`: Success
- `-1`: General error
- `-2`: File not found
- `-3`: JSON parsing error
- `-4`: Memory allocation failure
- `-5`: Invalid path
- `-6`: Type mismatch
- `-7`: Array index out of bounds
- `-8`: Maximum overrides exceeded

Each function logs detailed error messages using the SAGE logging system:

```c
if (file == NULL) {
    LOG_ERROR("Failed to open configuration file: %s", filename);
    return -2;
}
```

## Performance Considerations

1. **Caching Results**: For frequently accessed configuration values, cache them locally rather than calling the access functions repeatedly.

2. **Path Optimization**: The dot-notation path lookup performs a linear search at each level, so deeply nested paths can be slower to access.

3. **Memory Usage**: Configuration objects are dynamically allocated, so very large configurations can use significant memory.

4. **Load Once, Use Many**: Load the configuration once at startup rather than reloading for different components.

## Testing

Tests for the configuration system can be found in:
- `tests/test_config_system.c` - Unit tests for parsing and access
- `tests/test_config_overrides.c` - Tests for command-line overrides
- `tests/test_config_pipeline.c` - Tests for pipeline configuration

Run configuration system tests with:
```bash
make test_config_system
./tests/test_config_system
```

## See Also
- [Module System and Configuration](module_system_and_configuration.md)
- [Pipeline System](pipeline_phases.md)
- [Parameter System](parameter_system.md)

---

*Last updated: May 26, 2025*  
*Component version: 1.0*
