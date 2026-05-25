#include "btree.h"
#include <stdio.h>
#include <string.h>

static int passed = 0, failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { passed++; printf("  [ok]  %s\n", msg); } \
    else      { failed++; printf("  [FAIL] %s\n", msg); } \
} while (0)

static void test_empty(void) {
    printf("== btree: пустое дерево ==\n");
    BTree* t = createBTree();
    CHECK(t && t->root && t->root->n == 0 && t->root->is_leaf, "пусто");
    CHECK(btreeSearch(t, "nope") == NULL, "поиск в пустом NULL");
    freeBTree(t);
}

static void test_insert_no_split(void) {
    printf("== btree: вставка без сплита ==\n");
    BTree* t = createBTree();
    btreeInsert(t, "b", 1, "tb");
    btreeInsert(t, "a", 2, "ta");
    btreeInsert(t, "c", 3, "tc");
    /* должно лечь в один лист (n=3 < 2t-1=5) */
    CHECK(t->root->is_leaf && t->root->n == 3, "1 лист, 3 ключа");
    CHECK(btreeSearch(t, "a") != NULL, "найден a");
    CHECK(btreeSearch(t, "b") != NULL, "найден b");
    CHECK(btreeSearch(t, "c") != NULL, "найден c");
    freeBTree(t);
}

static void test_split(void) {
    printf("== btree: сплит корня ==\n");
    BTree* t = createBTree();
    /* при t=3 макс 5 ключей в узле, на 6-й происходит сплит корня */
    const char* k[] = {"a","b","c","d","e","f","g","h","i","j"};
    for (size_t i = 0; i < sizeof(k)/sizeof(k[0]); i++)
        btreeInsert(t, k[i], (int)i, "x");

    CHECK(!t->root->is_leaf, "корень стал внутренним после сплита");
    for (size_t i = 0; i < sizeof(k)/sizeof(k[0]); i++)
        CHECK(btreeSearch(t, k[i]) != NULL, k[i]);
    freeBTree(t);
}

static void test_duplicate(void) {
    printf("== btree: дубликаты ==\n");
    BTree* t = createBTree();
    btreeInsert(t, "key", 1, "t1");
    btreeInsert(t, "key", 2, "t2");
    btreeInsert(t, "key", 3, "t3");
    CHECK(t->size == 1, "size = 1");
    Vector* p = btreeSearch(t, "key");
    CHECK(p && p->size == 3, "posting содержит 3");
    freeBTree(t);
}

static char prev_key[64];
static int  inorder_ok;
static int  visited;

static void visit(const char* key, Vector* postings, void* ctx) {
    (void)postings; (void)ctx;
    if (visited > 0 && strcmp(prev_key, key) > 0) inorder_ok = 0;
    strncpy(prev_key, key, sizeof(prev_key) - 1);
    visited++;
}

static void test_traverse_sorted(void) {
    printf("== btree: traverse сортирован ==\n");
    BTree* t = createBTree();
    const char* k[] = {"m","b","y","a","c","x","z","aa","bb","cc","dd","ee","ff"};
    for (size_t i = 0; i < sizeof(k)/sizeof(k[0]); i++)
        btreeInsert(t, k[i], (int)i, "x");

    inorder_ok = 1;
    visited = 0;
    btreeTraverse(t, visit, NULL);
    CHECK(visited == 13, "13 уникальных ключей");
    CHECK(inorder_ok, "порядок возрастающий");
    freeBTree(t);
}

static void test_many(void) {
    printf("== btree: 500 вставок ==\n");
    BTree* t = createBTree();
    char buf[16];
    for (int i = 0; i < 500; i++) {
        snprintf(buf, sizeof(buf), "key%03d", i);
        btreeInsert(t, buf, i, "x");
    }
    CHECK(t->size == 500, "size = 500");
    snprintf(buf, sizeof(buf), "key%03d", 250);
    CHECK(btreeSearch(t, buf) != NULL, "ключ из середины найден");
    freeBTree(t);
}

int main(void) {
    test_empty();
    test_insert_no_split();
    test_split();
    test_duplicate();
    test_traverse_sorted();
    test_many();
    printf("\n== BTree: passed %d, failed %d ==\n", passed, failed);
    return failed ? 1 : 0;
}
