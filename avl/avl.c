#include "avl.h"
#include <stdlib.h>
#include <string.h>

static int height(AVLNode* n) { return n ? n->height : 0; }

static int max_int(int a, int b) { return a > b ? a : b; }

static void updHeight(AVLNode* n) {
    n->height = 1 + max_int(height(n->left), height(n->right));
}

static int balanceFactor(AVLNode* n) {
    return n ? height(n->left) - height(n->right) : 0;
}

static AVLNode* newNode(const char* key) {
    AVLNode* n = malloc(sizeof(AVLNode));
    n->key      = strdup(key);
    n->height   = 1;
    n->postings = createPostingList();
    n->left     = NULL;
    n->right    = NULL;
    return n;
}

/* правый поворот */
static AVLNode* rotR(AVLNode* y) {
    AVLNode* x  = y->left;
    AVLNode* t2 = x->right;
    x->right = y;
    y->left  = t2;
    updHeight(y);
    updHeight(x);
    return x;
}

/* левый поворот */
static AVLNode* rotL(AVLNode* x) {
    AVLNode* y  = x->right;
    AVLNode* t2 = y->left;
    y->left  = x;
    x->right = t2;
    updHeight(x);
    updHeight(y);
    return y;
}

static AVLNode* rebalance(AVLNode* n, const char* key) {
    updHeight(n);
    int bf = balanceFactor(n);

    // LL
    if (bf > 1 && strcmp(key, n->left->key) < 0)
        return rotR(n);
    // RR
    if (bf < -1 && strcmp(key, n->right->key) > 0)
        return rotL(n);
    // LR
    if (bf > 1 && strcmp(key, n->left->key) > 0) {
        n->left = rotL(n->left);
        return rotR(n);
    }
    // RL
    if (bf < -1 && strcmp(key, n->right->key) < 0) {
        n->right = rotR(n->right);
        return rotL(n);
    }
    return n;
}

static AVLNode* insertNode(AVLNode* n, const char* key,
                           int doc_id, const char* title,
                           int* inserted_new) {
    if (!n) {
        *inserted_new = 1;
        AVLNode* nn = newNode(key);
        appendPosting(nn->postings, doc_id, title);
        return nn;
    }
    int cmp = strcmp(key, n->key);
    if (cmp < 0) {
        n->left = insertNode(n->left, key, doc_id, title, inserted_new);
    } else if (cmp > 0) {
        n->right = insertNode(n->right, key, doc_id, title, inserted_new);
    } else {
        // ключ уже есть — просто добавим запись в posting list
        appendPosting(n->postings, doc_id, title);
        return n;
    }
    return rebalance(n, key);
}

AVLTree* createAVLTree(void) {
    AVLTree* t = malloc(sizeof(AVLTree));
    t->root = NULL;
    t->size = 0;
    return t;
}

static void freeNode(AVLNode* n) {
    if (!n) return;
    freeNode(n->left);
    freeNode(n->right);
    free(n->key);
    vectorFree(n->postings);
    free(n);
}

void freeAVLTree(AVLTree* tree) {
    if (!tree) return;
    freeNode(tree->root);
    free(tree);
}

void avlInsert(AVLTree* tree, const char* key, int doc_id, const char* title) {
    if (!tree || !key) return;
    int added = 0;
    tree->root = insertNode(tree->root, key, doc_id, title, &added);
    if (added) tree->size++;
}

static AVLNode* findNode(AVLNode* n, const char* key) {
    while (n) {
        int c = strcmp(key, n->key);
        if (c == 0) return n;
        n = (c < 0) ? n->left : n->right;
    }
    return NULL;
}

Vector* avlSearch(const AVLTree* tree, const char* key) {
    if (!tree || !key) return NULL;
    AVLNode* n = findNode(tree->root, key);
    return n ? n->postings : NULL;
}

static void traverseRec(AVLNode* n,
                        void (*visit)(const char*, Vector*, void*),
                        void* ctx) {
    if (!n) return;
    traverseRec(n->left, visit, ctx);
    visit(n->key, n->postings, ctx);
    traverseRec(n->right, visit, ctx);
}

void avlTraverse(const AVLTree* tree,
                 void (*visit)(const char* key, Vector* postings, void* ctx),
                 void* ctx) {
    if (!tree) return;
    traverseRec(tree->root, visit, ctx);
}
