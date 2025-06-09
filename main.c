#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// #include <minizip/unzip.h> // No longer needed directly here
// #include <float.h>        // No longer needed directly here
// #include <ctype.h>        // No longer needed directly here
// #include <stdbool.h>      // No longer needed directly here (included by others)
// #include <limits.h>       // No longer needed directly here
// #include <stdint.h>       // No longer needed directly here (included by others)

#include "utils.h"
#include "data_structures.h"
#include "zip_parser.h"
#include "binary_io.h"

// #define INITIAL_CAPACITY 100 // Moved to data_structures.h
// #define LOG_ENABLED false    // Moved to utils.h

// --- Timing ---
// clock_t lastTime; // Moved to utils.c (declared extern in utils.h)
// void printTimeSpent(const char *tag); // Moved to utils.h/utils.c

// All other functions (FloatArray, LongArray, ScripInfoArray helpers,
// JSON parsing, zip reading, binary writing/reading) have been moved
// to their respective .c files.

int main(int argc, char *argv[]) {
    lastTime = clock(); // Initialize lastTime from utils.h

    // const char *zip_file_path = "/Users/shakir/BhavAppData/DATA/TEST/1D_ALL_JSON_MoneyControl.zip";
     const char *output_bin_file = "ohlctv_values_v2.bin";
     const char *verification_txt_file = "verification_output.txt";
    //
    // if (argc > 1) {
    //     zip_file_path = argv[1];
    // }
    // if (argc > 2) {
    //     output_bin_file = argv[2];
    // }
    // if (argc > 3) {
    //     verification_txt_file = argv[3];
    // }
    //
    // printf("Processing Zip: %s\n", zip_file_path);
    // printf("Output Binary: %s\n", output_bin_file);
    // printf("Verification Text Output: %s\n", verification_txt_file);
    //
    // ScripInfoArray all_scrips_data;
    // init_scrip_info_array(&all_scrips_data);
    // printTimeSpent("Initialization");
    //
    // read_zip_and_parse_data(zip_file_path, &all_scrips_data);
    // printTimeSpent("Parsing all JSON files from ZIP");
    //
    // size_t scrips_to_write_count = all_scrips_data.count;
    //
    // if (scrips_to_write_count > 0) {
    //     if (write_binary_two_pass(output_bin_file, &all_scrips_data)) {
    //         printf("✅ Successfully wrote binary data to %s for %zu scrips.\n", output_bin_file, scrips_to_write_count);
    //     } else {
    //         fprintf(stderr, "❌ Failed to write binary data to %s\n", output_bin_file);
    //     }
    // } else {
    //     printf("ℹ️ No scrip data extracted from the zip file. Binary file not written.\n");
    // }
    // printTimeSpent("Writing binary file (2-pass)");
    //
    // free_scrip_info_array(&all_scrips_data);
    // printTimeSpent("Cleanup after writing");


        printf("\n--- Writing Verification Data to: %s ---\n", verification_txt_file);
        FILE *verification_file = fopen(verification_txt_file, "w");
        if (!verification_file) {
            perror("❌ Failed to open verification text file for writing");
        } else {
            read_and_print_binary_data_to_file(output_bin_file, verification_file);
            if (fclose(verification_file) != 0) {
                perror("❌ Failed to close verification text file");
            }
            printf("✅ Verification data written to %s\n", verification_txt_file);
        }
        printTimeSpent("Writing verification data to text file");


   // printf("\nTotal scrips processed for writing stage: %zu\n", scrips_to_write_count);

    return 0;
}