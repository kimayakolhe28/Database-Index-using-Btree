/*
 * ============================================================
 * FILE     : search.c
 * CONTAINS : search, list_all, list_range, list_ext, list_dir
 * ============================================================
 */

#include "structs.h"

/* ── Helper: human readable size string ─────────────────────── */
void fmt_size(long sz, char *buf, int bufsz) {
    if      (sz >= 1024*1024*1024)
        snprintf(buf, bufsz, "%.1f GB", sz/(1024.0*1024*1024));
    else if (sz >= 1024*1024)
        snprintf(buf, bufsz, "%.1f MB", sz/(1024.0*1024));
    else if (sz >= 1024)
        snprintf(buf, bufsz, "%.1f KB", sz/1024.0);
    else
        snprintf(buf, bufsz, "%ld B",   sz);
}

/* ── Helper: print one file row ─────────────────────────────── */
static void print_row(File *f) {
    char sz[20], ts[20];
    fmt_size(f->size, sz, sizeof sz);
    strftime(ts, sizeof ts, "%Y-%m-%d", localtime(&f->modified));
    printf("  %-32s  %-10s  %-6s  %s\n",
           f->name, sz, f->ext[0] ? f->ext : "-", ts);
}

/* ── Navigate to the leaf where name belongs ────────────────── */
Node *find_leaf(Tree *t, const char *name) {
    Node *cur = t->root;
    while (!cur->leaf) {
        int i = 0;
        while (i < cur->n && strcmp(name, cur->keys[i]) >= 0) i++;
        cur = cur->child[i];
    }
    return cur;
}

/* ── Search for a file by exact name ───────────────────────── */
File *search(Tree *t, const char *name) {
    Node *leaf = find_leaf(t, name);
    for (int i = 0; i < leaf->n; i++)
        if (strcmp(name, leaf->data[i]->name) == 0)
            return leaf->data[i];
    return NULL;
}

/* ── List ALL files in sorted order ─────────────────────────── */
int list_all(Tree *t) {
    printf("\n  %-32s  %-10s  %-6s  %s\n",
           "Filename", "Size", "Ext", "Modified");
    printf("  %s\n",
           "--------------------------------------------------------------");

    /* start from leftmost leaf */
    Node *leaf = t->root;
    while (!leaf->leaf) leaf = leaf->child[0];

    int count = 0;
    while (leaf) {
        for (int i = 0; i < leaf->n; i++) {
            print_row(leaf->data[i]);
            count++;
        }
        leaf = leaf->next;
    }
    printf("\n  Total: %d file(s)\n\n", count);
    return count;
}

/* ── Range search: files between start and end ──────────────── */
int list_range(Tree *t, const char *start, const char *end) {
    printf("\n  Range [ %s  ...  %s ]\n\n", start, end);
    printf("  %-32s  %-10s  %-6s  %s\n",
           "Filename", "Size", "Ext", "Modified");
    printf("  %s\n",
           "--------------------------------------------------------------");

    /* jump straight to start position using find_leaf */
    Node *leaf  = find_leaf(t, start);
    int   count = 0;

    while (leaf) {
        for (int i = 0; i < leaf->n; i++) {
            const char *nm = leaf->data[i]->name;
            if (strcmp(nm, start) < 0) continue;  /* before start */
            if (strcmp(nm, end)   > 0) goto done;  /* past end     */
            print_row(leaf->data[i]);
            count++;
        }
        leaf = leaf->next;
    }
done:
    printf("\n  Found: %d file(s) in range\n\n", count);
    return count;
}

/* ── List files by extension ────────────────────────────────── */
int list_ext(Tree *t, const char *ext) {
    printf("\n  Files with extension .%s :\n\n", ext);
    printf("  %-32s  %-10s  %s\n", "Filename", "Size", "Path");
    printf("  %s\n",
           "--------------------------------------------------------------");

    /* walk entire leaf chain */
    Node *leaf  = t->root;
    while (!leaf->leaf) leaf = leaf->child[0];
    int count = 0;

    while (leaf) {
        for (int i = 0; i < leaf->n; i++) {
            File *f = leaf->data[i];
            if (strcmp(f->ext, ext) == 0) {
                char sz[20];
                fmt_size(f->size, sz, sizeof sz);
                printf("  %-32s  %-10s  %s\n", f->name, sz, f->path);
                count++;
            }
        }
        leaf = leaf->next;
    }
    printf("\n  Found: %d .%s file(s)\n\n", count, ext);
    return count;
}

/* ── List files under a directory prefix ────────────────────── */
int list_dir(Tree *t, const char *prefix) {
    printf("\n  Files under: %s\n\n", prefix);
    printf("  %-32s  %-10s  %s\n", "Filename", "Size", "Full Path");
    printf("  %s\n",
           "--------------------------------------------------------------");

    Node *leaf  = t->root;
    while (!leaf->leaf) leaf = leaf->child[0];
    int   plen  = (int)strlen(prefix);
    int   count = 0;

    while (leaf) {
        for (int i = 0; i < leaf->n; i++) {
            File *f = leaf->data[i];
            if (strncmp(f->path, prefix, plen) == 0) {
                char sz[20];
                fmt_size(f->size, sz, sizeof sz);
                printf("  %-32s  %-10s  %s\n", f->name, sz, f->path);
                count++;
            }
        }
        leaf = leaf->next;
    }
    printf("\n  Found: %d file(s)\n\n", count);
    return count;
}
