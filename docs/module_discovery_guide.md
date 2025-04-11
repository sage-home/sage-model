# Module Discovery Guide

## Overview

The module discovery system in SAGE allows physics modules to be discovered and loaded at runtime without requiring recompilation of the core code. This document explains how the module discovery system works and how to configure it.

## Module Manifests

Each module must provide a manifest file (with a `.manifest` extension) that describes the module and its requirements. The manifest file uses a simple key-value format:

```
name: example_cooling
version: 1.0.0
author: SAGE Team
description: Example cooling module for SAGE
type: cooling
library: example_cooling.so
api_version: 1
auto_initialize: true
auto_activate: true
dependency.1: misc: 1.0.0
```

The following fields are required:
- `name`: The name of the module
- `version`: The version of the module (using semantic versioning)
- `type`: The type of the module (cooling, star_formation, feedback, etc.)
- `library`: Path to the module library (can be absolute or relative to the manifest)
- `api_version`: The core API version the module was built for

Optional fields include:
- `author`: The module author
- `description`: A description of the module's functionality
- `auto_initialize`: Whether to initialize the module automatically on load (default: true)
- `auto_activate`: Whether to set the module as active for its type (default: false)
- `dependency.X`: Module dependencies (format: `name: min_version`)

## Module Search Paths

The module system searches for modules in the configured search paths. Search paths can be configured through:

1. Parameter file:
   ```
   ModuleDir         ./modules      # Primary module directory
   NumModulePaths    2              # Number of additional module paths
   ModulePath[0]     /usr/local/lib/sage/modules
   ModulePath[1]     ~/sage/modules
   ```

2. JSON Configuration:
   ```json
   {
       "modules": {
           "discovery_enabled": true,
           "search_paths": [
               "./modules",
               "/usr/local/lib/sage/modules",
               "~/sage/modules"
           ]
       }
   }
   ```

3. Code:
   ```c
   module_add_search_path("./modules");
   module_add_search_path("/usr/local/lib/sage/modules");
   ```

## API Versioning

The module system uses API versioning to ensure compatibility between the core and modules:

- Each module specifies the core API version it was built for
- The current core API version is defined as `CORE_API_VERSION`
- The minimum compatible API version is defined as `CORE_API_MIN_VERSION`
- Modules must have an API version that is:
  - Equal to the current core API version, or
  - Between the minimum and current API versions

If a module's API version is incompatible, it will not be loaded.

## Dependencies

Modules can specify dependencies on other modules:

- Dependencies are listed in the manifest using the `dependency.X` format
- Each dependency specifies a module name and minimum version
- Optional dependencies are marked with `[optional]`
- Exact version matches are marked with `[exact]`

Example dependencies:
```
dependency.1: misc: 1.0.0
dependency.2: cooling[optional]: 2.1.0
dependency.3: star_formation[exact]: 1.5.2
```

The module system resolves dependencies during loading to ensure all requirements are met.

## Runtime Module Discovery

To enable module discovery at runtime:

1. Initialize the module system:
   ```c
   module_system_initialize();
   ```

2. Configure the system with search paths:
   ```c
   module_system_configure(params);
   ```

3. Enable module discovery:
   ```c
   params.runtime.EnableModuleDiscovery = 1;
   ```

4. Discover and load modules:
   ```c
   int modules_found = module_discover(params);
   ```

The discovery process will:
1. Scan all search paths for manifest files
2. Load and validate each manifest
3. Check API compatibility for each module
4. Resolve module dependencies
5. Load and initialize modules in the correct order
6. Set modules as active if specified

## Troubleshooting

If modules are not being discovered or loaded:

1. Check that module discovery is enabled
2. Verify that search paths are correctly configured
3. Ensure manifest files have the `.manifest` extension
4. Check that library files exist and are readable
5. Verify API version compatibility
6. Check for dependency issues

Enable debug logging to see detailed information about the discovery process:
```c
logging_set_minimum_level(LOG_LEVEL_DEBUG);
```
