## 2025-04-30

- Batch update: Replaced all instances of `#include "../physics/model_` with `#include "../physics/legacy/model_` in the following files:
  - src/io/save_gals_hdf5.c
  - src/io/save_gals_binary.c
  - src/io/io_binary_output.c
  - src/io/io_hdf5_output.c
  - src/physics/old/model_infall.c
  - src/physics/old/model_cooling_heating.c
  - src/physics/legacy/model_reincorporation.c
  - src/physics/legacy/model_misc.c
  - src/physics/legacy/model_infall.c
  - src/physics/legacy/model_mergers.c
  - src/physics/legacy/model_disk_instability.c

- Purpose: Ensure all legacy model includes use the correct path after refactor. This should resolve repeated build failures due to missing headers.

2025-04-30 10:21: update include in src/physics/legacy/model_disk_instability.c from ../physics/legacy/model_mergers.h to local header model_mergers.h for correct relative path after migration
2025-04-30 10:22: update include in src/physics/legacy/model_reincorporation.c from ../core/core_allvars.h to ../../core/core_allvars.h for correct relative path after migration
2025-04-30 10:25: Systematically updated include paths in all legacy files after directory restructuring:

1. Updated core includes in legacy files from "../core/" to "../../core/":
   - src/physics/legacy/model_reincorporation.c
   - src/physics/legacy/model_misc.c
   - src/physics/legacy/model_cooling_heating.c
   - src/physics/legacy/model_mergers.c
   - src/physics/legacy/model_infall.c
   - src/physics/legacy/model_disk_instability.c (previously updated)

2. Updated physics/legacy includes in legacy files from "../physics/legacy/" to direct local includes:
   - src/physics/legacy/model_reincorporation.c
   - src/physics/legacy/model_misc.c
   - src/physics/legacy/model_cooling_heating.c
   - src/physics/legacy/model_mergers.c
   - src/physics/legacy/model_infall.c
   - src/physics/legacy/model_disk_instability.c (previously updated)

All include paths are now consistent with the new directory structure. These changes should ensure correct compilation of all legacy model files after their migration to the src/physics/legacy/ directory.

2025-04-30 10:35: Updated include path for cooling_tables.h in src/physics/cooling_module.c from "cooling_tables.h" to "modules/cooling_tables.h" to reflect the new modules directory structure.


2025-04-30 10:58: Completed Step 1 of the Core Modularity Implementation Plan with the following changes:

1. Updated include path in src/physics/cooling_module.c from 'modules/cooling_tables.h' to 'cooling_tables.h' to reflect the new direct file location

2. Updated Makefile to reference cooling_tables.c directly in physics/ directory instead of physics/modules/

These changes complete the directory restructuring phase of the Core Modularity Implementation Plan.

2025-04-30 11:10: Renamed src/physics/old directory to src/physics/migrated

1. Used git mv to rename the directory while preserving history: `git mv src/physics/old src/physics/migrated`

2. Verified the following files were properly transferred:
   - src/physics/migrated/model_cooling_heating.c
   - src/physics/migrated/model_cooling_heating.h
   - src/physics/migrated/model_infall.c
   - src/physics/migrated/model_infall.h

3. Confirmed no code changes were needed as there were no references to this directory in the codebase.

This rename improves the project structure by using more descriptive terminology - "migrated" better reflects that these are legacy files that have already been converted to the new modular architecture, rather than just "old" files.
