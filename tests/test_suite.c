#include "../include/file_btree.h"
#include <assert.h>

static int tests_run    = 0;
static int tests_passed = 0;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  [PASS] %s\n", msg); } \
    else       { printf("  [FAIL] %s\n", msg); } \
} while(0)

/* ---------------------------------------------------------------- */
static void test_create(void) {
    printf("\n[1] Tree creation\n");
    BPlusTree *t = bpt_create();
    CHECK(t != NULL,           "Tree created");
    CHECK(t->root != NULL,     "Root exists");
    CHECK(t->root->is_leaf,    "Root is leaf initially");
    CHECK(t->total_files == 0, "Zero files");
    bpt_destroy(t);
    CHECK(1,                   "Tree destroyed without crash");
}

/* ---------------------------------------------------------------- */
static void test_insert_search(void) {
    printf("\n[2] Insert and search\n");
    BPlusTree *t = bpt_create();

    FileEntry *f1 = file_entry_create("zebra.txt", "/z/zebra.txt", 100);
    FileEntry *f2 = file_entry_create("alpha.c",   "/a/alpha.c",   200);
    FileEntry *f3 = file_entry_create("main.py",   "/m/main.py",   300);

    bpt_insert(t, f1);
    bpt_insert(t, f2);
    bpt_insert(t, f3);

    CHECK(t->total_files == 3,                      "3 files after insert");
    CHECK(bpt_search(t, "zebra.txt", NULL) != NULL, "Found zebra.txt");
    CHECK(bpt_search(t, "alpha.c",   NULL) != NULL, "Found alpha.c");
    CHECK(bpt_search(t, "main.py",   NULL) != NULL, "Found main.py");
    CHECK(bpt_search(t, "missing",   NULL) == NULL, "Missing file returns NULL");

    bpt_destroy(t);
}

/* ---------------------------------------------------------------- */
static void test_many_inserts(void) {
    printf("\n[3] Many inserts (triggers node splits)\n");
    BPlusTree *t = bpt_create();

    char names[200][64];
    for (int i = 0; i < 200; i++) {
        snprintf(names[i], 64, "file_%04d.dat", i);
        FileEntry *f = file_entry_create(names[i], "/bench", i * 512L);
        bpt_insert(t, f);
    }

    CHECK(t->total_files == 200, "200 files inserted");

    int found = 0;
    for (int i = 0; i < 200; i++)
        if (bpt_search(t, names[i], NULL)) found++;
    CHECK(found == 200, "All 200 files searchable");

    bpt_destroy(t);
}

/* ---------------------------------------------------------------- */
static void test_delete(void) {
    printf("\n[4] Delete operations\n");
    BPlusTree *t = bpt_create();

    const char *fnames[] = {"aaa.txt","bbb.txt","ccc.txt","ddd.txt","eee.txt"};
    for (int i = 0; i < 5; i++) {
        FileEntry *f = file_entry_create(fnames[i], "/t", (i+1)*100L);
        bpt_insert(t, f);
    }

    CHECK(bpt_delete(t, "aaa.txt"),                 "Delete aaa.txt (success)");
    CHECK(t->total_files == 4,                      "total_files decremented");
    CHECK(bpt_search(t, "aaa.txt", NULL) == NULL,   "aaa.txt no longer found");
    CHECK(bpt_search(t, "bbb.txt", NULL) != NULL,   "bbb.txt still present");
    CHECK(!bpt_delete(t, "does_not_exist.txt"),     "Delete non-existent returns false");
    CHECK(t->total_files == 4,                      "Count unchanged for non-existent");

    for (int i = 1; i < 5; i++) bpt_delete(t, fnames[i]);
    CHECK(t->total_files == 0, "All files deleted");

    bpt_destroy(t);
}

/* ---------------------------------------------------------------- */
static void test_range(void) {
    printf("\n[5] Range search\n");
    BPlusTree *t = bpt_create();

    const char *fnames[] = {
        "apple.txt","banana.txt","cherry.txt","date.txt",
        "elderberry.txt","fig.txt","grape.txt"
    };
    for (int i = 0; i < 7; i++) {
        FileEntry *f = file_entry_create(fnames[i], "/fruit", 100L);
        bpt_insert(t, f);
    }

    printf("  Range [banana.txt ... date.txt]:\n");
    int cnt = bpt_range(t, "banana.txt", "date.txt");
    CHECK(cnt == 3, "Range returns 3 files (banana, cherry, date)");

    bpt_destroy(t);
}

/* ---------------------------------------------------------------- */
static void test_list_by_type(void) {
    printf("\n[6] List by extension\n");
    BPlusTree *t = bpt_create();

    const char *fnames[] = {
        "doc1.pdf","doc2.pdf","code.c","header.h",
        "notes.txt","data.csv","image.pdf"
    };
    for (int i = 0; i < 7; i++) {
        FileEntry *f = file_entry_create(fnames[i], "/mixed", 256L);
        bpt_insert(t, f);
    }

    int pdf = bpt_list_by_type(t, "pdf");
    CHECK(pdf == 3, "3 PDF files found");

    int c = bpt_list_by_type(t, "c");
    CHECK(c == 1,   "1 .c file found");

    bpt_destroy(t);
}

/* ---------------------------------------------------------------- */
static void test_comparison_count(void) {
    printf("\n[7] Comparison count (O(log n))\n");
    BPlusTree *t = bpt_create();

    int n = 1000;
    for (int i = 0; i < n; i++) {
        char name[64];
        snprintf(name, 64, "f_%06d.dat", i);
        FileEntry *f = file_entry_create(name, "/", i * 100L);
        bpt_insert(t, f);
    }

    long total = 0;
    for (int i = 0; i < n; i++) {
        char name[64];
        snprintf(name, 64, "f_%06d.dat", i);
        long cmp = 0;
        bpt_search(t, name, &cmp);
        total += cmp;
    }
    double avg = (double)total / n;
    printf("  Average comparisons for %d files: %.2f\n", n, avg);
    CHECK(avg < 30.0, "Average comparisons < 30 (logarithmic)");

    bpt_destroy(t);
}

/* ---------------------------------------------------------------- */
static void test_persistence(void) {
    printf("\n[8] Save / Load\n");

    /* Use a local path that always works on Windows */
    const char *save_path = "test_index_temp.idx";

    BPlusTree *t1 = bpt_create();
    for (int i = 0; i < 20; i++) {
        char name[64];
        snprintf(name, 64, "persist_%02d.txt", i);
        FileEntry *f = file_entry_create(name, "/persist", i * 100L);
        bpt_insert(t1, f);
    }

    bool saved = bpt_save(t1, save_path);
    CHECK(saved, "Index saved");

    BPlusTree *t2 = bpt_load(save_path);
    CHECK(t2 != NULL,            "Index loaded");
    CHECK(t2->total_files == 20, "Correct file count after load");
    CHECK(bpt_search(t2, "persist_10.txt", NULL) != NULL,
          "File found after reload");

    bpt_destroy(t1);
    bpt_destroy(t2);
    remove(save_path);   /* clean up temp file */
}

/* ---------------------------------------------------------------- */
static void test_real_directory(void) {
    printf("\n[9] Real directory indexing\n");

    BPlusTree *t = bpt_create();

    /* Index the src/ folder of this very project */
    int count = bpt_index_directory(t, "src");

    CHECK(count > 0,        "Found files in src/ directory");
    CHECK(t->total_files == count, "total_files matches scanned count");

    /* At least one of our known source files should be there */
    FileEntry *f = bpt_search(t, "main.c", NULL);
    CHECK(f != NULL, "main.c found after indexing src/");

    printf("  Files indexed from src/: %d\n", count);
    bpt_destroy(t);
}

/* ---------------------------------------------------------------- */
int main(void) {
    printf("=============================================\n");
    printf("   B+ Tree File Manager - Test Suite\n");
    printf("=============================================\n");

    test_create();
    test_insert_search();
    test_many_inserts();
    test_delete();
    test_range();
    test_list_by_type();
    test_comparison_count();
    test_persistence();
    test_real_directory();

    printf("\n=============================================\n");
    printf("  Results: %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run)
        printf("  ALL TESTS PASSED\n");
    else
        printf("  %d test(s) FAILED\n", tests_run - tests_passed);
    printf("=============================================\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
