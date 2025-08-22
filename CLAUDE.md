# CLAUDE.md

This file provides guidance to Claude Code when working with SAGE. **Keep this concise - detailed info belongs in log files.**

## Project Overview
SAGE (Semi-Analytic Galaxy Evolution) is a C-based galaxy formation model that runs on N-body simulation merger trees. It's a modular framework for studying galaxy evolution in cosmological context.

## Essential Commands
- `./build.sh` - Build SAGE executable and library
- `./build.sh tests` - Run complete test suite  
- `./build.sh unit_tests` - Run unit tests only (fast development cycle)
- `./build.sh clean` - Remove compiled objects
- `./build.sh debug` - Configure debug build with memory checking
- `./build/sage input/millennium.par` - Run SAGE with parameter file

**Full command reference**: See `log/quick-reference.md`

## Key File Locations
- **Core**: `src/core/main.c`, `src/core/sage.c`, `src/core/core_allvars.h`
- **Physics**: `src/physics/model_*.c` files
- **I/O**: `src/io/read_tree_*.c`, `src/io/save_gals_*.c`
- **Config**: `src/core/config/` (unified .par/JSON system)
- **Memory**: `src/core/memory.h/.c` (modern abstraction layer)

## Development Workflow
```bash
./build.sh              # Build
./build.sh unit_tests   # Quick test cycle  
./build.sh tests        # Full validation
./build/sage input/millennium.par  # Run
```

## Context Files Organization

### ⚠️ CRITICAL: What to Update When
- **CLAUDE.md (this file)**: Essential commands, key paths, basic workflow only
- **log/quick-reference.md**: Detailed commands, all build options, examples
- **log/architecture.md**: System design, data structures, technical details
- **log/decisions.md**: Technical decisions with rationale
- **log/progress.md**: Implementation milestones and file changes

### Update Rules
- **NEVER duplicate information** between CLAUDE.md and log files
- **CLAUDE.md**: Keep under 100 lines, essentials only
- **Log files**: Contain all detailed technical information
- **When in doubt**: Put detailed info in logs, reference from CLAUDE.md

## Development Guidelines
- Read `log/sage-architecture-vision.md` and `log/sage-architecture-guide.md` for context
- All code to highest professional standards
- Never simplify tests - failing tests indicate real problems
- Report progress in `log/progress.md` with all changed files
- Ask before committing to git
- Archive files to `scrap/` instead of deleting
- Use logs for continuity - assume no persistent memory