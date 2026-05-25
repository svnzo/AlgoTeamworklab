#include "vector.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 8

Vector* createVector(size_t elem_size) {
    Vector* v = malloc(sizeof(Vector));
    if (!v) return NULL;
    v->elem_size = elem_size;
    v->size      = 0;
    v->capacity  = INITIAL_CAP;
    v->data      = malloc(elem_size * INITIAL_CAP);
    if (!v->data) { free(v); return NULL; }
    return v;
}

void vectorFree(Vector* v) {
    if (!v) return;
    free(v->data);
    free(v);
}

static void grow(Vector* v) {
    size_t new_cap = v->capacity * 2;
    void*  nd      = realloc(v->data, new_cap * v->elem_size);
    if (!nd) return;
    v->data     = nd;
    v->capacity = new_cap;
}

void appendVectorItem(Vector* v, const void* elem) {
    if (!v) return;
    if (v->size >= v->capacity) grow(v);
    memcpy((char*)v->data + v->size * v->elem_size, elem, v->elem_size);
    v->size++;
}

void* getVectorItem(Vector* v, size_t i) {
    if (!v || i >= v->size) return NULL;
    return (char*)v->data + i * v->elem_size;
}

void setVectorItem(Vector* v, size_t i, const void* elem) {
    if (!v || i >= v->size) return;
    memcpy((char*)v->data + i * v->elem_size, elem, v->elem_size);
}
