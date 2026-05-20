/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "esp_err.h"

#include "esp_media_db_types.h"
#include "epl_db_storage_ops.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef int epl_media_db_id_t;         /*!< Unique identifier type for media database rows and keys */
typedef uint32_t epl_media_db_size_t;  /*!< Value size type */

#define EPL_MEDIA_DB_INVALID_ID  ESP_MEDIA_INVALID_ID  /*!< Invalid row or key ID */

typedef enum {
    EPL_MEDIA_DB_FIELD_TYPE_INVALID   = 0,  /*!< Invalid field type */
    EPL_MEDIA_DB_FIELD_TYPE_INT       = 1,  /*!< Integer field type */
    EPL_MEDIA_DB_FIELD_TYPE_FLOAT     = 2,  /*!< Floating point field type */
    EPL_MEDIA_DB_FIELD_TYPE_STRING    = 3,  /*!< String field type */
    EPL_MEDIA_DB_FIELD_TYPE_BOOL      = 4,  /*!< Boolean field type */
    EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY = 5,  /*!< Integer array field type */
} epl_media_db_field_type_t;

typedef struct {
    epl_media_db_field_type_t  type;  /*!< Field type of the stored value */
    union {
        int    intv;            /*!< Integer value */
        float  floatv;          /*!< Float value */
        char  *strv;            /*!< String value */
        bool   boolv;           /*!< Boolean value */
        int   *intarrv;         /*!< Integer array value */
    } value;                    /*!< Union containing the actual value */
    epl_media_db_size_t  size;  /*!< Size of the value for arrays and strings */
} epl_media_db_cell_t;

typedef uint32_t epl_media_db_magic_t;         /*!< Magic number type for file validation */
typedef uint32_t epl_media_db_version_t;       /*!< Database version type */
typedef uint32_t epl_media_db_key_len_t;       /*!< Key length type */
typedef uint8_t epl_media_db_used_t;           /*!< Usage flag type */
typedef uint32_t epl_media_db_value_offset_t;  /*!< Value offset type for data file positioning */

/**
 * @brief  Database configuration structure
 *
 *         Configuration parameters for database initialization.
 */
typedef struct {
    const char                 *table_filename;   /*!< Name of the table metadata file */
    const char                 *data_filename;    /*!< Name of the data content file */
    const char                 *offset_filename;  /*!< Name of the offset index file */
    const esp_db_storage_ops_t *storage_ops;      /*!< Storage backend operations */
    void                       *storage_ctx;      /*!< Storage backend context */
} epl_media_db_cfg_t;

/**
 * @brief  Database key configuration structure
 *
 *         Defines the schema of key columns for the database.
 */
typedef struct {
    uint32_t                         key_count;  /*!< Number of key columns */
    const char                     **key_names;  /*!< Array of key column names */
    const epl_media_db_field_type_t *key_types;  /*!< Array of key column data types */
} epl_media_db_key_cfg_t;

/**
 * @enum epl_media_db_err_t
 * @brief  Database operation result codes
 */
typedef enum {
    EPL_MEDIA_DB_OK              = ESP_OK,               /*!< Operation completed successfully */
    EPL_MEDIA_DB_ERR_FAIL        = ESP_FAIL,             /*!< General error occurred */
    EPL_MEDIA_DB_ERR_NOT_FOUND   = ESP_ERR_NOT_FOUND,    /*!< Requested item not found */
    EPL_MEDIA_DB_ERR_INVALID_ARG = ESP_ERR_INVALID_ARG,  /*!< Invalid parameter provided */
    EPL_MEDIA_DB_ERR_NO_MEM      = ESP_ERR_NO_MEM,       /*!< Insufficient memory for operation */
} epl_media_db_err_t;

typedef struct epl_media_db_t *epl_media_db_handle_t;

/**
 * @brief  Initialize a database cell with specified type and value
 *
 *         Creates and initializes a database cell with the provided data.
 *
 * @param[in]  type     Value type for the cell
 * @param[in]  value_p  Pointer to the value data
 * @param[in]  size     Size of the value data
 *
 * @return
 *       - Initialized  database cell
 */
epl_media_db_cell_t epl_media_db_cell_init(epl_media_db_field_type_t type, const void *value_p, epl_media_db_size_t size);

/**
 * @brief  Create an invalid database cell
 *
 *         Creates a cell marked as invalid for error handling purposes.
 *
 * @return
 *       - Invalid  database cell
 */
epl_media_db_cell_t epl_media_db_cell_invalid();

/**
 * @brief  Free memory allocated for a database cell
 *
 *         Properly releases all memory associated with a database cell.
 *
 * @param[in]  cell  Pointer to the cell to free
 */
void epl_media_db_cell_free(epl_media_db_cell_t *cell);

/**
 * @brief  Print the contents of a database cell
 *
 *         Outputs the cell contents to the console for debugging purposes.
 *
 * @param[in]  cell  Database cell to print
 */
void epl_media_db_cell_print(epl_media_db_cell_t cell);

/**
 * @brief  Initialize a database instance
 *
 *         Creates and initializes a database instance with the specified configuration.
 *         Sets up file system operations and internal data structures.
 *
 * @param[in]   db_cfg     Pointer to database configuration structure
 * @param[out]  db_handle  Pointer to store the created database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid configuration parameters
 *       - EPL_MEDIA_DB_ERR_NO_MEM       Memory allocation failed
 */
epl_media_db_err_t epl_media_db_init(const epl_media_db_cfg_t *db_cfg, epl_media_db_handle_t *db_handle);

/**
 * @brief  Check if database files exist
 *
 *         Verifies whether the database files exist in the file system.
 *
 * @param[in]  db_handle  Database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK             Database files exist
 *       - EPL_MEDIA_DB_ERR_NOT_FOUND  Database files do not exist
 *       - EPL_MEDIA_DB_ERR_FAIL       Check operation failed
 */
epl_media_db_err_t epl_media_db_exists(epl_media_db_handle_t db_handle);

/**
 * @brief  Create a new database with specified key schema
 *
 *         Creates new database files and initializes them with the provided key configuration.
 *
 * @param[in]  db_handle  Pointer to database handle
 * @param[in]  key_cfg    Key schema configuration
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NO_MEM       Memory allocation failed
 *       - EPL_MEDIA_DB_ERR_FAIL         Creation failed
 */
epl_media_db_err_t epl_media_db_create(epl_media_db_handle_t *db_handle, const epl_media_db_key_cfg_t *key_cfg);

/**
 * @brief  Load existing database from storage
 *
 *         Loads database structure and metadata from existing files.
 *
 * @param[in]  db_handle  Pointer to database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK             Success
 *       - EPL_MEDIA_DB_ERR_NOT_FOUND  Database files not found
 *       - EPL_MEDIA_DB_ERR_FAIL       Load operation failed
 *       - EPL_MEDIA_DB_ERR_NO_MEM     Memory allocation failed
 */
epl_media_db_err_t epl_media_db_load(epl_media_db_handle_t *db_handle);

/**
 * @brief  Save database with compression and optimization
 *
 *         Saves the database to storage while removing unused space and optimizing structure.
 *
 * @param[in]  db_handle  Database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid handle
 *       - EPL_MEDIA_DB_ERR_FAIL         Save operation failed
 */
epl_media_db_err_t epl_media_db_save_squeeze(epl_media_db_handle_t db_handle);

/**
 * @brief  Save database to storage
 *
 *         Persists all database changes to the file system.
 *
 * @param[in]  db_handle  Database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid handle
 *       - EPL_MEDIA_DB_ERR_FAIL         Save operation failed
 */
epl_media_db_err_t epl_media_db_save(epl_media_db_handle_t db_handle);

/**
 * @brief  Flush and close temporary database files
 *
 *         Synchronizes and closes open temporary file handles while preserving
 *         the database structure and temporary files.
 *
 * @param[in]  db_handle  Database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid handle
 */
epl_media_db_err_t epl_media_db_flush(epl_media_db_handle_t db_handle);

/**
 * @brief  Close database instance
 *
 *         Closes all open file handles but preserves the database structure.
 *
 * @param[in]  db_handle  Database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid handle
 */
epl_media_db_err_t epl_media_db_close(epl_media_db_handle_t db_handle);

/**
 * @brief  Delete database files from storage
 *
 *         Permanently removes all database files from the file system.
 *
 * @param[in]  db_handle  Database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid handle
 *       - EPL_MEDIA_DB_ERR_FAIL         Delete operation failed
 */
epl_media_db_err_t epl_media_db_delete(epl_media_db_handle_t db_handle);

/**
 * @brief  Deinitialize and cleanup database instance
 *
 *         Releases all resources and frees memory associated with the database.
 *
 * @param[in]  db_handle  Database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid handle
 */
epl_media_db_err_t epl_media_db_deinit(epl_media_db_handle_t db_handle);

/**
 * @brief  Add a new row with specified key-value pairs
 *
 *         Creates a new row in the database with the provided key-value data.
 *
 * @param[in]  db_handle  Database handle
 * @param[in]  key        Array of key names
 * @param[in]  cell       Array of corresponding cell values
 * @param[in]  cnt        Number of key-value pairs
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NO_MEM       Memory allocation failed
 *       - EPL_MEDIA_DB_ERR_FAIL         Addition failed
 */
epl_media_db_err_t epl_media_db_add_row(epl_media_db_handle_t db_handle, const char *key[], epl_media_db_cell_t cell[], epl_media_db_id_t cnt);

/**
 * @brief  Update an existing row with new values
 *
 *         Updates the specified row with new key-value pairs.
 *
 * @param[in]  db_handle  Database handle
 * @param[in]  row_id     Unique identifier of the row to update
 * @param[in]  key        Array of key names to update
 * @param[in]  cell       Array of new cell values
 * @param[in]  cnt        Number of key-value pairs to update
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NOT_FOUND    Row not found
 *       - EPL_MEDIA_DB_ERR_FAIL         Update failed
 */
epl_media_db_err_t epl_media_db_update_row(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id, const char *key[], epl_media_db_cell_t cell[], epl_media_db_id_t cnt);

/**
 * @brief  Add an empty row to the database
 *
 *         Creates a new empty row and returns its ID for subsequent population.
 *
 * @param[in]   db_handle  Database handle
 * @param[out]  row_id     Pointer to store the new row ID
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NO_MEM       Memory allocation failed
 *       - EPL_MEDIA_DB_ERR_FAIL         Addition failed
 */
epl_media_db_err_t epl_media_db_add_empty_row(epl_media_db_handle_t db_handle, epl_media_db_id_t *row_id);

/**
 * @brief  Update a single cell in a row
 *
 *         Updates one specific cell in the specified row.
 *
 * @param[in]  db_handle  Database handle
 * @param[in]  row_id     Unique identifier of the row
 * @param[in]  key        Name of the key to update
 * @param[in]  cell       New cell value
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NOT_FOUND    Row or key not found
 *       - EPL_MEDIA_DB_ERR_FAIL         Update failed
 */
epl_media_db_err_t epl_media_db_update_cell(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id, const char *key, epl_media_db_cell_t cell);

/**
 * @brief  Delete a row from the database
 *
 *         Removes the specified row and all its associated data.
 *
 * @param[in]  db_handle  Database handle
 * @param[in]  row_id     Unique identifier of the row to delete
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NOT_FOUND    Row not found
 *       - EPL_MEDIA_DB_ERR_FAIL         Deletion failed
 */
epl_media_db_err_t epl_media_db_delete_row(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id);

/**
 * @brief  Check if a row exists in the database
 *
 *         Verifies whether a row with the specified ID exists.
 *
 * @param[in]   db_handle  Database handle
 * @param[in]   row_id     Row ID to check
 * @param[out]  is_exist   Pointer to store the existence result
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 */
epl_media_db_err_t epl_media_db_row_exists(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id, bool *is_exist);

/**
 * @brief  Get a specific cell value from a row
 *
 *         Retrieves the value of a specific key from the specified row.
 *
 * @param[in]   db_handle  Database handle
 * @param[in]   row_id     Unique identifier of the row
 * @param[in]   key        Name of the key to retrieve
 * @param[out]  cell       Pointer to store the retrieved cell value
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NOT_FOUND    Row or key not found
 */
epl_media_db_err_t epl_media_db_get_cell(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id, const char *key, epl_media_db_cell_t *cell);

/**
 * @brief  Find the first row matching a key-value condition
 *
 *         Searches for the first row where the specified key matches the given cell value.
 *
 * @param[in]   db_handle  Database handle
 * @param[in]   key        Name of the key to match
 * @param[in]   cell       Cell value to match against
 * @param[out]  row_id     Pointer to store the found row ID
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success, matching row found
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NOT_FOUND    No matching row found
 */
epl_media_db_err_t epl_media_db_match_first_row(epl_media_db_handle_t db_handle, const char *key, epl_media_db_cell_t cell, epl_media_db_id_t *row_id);

/**
 * @brief  Get all row IDs in the database
 *
 *         Retrieves an array of all valid row IDs currently in the database.
 *
 * @param[in]   db_handle  Database handle
 * @param[out]  row_ids    Pointer to store the allocated row ID array (caller must free)
 * @param[out]  row_count  Pointer to store the number of row IDs
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NO_MEM       Memory allocation failed
 */
epl_media_db_err_t epl_media_db_get_all_row_ids(epl_media_db_handle_t db_handle, epl_media_db_id_t **row_ids, epl_media_db_id_t *row_count);

/**
 * @brief  Get the total number of rows in the database
 *
 *         Returns the count of all rows, including both active and deleted rows.
 *
 * @param[in]   db_handle  Database handle
 * @param[out]  count      Pointer to store the row count
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 */
epl_media_db_err_t epl_media_db_get_row_count(epl_media_db_handle_t db_handle, int *count);

/**
 * @brief  Get the data type of a specific key
 *
 *         Retrieves the data type information for the specified key column.
 *
 * @param[in]   db_handle  Database handle
 * @param[in]   key        Name of the key to query
 * @param[out]  key_type   Pointer to store the key data type
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NOT_FOUND    Key not found
 */
epl_media_db_err_t epl_media_db_get_key_type(epl_media_db_handle_t db_handle, const char *key, epl_media_db_field_type_t *key_type);

/**
 * @brief  Get the internal ID of a key column
 *
 *         Retrieves the internal column ID for the specified key name.
 *
 * @param[in]   db_handle  Database handle
 * @param[in]   key        Name of the key to query
 * @param[out]  key_id     Pointer to store the key ID
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NOT_FOUND    Key not found
 */
epl_media_db_err_t epl_media_db_get_key_id(epl_media_db_handle_t db_handle, const char *key, epl_media_db_id_t *key_id);

/**
 * @brief  Get the sizes of all database files
 *
 *         Retrieves the current file sizes for all three database files.
 *
 * @param[in]   db_handle    Database handle
 * @param[out]  size_table   Pointer to store table file size
 * @param[out]  size_offset  Pointer to store offset file size
 * @param[out]  size_data    Pointer to store data file size
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_FAIL         File size query failed
 */
epl_media_db_err_t epl_media_db_get_file_size(epl_media_db_handle_t db_handle, off_t *size_table, off_t *size_offset, off_t *size_data);

/**
 * @brief  Display all key names and types
 *
 *         Prints the schema information including all key names and their data types.
 *
 * @param[in]  db_handle  Database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid handle
 */
epl_media_db_err_t epl_media_db_debug_show_keys(epl_media_db_handle_t db_handle);

/**
 * @brief  Display the contents of a specific row
 *
 *         Prints all key-value pairs for the specified row.
 *
 * @param[in]  db_handle  Database handle
 * @param[in]  row_id     Row ID to display
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 *       - EPL_MEDIA_DB_ERR_NOT_FOUND    Row not found
 */
epl_media_db_err_t epl_media_db_debug_show_row(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id);

/**
 * @brief  Display all rows in the database
 *
 *         Prints the contents of all rows in the database.
 *
 * @param[in]  db_handle  Database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid handle
 */
epl_media_db_err_t epl_media_db_debug_show_all_rows(epl_media_db_handle_t db_handle);

/**
 * @brief  Display all rows with specific keys only
 *
 *         Prints only the specified keys for all rows in the database.
 *
 * @param[in]  db_handle  Database handle
 * @param[in]  keys       Array of key names to display
 * @param[in]  key_num    Number of keys in the array
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid parameters
 */
epl_media_db_err_t epl_media_db_debug_show_all_rows_keys(epl_media_db_handle_t db_handle, const char **keys, epl_media_db_id_t key_num);

/**
 * @brief  Parse and display table file structure
 *
 *         Analyzes and displays the internal structure of the table metadata file.
 *
 * @param[in]  db_handle  Database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid handle
 *       - EPL_MEDIA_DB_ERR_FAIL         Parse operation failed
 */
epl_media_db_err_t epl_media_db_debug_table_parser(epl_media_db_handle_t db_handle);

/**
 * @brief  Parse and display data file structure
 *
 *         Analyzes and displays the internal structure of the data content file.
 *
 * @param[in]  db_handle  Database handle
 *
 * @return
 *       - EPL_MEDIA_DB_OK               Success
 *       - EPL_MEDIA_DB_ERR_INVALID_ARG  Invalid handle
 *       - EPL_MEDIA_DB_ERR_FAIL         Parse operation failed
 */
epl_media_db_err_t epl_media_db_debug_data_parser(epl_media_db_handle_t db_handle);

/**
 * @brief  Print raw contents of table file
 *
 *         Outputs the raw binary contents of the table metadata file for debugging.
 *
 * @param[in]  db_handle  Database handle
 */
void epl_media_db_debug_print_table_file(epl_media_db_handle_t db_handle);

/**
 * @brief  Print raw contents of offset file
 *
 *         Outputs the raw binary contents of the offset index file for debugging.
 *
 * @param[in]  db_handle  Database handle
 */
void epl_media_db_debug_print_offset_file(epl_media_db_handle_t db_handle);

/**
 * @brief  Print raw contents of data file
 *
 *         Outputs the raw binary contents of the data content file for debugging.
 *
 * @param[in]  db_handle  Database handle
 */
void epl_media_db_debug_print_data_file(epl_media_db_handle_t db_handle);

/**
 * @brief  Convert database error code to string
 *
 *         Provides human-readable error messages for debugging and logging.
 */
static inline const char *epl_media_db_err_to_str(epl_media_db_err_t err)
{
    switch (err) {
        case EPL_MEDIA_DB_OK:
            return "EPL_MEDIA_DB_OK";
        case EPL_MEDIA_DB_ERR_FAIL:
            return "EPL_MEDIA_DB_ERR_FAIL";
        case EPL_MEDIA_DB_ERR_NOT_FOUND:
            return "EPL_MEDIA_DB_ERR_NOT_FOUND";
        case EPL_MEDIA_DB_ERR_INVALID_ARG:
            return "EPL_MEDIA_DB_ERR_INVALID_ARG";
        case EPL_MEDIA_DB_ERR_NO_MEM:
            return "EPL_MEDIA_DB_ERR_NO_MEM";
        default:
            return "UNKNOWN_EPL_MEDIA_DB_ERR";
    }
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
