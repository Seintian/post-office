#ifndef POSTOFFICE_UTILS_FILES_H
#define POSTOFFICE_UTILS_FILES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

/** \ingroup utils */

/**
 * @file files.h
 * @brief A collection of general-purpose filesystem utilities.
 *
 * This module provides a robust and convenient interface for common filesystem
 * operations, such as checking file properties, reading/writing files, and
 * managing directories. All functions are prefixed with `fs_`.
 */

/**
 * @brief Checks if a file or directory exists at the given path.
 *
 * @param[in] path The path to check.
 * @return `true` if a file or directory exists at the path, `false` otherwise.
 */
bool fs_exists(const char *path);

/**
 * @brief Checks if the given path points to a regular file.
 *
 * @param[in] path The path to check.
 * @return `true` if the path exists and is a regular file, `false` otherwise.
 */
bool fs_is_regular_file(const char *path);

/**
 * @brief Checks if the given path points to a directory.
 *
 * @param[in] path The path to check.
 * @return `true` if the path exists and is a directory, `false` otherwise.
 */
bool fs_is_directory(const char *path);

/**
 * @brief Checks if the given path points to a domain socket.
 *
 * This is useful for cleaning up leftover socket files from previous runs.
 *
 * @param[in] path The path to check.
 * @return `true` if the path exists and is a socket, `false` otherwise.
 */
bool fs_is_socket(const char *path);

/**
 * @brief Reads an entire file into a newly allocated buffer.
 *
 * On success, the function allocates a buffer large enough to hold the file
 * contents plus a null terminator. The caller is responsible for freeing this
 * buffer with `free()`.
 *
 * @param[in] path The path to the file to read.
 * @param[out] out_size A pointer to a size_t variable where the size of the
 * file content (excluding the null terminator) will be stored.
 * @return A pointer to the allocated buffer on success, or `NULL` on failure
 * (e.g., file not found, cannot read, out of memory). If `NULL` is
 * returned, `out_size` is set to 0.
 */
char *fs_read_file_to_buffer(const char *path, size_t *out_size);

/**
 * @brief Writes the contents of a buffer to a file.
 *
 * This function will create the file if it doesn't exist or overwrite it
 * if it does.
 *
 * @param[in] path The path to the file to write.
 * @param[in] buffer A pointer to the data to write.
 * @param[in] size The number of bytes to write from the buffer.
 * @return `true` on success, `false` on failure. `errno` is set accordingly.
 */
bool fs_write_buffer_to_file(const char *path, const void *buffer, size_t size);

/**
 * @brief Creates a directory, including any necessary parent directories.
 *
 * This function is similar to the `mkdir -p` command.
 *
 * @param[in] path The full path of the directory to create.
 * @param[in] mode The file mode (permissions) for the new directory, e.g., `0755`.
 * @return `true` on success or if the directory already exists. `false` on
 * any other error.
 */
bool fs_create_directory_recursive(const char *path, mode_t mode);

/**
 * @brief Joins two path components together with a single directory separator.
 *
 * Allocates a new string for the combined path. The caller is responsible for
 * freeing this string with `free()`. Handles cases where the base path may or
 * may not have a trailing slash.
 *
 * @param[in] base The base path (e.g., "/var/log").
 * @param[in] leaf The component to append (e.g., "myapp.log").
 * @return A pointer to the newly allocated full path string, or `NULL` on
 * a memory allocation failure.
 */
char *fs_path_join(const char *base, const char *leaf);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // POSTOFFICE_UTILS_FILES_H