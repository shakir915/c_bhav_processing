#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <minizip/unzip.h>
#include <float.h>
#include <ctype.h>   // For isspace
#include <stdbool.h> // For bool type
#include <limits.h>  // For LONG_MIN, LONG_MAX
#include <stdint.h>  // For uint32_t, uint64_t

#define INITIAL_CAPACITY 100 // Initial capacity for dynamic arrays
#define LOG_ENABLED false    // Set to true for detailed logging

// --- Timing ---
clock_t lastTime;

void printTimeSpent(const char *tag) {
    if (tag != NULL && *tag != '\0') {
        double time_spent = (double)(clock() - lastTime) / CLOCKS_PER_SEC;
        printf("🕒 %s: %.3f seconds\n", tag, time_spent);
    }
    lastTime = clock();
}

// --- Dynamic array for floats ---
typedef struct {
    float *data;
    size_t count;
    size_t capacity;
} FloatArray;

void init_float_array(FloatArray *arr) {
    arr->data = malloc(INITIAL_CAPACITY * sizeof(float));
    if (!arr->data) {
        perror("❌ Failed to allocate memory for FloatArray");
        arr->count = 0;
        arr->capacity = 0;
        return;
    }
    arr->count = 0;
    arr->capacity = INITIAL_CAPACITY;
}

bool add_to_float_array(FloatArray *arr, float value) {
    if (arr->capacity == 0 && arr->data == NULL) {
        init_float_array(arr);
        if(arr->capacity == 0) return false;
    }
    if (arr->count >= arr->capacity) {
        size_t new_capacity = arr->capacity == 0 ? INITIAL_CAPACITY : arr->capacity * 2;
        float *temp = realloc(arr->data, new_capacity * sizeof(float));
        if (!temp) {
            perror("❌ Failed to reallocate memory for FloatArray");
            return false;
        }
        arr->data = temp;
        arr->capacity = new_capacity;
    }
    arr->data[arr->count++] = value;
    return true;
}

void free_float_array(FloatArray *arr) {
    free(arr->data);
    arr->data = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

// --- Dynamic array for long ints ---
typedef struct {
    long int *data;
    size_t count;
    size_t capacity;
} LongArray;

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

bool add_to_long_array(LongArray *arr, long int value) {
    if (arr->capacity == 0 && arr->data == NULL) {
        init_long_array(arr);
        if(arr->capacity == 0) return false;
    }
    if (arr->count >= arr->capacity) {
        size_t new_capacity = arr->capacity == 0 ? INITIAL_CAPACITY : arr->capacity * 2;
        long int *temp = realloc(arr->data, new_capacity * sizeof(long int));
        if (!temp) {
            perror("❌ Failed to reallocate memory for LongArray");
            return false;
        }
        arr->data = temp;
        arr->capacity = new_capacity;
    }
    arr->data[arr->count++] = value;
    return true;
}

void free_long_array(LongArray *arr) {
    free(arr->data);
    arr->data = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

// --- ScripInfo and ScripInfoArray ---
#define NUM_FLOAT_KEYS_CONST 4
#define NUM_LONG_KEYS_CONST 2

typedef struct {
    char scrip_name[101];
    unsigned char scrip_name_len;
    size_t expected_count;

    FloatArray float_data_arrays[NUM_FLOAT_KEYS_CONST];
    LongArray long_data_arrays[NUM_LONG_KEYS_CONST];

    uint64_t file_offset_for_data_start_ptr;
    uint64_t file_offset_for_data_end_ptr;
} ScripInfo;

typedef struct {
    ScripInfo *scrips;
    size_t count;
    size_t capacity;
} ScripInfoArray;

void init_scrip_info_array(ScripInfoArray *arr) {
    arr->scrips = malloc(INITIAL_CAPACITY * sizeof(ScripInfo));
    if (!arr->scrips) {
        perror("❌ Failed to allocate memory for ScripInfoArray");
        arr->count = 0;
        arr->capacity = 0;
        return;
    }
    arr->count = 0;
    arr->capacity = INITIAL_CAPACITY;
}

void add_to_scrip_info_array(ScripInfoArray *arr, ScripInfo scrip_to_add) {
    if (arr->capacity == 0) return;
    if (arr->count >= arr->capacity) {
        arr->capacity *= 2;
        ScripInfo *temp = realloc(arr->scrips, arr->capacity * sizeof(ScripInfo));
        if (!temp) {
            perror("❌ Failed to reallocate memory for ScripInfoArray");
            // Free internal arrays of scrip_to_add to prevent leaks if realloc fails
            for (int j = 0; j < NUM_FLOAT_KEYS_CONST; ++j) free_float_array(&scrip_to_add.float_data_arrays[j]);
            for (int j = 0; j < NUM_LONG_KEYS_CONST; ++j) free_long_array(&scrip_to_add.long_data_arrays[j]);
            return;
        }
        arr->scrips = temp;
    }
    arr->scrips[arr->count++] = scrip_to_add;
}

void free_scrip_info_array(ScripInfoArray *arr) {
    if (arr->scrips) {
        for (size_t i = 0; i < arr->count; ++i) {
            for (int j = 0; j < NUM_FLOAT_KEYS_CONST; ++j) {
                free_float_array(&arr->scrips[i].float_data_arrays[j]);
            }
            for (int j = 0; j < NUM_LONG_KEYS_CONST; ++j) {
                free_long_array(&arr->scrips[i].long_data_arrays[j]);
            }
        }
        free(arr->scrips);
    }
    arr->scrips = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

// --- JSON Parsing Helpers ---
static int extract_long_array_from_json(const char *content, const char *array_key_prefix, LongArray *output_array) {
    const char *p = strstr(content, array_key_prefix);
    if (!p) return 0;
    p += strlen(array_key_prefix);
    const char *end_of_array = strchr(p, ']');
    if (!end_of_array) return -1;

    int values_found = 0;
    char *current_pos = (char *)p;
    while (current_pos < end_of_array && *current_pos != '\0') {
        char *next_val_ptr;
        long int value = strtol(current_pos, &next_val_ptr, 10);
        if (current_pos == next_val_ptr) {
            if (*current_pos == ',' || isspace((unsigned char)*current_pos)) {
                current_pos++;
                continue;
            }
            break;
        }
        if(!add_to_long_array(output_array, value)) return -1;
        values_found++;
        current_pos = next_val_ptr;
        while (current_pos < end_of_array && (*current_pos == ',' || isspace((unsigned char)*current_pos))) {
            current_pos++;
        }
        if (current_pos >= end_of_array || *current_pos == ']') break;
    }
    return values_found > 0 ? 1 : 0;
}

static int extract_float_array_from_json(const char *content, const char *array_key_prefix, FloatArray *output_array) {
    const char *p = strstr(content, array_key_prefix);
    if (!p) return 0;
    p += strlen(array_key_prefix);
    const char *end_of_array = strchr(p, ']');
    if (!end_of_array) return -1;

    int values_found = 0;
    char *current_pos = (char *)p;
    while (current_pos < end_of_array && *current_pos != '\0') {
        char *next_val_ptr;
        float value = strtof(current_pos, &next_val_ptr);
        if (current_pos == next_val_ptr) {
            if (*current_pos == ',' || isspace((unsigned char)*current_pos)) {
                current_pos++;
                continue;
            }
            break;
        }
        if(!add_to_float_array(output_array, value)) return -1;
        values_found++;
        current_pos = next_val_ptr;
        while (current_pos < end_of_array && (*current_pos == ',' || isspace((unsigned char)*current_pos))) {
            current_pos++;
        }
        if (current_pos >= end_of_array || *current_pos == ']') break;
    }
    return values_found > 0 ? 1 : 0;
}

// --- Core Logic ---
bool parse_json_to_scrip_info(const char *content, const char *filename_in_zip, ScripInfo *out_scrip) {
    const char *keys_prefix[] = {"\"o\":[", "\"h\":[", "\"l\":[", "\"c\":[", "\"t\":[", "\"v\":["};
    const char *key_names[] = {"Open", "High", "Low", "Close", "Timestamp", "Volume"};
    const int TOTAL_KEYS_CONST = NUM_FLOAT_KEYS_CONST + NUM_LONG_KEYS_CONST;

    for (int i = 0; i < NUM_FLOAT_KEYS_CONST; ++i) init_float_array(&out_scrip->float_data_arrays[i]);
    for (int i = 0; i < NUM_LONG_KEYS_CONST; ++i) init_long_array(&out_scrip->long_data_arrays[i]);
    out_scrip->expected_count = 0;
    out_scrip->scrip_name_len = 0;
    out_scrip->scrip_name[0] = '\0';

    bool any_data_extracted = false;
    bool error_occurred = false;

    for (int i = 0; i < TOTAL_KEYS_CONST; ++i) {
        int result;
        if (i < NUM_FLOAT_KEYS_CONST) {
            result = extract_float_array_from_json(content, keys_prefix[i], &out_scrip->float_data_arrays[i]);
        } else {
            result = extract_long_array_from_json(content, keys_prefix[i], &out_scrip->long_data_arrays[i - NUM_FLOAT_KEYS_CONST]);
        }

        if (result == 1) {
            any_data_extracted = true;
        } else if (result == -1) {
            fprintf(stderr, "❌ Error parsing data for key %s in %s\n", key_names[i], filename_in_zip);
            error_occurred = true;
            break;
        }
    }

    if (error_occurred || !any_data_extracted) {
        goto cleanup_and_fail;
    }

    bool first_populated_array_found = false;
    size_t current_expected_count = 0;
    bool size_mismatch = false;

    for (int i = 0; i < NUM_FLOAT_KEYS_CONST; ++i) {
        if (out_scrip->float_data_arrays[i].count > 0) {
            if (!first_populated_array_found) {
                current_expected_count = out_scrip->float_data_arrays[i].count;
                first_populated_array_found = true;
            } else if (out_scrip->float_data_arrays[i].count != current_expected_count) {
                size_mismatch = true;
                break;
            }
        }
    }
    if (!size_mismatch) {
        for (int i = 0; i < NUM_LONG_KEYS_CONST; ++i) {
            if (out_scrip->long_data_arrays[i].count > 0) {
                if (!first_populated_array_found) {
                    current_expected_count = out_scrip->long_data_arrays[i].count;
                    first_populated_array_found = true;
                } else if (out_scrip->long_data_arrays[i].count != current_expected_count) {
                    size_mismatch = true;
                    break;
                }
            }
        }
    }

    if (!first_populated_array_found || size_mismatch) {
        if (size_mismatch) {
            fprintf(stderr, "❌ Error: Data array size mismatch for scrip %s. Expected %zu.\n", filename_in_zip, current_expected_count);
        }
        goto cleanup_and_fail;
    }
    out_scrip->expected_count = current_expected_count;

    const char *last_slash = strrchr(filename_in_zip, '/');
    const char *base_name_start = last_slash ? last_slash + 1 : filename_in_zip;
    strncpy(out_scrip->scrip_name, base_name_start, sizeof(out_scrip->scrip_name) - 1);
    out_scrip->scrip_name[sizeof(out_scrip->scrip_name) - 1] = '\0';
    char *dot_json = strstr(out_scrip->scrip_name, ".json");
    if (dot_json) *dot_json = '\0';
    out_scrip->scrip_name_len = (unsigned char)strlen(out_scrip->scrip_name);

    if (out_scrip->scrip_name_len == 0 || out_scrip->scrip_name_len > 100) {
        fprintf(stderr, "❌ Error: Scrip name '%s' (from %s) is invalid (empty or too long).\n", out_scrip->scrip_name, filename_in_zip);
        goto cleanup_and_fail;
    }

    return true;

cleanup_and_fail:
    for (int i = 0; i < NUM_FLOAT_KEYS_CONST; ++i) free_float_array(&out_scrip->float_data_arrays[i]);
    for (int i = 0; i < NUM_LONG_KEYS_CONST; ++i) free_long_array(&out_scrip->long_data_arrays[i]);
    out_scrip->expected_count = 0;
    return false;
}

void read_zip_and_parse_data(const char *zip_path, ScripInfoArray *all_scrips_info) {
    unzFile zip = unzOpen(zip_path);
    if (!zip) {
        fprintf(stderr, "❌ Failed to open zip: %s\n", zip_path);
        return;
    }

    if (unzGoToFirstFile(zip) != UNZ_OK) {
        printf("ℹ️ No files in zip: %s\n", zip_path);
        unzClose(zip);
        return;
    }

    do {
        char filename_in_zip[256];
        unz_file_info file_info;
        if (unzGetCurrentFileInfo(zip, &file_info, filename_in_zip, sizeof(filename_in_zip), NULL, 0, NULL, 0) != UNZ_OK) {
            continue;
        }

        if (strstr(filename_in_zip, ".json") && !strstr(filename_in_zip, "__MACOSX/")) {
            if (unzOpenCurrentFile(zip) == UNZ_OK) {
                char *buffer = malloc(file_info.uncompressed_size + 1);
                if (!buffer) {
                    fprintf(stderr, "❌ Memory allocation failed for %s content\n", filename_in_zip);
                    unzCloseCurrentFile(zip);
                    continue;
                }
                int read_size = unzReadCurrentFile(zip, buffer, file_info.uncompressed_size);
                if (read_size < 0 || (unsigned int)read_size != file_info.uncompressed_size) {
                    fprintf(stderr, "❌ Error reading file %s from zip. Expected %lu, got %d.\n",
                           filename_in_zip, (long unsigned)file_info.uncompressed_size, read_size);
                    free(buffer);
                    unzCloseCurrentFile(zip);
                    continue;
                }
                buffer[file_info.uncompressed_size] = '\0';

                ScripInfo current_scrip_data;
                if (parse_json_to_scrip_info(buffer, filename_in_zip, &current_scrip_data)) {
                    add_to_scrip_info_array(all_scrips_info, current_scrip_data);
                }
                free(buffer);
                unzCloseCurrentFile(zip);
            }
        }
    } while (unzGoToNextFile(zip) == UNZ_OK);
    unzClose(zip);
}

bool write_binary_two_pass(const char *output_filename, ScripInfoArray *all_scrips_info) {
    FILE *fout = fopen(output_filename, "wb");
    if (!fout) {
        perror("❌ Failed to open binary output file for writing");
        return false;
    }

    uint64_t placeholder_end_of_all_headers = 0;
    if (fwrite(&placeholder_end_of_all_headers, sizeof(uint64_t), 1, fout) != 1) {
        perror("❌ Failed to write placeholder for end_of_all_headers");
        fclose(fout); return false;
    }

    for (size_t i = 0; i < all_scrips_info->count; ++i) {
        ScripInfo *scrip = &all_scrips_info->scrips[i];
        if (scrip->expected_count == 0) continue;

        if (fwrite(&scrip->scrip_name_len, sizeof(unsigned char), 1, fout) != 1) {
            perror("❌ Failed to write scrip name length"); fclose(fout); return false;
        }
        if (fwrite(scrip->scrip_name, sizeof(char), scrip->scrip_name_len, fout) != scrip->scrip_name_len) {
            perror("❌ Failed to write scrip name"); fclose(fout); return false;
        }

        long current_offset_long = ftell(fout);
        if (current_offset_long == -1L) { perror("ftell failed before data_start_offset"); fclose(fout); return false; }
        scrip->file_offset_for_data_start_ptr = (uint64_t)current_offset_long;
        uint64_t placeholder_data_addr = 0;
        if (fwrite(&placeholder_data_addr, sizeof(uint64_t), 1, fout) != 1) {
            perror("❌ Failed to write placeholder for data_start_offset"); fclose(fout); return false;
        }

        current_offset_long = ftell(fout);
        if (current_offset_long == -1L) { perror("ftell failed before data_end_offset"); fclose(fout); return false; }
        scrip->file_offset_for_data_end_ptr = (uint64_t)current_offset_long;
        if (fwrite(&placeholder_data_addr, sizeof(uint64_t), 1, fout) != 1) {
            perror("❌ Failed to write placeholder for data_end_offset"); fclose(fout); return false;
        }
    }

    long actual_end_of_all_headers_long = ftell(fout);
    if (actual_end_of_all_headers_long == -1L) { perror("ftell failed for actual_end_of_all_headers"); fclose(fout); return false; }
    uint64_t actual_end_of_all_headers = (uint64_t)actual_end_of_all_headers_long;

    if (fseek(fout, 0, SEEK_SET) != 0) {
        perror("❌ Failed to seek to beginning to write end_of_all_headers"); fclose(fout); return false;
    }
    if (fwrite(&actual_end_of_all_headers, sizeof(uint64_t), 1, fout) != 1) {
        perror("❌ Failed to write actual end_of_all_headers"); fclose(fout); return false;
    }

    if (fseek(fout, (long)actual_end_of_all_headers, SEEK_SET) != 0) {
        perror("❌ Failed to seek to start of data section"); fclose(fout); return false;
    }

    for (size_t i = 0; i < all_scrips_info->count; ++i) {
        ScripInfo *scrip = &all_scrips_info->scrips[i];
        if (scrip->expected_count == 0) continue;

        long data_start_offset_long = ftell(fout);
        if (data_start_offset_long == -1L) { perror("ftell failed before writing data"); fclose(fout); return false; }
        uint64_t actual_data_start_offset = (uint64_t)data_start_offset_long;

        for (int k = 0; k < NUM_FLOAT_KEYS_CONST; ++k) {
            if (scrip->float_data_arrays[k].count > 0) {
                if (fwrite(scrip->float_data_arrays[k].data, sizeof(float), scrip->expected_count, fout) != scrip->expected_count) {
                    perror("❌ Failed to write float data"); fclose(fout); return false;
                }
            }
        }
        for (int k = 0; k < NUM_LONG_KEYS_CONST; ++k) {
            if (scrip->long_data_arrays[k].count > 0) {
                if (fwrite(scrip->long_data_arrays[k].data, sizeof(long int), scrip->expected_count, fout) != scrip->expected_count) {
                    perror("❌ Failed to write long data"); fclose(fout); return false;
                }
            }
        }

        long data_end_offset_long = ftell(fout);
        if (data_end_offset_long == -1L) { perror("ftell failed after writing data"); fclose(fout); return false; }
        uint64_t actual_data_end_offset = (uint64_t)data_end_offset_long;

        long current_pos_after_data_write = data_end_offset_long;

        if (fseek(fout, (long)scrip->file_offset_for_data_start_ptr, SEEK_SET) != 0) {
            perror("❌ Failed to seek to update data_start_offset"); fclose(fout); return false;
        }
        if (fwrite(&actual_data_start_offset, sizeof(uint64_t), 1, fout) != 1) {
            perror("❌ Failed to write actual_data_start_offset"); fclose(fout); return false;
        }

        if (fseek(fout, (long)scrip->file_offset_for_data_end_ptr, SEEK_SET) != 0) {
            perror("❌ Failed to seek to update data_end_offset"); fclose(fout); return false;
        }
        if (fwrite(&actual_data_end_offset, sizeof(uint64_t), 1, fout) != 1) {
            perror("❌ Failed to write actual_data_end_offset"); fclose(fout); return false;
        }

        if (fseek(fout, current_pos_after_data_write, SEEK_SET) != 0) {
            perror("❌ Failed to seek back to end of data section"); fclose(fout); return false;
        }
        if (LOG_ENABLED) {
             printf("Processed: %s | Appended data (%zu records). Start: %llu, End: %llu\n",
                   scrip->scrip_name, scrip->expected_count,
                   (unsigned long long)actual_data_start_offset,
                   (unsigned long long)actual_data_end_offset);
        }
    }

    if (fflush(fout) != 0) perror("❌ Failed to flush binary output file");
    long file_size_bytes = ftell(fout);
    if (file_size_bytes == -1L) {
        perror("❌ Failed to get final binary file size (ftell)");
    } else {
        if (LOG_ENABLED) printf("Total Binary File size: %.2f MB\n", (double)file_size_bytes / (1024.0 * 1024.0));
    }

    if (fclose(fout) != 0) {
        perror("❌ Failed to close binary output file");
        return false;
    }
    return true;
}

// Modified to write to a FILE* instead of stdout
void read_and_print_binary_data_to_file(const char *input_filename, FILE *outfile) {
    FILE *fin = fopen(input_filename, "rb");
    if (!fin) {
        perror("❌ Failed to open binary input file for reading");
        fprintf(outfile, "❌ Failed to open binary input file for reading: %s\n", input_filename);
        return;
    }

    uint64_t end_of_all_headers_offset;
    if (fread(&end_of_all_headers_offset, sizeof(uint64_t), 1, fin) != 1) {
        perror("❌ Failed to read end_of_all_headers_offset from binary file");
        fprintf(outfile, "❌ Failed to read end_of_all_headers_offset from binary file: %s\n", input_filename);
        fclose(fin);
        return;
    }

    fprintf(outfile, "Binary File: %s\n", input_filename);
    fprintf(outfile, "End of Headers at offset: %llu\n\n", (unsigned long long)end_of_all_headers_offset);

    long current_header_pos = ftell(fin);
     if (current_header_pos == -1L) {
        perror("ftell failed after reading end_of_all_headers_offset");
        fprintf(outfile, "ftell failed after reading end_of_all_headers_offset for %s\n", input_filename);
        fclose(fin);
        return;
    }


    const char *float_key_names[NUM_FLOAT_KEYS_CONST] = {"Open", "High", "Low", "Close"};
    const char *long_key_names[NUM_LONG_KEYS_CONST] = {"Timestamp", "Volume"};

    while (current_header_pos < (long)end_of_all_headers_offset && !feof(fin)) {
        unsigned char scrip_name_len;
        char scrip_name[101];
        uint64_t data_start_offset;
        uint64_t data_end_offset;

        float* temp_float_data[NUM_FLOAT_KEYS_CONST] = {NULL};
        long int* temp_long_data[NUM_LONG_KEYS_CONST] = {NULL};
        bool scrip_read_success = false;
        size_t num_records = 0;


        if (fread(&scrip_name_len, sizeof(unsigned char), 1, fin) != 1) {
            if (feof(fin) && current_header_pos == (long)end_of_all_headers_offset) break;
            perror("❌ Failed to read scrip_name_len");
            fprintf(outfile, "❌ Failed to read scrip_name_len for a scrip in %s\n", input_filename);
            goto next_scrip_or_exit;
        }

        if (scrip_name_len == 0 || scrip_name_len > 100) {
            fprintf(stderr, "❌ Invalid scrip_name_len: %u at offset %ld\n", scrip_name_len, ftell(fin)-1);
            fprintf(outfile, "❌ Invalid scrip_name_len: %u for a scrip in %s\n", scrip_name_len, input_filename);
            goto next_scrip_or_exit;
        }

        if (fread(scrip_name, sizeof(char), scrip_name_len, fin) != scrip_name_len) {
            perror("❌ Failed to read scrip_name");
            fprintf(outfile, "❌ Failed to read scrip_name for a scrip in %s\n", input_filename);
            goto next_scrip_or_exit;
        }
        scrip_name[scrip_name_len] = '\0';

        if (fread(&data_start_offset, sizeof(uint64_t), 1, fin) != 1) {
            perror("❌ Failed to read data_start_offset");
            fprintf(outfile, "❌ Failed to read data_start_offset for scrip %s in %s\n", scrip_name, input_filename);
            goto next_scrip_or_exit;
        }
        if (fread(&data_end_offset, sizeof(uint64_t), 1, fin) != 1) {
            perror("❌ Failed to read data_end_offset");
            fprintf(outfile, "❌ Failed to read data_end_offset for scrip %s in %s\n", scrip_name, input_filename);
            goto next_scrip_or_exit;
        }

        fprintf(outfile, "--- Scrip: %s ---\n", scrip_name);
        fprintf(outfile, "  Data Start: %llu, Data End: %llu\n", (unsigned long long)data_start_offset, (unsigned long long)data_end_offset);

        if (data_start_offset >= data_end_offset) {
            fprintf(outfile, "  No data or invalid offsets (start >= end).\n\n");
            current_header_pos = ftell(fin);
            if (current_header_pos == -1L) { perror("ftell error"); fclose(fin); return; }
            continue;
        }

        size_t total_data_size = data_end_offset - data_start_offset;
        size_t size_of_one_record_set = (NUM_FLOAT_KEYS_CONST * sizeof(float)) + (NUM_LONG_KEYS_CONST * sizeof(long int));

        if (size_of_one_record_set == 0) {
             fprintf(stderr, "Error: size_of_one_record_set is zero for scrip %s.\n", scrip_name);
             fprintf(outfile, "Error: size_of_one_record_set is zero for scrip %s.\n", scrip_name);
             goto cleanup_current_scrip_data;
        }
        if (total_data_size % size_of_one_record_set != 0) {
            fprintf(stderr, "❌ Data size mismatch for scrip %s. Total %zu not multiple of record set size %zu.\n",
                    scrip_name, total_data_size, size_of_one_record_set);
            fprintf(outfile, "❌ Data size mismatch for scrip %s. Total %zu not multiple of record set size %zu.\n",
                    scrip_name, total_data_size, size_of_one_record_set);
            goto cleanup_current_scrip_data;
        }
        num_records = total_data_size / size_of_one_record_set;
        fprintf(outfile, "  Number of records: %zu\n", num_records);

        if (num_records == 0) {
            fprintf(outfile, "  No data records for this scrip.\n\n");
            goto cleanup_current_scrip_data;
        }

        long next_header_entry_pos = ftell(fin);
         if (next_header_entry_pos == -1L) {
            perror("ftell error before data seek");
            fprintf(outfile, "ftell error before data seek for scrip %s\n", scrip_name);
            goto cleanup_current_scrip_data;
        }


        if (fseek(fin, (long)data_start_offset, SEEK_SET) != 0) {
            perror("❌ Failed to seek to data_start_offset");
            fprintf(outfile, "❌ Failed to seek to data_start_offset for scrip %s\n", scrip_name);
            goto cleanup_current_scrip_data;
        }

        for (int i = 0; i < NUM_FLOAT_KEYS_CONST; ++i) {
            temp_float_data[i] = malloc(num_records * sizeof(float));
            if (!temp_float_data[i]) {
                fprintf(stderr, "❌ Failed to allocate memory for %s data for scrip %s\n", float_key_names[i], scrip_name);
                fprintf(outfile, "❌ Failed to allocate memory for %s data for scrip %s\n", float_key_names[i], scrip_name);
                goto cleanup_current_scrip_data;
            }
            if (fread(temp_float_data[i], sizeof(float), num_records, fin) != num_records) {
                fprintf(stderr, "❌ Failed to read %s data for scrip %s\n", float_key_names[i], scrip_name);
                fprintf(outfile, "❌ Failed to read %s data for scrip %s\n", float_key_names[i], scrip_name);
                goto cleanup_current_scrip_data;
            }
        }

        for (int i = 0; i < NUM_LONG_KEYS_CONST; ++i) {
            temp_long_data[i] = malloc(num_records * sizeof(long int));
            if (!temp_long_data[i]) {
                fprintf(stderr, "❌ Failed to allocate memory for %s data for scrip %s\n", long_key_names[i], scrip_name);
                fprintf(outfile, "❌ Failed to allocate memory for %s data for scrip %s\n", long_key_names[i], scrip_name);
                goto cleanup_current_scrip_data;
            }
            if (fread(temp_long_data[i], sizeof(long int), num_records, fin) != num_records) {
                fprintf(stderr, "❌ Failed to read %s data for scrip %s\n", long_key_names[i], scrip_name);
                fprintf(outfile, "❌ Failed to read %s data for scrip %s\n", long_key_names[i], scrip_name);
                goto cleanup_current_scrip_data;
            }
        }
        scrip_read_success = true;

        fprintf(outfile, "  Data:\n    %-10s", "Index");
        for(int i=0; i<NUM_FLOAT_KEYS_CONST; ++i)
            fprintf(outfile, "%-15s", float_key_names[i]);
        for(int i=0; i<NUM_LONG_KEYS_CONST; ++i)
            fprintf(outfile, "%-15s", long_key_names[i]);
        fprintf(outfile, "\n");

        for (size_t i = 0; i < num_records; ++i) {
            fprintf(outfile, "    %-10zu", i);
            for (int k = 0; k < NUM_FLOAT_KEYS_CONST; ++k) {
                fprintf(outfile, "%-15.2f", temp_float_data[k][i]);
            }
            for (int k = 0; k < NUM_LONG_KEYS_CONST; ++k) {
                fprintf(outfile, "%-15ld", temp_long_data[k][i]);
            }
            fprintf(outfile, "\n");
        }

    cleanup_current_scrip_data:
        for (int i = 0; i < NUM_FLOAT_KEYS_CONST; ++i) free(temp_float_data[i]);
        for (int i = 0; i < NUM_LONG_KEYS_CONST; ++i) free(temp_long_data[i]);

        if (!scrip_read_success && num_records > 0) {
             fprintf(stderr, "--- Finished processing scrip %s with errors ---\n\n", scrip_name);
             fprintf(outfile, "--- Finished processing scrip %s with errors ---\n\n", scrip_name);
        } else {
            fprintf(outfile, "--- Finished processing scrip %s ---\n\n", scrip_name);
        }


        if (fseek(fin, next_header_entry_pos, SEEK_SET) != 0) {
            perror("❌ Failed to seek back to header section after scrip processing");
            fprintf(outfile, "❌ Failed to seek back to header section after scrip %s\n", scrip_name);
            break;
        }
        current_header_pos = ftell(fin);
        if (current_header_pos == -1L) {
            perror("ftell error at end of scrip loop");
            fprintf(outfile, "ftell error at end of scrip loop for %s\n", input_filename);
            break;
        }
        continue;

    next_scrip_or_exit:
        if (feof(fin) && current_header_pos >= (long)end_of_all_headers_offset) {
            // Normal EOF
        } else if (ferror(fin)) {
            perror("❌ File error during header processing");
            fprintf(outfile, "❌ File error during header processing for %s\n", input_filename);
        } else {
            fprintf(stderr, "❌ Unhandled error or premature EOF in header section.\n");
            fprintf(outfile, "❌ Unhandled error or premature EOF in header section for %s\n", input_filename);
        }
        break;
    }


    if (ferror(fin)) {
        perror("❌ A general error occurred during file reading operations");
        fprintf(outfile, "❌ A general error occurred during file reading operations for %s\n", input_filename);
    }
    fclose(fin);
}


int main(int argc, char *argv[]) {
    lastTime = clock();

    const char *zip_file_path = "/Users/shakir/BhavAppData/DATA/TEST/1D_ALL_JSON_MoneyControl.zip";
    const char *output_bin_file = "ohlctv_values_v2.bin";
    const char *verification_txt_file = "verification_output.txt"; // New text file for output

    if (argc > 1) {
        zip_file_path = argv[1];
    }
    if (argc > 2) {
        output_bin_file = argv[2];
    }
    if (argc > 3) {
        verification_txt_file = argv[3];
    }


    printf("Processing Zip: %s\n", zip_file_path);
    printf("Output Binary: %s\n", output_bin_file);
    printf("Verification Text Output: %s\n", verification_txt_file);


    ScripInfoArray all_scrips_data;
    init_scrip_info_array(&all_scrips_data);
    printTimeSpent("Initialization");

    read_zip_and_parse_data(zip_file_path, &all_scrips_data);
    printTimeSpent("Parsing all JSON files from ZIP");

    size_t scrips_to_write_count = all_scrips_data.count;

    if (scrips_to_write_count > 0) {
        if (write_binary_two_pass(output_bin_file, &all_scrips_data)) {
            printf("✅ Successfully wrote binary data to %s for %zu scrips.\n", output_bin_file, scrips_to_write_count);
        } else {
            fprintf(stderr, "❌ Failed to write binary data to %s\n", output_bin_file);
        }
    } else {
        printf("ℹ️ No scrip data extracted from the zip file. Binary file not written.\n");
    }
    printTimeSpent("Writing binary file (2-pass)");

    free_scrip_info_array(&all_scrips_data);
    printTimeSpent("Cleanup after writing");

    if (scrips_to_write_count > 0) {
        printf("\n--- Writing Verification Data to: %s ---\n", verification_txt_file);
        FILE *verification_file = fopen(verification_txt_file, "w");
        if (!verification_file) {
            perror("❌ Failed to open verification text file for writing");
        } else {
            // Call the modified function
            read_and_print_binary_data_to_file(output_bin_file, verification_file);
            if (fclose(verification_file) != 0) {
                perror("❌ Failed to close verification text file");
            }
            printf("✅ Verification data written to %s\n", verification_txt_file);
        }
        printTimeSpent("Writing verification data to text file");
    }

    printf("\nTotal scrips processed for writing stage: %zu\n", scrips_to_write_count);

    return 0;
}