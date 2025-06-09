#ifndef BINARY_IO_H
#define BINARY_IO_H

#include "data_structures.h" // For ScripInfoArray
#include <stdio.h>           // For FILE*

bool write_binary_two_pass(const char *output_filename, ScripInfoArray *all_scrips_info);
void read_and_print_binary_data_to_file(const char *input_filename, FILE *outfile);

#endif // BINARY_IO_H