#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <minizip/unzip.h>
#include <float.h>
#include <ctype.h>   // For isspace
#include <stdbool.h> // For bool type
#include <limits.h>  // For LONG_MIN



// Helper function to find, parse, and write a numeric array of LONG INTs for a given key
// content: The JSON string
// array_key_prefix: e.g., "\"t\":["
// fout: File to write long int values to
// max_val_aggregate: Pointer to update with the maximum value found in this array
// Returns: 1 if successful and values found, 0 if key not found/empty array, -1 on error
static int extract_and_process_long_array(const char* content, const char* array_key_prefix, FILE* fout, long int* max_val_aggregate) {
    const char* p = strstr(content, array_key_prefix);
    if (!p) {
        return 0; // Key not found
    }
    p += strlen(array_key_prefix); // Move past the key and '['

    const char* end_of_array = strchr(p, ']');
    if (!end_of_array) {
        // fprintf(stderr, "⚠️ Malformed JSON: Closing ']' not found for key '%s'\n", array_key_prefix);
        return -1; // Malformed JSON part
    }

    long int current_array_max = LONG_MIN;
    int values_found_in_array = 0;
    char* current_pos = (char*)p;

    while (current_pos < end_of_array && *current_pos != '\0') {
        char* next_val_ptr;
        long int value = strtol(current_pos, &next_val_ptr, 10); // Base 10

        if (current_pos == next_val_ptr) { // No conversion or error
            if (*current_pos == ',' || isspace((unsigned char)*current_pos)) {
                current_pos++;
                continue;
            }
            break;
        }

        if (value > current_array_max) {
            current_array_max = value;
        }

        if (fout && fwrite(&value, sizeof(long int), 1, fout) != 1) {
            perror("❌ Failed to write long int value to binary file");
            return -1; // Write error
        }
        values_found_in_array++;

        current_pos = next_val_ptr;
        while (current_pos < end_of_array && (*current_pos == ',' || isspace((unsigned char)*current_pos))) {
            current_pos++;
        }
        if (current_pos >= end_of_array || *current_pos == ']') {
            break;
        }
    }

    if (values_found_in_array > 0) {
        *max_val_aggregate = current_array_max;
        return 1; // Success
    }
    return 0; // No values processed
}






// Helper function to find, parse, and write a numeric array for a given key
// content: The JSON string
// array_key_prefix: e.g., "\"h\":["
// fout: File to write float values to
// max_val_aggregate: Pointer to update with the maximum value found in this array
// Returns: 1 if successful and values found, 0 if key not found/empty array, -1 on error
static int extract_and_process_array(const char* content, const char* array_key_prefix, FILE* fout, float* max_val_aggregate) {
    const char* p = strstr(content, array_key_prefix);
    if (!p) {
        // Key not found, not necessarily an error, could be optional data
        return 0;
    }
    p += strlen(array_key_prefix); // Move past the key and '['

    const char* end_of_array = strchr(p, ']');
    if (!end_of_array) {
        // fprintf(stderr, "⚠️ Malformed JSON: Closing ']' not found for key '%s'\n", array_key_prefix);
        return -1; // Malformed JSON part
    }

    float current_array_max = -FLT_MAX;
    int values_found_in_array = 0;
    char* current_pos = (char*)p; // Current position in the numeric data part of the array

    while (current_pos < end_of_array && *current_pos != '\0') {
        char* next_val_ptr;
        float value = strtof(current_pos, &next_val_ptr);

        if (current_pos == next_val_ptr) { // No conversion happened or error
            // Skip non-numeric characters like commas or whitespace if any, otherwise break
            if (*current_pos == ',' || isspace((unsigned char)*current_pos)) {
                current_pos++;
                continue;
            }
            break; // Assume end of numbers or malformed section
        }

        if (value > current_array_max) {
            current_array_max = value;
        }

        if (fout && fwrite(&value, sizeof(float), 1, fout) != 1) {
            perror("❌ Failed to write value to binary file");
            return -1; // Write error
        }
        values_found_in_array++;

        current_pos = next_val_ptr;
        // Skip comma and whitespace before next number or end of array
        while (current_pos < end_of_array && (*current_pos == ',' || isspace((unsigned char)*current_pos))) {
            current_pos++;
        }
        if (current_pos >= end_of_array || *current_pos == ']') { // Reached end of array content or ']'
            break;
        }
    }

    if (values_found_in_array > 0) {
        *max_val_aggregate = current_array_max;
        return 1; // Success, values processed
    }
    return 0; // No values processed (e.g., empty array or key found but no valid numbers)
}



void process_json(const char* content, const char*  filename) {
    // Define keys: o, h, l, c (floats) first, then t, v (longs)
    #define NUM_FLOAT_KEYS 4
    #define NUM_LONG_KEYS 2
    #define TOTAL_KEYS (NUM_FLOAT_KEYS + NUM_LONG_KEYS)

    const char* keys_prefix[TOTAL_KEYS] = {
        "\"o\":[", "\"h\":[", "\"l\":[", "\"c\":[", // Floats
        "\"t\":[", "\"v\":["                         // Longs
    };
    const char* key_names[TOTAL_KEYS] = {
        "Open", "High", "Low", "Close",             // Floats
        "Timestamp", "Volume"                       // Longs
    };

    float max_float_values[NUM_FLOAT_KEYS];
    long int max_long_values[NUM_LONG_KEYS];
    bool key_processed_successfully[TOTAL_KEYS];

    for (int i = 0; i < NUM_FLOAT_KEYS; ++i) max_float_values[i] = -FLT_MAX;
    for (int i = 0; i < NUM_LONG_KEYS; ++i) max_long_values[i] = LONG_MIN;
    for (int i = 0; i < TOTAL_KEYS; ++i) key_processed_successfully[i] = false;

    // Output file name reflects all data types
    FILE* fout = fopen("ohlctv_values.bin", "ab");
    if (!fout) {
        perror("❌ Failed to open ohlctv_values.bin for append");
        return;
    }

    bool any_data_processed_overall = false;

    for (int i = 0; i < TOTAL_KEYS; ++i) {
        int result;
        if (i < NUM_FLOAT_KEYS) { // Processing float keys (o, h, l, c)
            float current_key_max_float = -FLT_MAX;
            result = extract_and_process_array(content, keys_prefix[i], fout, &current_key_max_float);
            if (result == 1) {
                max_float_values[i] = current_key_max_float;
            }
        } else { // Processing long int keys (t, v)
            long int current_key_max_long = LONG_MIN;
            // Index for max_long_values is (i - NUM_FLOAT_KEYS)
            result = extract_and_process_long_array(content, keys_prefix[i], fout, &current_key_max_long);
            if (result == 1) {
                max_long_values[i - NUM_FLOAT_KEYS] = current_key_max_long;
            }
        }

        if (result == 1) {
            key_processed_successfully[i] = true;
            any_data_processed_overall = true;
        } else if (result == -1) {
            fprintf(stderr, "❌ Error processing data for key %s\n", key_names[i]);
            fclose(fout);
            return;
        }
    }

    long file_size_bytes = 0;
    double file_size_mb = 0.0;

    if (any_data_processed_overall) {
        if (fflush(fout) != 0) {
            perror("❌ Failed to flush output file");
        }
        if (fseek(fout, 0, SEEK_END) == 0) {
            file_size_bytes = ftell(fout);
            if (file_size_bytes == -1L) {
                perror("❌ Failed to get file size (ftell)");
            } else {
                file_size_mb = (double)file_size_bytes / (1024.0 * 1024.0);
            }
        } else {
            perror("❌ Failed to seek to end of output file (fseek)");
        }
    }

    if (fclose(fout) != 0) {
        perror("❌ Failed to close output file");
    }

    if (any_data_processed_overall) {
       // printf("📈 Processed: ");
        for (int i = 0; i < TOTAL_KEYS; ++i) {
            if (key_processed_successfully[i]) {
                if (i < NUM_FLOAT_KEYS) {
                    //printf("%s Max: %.2f", key_names[i], max_float_values[i]);
                } else {
                   // printf("%s Max: %ld", key_names[i], max_long_values[i - NUM_FLOAT_KEYS]);
                }
            } else {
                printf("%s: (no data)", key_names[i]);
            }
            if (i < TOTAL_KEYS - 1) {
                //printf(" | ");
            }
        }
        printf(" %s | File size: %.1f MB\n", filename ,file_size_mb);
    } else {
        // printf("ℹ️ No o, h, l, c, t, v data found in this JSON content.\n");
    }
}








void read_zip(const char* zip_path) {
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
        char filename[256];
        unz_file_info file_info;

        if (unzGetCurrentFileInfo(zip, &file_info, filename, sizeof(filename), NULL, 0, NULL, 0) == UNZ_OK) {
            if (strstr(filename, ".json") && !strstr(filename, "__MACOSX/")) {
                if (unzOpenCurrentFile(zip) == UNZ_OK) {
                    char *buffer = malloc(file_info.uncompressed_size + 1);
                    if (!buffer) {
                        printf("❌ Memory allocation failed\n");
                        unzCloseCurrentFile(zip);
                        continue;
                    }

                    unzReadCurrentFile(zip, buffer, file_info.uncompressed_size);
                    buffer[file_info.uncompressed_size] = '\0';

                    process_json(buffer,filename);
                    free(buffer);

                    unzCloseCurrentFile(zip);
                }
            }
        }
    } while (unzGoToNextFile(zip) == UNZ_OK);

    unzClose(zip);
}

int main(int argc, char *argv[]) {
    // Clean/create the output file at the start
    FILE* fout_init = fopen("ohlctv_values.bin", "wb"); // Updated filename
    if (!fout_init) {
        perror("❌ Failed to initialize output file ohlctv_values.bin");
        return 1;
    }
    fclose(fout_init);

    clock_t start_time = clock();  // ⏱️ Start timer

    read_zip("/Users/shakir/BhavAppData/DATA/TEST/1D_ALL_JSON_MoneyControl.zip");

    clock_t end_time = clock();    // ⏱️ End timer

    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("🕒 Total time: %.3f seconds\n", time_spent);

    return 0;
}