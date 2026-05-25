#pragma once

#include "index.h"

typedef struct {
    int  doc_id;
    char title[MAX_TITLE_LEN];
    int  score;
} SearchResult;

typedef struct {
    Vector* results;   /* elements: SearchResult, топ-10 */
    int     total;     /* всего найдено документов       */
    double  time_ms;
} SearchResults;

Vector*        intersectPostings(Vector** lists, int n);
SearchResults* search(Index* idx, const char* query);
void           printResultsText(const SearchResults* sr);
void           printResultsJSON(const SearchResults* sr);
void           freeSearchResults(SearchResults* sr);

/* ---- fuzzy search (Левенштейн) ---- */

typedef struct {
    char    term[128];
    int     distance;
    Vector* postings;   /* shared ref внутри индекса, не освобождать вручную */
} FuzzyCandidate;

Vector*        fuzzyFindCandidates(Index* idx, const char* term, int max_distance);
SearchResults* fuzzySearch(Index* idx, const char* query, int max_distance);
