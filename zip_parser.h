#ifndef ZIP_PARSER_H
#define ZIP_PARSER_H

#include "data_structures.h" // For ScripInfoArray

void read_zip_and_parse_data(const char *zip_path, ScripInfoArray *all_scrips_info);

#endif // ZIP_PARSER_H