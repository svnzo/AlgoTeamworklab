#include "index.h"
#include "../avl/avl.h"
#include "../rbtree/rbtree.h"
#include "../btree/btree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------- create/free ---------- */

Index* createIndex(TreeType type) {
    Index* idx = malloc(sizeof(Index));
    idx->type = type;
    switch (type) {
        case TREE_AVL:   idx->tree = createAVLTree(); break;
        case TREE_RB:    idx->tree = createRBTree();  break;
        case TREE_BTREE: idx->tree = createBTree();   break;
    }
    return idx;
}

void freeIndex(Index* idx) {
    if (!idx) return;
    switch (idx->type) {
        case TREE_AVL:   freeAVLTree((AVLTree*)idx->tree);   break;
        case TREE_RB:    freeRBTree((RBTree*)idx->tree);     break;
        case TREE_BTREE: freeBTree((BTree*)idx->tree);       break;
    }
    free(idx);
}

/* ---------- insert/lookup/traverse ---------- */

void insertTerm(Index* idx, const char* term, int doc_id, const char* title) {
    if (!idx) return;
    switch (idx->type) {
        case TREE_AVL:   avlInsert((AVLTree*)idx->tree, term, doc_id, title); break;
        case TREE_RB:    rbInsert((RBTree*)idx->tree, term, doc_id, title);   break;
        case TREE_BTREE: btreeInsert((BTree*)idx->tree, term, doc_id, title); break;
    }
}

Vector* lookupTerm(const Index* idx, const char* term) {
    if (!idx) return NULL;
    switch (idx->type) {
        case TREE_AVL:   return avlSearch((AVLTree*)idx->tree, term);
        case TREE_RB:    return rbSearch((RBTree*)idx->tree, term);
        case TREE_BTREE: return btreeSearch((BTree*)idx->tree, term);
    }
    return NULL;
}

void indexDocument(Index* idx, int doc_id, const char* title,
                   const char** tokens, int n_tokens) {
    if (!idx) return;
    for (int i = 0; i < n_tokens; i++) {
        insertTerm(idx, tokens[i], doc_id, title);
    }
}

void traverseIndex(const Index* idx,
                   void (*visit)(const char*, Vector*, void*), void* ctx) {
    if (!idx) return;
    switch (idx->type) {
        case TREE_AVL:   avlTraverse((AVLTree*)idx->tree, visit, ctx); break;
        case TREE_RB:    rbTraverse((RBTree*)idx->tree, visit, ctx);   break;
        case TREE_BTREE: btreeTraverse((BTree*)idx->tree, visit, ctx); break;
    }
}

/* ---------- type helpers ---------- */

TreeType parseType(const char* s) {
    if (!s) return TREE_AVL;
    if (strcmp(s, "avl")   == 0) return TREE_AVL;
    if (strcmp(s, "rb")    == 0) return TREE_RB;
    if (strcmp(s, "btree") == 0) return TREE_BTREE;
    fprintf(stderr, "Unknown tree type: %s, fallback to avl\n", s);
    return TREE_AVL;
}

const char* typeName(TreeType t) {
    switch (t) {
        case TREE_AVL:   return "avl";
        case TREE_RB:    return "rb";
        case TREE_BTREE: return "btree";
    }
    return "?";
}

/* ---------- save / load ----------
   Формат: каждая строка — "term\tdoc_id\ttitle".
   Title не должен содержать \t или \n (при парсинге JSONL мы их схлопываем
   в пробел в readJsonString). */

static void writeEntries(const char* key, Vector* postings, void* ctx) {
    FILE* f = (FILE*)ctx;
    for (size_t i = 0; i < postings->size; i++) {
        PostingEntry* e = getVectorItem(postings, i);
        fprintf(f, "%s\t%d\t%s\n", key, e->doc_id, e->title);
    }
}

void saveIndex(const Index* idx, const char* path) {
    if (!idx) return;
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "Cannot open for write: %s\n", path); return; }
    traverseIndex(idx, writeEntries, f);
    fclose(f);
}

Index* loadIndex(const char* path, TreeType type) {
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return NULL; }

    Index* idx = createIndex(type);
    char  line[4096];
    while (fgets(line, sizeof(line), f)) {
        size_t L = strlen(line);
        if (L && line[L - 1] == '\n') line[L - 1] = '\0';

        char* tab1 = strchr(line, '\t');
        if (!tab1) continue;
        *tab1 = '\0';
        const char* term = line;

        char* tab2 = strchr(tab1 + 1, '\t');
        if (!tab2) continue;
        *tab2 = '\0';
        int doc_id = atoi(tab1 + 1);

        const char* title = tab2 + 1;
        insertTerm(idx, term, doc_id, title);
    }
    fclose(f);
    return idx;
}

/* ---------- JSONL парсер для docs.jsonl ----------
   Ожидаемый формат строки:
   {"doc_id": "123", "title": "...", "tokens": ["t1", "t2", ...]}
   Свой мини-парсер, без зависимостей. */

static const char* afterTag(const char* s, const char* tag) {
    const char* p = strstr(s, tag);
    return p ? p + strlen(tag) : NULL;
}

/* Читает JSON-строку (без открывающей кавычки) в out до закрывающей кавычки.
   Обрабатывает \" \\ \/ \n \t \uXXXX (последний — заменяет на '?'). */
static const char* readJsonString(const char* s, char* out, size_t cap) {
    size_t o = 0;
    while (*s && o + 1 < cap) {
        if (*s == '\\' && (s[1] == '"' || s[1] == '\\' || s[1] == '/')) {
            out[o++] = s[1];
            s += 2;
        } else if (*s == '\\' && (s[1] == 'n' || s[1] == 't' || s[1] == 'r')) {
            out[o++] = ' ';   /* схлопываем перевод строки/таб чтобы не ломать TSV */
            s += 2;
        } else if (*s == '\\' && s[1] == 'u') {
            out[o++] = '?';
            s += 6;
        } else if (*s == '"') {
            out[o] = '\0';
            return s;
        } else {
            out[o++] = *s++;
        }
    }
    out[o] = '\0';
    return NULL;
}

typedef struct {
    int   doc_id;
    char  title[MAX_TITLE_LEN];
    char  tokbuf[16 * 1024];
    char* tokens[2048];
    int   n_tokens;
} ParsedDoc;

static int parseDocLine(const char* line, ParsedDoc* d) {
    d->n_tokens = 0;

    const char* p = afterTag(line, "\"doc_id\":");
    if (!p) return 0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') {
        char buf[64];
        p = readJsonString(p + 1, buf, sizeof(buf));
        if (!p) return 0;
        d->doc_id = atoi(buf);
    } else {
        d->doc_id = atoi(p);
    }

    p = afterTag(line, "\"title\":");
    if (!p) return 0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return 0;
    p = readJsonString(p + 1, d->title, MAX_TITLE_LEN);
    if (!p) return 0;

    p = afterTag(line, "\"tokens\":");
    if (!p) return 0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '[') return 0;
    p++;

    size_t bufoff = 0;
    int    max_tokens = (int)(sizeof(d->tokens) / sizeof(d->tokens[0]));
    while (*p && *p != ']') {
        while (*p == ' ' || *p == ',' || *p == '\t') p++;
        if (*p == ']' || *p == '\0') break;
        if (*p != '"') return 0;

        char* tokstart = d->tokbuf + bufoff;
        size_t remaining = sizeof(d->tokbuf) - bufoff;
        if (remaining < 2) break;

        const char* end = readJsonString(p + 1, tokstart, remaining);
        if (!end) return 0;

        size_t L = strlen(tokstart);
        if (L > 0 && d->n_tokens < max_tokens) {
            d->tokens[d->n_tokens++] = tokstart;
            bufoff += L + 1;
        }
        p = end + 1;
    }
    return 1;
}

void runIndex(TreeType type, const char* data_path, const char* idx_path) {
    FILE* f = fopen(data_path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open data: %s\n", data_path);
        return;
    }

    Index* idx = createIndex(type);

    size_t LINE_CAP = 64 * 1024;
    char*  line = malloc(LINE_CAP);
    if (!line) { fclose(f); freeIndex(idx); return; }

    ParsedDoc d;
    int       n_docs = 0;
    clock_t   t0 = clock();

    while (fgets(line, LINE_CAP, f)) {
        if (!parseDocLine(line, &d)) continue;
        indexDocument(idx, d.doc_id, d.title,
                      (const char**)d.tokens, d.n_tokens);
        n_docs++;
        if (n_docs % 10000 == 0) {
            fprintf(stderr, "Indexed %d docs...\n", n_docs);
        }
    }
    free(line);
    fclose(f);

    double sec = (double)(clock() - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "Done: %d docs in %.2fs (%s)\n",
            n_docs, sec, typeName(type));

    saveIndex(idx, idx_path);
    fprintf(stderr, "Index saved: %s\n", idx_path);
    freeIndex(idx);
}
