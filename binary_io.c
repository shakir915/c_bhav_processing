#include "binary_io.h"
#include "data_structures.h" // Already included, but good for clarity
#include "utils.h"           // For LOG_ENABLED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>          // For uint64_t

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