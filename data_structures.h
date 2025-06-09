#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stddef.h> // For size_t
#include <stdbool.h>
#include <stdint.h> // For uint64_t

#define INITIAL_CAPACITY 100
#define NUM_FLOAT_KEYS_CONST 4
#define NUM_LONG_KEYS_CONST 2

// --- Dynamic array for floats ---
typedef struct {
    float *data;
    size_t count;
    size_t capacity;
} FloatArray;

void init_float_array(FloatArray *arr);
bool add_to_float_array(FloatArray *arr, float value);
void free_float_array(FloatArray *arr);

// --- Dynamic array for long ints ---
typedef struct {
    long int *data;
    size_t count;
    size_t capacity;
} LongArray;

void init_long_array(LongArray *arr);
bool add_to_long_array(LongArray *arr, long int value);
void free_long_array(LongArray *arr);

// --- ScripInfo and ScripInfoArray ---
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

void init_scrip_info_array(ScripInfoArray *arr);
void add_to_scrip_info_array(ScripInfoArray *arr, ScripInfo scrip_to_add);
void free_scrip_info_array(ScripInfoArray *arr);

#endif // DATA_STRUCTURES_H