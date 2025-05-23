# Property Access Pattern Tests

This test suite validates that the codebase adheres to the core-physics separation principles by checking proper property access patterns.

## Background

The SAGE codebase has undergone a significant architectural shift to separate core infrastructure from physics implementations. This separation is enforced through strict property access patterns:

1. **Core Properties** must be accessed via `GALAXY_PROP_*` macros by any code.
2. **Physics Properties** must be accessed via generic property functions (`get_*_property()`, `set_*_property()`) by any code.
3. **No Direct Field Access** is allowed anywhere in the codebase.

## Tests

### test_property_access_patterns

This test validates the property access patterns in accordance with the core-physics separation principles:

- **Basic Property Macro Testing**: Tests the basic functionality of the `GALAXY_PROP_*` macros.
- **Core Property Access Testing**: Checks proper access patterns for core properties.
- **Physics Property Access Testing**: Verifies correct access patterns for physics properties using the generic property system.
- **Static Analysis**: Runs the Python validation script on placeholder modules to check for any direct field access.

### verify_placeholder_property_access.py

This Python script performs static analysis of the codebase to check for direct field access, which would violate the core-physics separation principles. It can be run manually on specific files or directories:

```bash
python verify_placeholder_property_access.py path/to/file_or_directory [--verbose] [--recursive]
```

## Usage

Run the test with:

```bash
make test_property_access_patterns
cd tests
./test_property_access_patterns
```

## When to Use This Test

This test is particularly valuable during Phase 5.2.G (Physics Module Migration) to ensure that all new physics modules comply with the core-physics separation principles.

## Replacement for test_galaxy_property_macros

This test replaces the outdated `test_galaxy_property_macros` which was designed for the transition period when both direct fields and property-based access existed simultaneously. The original test depended on:

1. Files that have been moved to the `ignore` directory (cooling_module.h, infall_module.h)
2. Synchronization infrastructure that has been removed (sync_properties_to_direct)
3. Original physics modules that have been replaced with placeholder modules

The new test achieves the same validation goals but works with the current architecture where complete core-physics separation has been implemented.
