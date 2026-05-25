#include "search.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TOP_K 10

/* ---------- intersect ---------- */

/* Простое пересечение N posting list'ов по doc_id.
   Алгоритм: берём самый короткий список как базу, для каждого doc_id
   проверяем что он есть во всех остальных. Финальный score — кол-во
   попаданий (для AND-семантики всегда = n). */

static int containsDoc(Vector* list, int doc_id, PostingEntry** found_out) {
    for (size_t i = 0; i < list->size; i++) {
        PostingEntry* e = getVectorItem(list, i);
        if (e->doc_id == doc_id) {
            if (found_out) *found_out = e;
            return 1;
        }
    }
    return 0;
}

Vector* intersectPostings(Vector** lists, int n) {
    Vector* out = createVector(sizeof(SearchResult));
    if (n <= 0 || !lists[0]) return out;

    /* найдём самый короткий — он определяет верхнюю границу пересечения */
    int short_idx = 0;
    for (int i = 1; i < n; i++) {
        if (!lists[i]) { return out; }
        if (lists[i]->size < lists[short_idx]->size) short_idx = i;
    }
    if (!lists[short_idx]) return out;

    Vector* base = lists[short_idx];
    for (size_t i = 0; i < base->size; i++) {
        PostingEntry* be = getVectorItem(base, i);

        int           ok       = 1;
        const char*   any_title = be->title;
        for (int j = 0; j < n && ok; j++) {
            if (j == short_idx) continue;
            PostingEntry* fnd = NULL;
            if (!containsDoc(lists[j], be->doc_id, &fnd)) { ok = 0; break; }
            if (fnd && fnd->title[0]) any_title = fnd->title;
        }
        if (!ok) continue;

        /* дедуп — если этот doc_id уже добавляли, пропускаем */
        int dup = 0;
        for (size_t k = 0; k < out->size; k++) {
            SearchResult* r = getVectorItem(out, k);
            if (r->doc_id == be->doc_id) { dup = 1; break; }
        }
        if (dup) continue;

        SearchResult r;
        r.doc_id = be->doc_id;
        strncpy(r.title, any_title, MAX_TITLE_LEN - 1);
        r.title[MAX_TITLE_LEN - 1] = '\0';
        r.score = n;
        appendVectorItem(out, &r);
    }
    return out;
}

/* ---------- query tokenize ---------- */

/* В out_tokens кладём указатели в собственный буфер buf.
   Возвращает кол-во токенов. */
static int tokenizeQuery(const char* query, char* buf, size_t buf_cap,
                         char** out_tokens, int max_tokens) {
    size_t L = strlen(query);
    if (L >= buf_cap) L = buf_cap - 1;
    memcpy(buf, query, L);
    buf[L] = '\0';

    /* lowercase + не-алфавитные в пробелы */
    for (size_t i = 0; i < L; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (!isalnum(c)) buf[i] = ' ';
        else             buf[i] = (char)tolower(c);
    }

    int n = 0;
    char* p = buf;
    while (*p && n < max_tokens) {
        while (*p == ' ') p++;
        if (!*p) break;
        out_tokens[n++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = '\0';
    }
    return n;
}

/* ---------- search (exact AND) ---------- */

SearchResults* search(Index* idx, const char* query) {
    SearchResults* sr = malloc(sizeof(SearchResults));
    sr->results = createVector(sizeof(SearchResult));
    sr->total   = 0;
    sr->time_ms = 0;

    if (!idx || !query) return sr;

    clock_t t0 = clock();

    char  qbuf[2048];
    char* toks[32];
    int   n = tokenizeQuery(query, qbuf, sizeof(qbuf), toks, 32);
    if (n == 0) return sr;

    Vector* lists[32] = {0};
    for (int i = 0; i < n; i++) {
        lists[i] = lookupTerm(idx, toks[i]);
        if (!lists[i]) {
            /* хотя бы один токен не найден — AND даёт пусто */
            sr->time_ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            return sr;
        }
    }

    Vector* hits = intersectPostings(lists, n);
    sr->total = (int)hits->size;

    /* топ-K */
    size_t take = hits->size < TOP_K ? hits->size : TOP_K;
    for (size_t i = 0; i < take; i++) {
        SearchResult* r = getVectorItem(hits, i);
        appendVectorItem(sr->results, r);
    }
    vectorFree(hits);

    sr->time_ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
    return sr;
}

/* ---------- print ---------- */

void printResultsText(const SearchResults* sr) {
    if (!sr) return;
    printf("Время: %.2f мс | Найдено: %d документов\n\n", sr->time_ms, sr->total);
    for (size_t i = 0; i < sr->results->size; i++) {
        SearchResult* r = getVectorItem((Vector*)sr->results, i);
        printf("%2zu. [id=%d] %s\n", i + 1, r->doc_id, r->title);
    }
}

/* минимальный JSON escape для печати title */
static void printEscaped(const char* s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\')        printf("\\%c", c);
        else if (c == '\n')               printf("\\n");
        else if (c == '\r')               printf("\\r");
        else if (c == '\t')               printf("\\t");
        else if (c < 0x20)                printf("\\u%04x", c);
        else                              putchar(c);
    }
}

void printResultsJSON(const SearchResults* sr) {
    if (!sr) { printf("{\"total\":0,\"time_ms\":0,\"results\":[]}\n"); return; }
    printf("{\"total\":%d,\"time_ms\":%.2f,\"results\":[",
           sr->total, sr->time_ms);
    for (size_t i = 0; i < sr->results->size; i++) {
        SearchResult* r = getVectorItem((Vector*)sr->results, i);
        if (i) printf(",");
        printf("{\"doc_id\":%d,\"title\":\"", r->doc_id);
        printEscaped(r->title);
        printf("\",\"score\":%d}", r->score);
    }
    printf("]}\n");
}

void freeSearchResults(SearchResults* sr) {
    if (!sr) return;
    vectorFree(sr->results);
    free(sr);
}

/* ---------- fuzzy search (Левенштейн) ---------- */

static int min3(int a, int b, int c) {
    int m = a < b ? a : b;
    return m < c ? m : c;
}

static int levenshtein(const char* a, const char* b, int cutoff) {
    int la = (int)strlen(a);
    int lb = (int)strlen(b);
    if (abs(la - lb) > cutoff) return cutoff + 1;

    /* rolling rows */
    static int prev[256], cur[256];
    if (lb > 255) return cutoff + 1;

    for (int j = 0; j <= lb; j++) prev[j] = j;
    for (int i = 1; i <= la; i++) {
        cur[0] = i;
        int row_min = cur[0];
        for (int j = 1; j <= lb; j++) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            cur[j] = min3(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost);
            if (cur[j] < row_min) row_min = cur[j];
        }
        if (row_min > cutoff) return cutoff + 1;   /* ранний выход */
        memcpy(prev, cur, sizeof(int) * (lb + 1));
    }
    return prev[lb];
}

typedef struct {
    const char* needle;
    int         max_dist;
    Vector*     out;   /* FuzzyCandidate */
} FuzzyCtx;

static void fuzzyVisit(const char* key, Vector* postings, void* ctx) {
    FuzzyCtx* fc = (FuzzyCtx*)ctx;
    int d = levenshtein(fc->needle, key, fc->max_dist);
    if (d <= fc->max_dist) {
        FuzzyCandidate c;
        strncpy(c.term, key, sizeof(c.term) - 1);
        c.term[sizeof(c.term) - 1] = '\0';
        c.distance = d;
        c.postings = postings;
        appendVectorItem(fc->out, &c);
    }
}

Vector* fuzzyFindCandidates(Index* idx, const char* term, int max_distance) {
    Vector* out = createVector(sizeof(FuzzyCandidate));
    if (!idx || !term) return out;
    FuzzyCtx fc = { .needle = term, .max_dist = max_distance, .out = out };
    traverseIndex(idx, fuzzyVisit, &fc);
    return out;
}

/* Объединение posting list'ов всех кандидатов одного токена.
   Дедупликация по doc_id. */
static Vector* unionCandidates(Vector* cands) {
    Vector* out = createVector(sizeof(PostingEntry));
    for (size_t ci = 0; ci < cands->size; ci++) {
        FuzzyCandidate* c = getVectorItem(cands, ci);
        if (!c->postings) continue;
        for (size_t i = 0; i < c->postings->size; i++) {
            PostingEntry* e = getVectorItem(c->postings, i);
            int dup = 0;
            for (size_t k = 0; k < out->size; k++) {
                PostingEntry* o = getVectorItem(out, k);
                if (o->doc_id == e->doc_id) { dup = 1; break; }
            }
            if (!dup) appendVectorItem(out, e);
        }
    }
    return out;
}

SearchResults* fuzzySearch(Index* idx, const char* query, int max_distance) {
    SearchResults* sr = malloc(sizeof(SearchResults));
    sr->results = createVector(sizeof(SearchResult));
    sr->total   = 0;
    sr->time_ms = 0;
    if (!idx || !query) return sr;

    clock_t t0 = clock();

    char  qbuf[2048];
    char* toks[32];
    int   n = tokenizeQuery(query, qbuf, sizeof(qbuf), toks, 32);
    if (n == 0) return sr;

    /* для каждого токена собираем кандидатов и объединяем их postings */
    Vector* per_token[32] = {0};
    int     avg_dist_sum  = 0;
    int     avg_dist_cnt  = 0;

    for (int i = 0; i < n; i++) {
        Vector* cands = fuzzyFindCandidates(idx, toks[i], max_distance);
        for (size_t k = 0; k < cands->size; k++) {
            FuzzyCandidate* c = getVectorItem(cands, k);
            avg_dist_sum += c->distance;
            avg_dist_cnt++;
        }
        per_token[i] = unionCandidates(cands);
        vectorFree(cands);
        if (per_token[i]->size == 0) {
            /* нет кандидатов для этого токена — AND даёт пусто */
            for (int j = 0; j <= i; j++) vectorFree(per_token[j]);
            sr->time_ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            return sr;
        }
    }

    Vector* hits = intersectPostings(per_token, n);

    int avg = avg_dist_cnt ? (avg_dist_sum / avg_dist_cnt) : 0;
    /* ранжируем — score = matched_terms * 10 - avg_distance */
    for (size_t i = 0; i < hits->size; i++) {
        SearchResult* r = getVectorItem(hits, i);
        r->score = n * 10 - avg;
    }

    sr->total = (int)hits->size;
    size_t take = hits->size < TOP_K ? hits->size : TOP_K;
    for (size_t i = 0; i < take; i++) {
        SearchResult* r = getVectorItem(hits, i);
        appendVectorItem(sr->results, r);
    }
    vectorFree(hits);
    for (int i = 0; i < n; i++) vectorFree(per_token[i]);

    sr->time_ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
    return sr;
}
