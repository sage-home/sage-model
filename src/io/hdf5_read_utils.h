#pragma once

#include <hdf5.h>

#ifdef __cplusplus
extern "C" {
#endif

    extern herr_t read_attribute(hid_t fd, const char *group_name, const char *attr_name, void *attribute, const size_t dst_size);
    extern herr_t read_dataset(hid_t fd, const char *dataset_name, hid_t dataset_id, void *buffer);

#ifdef __cplusplus
}
#endif
