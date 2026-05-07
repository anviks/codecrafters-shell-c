#include "array.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 16

void array_init(Array* a, size_t item_size, KeyFn get_key) {
    *a = (Array){
        .items = malloc(INITIAL_CAPACITY * item_size),
        .count = 0,
        .capacity = INITIAL_CAPACITY,
        .item_size = item_size,
        .get_key = get_key,
    };
}

void array_free(Array* a) {
    free(a->items);
    a->items = NULL;
    a->count = 0;
    a->capacity = 0;
}

void* array_at(Array* a, size_t index) {
    return (char*)a->items + index * a->item_size;
}

void array_add(Array* a, void* item) {
    // Expand if usage reaches 100%
    if (a->count >= a->capacity) {
        a->capacity *= 2;
        a->items = realloc(a->items, a->capacity * a->item_size);
    }

    memcpy(array_at(a, a->count), item, a->item_size);
    a->count++;
}

void array_remove_at(Array* a, size_t index) {
    int tail = a->count - index - 1;
    // If to-be-removed element is not at the end
    if (tail > 0) {
        memmove(array_at(a, index), array_at(a, index + 1), tail * a->item_size);
    }
    a->count--;

    // Shrink if usage drops below 25%
    if (a->count * 4 < a->capacity && a->capacity > INITIAL_CAPACITY) {
        a->capacity /= 2;
        a->items = realloc(a->items, a->capacity * a->item_size);
    }
}

void array_remove(Array* a, const char* key) {
    assert(a->get_key);
    for (int i = 0; i < a->count; i++) {
        if (strcmp(a->get_key(array_at(a, i)), key) == 0) {
            array_remove_at(a, i);
            break;
        }
    }
}

void* array_find(Array* a, const char* key) {
    assert(a->get_key);
    for (int i = 0; i < a->count; i++) {
        if (strcmp(a->get_key(array_at(a, i)), key) == 0) {
            return array_at(a, i);
        }
    }

    return NULL;
}
