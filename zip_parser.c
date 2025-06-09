#include "zip_parser.h"
#include "data_structures.h" // Already included via zip_parser.h, but good for clarity
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>    // For isspace
#include <minizip/unzip.h> // For zip operations

// Static helper functions (not exposed in header)
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

static bool parse_json_to_scrip_info(const char *content, const char *filename_in_zip, ScripInfo *out_scrip) {
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