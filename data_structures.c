#include "data_structures.h"
#include <stdio.h>  // For perror
#include <stdlib.h> // For malloc, realloc, free

// --- FloatArray Helper Functions ---
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

// --- ScripInfoArray Helper Functions ---
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