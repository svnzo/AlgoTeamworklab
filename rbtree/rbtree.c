#include "rbtree.h"
#include <stdlib.h>
#include <string.h>

/* Реализация по CLRS (глава 13).
   Используем sentinel-узел nil вместо NULL — это упрощает фиксап. */

static RBNode* makeNil(void) {
    RBNode* n = malloc(sizeof(RBNode));
    n->key      = NULL;
    n->color    = RB_BLACK;
    n->postings = NULL;
    n->left = n->right = n->parent = n;
    return n;
}

RBTree* createRBTree(void) {
    RBTree* t = malloc(sizeof(RBTree));
    t->nil  = makeNil();
    t->root = t->nil;
    t->size = 0;
    return t;
}

static void freeSubtree(RBNode* n, RBNode* nil) {
    if (n == nil) return;
    freeSubtree(n->left, nil);
    freeSubtree(n->right, nil);
    free(n->key);
    vectorFree(n->postings);
    free(n);
}

void freeRBTree(RBTree* tree) {
    if (!tree) return;
    freeSubtree(tree->root, tree->nil);
    free(tree->nil);
    free(tree);
}

static void leftRotate(RBTree* t, RBNode* x) {
    RBNode* y = x->right;
    x->right = y->left;
    if (y->left != t->nil) y->left->parent = x;
    y->parent = x->parent;
    if (x->parent == t->nil)        t->root = y;
    else if (x == x->parent->left)  x->parent->left = y;
    else                            x->parent->right = y;
    y->left = x;
    x->parent = y;
}

static void rightRotate(RBTree* t, RBNode* x) {
    RBNode* y = x->left;
    x->left = y->right;
    if (y->right != t->nil) y->right->parent = x;
    y->parent = x->parent;
    if (x->parent == t->nil)        t->root = y;
    else if (x == x->parent->right) x->parent->right = y;
    else                            x->parent->left = y;
    y->right = x;
    x->parent = y;
}

/* Фиксап после вставки: восстановление красно-чёрных свойств */
static void insertFixup(RBTree* t, RBNode* z) {
    while (z->parent->color == RB_RED) {
        if (z->parent == z->parent->parent->left) {
            RBNode* y = z->parent->parent->right; /* "дядя" */
            if (y->color == RB_RED) {
                z->parent->color = RB_BLACK;
                y->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    leftRotate(t, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rightRotate(t, z->parent->parent);
            }
        } else {
            RBNode* y = z->parent->parent->left;
            if (y->color == RB_RED) {
                z->parent->color = RB_BLACK;
                y->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rightRotate(t, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                leftRotate(t, z->parent->parent);
            }
        }
    }
    t->root->color = RB_BLACK;
}

static RBNode* findNode(const RBTree* t, const char* key) {
    RBNode* x = t->root;
    while (x != t->nil) {
        int c = strcmp(key, x->key);
        if (c == 0) return x;
        x = (c < 0) ? x->left : x->right;
    }
    return NULL;
}

void rbInsert(RBTree* t, const char* key, int doc_id, const char* title) {
    if (!t || !key) return;

    /* Если ключ уже есть — просто докидываем в posting */
    RBNode* exist = findNode(t, key);
    if (exist) {
        appendPosting(exist->postings, doc_id, title);
        return;
    }

    RBNode* z = malloc(sizeof(RBNode));
    z->key      = strdup(key);
    z->color    = RB_RED;
    z->postings = createPostingList();
    z->left = z->right = z->parent = t->nil;
    appendPosting(z->postings, doc_id, title);

    RBNode* y = t->nil;
    RBNode* x = t->root;
    while (x != t->nil) {
        y = x;
        x = (strcmp(z->key, x->key) < 0) ? x->left : x->right;
    }
    z->parent = y;
    if (y == t->nil)                       t->root = z;
    else if (strcmp(z->key, y->key) < 0)   y->left = z;
    else                                   y->right = z;

    insertFixup(t, z);
    t->size++;
}

Vector* rbSearch(const RBTree* t, const char* key) {
    if (!t || !key) return NULL;
    RBNode* n = findNode(t, key);
    return n ? n->postings : NULL;
}

static void traverseRec(RBNode* n, RBNode* nil,
                        void (*visit)(const char*, Vector*, void*), void* ctx) {
    if (n == nil) return;
    traverseRec(n->left, nil, visit, ctx);
    visit(n->key, n->postings, ctx);
    traverseRec(n->right, nil, visit, ctx);
}

void rbTraverse(const RBTree* t,
                void (*visit)(const char* key, Vector* postings, void* ctx),
                void* ctx) {
    if (!t) return;
    traverseRec(t->root, t->nil, visit, ctx);
}
