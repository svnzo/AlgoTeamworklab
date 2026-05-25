#include "btree.h"
#include <stdlib.h>
#include <string.h>

/* Классическая B-tree с минимальной степенью t = BTREE_T (=3).
   Каждый узел хранит от t-1 до 2t-1 ключей, у внутренних узлов
   на 1 больше детей. Алгоритм вставки — "split на спуске":
   если ребёнок полный, расщепляем до спуска в него. */

static BTreeNode* newNode(int is_leaf) {
    BTreeNode* n = malloc(sizeof(BTreeNode));
    n->n        = 0;
    n->is_leaf  = is_leaf;
    for (int i = 0; i < BTREE_MAX_KEYS; i++) {
        n->keys[i]     = NULL;
        n->postings[i] = NULL;
    }
    for (int i = 0; i < BTREE_MAX_CH; i++) {
        n->children[i] = NULL;
    }
    return n;
}

BTree* createBTree(void) {
    BTree* t = malloc(sizeof(BTree));
    t->root = newNode(1);
    t->size = 0;
    return t;
}

static void freeNodeRec(BTreeNode* n) {
    if (!n) return;
    if (!n->is_leaf) {
        for (int i = 0; i <= n->n; i++) freeNodeRec(n->children[i]);
    }
    for (int i = 0; i < n->n; i++) {
        free(n->keys[i]);
        vectorFree(n->postings[i]);
    }
    free(n);
}

void freeBTree(BTree* tree) {
    if (!tree) return;
    freeNodeRec(tree->root);
    free(tree);
}

/* Поиск в узле: возвращает индекс ключа в массиве или -1.
   Линейный — для t=3 это ок. */
static int findKeyInNode(BTreeNode* n, const char* key, int* found) {
    int i = 0;
    while (i < n->n && strcmp(key, n->keys[i]) > 0) i++;
    *found = (i < n->n && strcmp(key, n->keys[i]) == 0);
    return i;
}

static Vector* searchRec(BTreeNode* n, const char* key) {
    if (!n) return NULL;
    int found = 0;
    int i = findKeyInNode(n, key, &found);
    if (found) return n->postings[i];
    if (n->is_leaf) return NULL;
    return searchRec(n->children[i], key);
}

Vector* btreeSearch(const BTree* tree, const char* key) {
    if (!tree || !key) return NULL;
    return searchRec(tree->root, key);
}

/* Расщепление полного ребёнка y = x->children[i].
   После сплита средний ключ y поднимается в x. */
static void splitChild(BTreeNode* x, int i) {
    BTreeNode* y = x->children[i];
    BTreeNode* z = newNode(y->is_leaf);
    z->n = BTREE_T - 1;

    /* правая половина ключей идёт в z */
    for (int j = 0; j < BTREE_T - 1; j++) {
        z->keys[j]     = y->keys[j + BTREE_T];
        z->postings[j] = y->postings[j + BTREE_T];
        y->keys[j + BTREE_T]     = NULL;
        y->postings[j + BTREE_T] = NULL;
    }
    if (!y->is_leaf) {
        for (int j = 0; j < BTREE_T; j++) {
            z->children[j] = y->children[j + BTREE_T];
            y->children[j + BTREE_T] = NULL;
        }
    }
    y->n = BTREE_T - 1;

    /* сдвигаем детей x вправо */
    for (int j = x->n; j >= i + 1; j--) {
        x->children[j + 1] = x->children[j];
    }
    x->children[i + 1] = z;

    /* сдвигаем ключи x */
    for (int j = x->n - 1; j >= i; j--) {
        x->keys[j + 1]     = x->keys[j];
        x->postings[j + 1] = x->postings[j];
    }
    /* медианный ключ y поднимается в x */
    x->keys[i]     = y->keys[BTREE_T - 1];
    x->postings[i] = y->postings[BTREE_T - 1];
    y->keys[BTREE_T - 1]     = NULL;
    y->postings[BTREE_T - 1] = NULL;
    x->n++;
}

/* Вставка в узел, который точно не полный.
   Возвращает 1 если ключа раньше не было (нужно size++). */
static int insertNonFull(BTreeNode* x, const char* key,
                         int doc_id, const char* title) {
    int i = x->n - 1;

    /* сначала проверим — есть ли уже такой ключ в этом узле */
    for (int j = 0; j < x->n; j++) {
        if (strcmp(key, x->keys[j]) == 0) {
            appendPosting(x->postings[j], doc_id, title);
            return 0;
        }
    }

    if (x->is_leaf) {
        while (i >= 0 && strcmp(key, x->keys[i]) < 0) {
            x->keys[i + 1]     = x->keys[i];
            x->postings[i + 1] = x->postings[i];
            i--;
        }
        x->keys[i + 1]     = strdup(key);
        x->postings[i + 1] = createPostingList();
        appendPosting(x->postings[i + 1], doc_id, title);
        x->n++;
        return 1;
    } else {
        while (i >= 0 && strcmp(key, x->keys[i]) < 0) i--;
        i++;
        if (x->children[i]->n == BTREE_MAX_KEYS) {
            splitChild(x, i);
            /* после сплита может оказаться, что нужно идти вправо
               (или попали в медианный ключ) */
            int cmp = strcmp(key, x->keys[i]);
            if (cmp == 0) {
                appendPosting(x->postings[i], doc_id, title);
                return 0;
            }
            if (cmp > 0) i++;
        }
        return insertNonFull(x->children[i], key, doc_id, title);
    }
}

void btreeInsert(BTree* tree, const char* key, int doc_id, const char* title) {
    if (!tree || !key) return;
    BTreeNode* r = tree->root;
    if (r->n == BTREE_MAX_KEYS) {
        /* корень полный — создаём нового и сплитим */
        BTreeNode* s = newNode(0);
        s->children[0] = r;
        tree->root = s;
        splitChild(s, 0);
        int added = insertNonFull(s, key, doc_id, title);
        if (added) tree->size++;
    } else {
        int added = insertNonFull(r, key, doc_id, title);
        if (added) tree->size++;
    }
}

static void traverseRec(BTreeNode* n,
                        void (*visit)(const char*, Vector*, void*),
                        void* ctx) {
    if (!n) return;
    int i;
    for (i = 0; i < n->n; i++) {
        if (!n->is_leaf) traverseRec(n->children[i], visit, ctx);
        visit(n->keys[i], n->postings[i], ctx);
    }
    if (!n->is_leaf) traverseRec(n->children[i], visit, ctx);
}

void btreeTraverse(const BTree* tree,
                   void (*visit)(const char* key, Vector* postings, void* ctx),
                   void* ctx) {
    if (!tree) return;
    traverseRec(tree->root, visit, ctx);
}
