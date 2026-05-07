#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>

typedef const char* (*KeyFn)(const void*);
typedef struct {
    void* items;
    int count;
    int capacity;
    size_t item_size;
    KeyFn get_key;
} Array;

void array_init(Array* a, size_t item_size, KeyFn get_key);
void array_free(Array* a);
void* array_at(Array* a, size_t index);
void array_add(Array* a, void* item);
void array_remove_at(Array* a, size_t index);
void array_remove(Array* a, const char* key);
void* array_find(Array* a, const char* key);

#endif  // ARRAY_H
