# SAGE Quick Reference & Documentation Hub

**SAGE** (Semi-Analytic Galaxy Evolution) is a modern, modular galaxy formation framework with a physics-agnostic core and runtime-configurable physics modules.

---

## 8 Core Architectural Principles

SAGE's design is guided by eight foundational principles (detailed in [Architectural Vision](architectural-vision.md)):

1. **Physics-Agnostic Core Infrastructure** - Core systems operate independently of physics implementations
2. **Runtime Modularity** - Physics modules configurable at runtime without recompilation  
3. **Metadata-Driven Architecture** - System structure defined by metadata, not hardcoded implementations
4. **Single Source of Truth** - Galaxy data has one authoritative representation with consistent access
5. **Unified Processing Model** - Single consistent method for processing merger trees
6. **Memory Efficiency and Safety** - Bounded, predictable memory usage with automatic management
7. **Format-Agnostic I/O** - Multiple input/output formats through unified interfaces
8. **Type Safety and Validation** - Type-safe data access with automatic validation and fail-fast behavior

---

## Quick Navigation by Use Case

### 🚀 Getting Started
- **Installation & Building**: [CLAUDE.md](../CLAUDE.md) - Essential commands and workflow
- **First Run**: `./build.sh && ./build/sage input/millennium.par`
- **Architecture Overview**: [Architectural Vision](architectural-vision.md) - Core principles and design

### 🧪 Testing & Validation  
- **Unit Testing**: [Testing Framework](testing-framework.md) - CMake-based test suite (Principles 1, 3, 6, 8)
- **Scientific Validation**: End-to-end testing with reference data comparison
- **Performance Benchmarking**: [Benchmarking Guide](benchmarking.md) - Performance measurement tools (Principles 6, 8)

### ⚙️ Property System
- **Code Generation**: [Code Generation Interface](code-generation-interface.md) - Metadata-driven properties (Principles 3, 4, 8)
- **Schema Reference**: [Schema Reference](schema-reference.md) - Property and parameter definitions
- **Property Access**: Type-safe galaxy property access patterns *(TBD)*

### 🔧 Configuration & Parameters
- **Parameter Files**: Legacy .par format support for backward compatibility
- **Modern Configuration**: JSON-based configuration with schema validation *(TBD)*
- **Parameter Reference**: Complete parameter documentation and validation rules *(TBD)*

---

## Development Capabilities (Current)

### Core Infrastructure ✅
- Physics-agnostic pipeline execution and tree processing
- Unified memory management with scope-based cleanup
- Type-safe property access through generated code
- Comprehensive testing framework with scientific validation

### Build System ✅  
- CMake-based out-of-tree builds with IDE integration
- Multiple build configurations (Debug, Release, physics-free mode)
- Automatic code generation from YAML metadata
- Cross-platform compatibility (Linux, macOS)

### Property System ✅
- YAML-based property definitions with module dependencies
- Generated type-safe C access macros and structures
- Multi-dimensional property organization (module, access, memory, I/O)
- Compile-time availability checking for physics properties

### I/O System ✅
- Multiple merger tree format readers (LHalo, Gadget4, Genesis, ConsistentTrees)
- HDF5 output with hierarchical organization
- Endianness handling and cross-platform compatibility
- Property-based output field generation

---

## Future Capabilities (Planned)

### Module System (Phase 2D) *(TBD)*
- **Runtime Module Loading** - Dynamic physics module combinations *(TBD)*
- **Module Development Guide** - Creating and registering new physics modules *(TBD)*  
- **Inter-Module Communication** - Module dependencies and data sharing *(TBD)*
- **Module Lifecycle Management** - Initialization, execution phases, cleanup *(TBD)*

### Advanced I/O (Phase 2C-2D) *(TBD)*
- **Output Format Extensions** - Additional output formats beyond HDF5/binary *(TBD)*
- **Streaming I/O** - Memory-efficient processing of large datasets *(TBD)*
- **Parallel I/O** - MPI-based parallel reading and writing *(TBD)*
- **Data Validation** - Automatic consistency checking and error detection *(TBD)*

### Memory Optimization (Phase 2B-2C) *(TBD)*
- **Layout Optimization** - Cache-friendly memory organization *(TBD)*  
- **Memory Profiling** - Built-in memory usage analysis tools *(TBD)*
- **Pool Allocators** - Specialized allocation strategies for performance *(TBD)*
- **Garbage Collection** - Advanced automatic memory management *(TBD)*

### Core Architecture (Phase 2A-2B) *(TBD)*
- **Pipeline Customization** - User-defined processing workflows *(TBD)*
- **Data Structure Reference** - Galaxy, halo, and tree data organization *(TBD)*
- **Processing Model Guide** - Tree traversal algorithms and inheritance rules *(TBD)*
- **Event System** - Galaxy merger and evolution event handling *(TBD)*

### Advanced Features (Future) *(TBD)*
- **MPI Parallelization** - Distributed processing across compute nodes *(TBD)*
- **Performance Optimization** - Profiling tools and optimization strategies *(TBD)*  
- **Real-time Monitoring** - Live progress tracking and diagnostics *(TBD)*
- **Database Integration** - Direct database storage and retrieval *(TBD)*

---

## Essential Commands

```bash
# Build and test workflow
./build.sh                    # Build SAGE executable and library
./build.sh tests             # Run complete test suite
./build.sh unit_tests        # Fast development cycle testing
./build.sh clean            # Remove compiled objects
./build.sh debug            # Configure debug build with memory checking

# Running SAGE
./build/sage input/millennium.par    # Run with parameter file
./build/sage --help                  # Show all available options

# Development workflow  
mkdir build && cd build             # Create build directory
cmake .. && make                    # Configure and build
make test                           # Run all tests
ctest --output-on-failure          # Run tests with detailed output
```

---

## Documentation Standards

All SAGE documentation follows consistent standards defined in the [Documentation Guide](documentation-guide.md):

- **Self-contained descriptions** - Each document explains referenced concepts
- **Principle references** - Clear connections to architectural principles by number
- **Current vs future clarity** - Explicit status of capabilities and features
- **Use case organization** - Structure matches developer workflows
- **Cross-references** - Links between related documentation sections

---

## Key File Locations

```
sage-model/
├── docs/                      # All user and developer documentation
│   ├── quick-reference.md     # This file - main entry point
│   ├── architectural-vision.md  # Detailed principle reference
│   ├── testing-framework.md   # Testing system documentation
│   ├── benchmarking.md        # Performance measurement tools
│   ├── code-generation-interface.md  # Property system details
│   └── schema-reference.md    # Property/parameter metadata
├── src/                       # Source code
│   ├── core/                  # Physics-agnostic infrastructure
│   ├── physics/               # Physics modules and implementations
│   └── io/                    # I/O system and format handlers
├── schema/                    # Property and parameter definitions
│   ├── properties.yaml        # Galaxy property metadata
│   └── parameters.yaml        # Simulation parameter metadata
├── tests/                     # Comprehensive test suite
└── build/                     # Out-of-tree build directory
```

---

## Getting Help

- **Architecture Questions**: [Architectural Vision](architectural-vision.md) - Comprehensive design guide
- **Implementation Details**: [CLAUDE.md](../CLAUDE.md) - Development workflow and commands
- **Testing Issues**: [Testing Framework](testing-framework.md) - Test organization and execution
- **Performance Concerns**: [Benchmarking Guide](benchmarking.md) - Performance measurement and optimization
- **Property System**: [Code Generation Interface](code-generation-interface.md) - Metadata-driven development

---

## Contributing

SAGE development follows the architectural principles and maintains scientific accuracy through comprehensive testing. Before contributing:

1. **Understand the Architecture**: Review [Architectural Vision](architectural-vision.md) 
2. **Follow Development Standards**: Use [Documentation Guide](documentation-guide.md) for any documentation
3. **Test Thoroughly**: Ensure all tests pass with `./build.sh tests`
4. **Validate Scientific Accuracy**: Compare results against reference data

For questions about specific aspects of the system, use the navigation above to find the relevant documentation section.