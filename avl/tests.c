#include "avl.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { passed++; printf("  [ok]  %s\n", msg); } \
    else      { failed++; printf("  [FAIL] %s\n", msg); } \
} while (0)

static void test_create_free(void) {
    printf("== avl: create/free ==\n");
    AVLTree* t = createAVLTree();
    CHECK(t != NULL && t->root == NULL && t->size == 0, "empty tree");
    freeAVLTree(t);
}

static void test_insert_search(void) {
    printf("== avl: insert + search ==\n");
    AVLTree* t = createAVLTree();
    avlInsert(t, "apple", 1, "title-apple");
    avlInsert(t, "banana", 2, "title-banana");
    avlInsert(t, "cherry", 3, "title-cherry");

    CHECK(t->size == 3, "size after 3 inserts");

    Vector* p = avlSearch(t, "banana");
    CHECK(p != NULL && p->size == 1, "find banana");

    Vector* m = avlSearch(t, "missing");
    CHECK(m == NULL, "missing key returns NULL");

    freeAVLTree(t);
}

static void test_duplicate_key(void) {
    printf("== avl: дубликат ключа ==\n");
    AVLTree* t = createAVLTree();
    avlInsert(t, "python", 10, "doc-10");
    avlInsert(t, "python", 20, "doc-20");
    avlInsert(t, "python", 30, "doc-30");

    CHECK(t->size == 1, "size still 1 после повторов");
    Vector* p = avlSearch(t, "python");
    CHECK(p && p->size == 3, "posting list содержит 3 записи");

    freeAVLTree(t);
}

static void test_rotations(void) {
    printf("== avl: ротации (sorted insert) ==\n");
    /* Если бы не было балансировки, после вставки 1..7 высота была бы 7.
       С AVL должно быть около log2(7) ~ 3. */
    AVLTree* t = createAVLTree();
    char buf[16];
    for (int i = 1; i <= 7; i++) {
        snprintf(buf, sizeof(buf), "k%02d", i);
        avlInsert(t, buf, i, "x");
    }
    CHECK(t->root != NULL, "root есть");
    CHECK(t->root->height <= 4, "высота <= 4 при 7 элементах");

    /* проверим что все 7 ищутся */
    int found = 0;
    for (int i = 1; i <= 7; i++) {
        snprintf(buf, sizeof(buf), "k%02d", i);
        if (avlSearch(t, buf)) found++;
    }
    CHECK(found == 7, "все 7 ключей находятся");

    freeAVLTree(t);
}

static char prev_key[64];
static int  inorder_sorted;
static int  inorder_count;

static void inorder_visit(const char* key, Vector* postings, void* ctx) {
    (void)postings; (void)ctx;
    if (inorder_count > 0 && strcmp(prev_key, key) > 0) inorder_sorted = 0;
    strncpy(prev_key, key, sizeof(prev_key) - 1);
    inorder_count++;
}

static void test_traverse_order(void) {
    printf("== avl: traverse в отсортированном порядке ==\n");
    AVLTree* t = createAVLTree();
    const char* keys[] = {"zebra", "apple", "mango", "banana", "kiwi"};
    for (int i = 0; i < 5; i++) avlInsert(t, keys[i], i, "x");

    inorder_sorted = 1;
    inorder_count = 0;
    avlTraverse(t, inorder_visit, NULL);
    CHECK(inorder_count == 5, "обошли 5 ключей");
    CHECK(inorder_sorted, "порядок возрастающий");

    freeAVLTree(t);
}

static void test_many_inserts(void) {
    printf("== avl: 1000 вставок ==\n");
    AVLTree* t = createAVLTree();
    char buf[16];
    for (int i = 0; i < 1000; i++) {
        snprintf(buf, sizeof(buf), "key%04d", i);
        avlInsert(t, buf, i, "title");
    }
    CHECK(t->size == 1000, "size = 1000");

    snprintf(buf, sizeof(buf), "key%04d", 500);
    Vector* p = avlSearch(t, buf);
    CHECK(p != NULL, "найден ключ из середины");

    freeAVLTree(t);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    test_create_free();
    test_insert_search();
    test_duplicate_key();
    test_rotations();
    test_traverse_order();
    test_many_inserts();
    printf("\n== AVL: passed %d, failed %d ==\n", passed, failed);
    return failed ? 1 : 0;
}
