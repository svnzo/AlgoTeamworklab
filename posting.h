#pragma once

#include "vector.h"

#define MAX_TITLE_LEN 256

typedef struct {
    int  doc_id;
    char title[MAX_TITLE_LEN];
} PostingEntry;

/* Posting list — Vector с элементами PostingEntry.
   Используйте createPostingList/appendPosting/clonePostingList,
   освобождайте через vectorFree(). */

Vector* createPostingList(void);
void    appendPosting(Vector* list, int doc_id, const char* title);
Vector* clonePostingList(const Vector* list);
