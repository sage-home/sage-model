#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct buffered_io{
    size_t bytes_allocated;
    size_t bytes_stored;
    int file_descriptor;
    off_t current_offset;
    void *buffer;
};

    extern int setup_buffered_io(struct buffered_io *buf_io, const size_t buffer_size, int output_fd, const off_t start_offset);
    extern int write_buffered_io(struct buffered_io *buf_io, const void *src, size_t num_bytes_to_write);
    extern int cleanup_buffered_io(struct buffered_io *buf_io);

#ifdef __cplusplus
}
#endif
