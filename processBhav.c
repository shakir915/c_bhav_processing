#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <minizip/unzip.h>
#include <float.h>
#include <ctype.h>   // For isspace
#include <stdbool.h> // For bool type
#include <limits.h>  // For LONG_MIN
#include <stdint.h> // For uint32_t

#define INITIAL_CAPACITY 100 // Initial capacity for dynamic arrays
#define LOG_ENABLED false

clock_t lastTime; // Declare globally, initialize in main

void printTimeSpent(char *tag) {
    // Check if tag is not NULL and not an empty string
    if (tag != NULL && *tag != '\0') {
        double time_spent = (double) (clock() - lastTime) / CLOCKS_PER_SEC;
        printf("🕒 %s: %.3f seconds\n", tag, time_spent);
    }
    lastTime = clock();
}


// Dynamic array for floats
typedef struct {
    float *data;
    size_t count;
    size_t capacity;
} FloatArray;

// Dynamic array for long ints
typedef struct {
    long int *data;
    size_t count;
    size_t capacity;
} LongArray;

// --- FloatArray Helper Functions ---
void init_float_array(FloatArray *arr) {
    arr->data = malloc(INITIAL_CAPACITY * sizeof(float));
    if (!arr->data) {
        perror("❌ Failed to allocate memory for FloatArray");
        // In a real app, might want to exit or have more robust error handling
        arr->count = 0;
        arr->capacity = 0;
        return;
    }
    arr->count = 0;
    arr->capacity = INITIAL_CAPACITY;
}

void add_to_float_array(FloatArray *arr, float value) {
    if (arr->capacity == 0) return; // Not initialized
    if (arr->count >= arr->capacity) {
        arr->capacity *= 2;
        float *temp = realloc(arr->data, arr->capacity * sizeof(float));
        if (!temp) {
            perror("❌ Failed to reallocate memory for FloatArray");
            // Data loss or handle error
            return;
        }
        arr->data = temp;
    }
    arr->data[arr->count++] = value;
}

void free_float_array(FloatArray *arr) {
    free(arr->data);
    arr->data = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

// --- LongArray Helper Functions ---
void init_long_array(LongArray *arr) {
    arr->data = malloc(INITIAL_CAPACITY * sizeof(long int));
    if (!arr->data) {
        perror("❌ Failed to allocate memory for LongArray");
        arr->count = 0;
        arr->capacity = 0;
        return;
    }
    arr->count = 0;
    arr->capacity = INITIAL_CAPACITY;
}

void add_to_long_array(LongArray *arr, long int value) {
    if (arr->capacity == 0) return; // Not initialized
    if (arr->count >= arr->capacity) {
        arr->capacity *= 2;
        long int *temp = realloc(arr->data, arr->capacity * sizeof(long int));
        if (!temp) {
            perror("❌ Failed to reallocate memory for LongArray");
            // Data loss or handle error
            return;
        }
        arr->data = temp;
    }
    arr->data[arr->count++] = value;
}

void free_long_array(LongArray *arr) {
    free(arr->data);
    arr->data = NULL;
    arr->count = 0;
    arr->capacity = 0;
}


// Helper function to find and parse a numeric array of LONG INTs into a LongArray
// Returns: 1 if successful and values found, 0 if key not found/empty array, -1 on error
static int extract_long_array_to_memory(const char *content, const char *array_key_prefix, LongArray *output_array) {
    const char *p = strstr(content, array_key_prefix);
    if (!p) {
        return 0; // Key not found
    }
    p += strlen(array_key_prefix); // Move past the key and '['

    const char *end_of_array = strchr(p, ']');
    if (!end_of_array) {
        return -1; // Malformed JSON part
    }

    int values_found_in_array = 0;
    char *current_pos = (char *) p;

    while (current_pos < end_of_array && *current_pos != '\0') {
        char *next_val_ptr;
        long int value = strtol(current_pos, &next_val_ptr, 10);

        if (current_pos == next_val_ptr) {
            if (*current_pos == ',' || isspace((unsigned char) *current_pos)) {
                current_pos++;
                continue;
            }
            break;
        }
        add_to_long_array(output_array, value);
        values_found_in_array++;

        current_pos = next_val_ptr;
        while (current_pos < end_of_array && (*current_pos == ',' || isspace((unsigned char) *current_pos))) {
            current_pos++;
        }
        if (current_pos >= end_of_array || *current_pos == ']') {
            break;
        }
    }
    return values_found_in_array > 0 ? 1 : 0;
}

// Helper function to find and parse a numeric array of FLOATs into a FloatArray
// Returns: 1 if successful and values found, 0 if key not found/empty array, -1 on error
static int extract_float_array_to_memory(const char *content, const char *array_key_prefix, FloatArray *output_array) {
    const char *p = strstr(content, array_key_prefix);
    if (!p) {
        return 0;
    }
    p += strlen(array_key_prefix);

    const char *end_of_array = strchr(p, ']');
    if (!end_of_array) {
        return -1;
    }

    int values_found_in_array = 0;
    char *current_pos = (char *) p;

    while (current_pos < end_of_array && *current_pos != '\0') {
        char *next_val_ptr;
        float value = strtof(current_pos, &next_val_ptr);

        if (current_pos == next_val_ptr) {
            if (*current_pos == ',' || isspace((unsigned char) *current_pos)) {
                current_pos++;
                continue;
            }
            break;
        }
        add_to_float_array(output_array, value);
        values_found_in_array++;

        current_pos = next_val_ptr;
        while (current_pos < end_of_array && (*current_pos == ',' || isspace((unsigned char) *current_pos))) {
            current_pos++;
        }
        if (current_pos >= end_of_array || *current_pos == ']') {
            break;
        }
    }
    return values_found_in_array > 0 ? 1 : 0;
}


void process_json(const char *content, const char *filename) {
#define NUM_FLOAT_KEYS 4
#define NUM_LONG_KEYS 2
#define TOTAL_KEYS (NUM_FLOAT_KEYS + NUM_LONG_KEYS)

    const char *keys_prefix[TOTAL_KEYS] = {
        "\"o\":[", "\"h\":[", "\"l\":[", "\"c\":[", // Floats
        "\"t\":[", "\"v\":[" // Longs
    };
    const char *key_names[TOTAL_KEYS] = {
        "Open", "High", "Low", "Close", // Floats
        "Timestamp", "Volume" // Longs
    };

    FloatArray float_data_arrays[NUM_FLOAT_KEYS];
    LongArray long_data_arrays[NUM_LONG_KEYS];

    for (int i = 0; i < NUM_FLOAT_KEYS; ++i) {
        init_float_array(&float_data_arrays[i]);
    }
    for (int i = 0; i < NUM_LONG_KEYS; ++i) {
        init_long_array(&long_data_arrays[i]);
    }

    bool any_data_extracted_to_memory = false;
    bool error_occurred = false;
    size_t expected_count = 0; // Will store the common array size

    for (int i = 0; i < TOTAL_KEYS; ++i) {
        int result;
        if (i < NUM_FLOAT_KEYS) {
            // Processing float keys (o, h, l, c)
            result = extract_float_array_to_memory(content, keys_prefix[i], &float_data_arrays[i]);
        } else {
            // Processing long int keys (t, v)
            result = extract_long_array_to_memory(content, keys_prefix[i], &long_data_arrays[i - NUM_FLOAT_KEYS]);
        }

        if (result == 1) {
            any_data_extracted_to_memory = true;
        } else if (result == -1) {
            fprintf(stderr, "❌ Error parsing data for key %s in %s\n", key_names[i], filename);
            error_occurred = true;
            break; // Stop processing this JSON if parsing error
        }
    }

    if (error_occurred) {
        goto cleanup;
    }

    if (any_data_extracted_to_memory) {
        // Check for consistent array sizes
        bool first_populated_array_found = false;
        bool size_mismatch = false;

        for (int i = 0; i < NUM_FLOAT_KEYS; ++i) {
            if (float_data_arrays[i].count > 0) {
                if (!first_populated_array_found) {
                    expected_count = float_data_arrays[i].count;
                    first_populated_array_found = true;
                } else if (float_data_arrays[i].count != expected_count) {
                    size_mismatch = true;
                    break;
                }
            }
        }

        if (!size_mismatch) {
            for (int i = 0; i < NUM_LONG_KEYS; ++i) {
                if (long_data_arrays[i].count > 0) {
                    if (!first_populated_array_found) {
                        expected_count = long_data_arrays[i].count;
                        first_populated_array_found = true;
                    } else if (long_data_arrays[i].count != expected_count) {
                        size_mismatch = true;
                        break;
                    }
                }
            }
        }

        // If no data was extracted at all, expected_count will be 0, and first_populated_array_found will be false.
        // In this case, we don't want to write anything for this scrip.
        if (!first_populated_array_found) {
            // No data to write
            any_data_extracted_to_memory = false; // Ensure we skip writing block
        } else if (size_mismatch) {
            // Data found, but sizes inconsistent
            fprintf(
                stderr,
                "❌ Error: Data array size mismatch for scrip %s. Expected %zu elements for all populated fields.\n",
                filename, expected_count);
            for (int i = 0; i < NUM_FLOAT_KEYS; ++i) {
                if (float_data_arrays[i].count > 0)
                    fprintf(stderr, "  %s: %zu\n", key_names[i],
                            float_data_arrays[i].count);
            }
            for (int i = 0; i < NUM_LONG_KEYS; ++i) {
                if (long_data_arrays[i].count > 0)
                    fprintf(stderr, "  %s: %zu\n", key_names[NUM_FLOAT_KEYS + i],
                            long_data_arrays[i].count);
            }
            error_occurred = true;
        }


        // Extract scrip name and check length
        char scrip_name[101]; // Max 100 chars + null terminator
        const char *last_slash = strrchr(filename, '/');
        const char *base_name_start = last_slash ? last_slash + 1 : filename;

        strncpy(scrip_name, base_name_start, sizeof(scrip_name) -1);
        scrip_name[sizeof(scrip_name) - 1] = '\0'; // Ensure null termination

        char *dot_json = strstr(scrip_name, ".json");
        if (dot_json) {
            *dot_json = '\0'; // Truncate .json
        }

        size_t scrip_name_len = strlen(scrip_name);

        if (scrip_name_len == 0 && any_data_extracted_to_memory) {
            // Only an error if we intended to write data
            fprintf(stderr, "❌ Error: Extracted scrip name is empty for %s, but data was found.\n", filename);
            error_occurred = true;
        } else if (scrip_name_len > 100) {
            fprintf(stderr, "❌ Error: Scrip name '%s' (from %s) is too long (%zu > 100).\n", scrip_name, filename,
                    scrip_name_len);
            error_occurred = true;
        }

        // Proceed to write only if data was extracted, no parsing errors, no size mismatch, and no scrip name issues
        if (any_data_extracted_to_memory && !error_occurred) {
            FILE *fout_bin = fopen("ohlctv_values.bin", "ab");
            if (!fout_bin) {
                perror("❌ Failed to open ohlctv_values.bin for append in process_json");
                error_occurred = true; // Mark error to ensure cleanup, though can't write
            } else {
                // 1. Write scrip name length (1 byte)
                unsigned char name_len_byte = (unsigned char) scrip_name_len;
                if (fwrite(&name_len_byte, sizeof(unsigned char), 1, fout_bin) != 1) {
                    perror("❌ Failed to write scrip name length to binary file");
                    error_occurred = true;
                }

                // 2. Write scrip name (string)
                if (!error_occurred) {
                    if (fwrite(scrip_name, sizeof(char), scrip_name_len, fout_bin) != scrip_name_len) {
                        perror("❌ Failed to write scrip name to binary file");
                        error_occurred = true;
                    }
                }

                // 3. Write array size (expected_count) as uint32_t
                if (!error_occurred) {
                    uint32_t array_size_to_write = (uint32_t) expected_count;
                    if (fwrite(&array_size_to_write, sizeof(uint32_t), 1, fout_bin) != 1) {
                        perror("❌ Failed to write array size to binary file");
                        error_occurred = true;
                    }
                }

                // 4. Write float arrays if no error so far
                if (!error_occurred) {
                    for (int i = 0; i < NUM_FLOAT_KEYS; ++i) {
                        if (float_data_arrays[i].count > 0) {
                            // Should always be expected_count if we reach here
                            if (fwrite(float_data_arrays[i].data, sizeof(float), float_data_arrays[i].count, fout_bin)
                                != float_data_arrays[i].count) {
                                perror("❌ Failed to write float data to binary file");
                                error_occurred = true;
                                break;
                            }
                        }
                    }
                }
                // 4. Write long arrays if no error so far
                if (!error_occurred) {
                    for (int i = 0; i < NUM_LONG_KEYS; ++i) {
                        if (long_data_arrays[i].count > 0) {
                            // Should always be expected_count
                            if (fwrite(long_data_arrays[i].data, sizeof(long int), long_data_arrays[i].count, fout_bin)
                                != long_data_arrays[i].count) {
                                perror("❌ Failed to write long data to binary file");
                                error_occurred = true;
                                break;
                            }
                        }
                    }
                }

                long file_size_bytes = 0;
                double file_size_mb = 0.0;

                if (!error_occurred) {
                    if (fflush(fout_bin) != 0) {
                        perror("❌ Failed to flush binary output file");
                    }
                    if (fseek(fout_bin, 0, SEEK_END) == 0) {
                        file_size_bytes = ftell(fout_bin);
                        if (file_size_bytes == -1L) {
                            perror("❌ Failed to get binary file size (ftell)");
                        } else {
                            file_size_mb = (double) file_size_bytes / (1024.0 * 1024.0);
                        }
                    } else {
                        perror("❌ Failed to seek to end of binary output file (fseek)");
                    }
                }

                if (fclose(fout_bin) != 0) {
                    perror("❌ Failed to close binary output file");
                }
                if (!error_occurred && LOG_ENABLED) {
                    printf("Processed: %s (%s) | Appended data (%zu records). Total Binary File size: %.1f MB\n",
                           filename, scrip_name, expected_count, file_size_mb);
                }
            }
        }
    }

cleanup:
    // Free allocated memory
    for (int i = 0; i < NUM_FLOAT_KEYS; ++i) free_float_array(&float_data_arrays[i]);
    for (int i = 0; i < NUM_LONG_KEYS; ++i) free_long_array(&long_data_arrays[i]);
}


void read_zip(const char *zip_path) {
    unzFile zip = unzOpen(zip_path);
    if (!zip) {
        printf("❌ Failed to open: %s\n", zip_path);
        return;
    }

    if (unzGoToFirstFile(zip) != UNZ_OK) {
        printf("❌ No files in zip\n");
        unzClose(zip);
        return;
    }

    do {
        char filename_in_zip[256]; // Renamed to avoid conflict with parameter `filename` in process_json
        unz_file_info file_info;

        if (unzGetCurrentFileInfo(zip, &file_info, filename_in_zip, sizeof(filename_in_zip), NULL, 0, NULL, 0) ==
            UNZ_OK) {
            if (strstr(filename_in_zip, ".json") && !strstr(filename_in_zip, "__MACOSX/")) {
                if (unzOpenCurrentFile(zip) == UNZ_OK) {
                    char *buffer = malloc(file_info.uncompressed_size + 1);
                    if (!buffer) {
                        printf("❌ Memory allocation failed for %s\n", filename_in_zip);
                        unzCloseCurrentFile(zip);
                        continue;
                    }

                    int read_size = unzReadCurrentFile(zip, buffer, file_info.uncompressed_size);
                    if (read_size < 0 || (unsigned int) read_size != file_info.uncompressed_size) {
                        // Check read_size
                        printf("❌ Error reading file %s from zip. Expected %lu, got %d. Error code: %d\n", // Changed %u to %lu
                               filename_in_zip, file_info.uncompressed_size, read_size, read_size < 0 ? read_size : 0);
                        free(buffer);
                        unzCloseCurrentFile(zip);
                        continue;
                    }
                    buffer[file_info.uncompressed_size] = '\0';

                    process_json(buffer, filename_in_zip); // Pass filename_in_zip
                    free(buffer);

                    unzCloseCurrentFile(zip);
                }
            }
        }
    } while (unzGoToNextFile(zip) == UNZ_OK);

    unzClose(zip);
}


// Function to read back and print the data from the binary file
void read_and_print_binary_data(const char *bin_filepath) {
    FILE *fin = fopen(bin_filepath, "rb");
    if (!fin) {
        perror("❌ Failed to open binary file for reading");
        return;
    }

    if (LOG_ENABLED)
        printf("\n--- Reading and Printing Data from %s ---\n", bin_filepath);

    while (true) {
        unsigned char scrip_name_len_byte;
        size_t items_read;

        // 1. Read scrip name length
        items_read = fread(&scrip_name_len_byte, sizeof(unsigned char), 1, fin);
        if (items_read != 1) {
            if (feof(fin)) {
                // printf("ℹ️ End of file reached.\n");
                break; // Expected EOF
            }
            perror("❌ Failed to read scrip name length");
            break;
        }
        uint8_t scrip_name_len = scrip_name_len_byte; // Cast to uint8_t for clarity

        // 2. Read scrip name
        char scrip_name[101]; // Max 100 chars + null terminator
        if (scrip_name_len > 100) {
            fprintf(stderr, "❌ Error: Scrip name length %u in file is too large (max 100).\n", scrip_name_len);
            // Attempt to skip the rest of this record, though file might be corrupted
            // This is a simplified error handling for this case.
            break;
        }
        items_read = fread(scrip_name, sizeof(char), scrip_name_len, fin);
        if (items_read != scrip_name_len) {
            perror("❌ Failed to read scrip name");
            break;
        }
        scrip_name[scrip_name_len] = '\0'; // Null-terminate the scrip name

        // 3. Read array size
        uint32_t array_size;
        items_read = fread(&array_size, sizeof(uint32_t), 1, fin);
        if (items_read != 1) {
            perror("❌ Failed to read array size");
            break;
        }

        if (array_size == 0 && scrip_name_len > 0) {
            // Scrip name present but no data
            printf("Symbol: %s | No data records (array size is 0).\n", scrip_name);
            continue;
        }
        if (array_size == 0 && scrip_name_len == 0) {
            // Should not happen if writer is correct
            fprintf(stderr, "Warning: Encountered zero length scrip name and zero array size.\n");
            continue;
        }


        if (LOG_ENABLED)
            printf("\nSymbol: %s (Array Size: %u)\n", scrip_name, array_size);

        // 4. Allocate memory and read o, h, l, c, t, v data
        float *o_data = malloc(array_size * sizeof(float));
        float *h_data = malloc(array_size * sizeof(float));
        float *l_data = malloc(array_size * sizeof(float));
        float *c_data = malloc(array_size * sizeof(float));
        long int *t_data = malloc(array_size * sizeof(long int));
        long int *v_data = malloc(array_size * sizeof(long int));

        bool read_error = false;
        if (!o_data || !h_data || !l_data || !c_data || !t_data || !v_data) {
            perror("❌ Memory allocation failed for reading data arrays");
            read_error = true;
        }

        if (!read_error) {
            if (fread(o_data, sizeof(float), array_size, fin) != array_size) read_error = true;
            if (!read_error && fread(h_data, sizeof(float), array_size, fin) != array_size) read_error = true;
            if (!read_error && fread(l_data, sizeof(float), array_size, fin) != array_size) read_error = true;
            if (!read_error && fread(c_data, sizeof(float), array_size, fin) != array_size) read_error = true;
            if (!read_error && fread(t_data, sizeof(long int), array_size, fin) != array_size) read_error = true;
            if (!read_error && fread(v_data, sizeof(long int), array_size, fin) != array_size) read_error = true;
        }

        if (read_error) {
            fprintf(stderr, "❌ Error reading OHLCVT data for %s.\n", scrip_name);
            // Free any partially allocated buffers
            free(o_data);
            free(h_data);
            free(l_data);
            free(c_data);
            free(t_data);
            free(v_data);
            break; // Stop processing further
        }

        // Print the data (example: print first few and last few if many)
        if (LOG_ENABLED) {
            printf("  O: ");
            for (uint32_t i = 0; i < array_size && i < 5; ++i) printf("%.2f ", o_data[i]);
            if (array_size > 5) printf("...");
            printf("\n");
            printf("  H: ");
            for (uint32_t i = 0; i < array_size && i < 5; ++i) printf("%.2f ", h_data[i]);
            if (array_size > 5) printf("...");
            printf("\n");
            printf("  L: ");
            for (uint32_t i = 0; i < array_size && i < 5; ++i) printf("%.2f ", l_data[i]);
            if (array_size > 5) printf("...");
            printf("\n");
            printf("  C: ");
            for (uint32_t i = 0; i < array_size && i < 5; ++i) printf("%.2f ", c_data[i]);
            if (array_size > 5) printf("...");
            printf("\n");
            printf("  T: ");
            for (uint32_t i = 0; i < array_size && i < 5; ++i) printf("%ld ", t_data[i]);
            if (array_size > 5) printf("...");
            printf("\n");
            printf("  V: ");
            for (uint32_t i = 0; i < array_size && i < 5; ++i) printf("%ld ", v_data[i]);
            if (array_size > 5) printf("...");
            printf("\n");
        }
        free(o_data);
        free(h_data);
        free(l_data);
        free(c_data);
        free(t_data);
        free(v_data);
    }

    if (fclose(fin) != 0) {
        perror("❌ Failed to close binary file after reading");
    }
    if (LOG_ENABLED)
        printf("\n--- Finished reading %s ---\n", bin_filepath);
}




// Structure to hold a list of symbol strings
typedef struct {
    char **symbols;
    size_t count;
    size_t capacity;
} SymbolList;

// Helper functions for SymbolList
void init_symbol_list(SymbolList *list) {
    list->symbols = malloc(INITIAL_CAPACITY * sizeof(char *));
    if (!list->symbols) {
        perror("❌ Failed to allocate memory for SymbolList");
        list->count = 0;
        list->capacity = 0;
        return;
    }
    list->count = 0;
    list->capacity = INITIAL_CAPACITY;
}

void add_to_symbol_list(SymbolList *list, const char *symbol) {
    if (list->capacity == 0) return; // Not initialized
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        char **temp = realloc(list->symbols, list->capacity * sizeof(char *));
        if (!temp) {
            perror("❌ Failed to reallocate memory for SymbolList");
            return; // Symbol not added
        }
        list->symbols = temp;
    }
    list->symbols[list->count] = strdup(symbol); // Duplicate the string
    if (!list->symbols[list->count]) {
        perror("❌ Failed to duplicate symbol string");
        return; // Symbol not added
    }
    list->count++;
}

void free_symbol_list(SymbolList *list) {
    if (list->symbols) {
        for (size_t i = 0; i < list->count; ++i) {
            free(list->symbols[i]); // Free each duplicated string
        }
        free(list->symbols);
    }
    list->symbols = NULL;
    list->count = 0;
    list->capacity = 0;
}



// Function to extract all symbols from the binary file
SymbolList extract_all_symbols_from_binary(const char *bin_filepath) {
    SymbolList sl;
    init_symbol_list(&sl);

    FILE *fin = fopen(bin_filepath, "rb");
    if (!fin) {
        perror("❌ Failed to open binary file for reading symbols");
        return sl; // Return empty list
    }

    if (LOG_ENABLED)
        printf("\n--- Extracting Symbols from %s ---\n", bin_filepath);

    while (true) {
        unsigned char scrip_name_len_byte;
        size_t items_read;

        // 1. Read scrip name length
        items_read = fread(&scrip_name_len_byte, sizeof(unsigned char), 1, fin);
        if (items_read != 1) {
            if (feof(fin)) break; // Expected EOF
            perror("❌ Failed to read scrip name length (extract_symbols)");
            break;
        }
        uint8_t scrip_name_len = scrip_name_len_byte;

        // 2. Read scrip name
        char scrip_name_buffer[101]; // Max 100 chars + null terminator
        if (scrip_name_len > 100) {
            fprintf(stderr, "❌ Error: Scrip name length %u in file is too large (max 100) (extract_symbols).\n", scrip_name_len);
            // Attempt to recover or break. For now, break.
            break;
        }
        if (scrip_name_len > 0) { // Only read if length is positive
            items_read = fread(scrip_name_buffer, sizeof(char), scrip_name_len, fin);
            if (items_read != scrip_name_len) {
                perror("❌ Failed to read scrip name (extract_symbols)");
                break;
            }
            scrip_name_buffer[scrip_name_len] = '\0'; // Null-terminate
            add_to_symbol_list(&sl, scrip_name_buffer);
        } else if (scrip_name_len == 0) {
             // Potentially an empty scrip record, or end of meaningful data.
             // We need to read array_size to know if we should continue or if it's an error.
        }


        // 3. Read array size to know how much data to skip
        uint32_t array_size;
        items_read = fread(&array_size, sizeof(uint32_t), 1, fin);
        if (items_read != 1) {
            if (feof(fin) && scrip_name_len == 0) { // If we read 0 len scrip and then EOF, it's fine.
                 break;
            }
            perror("❌ Failed to read array size (extract_symbols)");
            break;
        }

        if (scrip_name_len == 0 && array_size == 0 && feof(fin)) {
            // This could be a padding or an empty record at the end.
            break;
        }


        // 4. Skip OHLCVT data
        long data_block_size = 0;
        if (array_size > 0) {
            // Size of float data: NUM_FLOAT_KEYS * array_size * sizeof(float)
            // Size of long data: NUM_LONG_KEYS * array_size * sizeof(long int)
            // Note: NUM_FLOAT_KEYS and NUM_LONG_KEYS are defined in process_json.
            // For robustness, let's use the constants directly or pass them.
            // Assuming they are 4 and 2 respectively as per process_json.
            data_block_size = (long)array_size * (4 * sizeof(float) + 2 * sizeof(long int));
        }

        if (data_block_size > 0) {
            if (fseek(fin, data_block_size, SEEK_CUR) != 0) {
                perror("❌ Failed to seek past data block (extract_symbols)");
                break;
            }
        } else if (array_size > 0 && data_block_size == 0) {
            // This case implies array_size > 0 but calculated block size is 0, which is an issue.
            // However, our calculation of data_block_size should prevent this if array_size > 0.
        }
        // If array_size is 0, data_block_size will be 0, and no seek is needed.
    }

    if (fclose(fin) != 0) {
        perror("❌ Failed to close binary file after extracting symbols");
    }
    if (LOG_ENABLED)
        printf("--- Finished extracting %zu symbols ---\n", sl.count);
    return sl;
}

int main(int argc, char *argv[]) {
    // Initialize lastTime at the beginning of main
    lastTime = clock();

    // Clean/create the binary output file at the start of the program
    FILE *fout_bin_init = fopen("ohlctv_values.bin", "wb");
    if (!fout_bin_init) {
        perror("❌ Failed to initialize binary output file ohlctv_values.bin");
        return 1;
    }
    fclose(fout_bin_init);


    read_zip("/Users/shakir/BhavAppData/DATA/TEST/1D_ALL_JSON_MoneyControl.zip");
    printTimeSpent("Total time for writing");

    // Read and print the data from the binary file (optional, can be commented out)
    // read_and_print_binary_data("ohlctv_values.bin");
    // printTimeSpent("Read and Print OHLCVT Data Time");

    // Extract and print all symbols
    SymbolList symbols = extract_all_symbols_from_binary("ohlctv_values.bin");
    printTimeSpent("Extract Symbols Time");
    if (LOG_ENABLED) {
        printf("\n--- Extracted Symbols (%zu) ---\n", symbols.count);
        for (size_t i = 0; i < symbols.count; ++i) {
            printf("%s\n", symbols.symbols[i]);
        }
    }

    free_symbol_list(&symbols);


    return 0;
}