#pragma once

#include <stdint.h>
#include <hdf5.h>

#ifdef __cplusplus
extern "C" {
#endif

    extern herr_t read_attribute(hid_t fd, const char *group_name, const char *attr_name, const hid_t attr_dtype, void *attribute);
    extern herr_t read_dataset(hid_t fd, const char *dataset_name, hid_t dataset_id, const hid_t datatype, void *buffer);

#ifdef __cplusplus
}
#endif
