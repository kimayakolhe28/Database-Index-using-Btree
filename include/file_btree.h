#ifndef FILE_BTREE_H
#define FILE_BTREE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

/* ─── B+ Tree Configuration ─────────────────────────────────── */
#define MIN_DEGREE   3                      /* t = 3              */
#define MAX_KEYS     (2 * MIN_DEGREE - 1)   /* 5 keys per node    */
#define MAX_CHILDREN (2 * MIN_DEGREE)       /* 6 children         */

/* ─── File Entry (stored only in leaf nodes) ─────────────────── */
typedef struct FileEntry {
    char filename[256];
    char path[512];
    long size;                  /* bytes                          */
    char type[16];              /* extension without dot          */
    time_t created;
    time_t modified;
} FileEntry;

/* ─── B+ Tree Node ───────────────────────────────────────────── */
typedef struct BPlusNode {
    char                keys[MAX_KEYS][256]; /* routing keys       */
    struct BPlusNode   *children[MAX_CHILDREN];
    FileEntry          *files[MAX_KEYS];     /* leaf data (NULL in internal) */
    struct BPlusNode   *next;                /* leaf linked list   */
    int                 num_keys;
    bool                is_leaf;
} BPlusNode;

/* ─── B+ Tree ────────────────────────────────────────────────── */
typedef struct BPlusTree {
    BPlusNode *root;
    int        degree;
    int        total_files;
    long       total_size;
} BPlusTree;

/* ─── Statistics ─────────────────────────────────────────────── */
typedef struct TreeStats {
    int  total_files;
    int  total_nodes;
    int  leaf_nodes;
    int  internal_nodes;
    int  height;
    long total_size;
    double avg_keys_per_leaf;
} TreeStats;

/* ─── API ────────────────────────────────────────────────────── */

/* Creation / destruction */
BPlusTree  *bpt_create(void);
void        bpt_destroy(BPlusTree *tree);
BPlusNode  *bpt_node_create(bool is_leaf);

/* File entry helpers */
FileEntry  *file_entry_create(const char *filename, const char *path, long size);
void        file_entry_print(const FileEntry *f);
void        file_entry_free(FileEntry *f);

/* Core operations */
bool        bpt_insert(BPlusTree *tree, FileEntry *file);
FileEntry  *bpt_search(BPlusTree *tree, const char *filename, long *comparisons);
bool        bpt_delete(BPlusTree *tree, const char *filename);

/* Range / listing */
int         bpt_range(BPlusTree *tree, const char *start, const char *end);
int         bpt_list_by_type(BPlusTree *tree, const char *ext);
int         bpt_list_all(BPlusTree *tree);
int         bpt_list_dir(BPlusTree *tree, const char *dir_prefix);

/* Real filesystem integration */
int         bpt_index_directory(BPlusTree *tree, const char *dir_path);

/* Visualisation / stats */
void        bpt_print_tree(BPlusTree *tree);
void        bpt_print_stats(BPlusTree *tree);
TreeStats   bpt_get_stats(BPlusTree *tree);

/* Persistence */
bool        bpt_save(BPlusTree *tree, const char *filename);
BPlusTree  *bpt_load(const char *filename);

/* Benchmarking */
void        bpt_benchmark(int num_files);

/* Internal helpers (used across translation units) */
BPlusNode  *bpt_find_leaf(BPlusTree *tree, const char *key);
void        bpt_split_leaf(BPlusTree *tree, BPlusNode *parent, int idx, BPlusNode *leaf);
void        bpt_split_internal(BPlusTree *tree, BPlusNode *parent, int idx, BPlusNode *node);
void        bpt_insert_into_parent(BPlusTree *tree, BPlusNode *left,
                                   const char *key, BPlusNode *right);
void        bpt_stats_recursive(BPlusNode *node, TreeStats *s, int depth, int *max_depth);

#endif /* FILE_BTREE_H */
