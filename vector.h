#pragma once

#include <stddef.h>

/* Простой generic-вектор: хранит элементы фиксированного размера elem_size.
   Используется как posting list, список результатов поиска, список кандидатов
   fuzzy-поиска и т.д. */

typedef struct {
    void*  data;
    size_t size;
    size_t capacity;
    size_t elem_size;
} Vector;

Vector* createVector(size_t elem_size);
void    vectorFree(Vector* v);

void    appendVectorItem(Vector* v, const void* elem);
void*   getVectorItem(Vector* v, size_t i);
void    setVectorItem(Vector* v, size_t i, const void* elem);
