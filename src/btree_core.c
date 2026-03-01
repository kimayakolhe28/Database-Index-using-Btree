#include "../include/file_btree.h"

/* --- File Entry ----------------------------------------------- */

FileEntry *file_entry_create(const char *filename, const char *path, long size) {
    FileEntry *f = calloc(1, sizeof(FileEntry));
    if (!f) return NULL;

    strncpy(f->filename, filename, 255);
    strncpy(f->path,     path,     511);
    f->size    = size;
    f->created = f->modified = time(NULL);

    /* extract extension */
    const char *dot = strrchr(filename, '.');
    if (dot && dot != filename)
        strncpy(f->type, dot + 1, 15);
    else
        strcpy(f->type, "");

    return f;
}

void file_entry_free(FileEntry *f) { free(f); }

void file_entry_print(const FileEntry *f) {
    if (!f) return;

    /* human-readable size */
    char sz[32];
    if (f->size >= 1024*1024)
        snprintf(sz, sizeof sz, "%.1f MB", f->size / (1024.0*1024.0));
    else if (f->size >= 1024)
        snprintf(sz, sizeof sz, "%.1f KB", f->size / 1024.0);
    else
        snprintf(sz, sizeof sz, "%ld B", f->size);

    char mtime[32];
    struct tm *tm_info = localtime(&f->modified);
    strftime(mtime, sizeof mtime, "%Y-%m-%d %H:%M", tm_info);

    printf("  %-30s  %-10s  %-5s  %s\n",
           f->filename, sz,
           f->type[0] ? f->type : "-",
           mtime);
}

/* --- Node helpers --------------------------------------------- */

BPlusNode *bpt_node_create(bool is_leaf) {
    BPlusNode *n = calloc(1, sizeof(BPlusNode));
    if (!n) { fprintf(stderr, "OOM: node alloc\n"); exit(1); }
    n->is_leaf  = is_leaf;
    n->num_keys = 0;
    n->next     = NULL;
    return n;
}

static void node_destroy(BPlusNode *n) {
    if (!n) return;
    if (!n->is_leaf)
        for (int i = 0; i <= n->num_keys; i++)
            node_destroy(n->children[i]);
    else
        for (int i = 0; i < n->num_keys; i++)
            file_entry_free(n->files[i]);
    free(n);
}

/* --- Tree lifecycle ------------------------------------------- */

BPlusTree *bpt_create(void) {
    BPlusTree *t = calloc(1, sizeof(BPlusTree));
    if (!t) { fprintf(stderr, "OOM: tree alloc\n"); exit(1); }
    t->root   = bpt_node_create(true);
    t->degree = MIN_DEGREE;
    return t;
}

void bpt_destroy(BPlusTree *t) {
    if (!t) return;
    node_destroy(t->root);
    free(t);
}

/* --- Navigate to leaf ----------------------------------------- */

BPlusNode *bpt_find_leaf(BPlusTree *t, const char *key) {
    BPlusNode *cur = t->root;
    while (!cur->is_leaf) {
        int i = 0;
        while (i < cur->num_keys && strcmp(key, cur->keys[i]) >= 0)
            i++;
        cur = cur->children[i];
    }
    return cur;
}

/* --- Statistics ----------------------------------------------- */

void bpt_stats_recursive(BPlusNode *n, TreeStats *s, int depth, int *max_depth) {
    if (!n) return;
    s->total_nodes++;
    if (depth > *max_depth) *max_depth = depth;

    if (n->is_leaf) {
        s->leaf_nodes++;
        s->total_files += n->num_keys;
        for (int i = 0; i < n->num_keys; i++)
            if (n->files[i]) s->total_size += n->files[i]->size;
    } else {
        s->internal_nodes++;
        for (int i = 0; i <= n->num_keys; i++)
            bpt_stats_recursive(n->children[i], s, depth+1, max_depth);
    }
}

TreeStats bpt_get_stats(BPlusTree *t) {
    TreeStats s = {0};
    int max_depth = 0;
    bpt_stats_recursive(t->root, &s, 1, &max_depth);
    s.height      = max_depth;
    s.total_size  = t->total_size;
    s.total_files = t->total_files;
    if (s.leaf_nodes > 0)
        s.avg_keys_per_leaf = (double)s.total_files / s.leaf_nodes;
    return s;
}

void bpt_print_stats(BPlusTree *t) {
    TreeStats s = bpt_get_stats(t);

    /* size string */
    char sz[32];
    if (s.total_size >= 1024*1024*1024)
        snprintf(sz, sizeof sz, "%.2f GB", s.total_size/(1024.0*1024.0*1024.0));
    else if (s.total_size >= 1024*1024)
        snprintf(sz, sizeof sz, "%.2f MB", s.total_size/(1024.0*1024.0));
    else if (s.total_size >= 1024)
        snprintf(sz, sizeof sz, "%.2f KB", s.total_size/1024.0);
    else
        snprintf(sz, sizeof sz, "%ld B",  s.total_size);

    printf("\n==========================================\n");
    printf("          File System Statistics          \n");
    printf("==========================================\n");
    printf("  Total Files    : %d\n",    s.total_files);
    printf("  Total Size     : %s\n",    sz);
    printf("  Tree Height    : %d\n",    s.height);
    printf("  Total Nodes    : %d\n",    s.total_nodes);
    printf("    Leaf Nodes   : %d\n",    s.leaf_nodes);
    printf("    Internal     : %d\n",    s.internal_nodes);
    printf("  Avg Keys/Leaf  : %.2f\n",  s.avg_keys_per_leaf);
    printf("==========================================\n\n");
}

/* --- Tree visualisation (text) -------------------------------- */

static void print_node(BPlusNode *n, int level) {
    if (!n) return;

    for (int i = 0; i < level; i++) printf("    ");

    if (n->is_leaf) {
        printf("[LEAF] [");
        for (int i = 0; i < n->num_keys; i++) {
            printf("%s", n->files[i]->filename);
            if (i < n->num_keys-1) printf(", ");
        }
        printf("]\n");
    } else {
        printf("[NODE] [");
        for (int i = 0; i < n->num_keys; i++) {
            printf("%s", n->keys[i]);
            if (i < n->num_keys-1) printf(", ");
        }
        printf("]\n");
        for (int i = 0; i <= n->num_keys; i++)
            print_node(n->children[i], level+1);
    }
}

void bpt_print_tree(BPlusTree *t) {
    printf("\n=============================================\n");
    printf("     B+ Tree Structure (order %d, t=%d)     \n",
           2*MIN_DEGREE, MIN_DEGREE);
    printf("     Files: %d\n", t->total_files);
    printf("=============================================\n\n");
    print_node(t->root, 0);
    printf("\n");
}

/* --- Persistence ---------------------------------------------- */

static void save_leaf_walk(BPlusNode *n, FILE *fp) {
    if (!n) return;
    if (n->is_leaf) {
        for (int i = 0; i < n->num_keys; i++) {
            FileEntry *f = n->files[i];
            fprintf(fp, "%s|%s|%ld|%ld|%ld\n",
                    f->filename, f->path, f->size,
                    (long)f->created, (long)f->modified);
        }
    } else {
        for (int i = 0; i <= n->num_keys; i++)
            save_leaf_walk(n->children[i], fp);
    }
}

bool bpt_save(BPlusTree *t, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) { perror("bpt_save"); return false; }
    fprintf(fp, "# BPlusTree index v1\n");
    fprintf(fp, "# degree=%d total=%d\n", t->degree, t->total_files);
    save_leaf_walk(t->root, fp);
    fclose(fp);
    printf("  [OK] Index saved to %s (%d files)\n", filename, t->total_files);
    return true;
}

BPlusTree *bpt_load(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("bpt_load"); return NULL; }

    BPlusTree *t = bpt_create();
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof line, fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = 0;

        char fname[256], path[512];
        long sz, cr, mo;
        if (sscanf(line, "%255[^|]|%511[^|]|%ld|%ld|%ld",
                   fname, path, &sz, &cr, &mo) == 5) {
            FileEntry *f = file_entry_create(fname, path, sz);
            f->created  = (time_t)cr;
            f->modified = (time_t)mo;
            bpt_insert(t, f);
            count++;
        }
    }
    fclose(fp);
    printf("  [OK] Loaded %d files from %s\n", count, filename);
    return t;
}
