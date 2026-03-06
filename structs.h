/*
 * ============================================================
 * FILE     : structs.h
 * CONTAINS : All constants, structs, and function declarations
 * ============================================================
 */

#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <dirent.h>
  #include <sys/stat.h>
#endif

/* ── Constants ──────────────────────────────────────────────── */
#define T        3            /* minimum degree                 */
#define MAX_KEYS (2*T - 1)   /* max keys per node = 5          */
#define MAX_CH   (2*T)       /* max children per node = 6      */

/* ── File record ────────────────────────────────────────────── */
typedef struct {
    char   name[256];   /* filename  e.g. "report.pdf"         */
    char   path[512];   /* full path e.g. "C:\docs\report.pdf" */
    char   ext[16];     /* extension e.g. "pdf"                */
    long   size;        /* size in bytes                       */
    time_t modified;    /* last modified timestamp             */
} File;

/* ── B+ Tree Node ───────────────────────────────────────────── */
typedef struct Node {
    char         keys[MAX_KEYS][256]; /* sorted filenames      */
    struct Node *child[MAX_CH];       /* child pointers        */
    File        *data[MAX_KEYS];      /* file data (leaf only) */
    struct Node *next;                /* next leaf pointer     */
    int          n;                   /* current key count     */
    bool         leaf;                /* is this a leaf node?  */
} Node;

/* ── Tree ───────────────────────────────────────────────────── */
typedef struct {
    Node *root;
    int   count;   /* total files indexed                      */
    long  total;   /* total size in bytes                      */
} Tree;

/* ── insert.c ───────────────────────────────────────────────── */
Tree *new_tree(void);
Node *new_node(bool leaf);
File *new_file(const char *name, const char *path, long size);
void  insert(Tree *t, File *f);

/* ── search.c ───────────────────────────────────────────────── */
Node *find_leaf(Tree *t, const char *name);
File *search(Tree *t, const char *name);
void  fmt_size(long sz, char *buf, int bufsz);
int   list_all(Tree *t);
int   list_range(Tree *t, const char *start, const char *end);
int   list_ext(Tree *t, const char *ext);
int   list_dir(Tree *t, const char *prefix);

/* ── delete.c ───────────────────────────────────────────────── */
bool  delete_file(Tree *t, const char *name);

/* ── operations.c ───────────────────────────────────────────── */
void  save_index(Tree *t, const char *filename);
void  load_index(Tree *t, const char *filename);
int   index_directory(Tree *t, const char *path);
void  benchmark(void);
void  load_samples(Tree *t);

/* ── visual.c ───────────────────────────────────────────────── */
void  print_tree_terminal(Tree *t);
void  export_dot(Tree *t, const char *filename);
void  export_html(Tree *t, const char *filename);

#endif /* STRUCTS_H */
