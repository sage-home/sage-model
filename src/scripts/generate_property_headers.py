#!/usr/bin/env python3
"""
SAGE Property Header Generation

This script generates type-safe C headers from YAML property definitions.
It creates macros for property access, enumeration, and availability checking
to enable compile-time optimization and runtime modularity.

Usage:
    python scripts/generate_property_headers.py schema/properties.yaml build/src/core/

Generated files:
    - property_generated.h      : Core property definitions and structures
    - property_enums.h         : Property enumeration and masks  
    - property_access.h        : Type-safe access macros
"""

import argparse
import os
import sys
import yaml
from pathlib import Path
from typing import Dict, List, Any, Optional, Set
from dataclasses import dataclass
from datetime import datetime


# Version information for generated code
GENERATOR_VERSION = "1.0.0"
GENERATOR_NAME = "SAGE Property Header Generator"


@dataclass
class PropertyInfo:
    """Information about a single property from YAML schema."""
    name: str
    type: str
    category: str
    access_group: str
    memory_group: str
    io_group: str
    description: str
    units: str
    required_by: List[str]
    provided_by: List[str] = None
    array_size: Optional[str] = None
    dynamic_array: bool = False
    range: Optional[List[float]] = None
    validation: Optional[Dict] = None
    io_mappings: Optional[Dict] = None
    calculated_from: Optional[List[str]] = None
    calculation: Optional[str] = None

    @classmethod
    def from_yaml_entry(cls, name: str, data: Dict[str, Any]) -> 'PropertyInfo':
        """Create PropertyInfo from YAML entry."""
        return cls(
            name=name,
            type=data['type'],
            category=data['category'],
            access_group=data['access_group'],
            memory_group=data['memory_group'], 
            io_group=data['io_group'],
            description=data['description'],
            units=data['units'],
            required_by=data['required_by'],
            provided_by=data.get('provided_by', []),
            array_size=data.get('array_size'),
            dynamic_array=data.get('dynamic_array', False),
            range=data.get('range'),
            validation=data.get('validation'),
            io_mappings=data.get('io_mappings'),
            calculated_from=data.get('calculated_from'),
            calculation=data.get('calculation')
        )

    def is_array(self) -> bool:
        """Check if this property is an array type."""
        return '[' in self.type or self.dynamic_array

    def get_base_type(self) -> str:
        """Get the base C type without array specifiers."""
        return self.type.split('[')[0]

    def is_core_property(self) -> bool:
        """Check if this is a core (non-physics) property."""
        return self.category == 'core'

    def is_physics_property(self) -> bool:
        """Check if this is a physics-dependent property."""
        return self.category == 'physics'

    def is_output_property(self) -> bool:
        """Check if this property should be included in output."""
        return self.io_group in ['essential', 'optional', 'derived']

    def get_c_declaration(self) -> str:
        """Generate C struct member declaration."""
        base_type = self.get_base_type()
        
        if self.dynamic_array:
            # Dynamic arrays are pointers
            return f"{base_type}* {self.name}"
        elif self.is_array():
            # Static arrays with size
            if self.array_size and isinstance(self.array_size, str) and self.array_size.isdigit():
                return f"{base_type} {self.name}[{self.array_size}]"
            elif self.array_size and isinstance(self.array_size, str) and self.array_size.startswith('${'):
                # Configuration parameter - use STEPS as fallback for now
                param = self.array_size[2:-1]  # Remove ${ and }
                return f"{base_type} {self.name}[STEPS]"  # TODO: Make configurable
            elif self.array_size and isinstance(self.array_size, int):
                # Integer array size
                return f"{base_type} {self.name}[{self.array_size}]"
            else:
                # Extract size from type
                if '[' in self.type and ']' in self.type:
                    size = self.type.split('[')[1].split(']')[0]
                    return f"{base_type} {self.name}[{size}]"
                else:
                    return f"{base_type} {self.name}[1]"  # Fallback
        else:
            # Simple scalar type
            return f"{base_type} {self.name}"


class PropertySchema:
    """Container for the complete property schema from YAML."""
    
    def __init__(self, yaml_data: Dict[str, Any]):
        self.schema_version = yaml_data['schema_version']
        self.generated_code_version = yaml_data['generated_code_version']
        self.config = yaml_data['config']
        self.dimensions = yaml_data['dimensions']
        self.properties = {}
        self.availability_matrix = yaml_data['availability_matrix']
        self.memory_layouts = yaml_data['memory_layouts']
        self.validation_rules = yaml_data['validation_rules']
        self.io_formats = yaml_data['io_formats']
        
        # Parse properties
        for name, data in yaml_data['properties'].items():
            self.properties[name] = PropertyInfo.from_yaml_entry(name, data)

    def get_properties_by_category(self, category: str) -> List[PropertyInfo]:
        """Get all properties of a given category."""
        return [prop for prop in self.properties.values() if prop.category == category]

    def get_properties_by_memory_group(self, group: str) -> List[PropertyInfo]:
        """Get all properties in a memory group."""
        return [prop for prop in self.properties.values() if prop.memory_group == group]

    def get_properties_by_access_group(self, group: str) -> List[PropertyInfo]:
        """Get all properties in an access group."""
        return [prop for prop in self.properties.values() if prop.access_group == group]

    def get_output_properties(self) -> List[PropertyInfo]:
        """Get all properties that should be included in output."""
        return [prop for prop in self.properties.values() if prop.is_output_property()]

    def get_core_properties(self) -> List[PropertyInfo]:
        """Get all core properties."""
        return self.get_properties_by_category('core')

    def get_physics_properties(self) -> List[PropertyInfo]:
        """Get all physics properties."""
        return self.get_properties_by_category('physics')


class HeaderGenerator:
    """Base class for generating C header files."""
    
    def __init__(self, schema: PropertySchema, output_dir: Path):
        self.schema = schema
        self.output_dir = output_dir
        self.timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    def generate_header_comment(self, filename: str, description: str) -> str:
        """Generate standard header comment block."""
        return f"""/*
 * {filename}
 * 
 * {description}
 * 
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated by: {GENERATOR_NAME} v{GENERATOR_VERSION}
 * Source schema: properties.yaml v{self.schema.schema_version}
 * Generated on: {self.timestamp}
 * 
 * This file is part of the SAGE property system that provides:
 * - Type-safe property access through macros
 * - Compile-time property availability checking
 * - Runtime modularity support
 * - Memory layout optimization
 */

#ifndef SAGE_{filename.upper().replace('.', '_')}
#define SAGE_{filename.upper().replace('.', '_')}

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {{
#endif

"""

    def generate_header_footer(self, filename: str) -> str:
        """Generate standard header footer."""
        return f"""
#ifdef __cplusplus
}}
#endif

#endif /* SAGE_{filename.upper().replace('.', '_')} */
"""

    def write_header(self, filename: str, content: str):
        """Write header file to output directory."""
        filepath = self.output_dir / filename
        filepath.parent.mkdir(parents=True, exist_ok=True)
        
        with open(filepath, 'w') as f:
            f.write(content)
        
        print(f"Generated: {filepath}")


def load_schema(yaml_file: Path) -> PropertySchema:
    """Load and parse the property schema from YAML file."""
    try:
        with open(yaml_file, 'r') as f:
            data = yaml.safe_load(f)
        return PropertySchema(data)
    except Exception as e:
        print(f"Error loading schema from {yaml_file}: {e}")
        sys.exit(1)


def main():
    """Main entry point for the property header generator."""
    parser = argparse.ArgumentParser(
        description="Generate type-safe C headers from YAML property definitions",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        'schema_file',
        type=Path,
        help='Path to properties.yaml schema file'
    )
    
    parser.add_argument(
        'output_dir', 
        type=Path,
        help='Output directory for generated headers'
    )
    
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose output'
    )
    
    args = parser.parse_args()
    
    # Validate input
    if not args.schema_file.exists():
        print(f"Error: Schema file not found: {args.schema_file}")
        sys.exit(1)
    
    # Load schema
    if args.verbose:
        print(f"Loading schema from: {args.schema_file}")
    
    schema = load_schema(args.schema_file)
    
    if args.verbose:
        print(f"Loaded schema v{schema.schema_version}")
        print(f"Found {len(schema.properties)} properties")
        print(f"Core properties: {len(schema.get_core_properties())}")
        print(f"Physics properties: {len(schema.get_physics_properties())}")
    
    # Create output directory
    args.output_dir.mkdir(parents=True, exist_ok=True)
    
    # Generate headers
    generators = [
        StructureGenerator(schema, args.output_dir),
        EnumGenerator(schema, args.output_dir), 
        AccessGenerator(schema, args.output_dir)
    ]
    
    for generator in generators:
        generator.generate()
    
    print("\nProperty header generation completed successfully!")
    print(f"Generated files in: {args.output_dir}")


class StructureGenerator(HeaderGenerator):
    """Generates property_generated.h with struct definitions."""
    
    def generate(self):
        """Generate the main property structures header."""
        content = self.generate_header_comment(
            "property_generated.h",
            "Core property structure definitions with memory layout optimization"
        )
        
        content += self._generate_includes()
        content += self._generate_configuration_constants()
        content += self._generate_core_struct()
        content += self._generate_physics_struct()
        content += self._generate_internal_struct()
        content += self._generate_galaxy_struct()
        content += self._generate_struct_sizes()
        
        content += self.generate_header_footer("property_generated.h")
        
        self.write_header("property_generated.h", content)

    def _generate_includes(self) -> str:
        """Generate necessary includes."""
        return """
/* Required includes for property system */
#include "core_simulation.h"  /* For STEPS and other constants */

"""

    def _generate_configuration_constants(self) -> str:
        """Generate configuration constants from schema."""
        config = self.schema.config
        return f"""
/* Property system configuration */
#define SAGE_COMPILE_TIME_CHECKS {1 if config.get('compile_time_checks') else 0}
#define SAGE_RUNTIME_CHECKS {1 if config.get('runtime_checks') else 0}
#define SAGE_MEMORY_OPTIMIZATION {1 if config.get('memory_optimization') else 0}
#define SAGE_DYNAMIC_ARRAYS {1 if config.get('dynamic_arrays') else 0}

"""

    def _generate_core_struct(self) -> str:
        """Generate the core properties structure."""
        core_props = self.schema.get_properties_by_memory_group('core_block')
        
        content = """
/*
 * Core Properties Structure
 * Contains physics-agnostic properties that are always available.
 * Optimized for cache efficiency with 64-byte alignment.
 */
typedef struct {
"""
        
        for prop in core_props:
            content += f"    {prop.get_c_declaration()};  /* {prop.description} */\n"
        
        content += """} sage_galaxy_core_t;

"""
        return content

    def _generate_physics_struct(self) -> str:
        """Generate the physics properties structure.""" 
        physics_props = self.schema.get_properties_by_memory_group('physics_block')
        
        content = """
/*
 * Physics Properties Structure  
 * Contains physics-dependent properties that are conditionally available.
 * Only allocated when physics modules are loaded.
 */
typedef struct {
"""
        
        for prop in physics_props:
            content += f"    {prop.get_c_declaration()};  /* {prop.description} */\n"
        
        content += """} sage_galaxy_physics_t;

"""
        return content

    def _generate_internal_struct(self) -> str:
        """Generate structure for internal-only properties."""
        internal_props = [prop for prop in self.schema.properties.values() 
                         if prop.io_group == 'internal']
        
        if not internal_props:
            return ""
            
        content = """
/*
 * Internal Properties Structure
 * Contains properties used for calculations but never output.
 * Separate from physics properties for memory optimization.
 */
typedef struct {
"""
        
        for prop in internal_props:
            content += f"    {prop.get_c_declaration()};  /* {prop.description} */\n"
        
        content += """} sage_galaxy_internal_t;

"""
        return content

    def _generate_galaxy_struct(self) -> str:
        """Generate the main GALAXY structure with property groups."""
        return """
/*
 * Main Galaxy Structure
 * Combines all property groups into a single structure for compatibility.
 * Physics and internal properties are conditionally included.
 */
struct GALAXY {
    /* Core properties - always available */
    sage_galaxy_core_t core;
    
#ifdef SAGE_PHYSICS_ENABLED
    /* Physics properties - available when physics modules loaded */
    sage_galaxy_physics_t* physics;
#endif

#ifdef SAGE_INTERNAL_PROPERTIES_ENABLED  
    /* Internal properties - for calculations only */
    sage_galaxy_internal_t* internal;
#endif

    /* Dynamic arrays - allocated separately for memory efficiency */
    struct {
        float* SfrDisk;              /* [STEPS] Star formation rate in disk */
        float* SfrBulge;             /* [STEPS] Star formation rate in bulge */
        float* SfrDiskColdGas;       /* [STEPS] Cold gas used in disk SF */
        float* SfrDiskColdGasMetals; /* [STEPS] Metals in disk SF cold gas */
        float* SfrBulgeColdGas;      /* [STEPS] Cold gas used in bulge SF */ 
        float* SfrBulgeColdGasMetals;/* [STEPS] Metals in bulge SF cold gas */
        int array_size;              /* Actual size of allocated arrays */
    } arrays;
};

"""

    def _generate_struct_sizes(self) -> str:
        """Generate size information for memory management."""
        return """
/* Structure size information for memory management */
#define SAGE_GALAXY_CORE_SIZE sizeof(sage_galaxy_core_t)
#define SAGE_GALAXY_PHYSICS_SIZE sizeof(sage_galaxy_physics_t)
#define SAGE_GALAXY_INTERNAL_SIZE sizeof(sage_galaxy_internal_t)

/* Memory alignment for cache optimization */
#define SAGE_CACHE_LINE_SIZE 64
#define SAGE_GALAXY_CORE_ALIGNED_SIZE ((SAGE_GALAXY_CORE_SIZE + SAGE_CACHE_LINE_SIZE - 1) & ~(SAGE_CACHE_LINE_SIZE - 1))

"""


class EnumGenerator(HeaderGenerator):
    """Generates property_enums.h with property enumeration and masks."""
    
    def generate(self):
        """Generate property enumeration header."""
        content = self.generate_header_comment(
            "property_enums.h", 
            "Property enumeration, masks, and availability checking"
        )
        
        content += self._generate_property_enum()
        content += self._generate_module_enum()
        content += self._generate_availability_masks()
        content += self._generate_property_info_struct()
        content += self._generate_property_registry()
        
        content += self.generate_header_footer("property_enums.h")
        
        self.write_header("property_enums.h", content)

    def _generate_property_enum(self) -> str:
        """Generate enumeration of all properties."""
        content = """
/*
 * Property Enumeration
 * Unique identifier for each property in the system.
 */
typedef enum {
    SAGE_PROPERTY_INVALID = 0,
"""
        
        # Core properties first
        for prop in self.schema.get_core_properties():
            content += f"    SAGE_PROPERTY_{prop.name.upper()},\n"
        
        content += "\n    /* Physics properties */\n"
        
        # Physics properties
        for prop in self.schema.get_physics_properties():
            content += f"    SAGE_PROPERTY_{prop.name.upper()},\n"
        
        content += """
    SAGE_PROPERTY_COUNT  /* Total number of properties */
} sage_property_id_t;

"""
        return content

    def _generate_module_enum(self) -> str:
        """Generate enumeration of physics modules."""
        modules = self.schema.dimensions['by_module'].keys()
        
        content = """
/*
 * Physics Module Enumeration  
 * Identifies available physics modules in the system.
 */
typedef enum {
    SAGE_MODULE_NONE = 0,
"""
        
        for module in modules:
            content += f"    SAGE_MODULE_{module.upper()},\n"
        
        content += """
    SAGE_MODULE_COUNT
} sage_module_id_t;

/*
 * Module Bitmasks
 * Efficient storage of module combinations.
 */
"""
        
        for i, module in enumerate(modules):
            content += f"#define SAGE_MODULE_MASK_{module.upper()} (1U << {i})\n"
        
        content += "\n"
        return content

    def _generate_availability_masks(self) -> str:
        """Generate property availability masks for different module combinations."""
        content = """
/*
 * Property Availability Masks
 * Bitmasks indicating which properties are available for different module combinations.
 */

/* Core properties mask - always available */
#define SAGE_PROPERTIES_CORE_MASK \\\n"""
        
        core_props = self.schema.get_core_properties()
        core_mask_lines = []
        for prop in core_props:
            core_mask_lines.append(f"    (1ULL << SAGE_PROPERTY_{prop.name.upper()})")
        
        content += " | \\\n".join(core_mask_lines)
        content += "\n\n"
        
        # Generate masks for module combinations from availability matrix
        for config_name, config_data in self.schema.availability_matrix.items():
            if config_name == 'custom':
                continue  # Skip dynamic configuration
                
            content += f"/* {config_name.replace('_', ' ').title()} properties mask */\n"
            content += f"#define SAGE_PROPERTIES_{config_name.upper()}_MASK \\\n"
            
            if 'includes' in config_data:
                includes = config_data['includes']
                if isinstance(includes, list) and includes and isinstance(includes[0], str):
                    # Direct property list
                    mask_lines = []
                    for prop_name in includes:
                        if prop_name in self.schema.properties:
                            mask_lines.append(f"    (1ULL << SAGE_PROPERTY_{prop_name.upper()})")
                        elif prop_name == 'core_only':
                            mask_lines.append("    SAGE_PROPERTIES_CORE_MASK")
                        elif prop_name == 'minimal_physics':
                            mask_lines.append("    SAGE_PROPERTIES_MINIMAL_PHYSICS_MASK")
                    
                    content += " | \\\n".join(mask_lines)
            
            content += "\n\n"
        
        return content

    def _generate_property_info_struct(self) -> str:
        """Generate structure for property metadata."""
        return """
/*
 * Property Information Structure
 * Runtime metadata about each property.
 */
typedef struct {
    sage_property_id_t id;
    const char* name;
    const char* type;
    const char* description;
    const char* units;
    uint64_t required_modules;     /* Bitmask of required modules */
    uint64_t provided_modules;     /* Bitmask of providing modules */
    size_t offset_in_core;         /* Offset in core struct (or SIZE_MAX if not core) */
    size_t offset_in_physics;      /* Offset in physics struct (or SIZE_MAX if not physics) */
    size_t offset_in_internal;     /* Offset in internal struct (or SIZE_MAX if not internal) */
    bool is_array;
    bool is_dynamic_array;
    int array_size;                /* Size for static arrays, -1 for dynamic */
    bool is_output;                /* Whether property appears in output */
} sage_property_info_t;

"""

    def _generate_property_registry(self) -> str:
        """Generate property registry with metadata."""
        return """
/*
 * Property Registry Declaration
 * Contains runtime metadata for all properties.
 * Implemented in generated property_registry.c
 */
extern const sage_property_info_t sage_property_registry[SAGE_PROPERTY_COUNT];

/* Property registry access functions */
const sage_property_info_t* sage_get_property_info(sage_property_id_t property_id);
const char* sage_get_property_name(sage_property_id_t property_id);
bool sage_is_property_available(sage_property_id_t property_id, uint64_t module_mask);
uint64_t sage_get_available_properties_mask(uint64_t module_mask);

"""


class AccessGenerator(HeaderGenerator):
    """Generates property_access.h with type-safe access macros."""
    
    def generate(self):
        """Generate property access macros header."""
        content = self.generate_header_comment(
            "property_access.h",
            "Type-safe property access macros with compile-time validation"
        )
        
        content += self._generate_includes()
        content += self._generate_access_validation()
        content += self._generate_core_accessors()
        content += self._generate_physics_accessors() 
        content += self._generate_array_accessors()
        content += self._generate_utility_macros()
        
        content += self.generate_header_footer("property_access.h")
        
        self.write_header("property_access.h", content)

    def _generate_includes(self) -> str:
        """Generate required includes for access macros."""
        return """
#include "property_generated.h"
#include "property_enums.h"

/* Enable compile-time checking if requested */
#if SAGE_COMPILE_TIME_CHECKS
#include <assert.h>
#define SAGE_PROPERTY_ASSERT(condition) assert(condition)
#else
#define SAGE_PROPERTY_ASSERT(condition) ((void)0)
#endif

"""

    def _generate_access_validation(self) -> str:
        """Generate macros for property access validation."""
        return """
/*
 * Property Access Validation
 * Compile-time and runtime checks for property availability.
 */

/* Compile-time property availability checking */
#define SAGE_PROPERTY_IS_CORE(prop) \\\n    ((SAGE_PROPERTIES_CORE_MASK & (1ULL << SAGE_PROPERTY_##prop)) != 0)

#define SAGE_PROPERTY_IS_PHYSICS(prop) \\\n    (!SAGE_PROPERTY_IS_CORE(prop))

/* Runtime property availability checking */
#define SAGE_PROPERTY_AVAILABLE(prop, modules) \\\n    sage_is_property_available(SAGE_PROPERTY_##prop, modules)

/* Property validation for debug builds */
#if SAGE_RUNTIME_CHECKS
#define SAGE_VALIDATE_CORE_PROPERTY_ACCESS(galaxy, prop) do { \\\n    SAGE_PROPERTY_ASSERT((galaxy) != NULL); \\\n} while(0)

#define SAGE_VALIDATE_PHYSICS_PROPERTY_ACCESS(galaxy, prop) do { \\\n    SAGE_PROPERTY_ASSERT((galaxy) != NULL); \\\n    SAGE_PROPERTY_ASSERT((galaxy)->physics != NULL); \\\n} while(0)
#else
#define SAGE_VALIDATE_CORE_PROPERTY_ACCESS(galaxy, prop) ((void)0)
#define SAGE_VALIDATE_PHYSICS_PROPERTY_ACCESS(galaxy, prop) ((void)0)
#endif

"""

    def _generate_core_accessors(self) -> str:
        """Generate accessor macros for core properties."""
        content = """
/*
 * Core Property Accessors
 * Type-safe access to core properties (always available).
 */

"""
        
        for prop in self.schema.get_core_properties():
            prop_upper = prop.name.upper()
            
            # Generate getter macro
            content += f"/* {prop.description} */\n"
            content += f"#define GALAXY_GET_{prop_upper}(galaxy) \\\n"
            content += f"    ((galaxy)->core.{prop.name})\n\n"
            
            # Generate setter macro
            content += f"#define GALAXY_SET_{prop_upper}(galaxy, value) do {{ \\\n"
            content += f"    SAGE_VALIDATE_CORE_PROPERTY_ACCESS(galaxy, {prop_upper}); \\\n"
            content += f"    (galaxy)->core.{prop.name} = (value); \\\n"
            content += f"}} while(0)\n\n"
            
            # Generate reference macro for arrays
            if prop.is_array():
                content += f"#define GALAXY_REF_{prop_upper}(galaxy) \\\n"
                content += f"    ((galaxy)->core.{prop.name})\n\n"
        
        return content

    def _generate_physics_accessors(self) -> str:
        """Generate accessor macros for physics properties."""
        content = """
/*
 * Physics Property Accessors  
 * Type-safe access to physics properties (conditionally available).
 */

"""
        
        for prop in self.schema.get_physics_properties():
            prop_upper = prop.name.upper()
            
            # Skip array properties that are handled separately
            if prop.name in ['SfrDisk', 'SfrBulge', 'SfrDiskColdGas', 'SfrDiskColdGasMetals',
                           'SfrBulgeColdGas', 'SfrBulgeColdGasMetals']:
                continue
            
            # Generate getter macro
            content += f"/* {prop.description} */\n"
            content += f"#define GALAXY_GET_{prop_upper}(galaxy) \\\n"
            content += f"    ((galaxy)->physics ? (galaxy)->physics->{prop.name} : 0)\n\n"
            
            # Generate setter macro
            content += f"#define GALAXY_SET_{prop_upper}(galaxy, value) do {{ \\\n"
            content += f"    SAGE_VALIDATE_PHYSICS_PROPERTY_ACCESS(galaxy, {prop_upper}); \\\n"
            content += f"    if ((galaxy)->physics) {{ \\\n"
            content += f"        (galaxy)->physics->{prop.name} = (value); \\\n"
            content += f"    }} \\\n"
            content += f"}} while(0)\n\n"
        
        return content

    def _generate_array_accessors(self) -> str:
        """Generate accessor macros for dynamic array properties."""
        return """
/*
 * Dynamic Array Property Accessors
 * Access to variable-sized arrays with bounds checking.
 */

/* Star formation rate arrays */
#define GALAXY_GET_SFR_DISK(galaxy, index) \\\n    (((galaxy)->arrays.SfrDisk && (index) < (galaxy)->arrays.array_size) ? \\\n     (galaxy)->arrays.SfrDisk[index] : 0.0f)

#define GALAXY_SET_SFR_DISK(galaxy, index, value) do { \\\n    SAGE_PROPERTY_ASSERT((galaxy)->arrays.SfrDisk != NULL); \\\n    SAGE_PROPERTY_ASSERT((index) < (galaxy)->arrays.array_size); \\\n    (galaxy)->arrays.SfrDisk[index] = (value); \\\n} while(0)

#define GALAXY_REF_SFR_DISK(galaxy) \\\n    ((galaxy)->arrays.SfrDisk)

#define GALAXY_GET_SFR_BULGE(galaxy, index) \\\n    (((galaxy)->arrays.SfrBulge && (index) < (galaxy)->arrays.array_size) ? \\\n     (galaxy)->arrays.SfrBulge[index] : 0.0f)

#define GALAXY_SET_SFR_BULGE(galaxy, index, value) do { \\\n    SAGE_PROPERTY_ASSERT((galaxy)->arrays.SfrBulge != NULL); \\\n    SAGE_PROPERTY_ASSERT((index) < (galaxy)->arrays.array_size); \\\n    (galaxy)->arrays.SfrBulge[index] = (value); \\\n} while(0)

#define GALAXY_REF_SFR_BULGE(galaxy) \\\n    ((galaxy)->arrays.SfrBulge)

/* Internal array accessors for calculations */
#define GALAXY_GET_SFR_DISK_COLD_GAS(galaxy, index) \\\n    (((galaxy)->arrays.SfrDiskColdGas && (index) < (galaxy)->arrays.array_size) ? \\\n     (galaxy)->arrays.SfrDiskColdGas[index] : 0.0f)

#define GALAXY_SET_SFR_DISK_COLD_GAS(galaxy, index, value) do { \\\n    SAGE_PROPERTY_ASSERT((galaxy)->arrays.SfrDiskColdGas != NULL); \\\n    SAGE_PROPERTY_ASSERT((index) < (galaxy)->arrays.array_size); \\\n    (galaxy)->arrays.SfrDiskColdGas[index] = (value); \\\n} while(0)

/* Array size accessor */
#define GALAXY_GET_ARRAY_SIZE(galaxy) \\\n    ((galaxy)->arrays.array_size)

#define GALAXY_SET_ARRAY_SIZE(galaxy, size) do { \\\n    (galaxy)->arrays.array_size = (size); \\\n} while(0)

"""

    def _generate_utility_macros(self) -> str:
        """Generate utility macros for property operations."""
        return """
/*
 * Property Utility Macros
 * Helper functions for property management and iteration.
 */

/* Property existence checking */
#define GALAXY_HAS_PHYSICS(galaxy) \\\n    ((galaxy)->physics != NULL)

#define GALAXY_HAS_ARRAYS(galaxy) \\\n    ((galaxy)->arrays.SfrDisk != NULL)

/* Safe property access with default values */
#define GALAXY_GET_OR_DEFAULT(galaxy, prop, default_val) \\\n    (GALAXY_HAS_PHYSICS(galaxy) ? GALAXY_GET_##prop(galaxy) : (default_val))

/* Property copying between galaxies */
#define GALAXY_COPY_CORE_PROPERTY(dest, src, prop) do { \\\n    GALAXY_SET_##prop(dest, GALAXY_GET_##prop(src)); \\\n} while(0)

#define GALAXY_COPY_PHYSICS_PROPERTY(dest, src, prop) do { \\\n    if (GALAXY_HAS_PHYSICS(src) && GALAXY_HAS_PHYSICS(dest)) { \\\n        GALAXY_SET_##prop(dest, GALAXY_GET_##prop(src)); \\\n    } \\\n} while(0)

/* Property validation */
#define GALAXY_VALIDATE_MASS_CONSERVATION(galaxy) \\\n    SAGE_PROPERTY_ASSERT(GALAXY_GET_COLDGAS(galaxy) >= 0.0f && \\\n                        GALAXY_GET_STELLARMASS(galaxy) >= 0.0f && \\\n                        GALAXY_GET_HOTGAS(galaxy) >= 0.0f)

"""


if __name__ == '__main__':
    main()