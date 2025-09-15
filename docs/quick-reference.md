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

## How to Use This Documentation

**For AI Development:**
1. **Always start here** - This document is the navigation hub for everything
2. **Follow the use case navigation** - Go directly to what you need
3. **Reference principles by number** - Use "Principle N: Name" format consistently  
4. **Check CLAUDE.md for commands** - All essential commands and workflows are there
5. **Maintain architectural compliance** - All work must align with the 8 core principles

**Essential Commands and Workflows**: See [CLAUDE.md](../CLAUDE.md) for all build commands, testing, and development workflows.

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

### 🔧 Development & Contributing
- **Architecture Questions**: [Architectural Vision](architectural-vision.md) - Comprehensive design guide and detailed principles
- **Writing Documentation**: [Documentation Guide](documentation-guide.md) - Style standards and guidelines
- **Commands & Workflows**: [CLAUDE.md](../CLAUDE.md) - All essential commands and development workflows
- **Contributing Guidelines**: Follow architectural principles, test thoroughly, validate scientific accuracy

---

## Current Development Status

**Active Phase**: See [log/phase.md](../log/phase.md) for current work and progress  
**Full Roadmap**: See [log/sage-master-plan.md](../log/sage-master-plan.md) for complete implementation plan  
**Implementation Progress**: All phases and capabilities tracked in master plan with completion status

---

## Documentation Standards

All SAGE documentation follows consistent standards defined in the [Documentation Guide](documentation-guide.md):

- **Self-contained descriptions** - Each document explains referenced concepts
- **Principle references** - Clear connections to architectural principles by number
- **Current vs future clarity** - Explicit status of capabilities and features
- **Use case organization** - Structure matches developer workflows
- **Cross-references** - Links between related documentation sections
