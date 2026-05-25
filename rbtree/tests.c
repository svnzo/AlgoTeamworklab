#include "rbtree.h"
#include <stdio.h>
#include <string.h>

static int passed = 0, failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { passed++; printf("  [ok]  %s\n", msg); } \
    else      { failed++; printf("  [FAIL] %s\n", msg); } \
} while (0)

static void test_create(void) {
    printf("== rb: create/free ==\n");
    RBTree* t = createRBTree();
    CHECK(t != NULL && t->root == t->nil && t->size == 0, "пустое дерево");
    CHECK(t->nil->color == RB_BLACK, "nil чёрный");
    freeRBTree(t);
}

static void test_basic_insert(void) {
    printf("== rb: insert + search ==\n");
    RBTree* t = createRBTree();
    rbInsert(t, "a", 1, "ta");
    rbInsert(t, "b", 2, "tb");
    rbInsert(t, "c", 3, "tc");

    CHECK(t->size == 3, "size = 3");
    CHECK(rbSearch(t, "a") != NULL, "найден a");
    CHECK(rbSearch(t, "b") != NULL, "найден b");
    CHECK(rbSearch(t, "c") != NULL, "найден c");
    CHECK(rbSearch(t, "z") == NULL, "z не найден");

    freeRBTree(t);
}

static void test_duplicate(void) {
    printf("== rb: дубликаты ==\n");
    RBTree* t = createRBTree();
    rbInsert(t, "x", 1, "t1");
    rbInsert(t, "x", 2, "t2");
    CHECK(t->size == 1, "size остаётся 1");
    Vector* p = rbSearch(t, "x");
    CHECK(p && p->size == 2, "posting содержит обе записи");
    freeRBTree(t);
}

/* Проверка инварианта: корень всегда чёрный */
static void test_root_black(void) {
    printf("== rb: корень чёрный ==\n");
    RBTree* t = createRBTree();
    for (int i = 0; i < 20; i++) {
        char k[8]; snprintf(k, sizeof(k), "k%d", i);
        rbInsert(t, k, i, "x");
    }
    CHECK(t->root->color == RB_BLACK, "корень чёрный");
    freeRBTree(t);
}

/* Инвариант: ни у одного красного нет красного ребёнка */
static int check_no_red_red(RBNode* n, RBNode* nil) {
    if (n == nil) return 1;
    if (n->color == RB_RED) {
        if (n->left  != nil && n->left->color  == RB_RED) return 0;
        if (n->right != nil && n->right->color == RB_RED) return 0;
    }
    return check_no_red_red(n->left, nil) && check_no_red_red(n->right, nil);
}

static void test_no_red_red(void) {
    printf("== rb: нет двух красных подряд ==\n");
    RBTree* t = createRBTree();
    const char* k[] = {"m","b","y","a","c","x","z","aa","bb","cc","dd"};
    for (size_t i = 0; i < sizeof(k)/sizeof(k[0]); i++)
        rbInsert(t, k[i], (int)i, "x");
    CHECK(check_no_red_red(t->root, t->nil), "инвариант red-red держится");
    freeRBTree(t);
}

/* Проверка чёрной высоты — одинаковое число чёрных от корня до листа */
static int black_height(RBNode* n, RBNode* nil, int* ok) {
    if (n == nil) return 1;
    int lh = black_height(n->left,  nil, ok);
    int rh = black_height(n->right, nil, ok);
    if (lh != rh) *ok = 0;
    return lh + (n->color == RB_BLACK ? 1 : 0);
}

static void test_black_height(void) {
    printf("== rb: чёрная высота одинакова ==\n");
    RBTree* t = createRBTree();
    char k[8];
    for (int i = 0; i < 50; i++) {
        snprintf(k, sizeof(k), "k%02d", i);
        rbInsert(t, k, i, "x");
    }
    int ok = 1;
    black_height(t->root, t->nil, &ok);
    CHECK(ok, "black-height сбалансирован");
    freeRBTree(t);
}

int main(void) {
    test_create();
    test_basic_insert();
    test_duplicate();
    test_root_black();
    test_no_red_red();
    test_black_height();
    printf("\n== RB: passed %d, failed %d ==\n", passed, failed);
    return failed ? 1 : 0;
}
