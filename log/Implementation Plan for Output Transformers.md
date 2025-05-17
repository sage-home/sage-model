# Implementation Plan for Output Transformers

This document provides detailed instructions for implementing the "Output Transformers Implementation Plan" for handling special pre-output processing of galaxy properties, ensuring the core I/O system remains physics-agnostic.

**Goal:** To move all physics-specific output transformations (unit conversions, derivations) out of the core I/O code (`src/io/`) and into module-provided functions, invoked via an auto-generated dispatch mechanism. The original galaxy property state must remain unchanged by this output preparation process.

---

### Implementation Plan: Module-Registered Output Transformers

**Overall Strategy:**

1.  **YAML Definition:** Augment `properties.yaml` to allow specifying a custom C function for transforming a property's value specifically for output.
2.  **Module Implementation:** Physics modules (or dedicated output utility files within `src/physics/`) will implement these C transformer functions.
3.  **Code Generation:** The existing `generate_property_headers.py` script will be extended to:
    *   Parse the new YAML field.
    *   Generate a new C file (e.g., `generated_output_transformers.c`) containing `extern` declarations for all specified transformer functions and a central `dispatch_property_transformer()` function. This dispatch function will call the appropriate module-provided transformer or a default identity transform.
4.  **Core I/O Modification:** The `prepare_galaxy_for_hdf5_output.c` function will be simplified to call this `dispatch_property_transformer()` for each physics property, thus becoming agnostic to the specific transformations.
5.  **Prerequisite:** A generic array element accessor for physics properties (e.g., `get_float_array_element_property()`) must be implemented in `core_property_utils.c/h` as it's needed by transformer functions that derive output from array properties.

---

**Detailed Steps:**

**Phase 1: Prerequisite - Generic Array Element Accessor** ✅ COMPLETED

*   **Where:** `src/core/core_property_utils.h` and `src/core/core_property_utils.c`
*   **Why:** Transformer functions (e.g., for deriving scalar `SfrDisk` from an `SfrDiskHistoryArray`) need a way to read elements from physics array properties generically. The current `GALAXY_PROP_ARRAY_SAFE` is for core properties or properties directly in `galaxy->properties`.
*   **What:**
    1.  **In `core_property_utils.h`:**
        Declare functions like:
        ```c
        float get_float_array_element_property(const struct GALAXY *g, property_id_t array_prop_id, int index, float default_value);
        int get_int_array_element_property(const struct GALAXY *g, property_id_t array_prop_id, int index, int default_value);
        // Add for other relevant types (double, etc.)
        int get_array_property_size(const struct GALAXY *g, property_id_t array_prop_id); // To get the runtime size
        ```
    2.  **In `core_property_utils.c`:**
        Implement these functions. They will need to:
        *   Get the `galaxy_properties_t` struct from `g->properties`.
        *   Use the `array_prop_id` to find the correct array pointer and its `_size` field within `galaxy_properties_t` (e.g., `g->properties->MyArray` and `g->properties->MyArray_size`).
        *   Perform bounds checking using the `_size` field.
        *   Return the element or `default_value`.
        *   The `get_array_property_size` function will return the `_size` field.

**Phase 2: Augment `properties.yaml` and `generate_property_headers.py`**

*   **Where:**
    *   `src/properties.yaml`
    *   `src/generate_property_headers.py`
*   **Why:** To declare which properties need custom output transformation and to auto-generate the dispatch mechanism.
*   **What:**
    1.  **Modify `properties.yaml`:**
        *   For any property requiring special output transformation, add a new optional field:
          `output_transformer_function: "C_FunctionName_For_Transform"`. Should be drawn from the relevant direct physics-specific logic in `prepare_galaxy_for_hdf5_output.c`.
        *   Example:
            ```yaml
            properties:
              - name: Cooling
                type: double # Internal type
                # ...
                output: true
                output_transformer_function: "transform_output_Cooling"

              - name: SfrDisk # This is the SCALAR output property
                type: float  # Output type
                # ...
                output: true
                output_transformer_function: "derive_output_SfrDisk"
            ```
    2.  **Modify `generate_property_headers.py`:**
        *   **New Output File:** The script should now generate a third file: `generated_output_transformers.c` (in `src/core/`).
        *   **Parse New Field:** Read the `output_transformer_function` field from the YAML.
        *   **In `generated_output_transformers.c`:**
            *   Generate `extern int FunctionName(...);` declarations for every unique function name found in `output_transformer_function` fields.
            *   Generate the `dispatch_property_transformer()` function:
                ```c
                // In src/core/generated_output_transformers.c

                // Extern declarations (auto-generated)
                extern int transform_output_Cooling(const struct GALAXY *galaxy, property_id_t output_prop_id, void *output_buffer_element_ptr, const struct params *run_params);
                extern int derive_output_SfrDisk(const struct GALAXY *galaxy, property_id_t output_prop_id, void *output_buffer_element_ptr, const struct params *run_params);
                // ... more externs

                /**
                 * @brief Dispatches to the appropriate output transformer or performs default identity transform.
                 * @param galaxy The galaxy being processed.
                 * @param output_prop_id Property ID of the field being written to HDF5.
                 * @param output_prop_name Name of the field being written to HDF5.
                 * @param output_buffer_element_ptr Pointer to the HDF5 buffer slot for this galaxy's property.
                 * @param run_params Global run parameters.
                 * @param h5_dtype HDF5 data type of the output field.
                 * @return 0 on success, non-zero on error.
                 */
                int dispatch_property_transformer(const struct GALAXY *galaxy,
                                                  property_id_t output_prop_id,
                                                  const char *output_prop_name,
                                                  void *output_buffer_element_ptr,
                                                  const struct params *run_params,
                                                  hid_t h5_dtype) {
                    int status = 0;

                    // Auto-generated if/else if chain based on output_prop_name
                    if (strcmp(output_prop_name, "Cooling") == 0) {
                        status = transform_output_Cooling(galaxy, output_prop_id, output_buffer_element_ptr, run_params);
                    } else if (strcmp(output_prop_name, "SfrDisk") == 0) {
                        status = derive_output_SfrDisk(galaxy, output_prop_id, output_buffer_element_ptr, run_params);
                    // ... more else if blocks for other transformed properties
                    } else {
                        // Default identity transformation for properties without a specific transformer
                        // This uses the output_prop_id to fetch the raw value.
                        if (h5_dtype == H5T_NATIVE_FLOAT) {
                            *((float*)output_buffer_element_ptr) = get_float_property(galaxy, output_prop_id, 0.0f);
                        } else if (h5_dtype == H5T_NATIVE_DOUBLE) {
                            *((double*)output_buffer_element_ptr) = get_double_property(galaxy, output_prop_id, 0.0);
                        } else if (h5_dtype == H5T_NATIVE_INT) {
                            *((int32_t*)output_buffer_element_ptr) = get_int32_property(galaxy, output_prop_id, 0);
                        } else if (h5_dtype == H5T_NATIVE_LLONG) {
                            // Assuming int64_t for H5T_NATIVE_LLONG output
                            *((int64_t*)output_buffer_element_ptr) = get_int64_property(galaxy, output_prop_id, 0LL);
                        } else {
                            LOG_ERROR("Unsupported HDF5 type for default identity transform of property '%s'", output_prop_name);
                            status = -1;
                        }
                    }
                    return status;
                }
                ```
        *   **In `core_properties.h` (generated):**
            *   Add a forward declaration for `dispatch_property_transformer()`.
                ```c
                // In core_properties.h
                int dispatch_property_transformer(const struct GALAXY *galaxy, property_id_t output_prop_id, const char *output_prop_name, void *output_buffer_element_ptr, const struct params *run_params, hid_t h5_dtype);
                ```

**Phase 3: Implement Transformer Functions in Physics Modules**

*   **Where:** Relevant `src/physics/` placeholder C files (e.g., `placeholder_cooling_module.c`, `placeholder_starformation_module.c`) or a new `src/physics/physics_output_transformers.c`.
*   **Why:** To encapsulate the physics-specific transformation logic.
*   **What:**
    1.  Implement the C functions named in the `output_transformer_function` fields in `properties.yaml`. Functions should be drawn from the relevant direct physics-specific logic in `prepare_galaxy_for_hdf5_output.c`.
    2.  **Signature:**
        `int C_FunctionName_For_Transform(const struct GALAXY *galaxy, property_id_t output_prop_id, void *output_buffer_element_ptr, const struct params *run_params);`
        *   `galaxy`: `const` pointer, do not modify its state.
        *   `output_prop_id`: The ID of the property *as it will appear in the output file*. This helps the transformer if its logic depends on which output field it's populating.
        *   `output_buffer_element_ptr`: A `void*` pointing to the exact memory location in the HDF5 output buffer (e.g., `save_info->property_buffers[snap_idx][i].data[gals_in_buffer]`) where the *single transformed value* should be written. The function must cast this pointer to the correct output type (e.g., `float*`, `double*`).
        *   `run_params`: For unit conversions or other global parameters.
        *   Return `0` for success, non-zero for error.
    3.  **Logic:**
        *   Use `get_cached_property_id()` to get IDs of any *source* physics properties needed for the transformation (e.g., `derive_output_SfrDisk` needs the ID of `SfrDiskHistoryArray`).
        *   Use generic property accessors (`get_float_property`, `get_int_property`, the new `get_float_array_element_property`, etc.) to read raw data from the `galaxy`.
        *   Perform calculations/conversions.
        *   Write the final, transformed scalar value to `*((float*)output_buffer_element_ptr) = transformed_value;` (adjust cast as needed).
    4.  Example for `transform_output_Cooling`:
        ```c
        // In e.g. src/physics/model_cooling_heating.c
        #include "core_properties.h" // For get_double_property, etc.
        #include "core_allvars.h"    // For params

        int transform_output_Cooling(const struct GALAXY *galaxy, property_id_t output_cooling_id, void *output_buffer_element_ptr, const struct params *run_params) {
            // output_cooling_id is PROP_Cooling (if Cooling is also the internal name)
            // Or, if internal name is different, use get_cached_property_id("InternalCoolingName")
            property_id_t internal_cooling_id = get_cached_property_id("Cooling"); // Assuming internal name is also "Cooling"
            if (internal_cooling_id == PROP_COUNT || !has_property(galaxy, internal_cooling_id)) {
                *((float*)output_buffer_element_ptr) = 0.0f; // Default if property missing
                return 0; // Or an error if it's unexpected
            }

            double raw_cooling = get_double_property(galaxy, internal_cooling_id, 0.0);
            float *output_val_ptr = (float*)output_buffer_element_ptr;

            if (raw_cooling > 0.0) {
                *output_val_ptr = (float)log10(raw_cooling * run_params->units.UnitEnergy_in_cgs / run_params->units.UnitTime_in_s);
            } else {
                *output_val_ptr = 0.0f; // Or appropriate default for log of non-positive
            }
            return 0;
        }
        ```

**Phase 4: Modify `prepare_galaxy_for_hdf5_output.c`**

*   **Where:** `src/io/prepare_galaxy_for_hdf5_output.c`
*   **Why:** To remove direct physics-specific logic and use the new dispatch mechanism.
*   **What:**
    1.  Include `core_properties.h` for the `dispatch_property_transformer` declaration.
    2.  Inside the loop that iterates through `save_info->num_properties`:
        *   The existing `if (buffer->is_core_prop)` block remains largely the same (direct `GALAXY_PROP_*` access into the buffer).
        *   The `else` block (for physics properties) is **completely replaced**.
        *   **New `else` block logic:**
            ```c
            // In src/io/prepare_galaxy_for_hdf5_output.c
            // ... inside loop over properties ...
            } else { // Handle physics properties
                struct property_buffer_info *buffer = &save_info->property_buffers[output_snap_idx][i];
                property_id_t output_prop_id = buffer->prop_id; // This is the ID of the property in the output file
                const char *output_prop_name = buffer->name;
                hid_t h5_dtype = buffer->h5_dtype;

                // Determine the element size for pointer arithmetic
                size_t element_size = 0;
                if (h5_dtype == H5T_NATIVE_FLOAT) element_size = sizeof(float);
                else if (h5_dtype == H5T_NATIVE_DOUBLE) element_size = sizeof(double);
                else if (h5_dtype == H5T_NATIVE_INT) element_size = sizeof(int32_t);
                else if (h5_dtype == H5T_NATIVE_LLONG) element_size = sizeof(int64_t);
                // Add other types if necessary

                if (element_size == 0) {
                    LOG_ERROR("Unsupported HDF5 type for property '%s' in prepare_galaxy_for_hdf5_output", output_prop_name);
                    return EXIT_FAILURE;
                }

                void *output_buffer_element_ptr = ((char*)buffer->data) + (gals_in_buffer * element_size);

                int transform_status = dispatch_property_transformer(g,
                                                                     output_prop_id,
                                                                     output_prop_name,
                                                                     output_buffer_element_ptr,
                                                                     run_params,
                                                                     h5_dtype);
                if (transform_status != 0) {
                    LOG_ERROR("Error transforming property '%s' (ID: %d) for output.", output_prop_name, output_prop_id);
                    // Decide on error handling: continue with default, or return failure?
                    // For now, let's assume dispatch_property_transformer handles defaults on error.
                    // If critical, return EXIT_FAILURE;
                }
            }
            // ...
            ```
    3.  Remove the old `if/else if` chain for "Cooling", "SfrDisk", etc.
    4.  Remove the placeholder `get_float_array_property` function from this file (it should be in `core_property_utils.c`).

**Phase 5: Update Makefile**

*   **Where:** `Makefile` (or `Makefile.txt`)
*   **Why:** To ensure `generated_output_transformers.c` is created and compiled.
*   **What:**
    1.  Add `generated_output_transformers.c` to the list of generated files and source files to be compiled.
        ```makefile
        # Example:
        GENERATED_C_FILES := core/core_properties.c core/generated_output_transformers.c
        GENERATED_H_FILES := core/core_properties.h
        GENERATED_FILES := $(addprefix $(SRC_PREFIX)/, $(GENERATED_H_FILES)) $(addprefix $(SRC_PREFIX)/, $(GENERATED_C_FILES))

        CORE_SRC := core/sage.c ... core/core_properties.c core/generated_output_transformers.c ...
        ```
    2.  Ensure the rule that runs `generate_property_headers.py` (e.g., for `generate_properties.stamp`) correctly lists `$(SRC_PREFIX)/core/generated_output_transformers.c` as an output, so it's rebuilt when `properties.yaml` or the script changes.
        ```makefile
        $(SRC_PREFIX)/core/core_properties.h $(SRC_PREFIX)/core/core_properties.c $(SRC_PREFIX)/core/generated_output_transformers.c: $(ROOT_DIR)/.stamps/generate_properties.stamp
        	@# This rule body might be empty if the stamp file rule does the generation.
        	@# Just ensure these files are listed as targets of the stamp file.

        $(ROOT_DIR)/.stamps/generate_properties.stamp: $(SRC_PREFIX)/$(PROPERTIES_FILE) $(SRC_PREFIX)/generate_property_headers.py
        	@echo "Generating property headers and transformers from $(PROPERTIES_FILE)..."
        	cd $(SRC_PREFIX) && python3 generate_property_headers.py --input $(notdir $(PROPERTIES_FILE))
        	@mkdir -p $(dir $@)
        	@touch $@
        ```

**Phase 6: Testing**

*   **Where:** `tests/`
*   **Why:** To verify the new system works correctly and produces the same (or intentionally different and correct) output.
*   **What:**
    1.  Update `tests/test_property_system_hdf5.c` (or create new tests) to:
        *   Define mock galaxies with properties that require transformation (e.g., "Cooling" with a known raw value, an SFH array).
        *   Define mock transformer C functions (these would normally be in physics modules, but for a unit test, they can be local).
        *   Call the HDF5 saving routines.
        *   Read the HDF5 file back and assert that the output values are correctly transformed (e.g., "Cooling" is log10-scaled and unit-converted, scalar "SfrDisk" is the correct sum/average).
    2.  Run existing end-to-end tests and compare output with a reference. If transformations change output values (e.g., fixing a bug or changing a unit), the reference output may need updating.

This detailed plan should provide a clear path for the junior developer to implement the refined Output Transformers Implementation Plan, achieving a clean separation of concerns for output property transformations.
