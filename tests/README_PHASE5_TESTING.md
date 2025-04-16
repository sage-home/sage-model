# SAGE Testing During Phase 5 Development

## Testing Strategy

During Phase 5 (Core Module Migration), the codebase is undergoing significant architectural changes as we:

1. Transform `evolve_galaxies()` to exclusively use the pipeline system
2. Convert physics components into standalone modules
3. Implement a fully modular architecture

## Expected Test Failures

Until all modules are fully implemented, **end-to-end benchmark comparison tests will fail**. This is expected and should not block development. The testing system has been modified to:

- Continue running all tests even when end-to-end benchmarking fails
- Clearly distinguish between expected failures (end-to-end) and unexpected failures (unit tests)
- Provide detailed reporting to help identify issues

## Running Tests

```bash
make tests
```

This will:
1. Compile SAGE
2. Run end-to-end tests (which will likely fail during Phase 5)
3. Continue running all unit and component tests regardless
4. Report any failures at the end

## Test Categories

### Expected to Fail During Phase 5
- Binary output comparison tests
- HDF5 output comparison tests

These tests compare simulation results against benchmarks from the original monolithic implementation. They will naturally fail until all modules are fully implemented and tuned to match the original physics.

### Should Always Pass
- All unit tests
- Component tests
- Module framework tests
- IO validation tests

These tests verify the functionality of individual components and should always pass, regardless of the state of physics module migration.

## Restoring Full Validation

When all modules are successfully implemented in Phase 5.3, the end-to-end benchmark tests will be reactivated as strict requirements.
