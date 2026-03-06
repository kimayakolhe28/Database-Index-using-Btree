/*
 * B+ Tree File Management System
 * Single-file, easy to read and understand
 * Compile: gcc -Wall -std=c11 -o fm file_btree_simple.c
 * Run:     ./fm
 */

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

/* ============================================================
   CONFIGURATION
   ============================================================ */
#define BTREE_STRUCTS_DEFINED   /* prevents redefinition in visual.c */
#define T        3          /* minimum degree                  */
#define MAX_KEYS (2*T - 1)  /* max keys per node = 5           */
#define MAX_CH   (2*T)      /* max children per node = 6       */

/* ============================================================
   DATA STRUCTURES
   ============================================================ */

/* One file record stored in the tree */
typedef struct {
    char name[256];    /* filename e.g. "report.pdf"          */
    char path[512];    /* full path                           */
    char ext[16];      /* extension e.g. "pdf"                */
    long size;         /* size in bytes                       */
    time_t modified;   /* last modified timestamp             */
} File;

/* One node in the B+ tree */
typedef struct Node {
    char         keys[MAX_KEYS][256]; /* sorted filenames     */
    struct Node *child[MAX_CH];       /* child pointers       */
    File        *data[MAX_KEYS];      /* file data (leaf only)*/
    struct Node *next;                /* next leaf (leaf only)*/
    int          n;                   /* number of keys       */
    bool         leaf;                /* is this a leaf?      */
} Node;

/* The tree itself */
typedef struct {
    Node *root;
    int   count;   /* total files                             */
    long  total;   /* total size in bytes                     */
} Tree;

/* ============================================================
   CREATE / FREE
   ============================================================ */

Node *new_node(bool leaf) {
    Node *nd = calloc(1, sizeof(Node));
    nd->leaf = leaf;
    return nd;
}

Tree *new_tree(void) {
    Tree *t  = calloc(1, sizeof(Tree));
    t->root  = new_node(true);
    return t;
}

File *new_file(const char *name, const char *path, long size) {
    File *f = calloc(1, sizeof(File));
    strncpy(f->name, name, 255);
    strncpy(f->path, path, 511);
    f->size     = size;
    f->modified = time(NULL);

    /* extract extension */
    const char *dot = strrchr(name, '.');
    if (dot && dot != name)
        strncpy(f->ext, dot + 1, 15);

    return f;
}

/* ============================================================
   SEARCH
   ============================================================ */

/* Find the leaf where 'name' would live */
Node *find_leaf(Tree *t, const char *name) {
    Node *cur = t->root;
    while (!cur->leaf) {
        int i = 0;
        while (i < cur->n && strcmp(name, cur->keys[i]) >= 0)
            i++;
        cur = cur->child[i];
    }
    return cur;
}

/* Search for a file by exact name. Returns pointer or NULL. */
File *search(Tree *t, const char *name) {
    Node *leaf = find_leaf(t, name);
    for (int i = 0; i < leaf->n; i++)
        if (strcmp(name, leaf->data[i]->name) == 0)
            return leaf->data[i];
    return NULL;
}

/* ============================================================
   INSERT  (top-down pre-split so we never go back up)
   ============================================================ */

/* Insert into a leaf that has room */
static void leaf_put(Node *leaf, File *f) {
    int pos = leaf->n;
    while (pos > 0 && strcmp(f->name, leaf->keys[pos-1]) < 0)
        pos--;

    /* duplicate? just update */
    if (pos < leaf->n && strcmp(f->name, leaf->keys[pos]) == 0) {
        free(leaf->data[pos]);
        leaf->data[pos] = f;
        return;
    }
    /* shift right and insert */
    for (int i = leaf->n; i > pos; i--) {
        strncpy(leaf->keys[i], leaf->keys[i-1], 255);
        leaf->data[i] = leaf->data[i-1];
    }
    strncpy(leaf->keys[pos], f->name, 255);
    leaf->data[pos] = f;
    leaf->n++;
}

/* Split a full leaf; parent must have room */
static void split_leaf(Tree *t, Node *parent, int idx, Node *leaf) {
    Node *right = new_node(true);

    int split       = T;
    right->n        = MAX_KEYS - split;

    for (int i = 0; i < right->n; i++) {
        strncpy(right->keys[i], leaf->keys[split + i], 255);
        right->data[i]        = leaf->data[split + i];
        leaf->data[split + i] = NULL;
    }
    leaf->n    = split;
    right->next = leaf->next;
    leaf->next  = right;

    if (!parent) {
        /* leaf was root — make new root */
        Node *nr = new_node(false);
        strncpy(nr->keys[0], right->keys[0], 255);
        nr->child[0] = leaf;
        nr->child[1] = right;
        nr->n        = 1;
        t->root      = nr;
    } else {
        /* push separator up into parent */
        for (int i = parent->n; i > idx; i--) {
            strncpy(parent->keys[i], parent->keys[i-1], 255);
            parent->child[i+1] = parent->child[i];
        }
        strncpy(parent->keys[idx], right->keys[0], 255);
        parent->child[idx+1] = right;
        parent->n++;
    }
}

/* Split a full internal node; parent must have room */
static void split_internal(Tree *t, Node *parent, int idx, Node *nd) {
    Node *right = new_node(false);
    char  mid[256];
    strncpy(mid, nd->keys[T-1], 255);

    right->n = MAX_KEYS - T;
    for (int i = 0; i < right->n; i++) {
        strncpy(right->keys[i], nd->keys[T + i], 255);
        right->child[i] = nd->child[T + i];
    }
    right->child[right->n] = nd->child[MAX_KEYS];
    nd->n = T - 1;

    if (!parent) {
        Node *nr = new_node(false);
        strncpy(nr->keys[0], mid, 255);
        nr->child[0] = nd;
        nr->child[1] = right;
        nr->n        = 1;
        t->root      = nr;
    } else {
        for (int i = parent->n; i > idx; i--) {
            strncpy(parent->keys[i], parent->keys[i-1], 255);
            parent->child[i+1] = parent->child[i];
        }
        strncpy(parent->keys[idx], mid, 255);
        parent->child[idx+1] = right;
        parent->n++;
    }
}

/* Recursive insert with pre-splitting */
static void insert_rec(Tree *t, Node *nd, File *f) {
    if (nd->leaf) {
        leaf_put(nd, f);
        return;
    }
    int i = 0;
    while (i < nd->n && strcmp(f->name, nd->keys[i]) >= 0) i++;

    Node *ch = nd->child[i];
    if (ch->n == MAX_KEYS) {
        if (ch->leaf) split_leaf(t, nd, i, ch);
        else          split_internal(t, nd, i, ch);
        if (strcmp(f->name, nd->keys[i]) >= 0) i++;
        ch = nd->child[i];
    }
    insert_rec(t, ch, f);
}

void insert(Tree *t, File *f) {
    /* pre-split root if full */
    if (t->root->n == MAX_KEYS) {
        if (t->root->leaf) split_leaf(t, NULL, 0, t->root);
        else               split_internal(t, NULL, 0, t->root);
    }
    insert_rec(t, t->root, f);
    t->count++;
    t->total += f->size;
}

/* ============================================================
   DELETE  (simple leaf delete with borrow/merge)
   ============================================================ */

#define MAX_DEPTH 64
typedef struct { Node *nd; int idx; } PathStep;

static bool delete_rec(Tree *t, Node *nd, const char *name,
                        PathStep path[], int depth) {
    if (nd->leaf) {
        /* find and remove */
        int pos = -1;
        for (int i = 0; i < nd->n; i++)
            if (strcmp(name, nd->data[i]->name) == 0) { pos = i; break; }
        if (pos < 0) return false;

        free(nd->data[pos]);
        for (int i = pos; i < nd->n - 1; i++) {
            strncpy(nd->keys[i], nd->keys[i+1], 255);
            nd->data[i] = nd->data[i+1];
        }
        nd->n--;

        /* fix underflow */
        int min = T - 1;
        if (nd == t->root || nd->n >= min) return true;

        Node *par = path[depth-1].nd;
        int   ci  = path[depth-1].idx;

        /* borrow from right sibling */
        if (ci < par->n) {
            Node *right = par->child[ci+1];
            if (right->n > min) {
                strncpy(nd->keys[nd->n], right->data[0]->name, 255);
                nd->data[nd->n++] = right->data[0];
                for (int i = 0; i < right->n-1; i++) {
                    strncpy(right->keys[i], right->keys[i+1], 255);
                    right->data[i] = right->data[i+1];
                }
                right->n--;
                strncpy(par->keys[ci], right->data[0]->name, 255);
                return true;
            }
        }
        /* borrow from left sibling */
        if (ci > 0) {
            Node *left = par->child[ci-1];
            if (left->n > min) {
                for (int i = nd->n; i > 0; i--) {
                    strncpy(nd->keys[i], nd->keys[i-1], 255);
                    nd->data[i] = nd->data[i-1];
                }
                strncpy(nd->keys[0], left->data[left->n-1]->name, 255);
                nd->data[0] = left->data[left->n-1];
                nd->n++;
                left->n--;
                strncpy(par->keys[ci-1], nd->data[0]->name, 255);
                return true;
            }
        }
        /* merge with right */
        if (ci < par->n) {
            Node *right = par->child[ci+1];
            for (int i = 0; i < right->n; i++) {
                strncpy(nd->keys[nd->n+i], right->data[i]->name, 255);
                nd->data[nd->n+i] = right->data[i];
            }
            nd->n   += right->n;
            nd->next = right->next;
            free(right);
            for (int i = ci; i < par->n-1; i++) {
                strncpy(par->keys[i], par->keys[i+1], 255);
                par->child[i+1] = par->child[i+2];
            }
            par->n--;
            if (par == t->root && par->n == 0) {
                t->root = nd; free(par);
            }
        } else if (ci > 0) {
            /* merge with left */
            Node *left = par->child[ci-1];
            for (int i = 0; i < nd->n; i++) {
                strncpy(left->keys[left->n+i], nd->data[i]->name, 255);
                left->data[left->n+i] = nd->data[i];
            }
            left->n  += nd->n;
            left->next = nd->next;
            free(nd);
            for (int i = ci-1; i < par->n-1; i++) {
                strncpy(par->keys[i], par->keys[i+1], 255);
                par->child[i+1] = par->child[i+2];
            }
            par->n--;
            if (par == t->root && par->n == 0) {
                t->root = left; free(par);
            }
        }
        return true;
    }

    /* internal: descend */
    int i = 0;
    while (i < nd->n && strcmp(name, nd->keys[i]) >= 0) i++;
    path[depth].nd  = nd;
    path[depth].idx = i;
    return delete_rec(t, nd->child[i], name, path, depth+1);
}

bool delete_file(Tree *t, const char *name) {
    File *f = search(t, name);
    if (!f) return false;
    long sz = f->size;
    PathStep path[MAX_DEPTH] = {0};
    bool ok = delete_rec(t, t->root, name, path, 0);
    if (ok) { t->count--; t->total -= sz; }
    return ok;
}

/* ============================================================
   LISTING / QUERIES
   ============================================================ */

/* Helper: human-readable size */
static void fmt_size(long sz, char *buf, int bufsz) {
    if      (sz >= 1024*1024*1024) snprintf(buf, bufsz, "%.1f GB", sz/(1024.0*1024*1024));
    else if (sz >= 1024*1024)      snprintf(buf, bufsz, "%.1f MB", sz/(1024.0*1024));
    else if (sz >= 1024)           snprintf(buf, bufsz, "%.1f KB", sz/1024.0);
    else                           snprintf(buf, bufsz, "%ld B",   sz);
}

/* Print one file row */
static void print_row(File *f) {
    char sz[20];
    fmt_size(f->size, sz, sizeof sz);
    char ts[20];
    strftime(ts, sizeof ts, "%Y-%m-%d", localtime(&f->modified));
    printf("  %-32s  %-10s  %-6s  %s\n", f->name, sz,
           f->ext[0] ? f->ext : "-", ts);
}

/* List ALL files in sorted order (walks leaf chain) */
int list_all(Tree *t) {
    printf("\n  %-32s  %-10s  %-6s  %s\n",
           "Filename", "Size", "Ext", "Modified");
    printf("  %s\n",
           "--------------------------------------------------------------");
    Node *leaf = t->root;
    while (!leaf->leaf) leaf = leaf->child[0];
    int count = 0;
    while (leaf) {
        for (int i = 0; i < leaf->n; i++) { print_row(leaf->data[i]); count++; }
        leaf = leaf->next;
    }
    printf("\n  Total: %d file(s)\n\n", count);
    return count;
}

/* List files whose name is between start and end (alphabetically) */
int list_range(Tree *t, const char *start, const char *end) {
    printf("\n  Range [%s  ...  %s]\n\n", start, end);
    printf("  %-32s  %-10s  %-6s  %s\n",
           "Filename","Size","Ext","Modified");
    printf("  %s\n",
           "--------------------------------------------------------------");
    Node *leaf = find_leaf(t, start);
    int count = 0;
    while (leaf) {
        for (int i = 0; i < leaf->n; i++) {
            const char *nm = leaf->data[i]->name;
            if (strcmp(nm, start) < 0) continue;
            if (strcmp(nm, end)   > 0) goto done;
            print_row(leaf->data[i]);
            count++;
        }
        leaf = leaf->next;
    }
done:
    printf("\n  Found: %d file(s)\n\n", count);
    return count;
}

/* List files by extension */
int list_ext(Tree *t, const char *ext) {
    printf("\n  Files with extension .%s:\n\n", ext);
    printf("  %-32s  %-10s  %s\n", "Filename","Size","Path");
    printf("  %s\n",
           "--------------------------------------------------------------");
    Node *leaf = t->root;
    while (!leaf->leaf) leaf = leaf->child[0];
    int count = 0;
    while (leaf) {
        for (int i = 0; i < leaf->n; i++) {
            File *f = leaf->data[i];
            if (strcmp(f->ext, ext) == 0) {
                char sz[20]; fmt_size(f->size, sz, sizeof sz);
                printf("  %-32s  %-10s  %s\n", f->name, sz, f->path);
                count++;
            }
        }
        leaf = leaf->next;
    }
    printf("\n  Found: %d .%s file(s)\n\n", count, ext);
    return count;
}

/* List files under a directory prefix */
int list_dir(Tree *t, const char *prefix) {
    printf("\n  Files under: %s\n\n", prefix);
    printf("  %-32s  %-10s  %s\n", "Filename","Size","Full Path");
    printf("  %s\n",
           "--------------------------------------------------------------");
    Node *leaf = t->root;
    while (!leaf->leaf) leaf = leaf->child[0];
    int count = 0;
    int plen  = (int)strlen(prefix);
    while (leaf) {
        for (int i = 0; i < leaf->n; i++) {
            File *f = leaf->data[i];
            if (strncmp(f->path, prefix, plen) == 0) {
                char sz[20]; fmt_size(f->size, sz, sizeof sz);
                printf("  %-32s  %-10s  %s\n", f->name, sz, f->path);
                count++;
            }
        }
        leaf = leaf->next;
    }
    printf("\n  Found: %d file(s)\n\n", count);
    return count;
}

/* Print stats */
void print_stats(Tree *t) {
    /* count nodes and height */
    int nodes = 0, leaves = 0, height = 0;
    /* BFS-style walk just counting */
    Node *stack[10000]; int top = 0, depth_stack[10000];
    stack[top] = t->root; depth_stack[top++] = 1;
    while (top > 0) {
        Node *nd = stack[--top];
        int   d  = depth_stack[top];
        nodes++;
        if (d > height) height = d;
        if (nd->leaf) { leaves++; continue; }
        for (int i = 0; i <= nd->n; i++) {
            stack[top]       = nd->child[i];
            depth_stack[top] = d + 1;
            top++;
        }
    }

    char sz[20]; fmt_size(t->total, sz, sizeof sz);
    printf("\n  ============================================\n");
    printf("              Index Statistics\n");
    printf("  ============================================\n");
    printf("  Total files    : %d\n",   t->count);
    printf("  Total size     : %s\n",   sz);
    printf("  Tree height    : %d\n",   height);
    printf("  Total nodes    : %d\n",   nodes);
    printf("  Leaf nodes     : %d\n",   leaves);
    printf("  Internal nodes : %d\n",   nodes - leaves);
    printf("  ============================================\n\n");
}

/* Print tree structure visually */
void print_tree(Tree *t) {
    /* Simple level-order print */
    Node *queue[10000]; int head = 0, tail = 0;
    int   level[10000];
    queue[tail] = t->root; level[tail++] = 0;
    int cur_level = -1;
    printf("\n  Tree structure:\n\n");
    while (head < tail) {
        Node *nd = queue[head];
        int   lv = level[head++];
        if (lv != cur_level) {
            if (cur_level >= 0) printf("\n");
            printf("  Level %d: ", lv);
            cur_level = lv;
        }
        printf("[");
        for (int i = 0; i < nd->n; i++) {
            printf("%s", nd->keys[i]);
            if (i < nd->n-1) printf("|");
        }
        printf("]%s  ", nd->leaf ? "(L)" : "");
        if (!nd->leaf)
            for (int i = 0; i <= nd->n; i++) {
                queue[tail]  = nd->child[i];
                level[tail++] = lv + 1;
            }
    }
    printf("\n\n");
}

/* ============================================================
   REAL DIRECTORY INDEXING
   ============================================================ */

#ifdef _WIN32
static int scan_dir(Tree *t, const char *dirpath) {
    char pattern[1024];
    snprintf(pattern, sizeof pattern, "%s\\*", dirpath);

    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        printf("  [ERROR] Cannot open: %s\n", dirpath);
        return 0;
    }

    int count = 0;
    do {
        if (!strcmp(fd.cFileName,".") || !strcmp(fd.cFileName,"..")) continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof fullpath, "%s\\%s", dirpath, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            count += scan_dir(t, fullpath);
        } else {
            long sz = (long)((ULONGLONG)fd.nFileSizeHigh << 32 | fd.nFileSizeLow);
            File *f = new_file(fd.cFileName, fullpath, sz);
            ULONGLONG ft = ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32)
                         | fd.ftLastWriteTime.dwLowDateTime;
            f->modified = (time_t)((ft - 116444736000000000ULL) / 10000000ULL);
            insert(t, f);
            count++;
            if (count % 100 == 0) printf("  ...%d files\n", count);
        }
    } while (FindNextFile(h, &fd));

    FindClose(h);
    return count;
}
#else
static int scan_dir(Tree *t, const char *dirpath) {
    DIR *d = opendir(dirpath);
    if (!d) { perror(dirpath); return 0; }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof fullpath, "%s/%s", dirpath, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) continue;

        if (S_ISREG(st.st_mode)) {
            File *f = new_file(entry->d_name, fullpath, st.st_size);
            f->modified = st.st_mtime;
            insert(t, f);
            count++;
        } else if (S_ISDIR(st.st_mode)) {
            count += scan_dir(t, fullpath);
        }
    }
    closedir(d);
    return count;
}
#endif

int index_directory(Tree *t, const char *raw_path) {
    /* clean up the path: strip quotes, trailing slashes */
    char path[512];
    strncpy(path, raw_path, 511);
    int len = (int)strlen(path);

    /* strip surrounding quotes */
    if (len >= 2 && path[0] == '"' && path[len-1] == '"') {
        memmove(path, path+1, len-2);
        path[len-2] = 0;
        len -= 2;
    }
    /* strip trailing slash */
    while (len > 1 && (path[len-1]=='\\' || path[len-1]=='/'))
        path[--len] = 0;

#ifdef _WIN32
    for (int i = 0; path[i]; i++)
        if (path[i] == '/') path[i] = '\\';
#endif

    printf("  Scanning: %s\n", path);
    clock_t t0 = clock();
    int n = scan_dir(t, path);
    double ms = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
    printf("  [OK] %d file(s) indexed in %.0f ms\n\n", n, ms);
    return n;
}

/* ============================================================
   SAVE / LOAD
   ============================================================ */

void save_index(Tree *t, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) { printf("  [ERROR] Cannot save to %s\n", filename); return; }

    /* walk all leaves */
    Node *leaf = t->root;
    while (!leaf->leaf) leaf = leaf->child[0];
    int count = 0;
    while (leaf) {
        for (int i = 0; i < leaf->n; i++) {
            File *f = leaf->data[i];
            fprintf(fp, "%s|%s|%ld|%ld\n",
                    f->name, f->path, f->size, (long)f->modified);
            count++;
        }
        leaf = leaf->next;
    }
    fclose(fp);
    printf("  [OK] Saved %d files to %s\n", count, filename);
}

void load_index(Tree *t, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { printf("  [ERROR] Cannot open %s\n", filename); return; }

    char line[1024];
    int count = 0;
    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\n")] = 0;
        char name[256], path[512];
        long sz, mod;
        if (sscanf(line, "%255[^|]|%511[^|]|%ld|%ld",
                   name, path, &sz, &mod) == 4) {
            File *f = new_file(name, path, sz);
            f->modified = (time_t)mod;
            insert(t, f);
            count++;
        }
    }
    fclose(fp);
    printf("  [OK] Loaded %d files from %s\n\n", count, filename);
}

/* ============================================================
   BENCHMARK
   ============================================================ */

void benchmark(void) {
    printf("\n  ============================================\n");
    printf("      B+ Tree vs Linear Search Benchmark\n");
    printf("  ============================================\n\n");

    int sizes[] = {1000, 5000, 10000, 50000};
    int ns = 4;

    printf("  %-10s  %-12s  %-12s  %-8s\n",
           "Files", "B+Tree(ms)", "Linear(ms)", "Speedup");
    printf("  %-10s  %-12s  %-12s  %-8s\n",
           "----------","------------","------------","--------");

    for (int s = 0; s < ns; s++) {
        int n = sizes[s];

        /* build tree */
        Tree *t = new_tree();
        File **arr = malloc(n * sizeof(File*));
        for (int i = 0; i < n; i++) {
            char nm[64];
            snprintf(nm, 64, "file_%06d.dat", i);
            arr[i] = new_file(nm, "/bench", i * 512L);
            File *f2 = new_file(nm, "/bench", i * 512L);
            insert(t, f2);
        }

        int queries = 1000;

        /* B+ tree search */
        clock_t t0 = clock();
        for (int q = 0; q < queries; q++) {
            char nm[64];
            snprintf(nm, 64, "file_%06d.dat", rand() % n);
            search(t, nm);
        }
        double bt = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;

        /* linear search */
        t0 = clock();
        for (int q = 0; q < queries; q++) {
            char nm[64];
            snprintf(nm, 64, "file_%06d.dat", rand() % n);
            for (int i = 0; i < n; i++)
                if (strcmp(arr[i]->name, nm) == 0) break;
        }
        double lin = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;

        printf("  %-10d  %-12.3f  %-12.3f  %.1fx\n",
               n, bt, lin, bt > 0 ? lin/bt : 0);

        /* cleanup */
        for (int i = 0; i < n; i++) free(arr[i]);
        free(arr);
        /* note: tree nodes not freed for brevity in benchmark */
    }
    printf("\n");
}

/* ============================================================
   SAMPLE DATA
   ============================================================ */

void load_samples(Tree *t) {
    const char *names[] = {
        "main.c","utils.c","btree.c","search.c","delete.c",
        "main.h","btree.h","README.md","Makefile","report.pdf",
        "notes.txt","data.json","config.ini","users.csv","sales.csv",
        "photo.jpg","banner.png","backup.zip","deploy.py","server.log"
    };
    const char *dirs[] = {
        "/src","/src","/src","/src","/src",
        "/include","/include","/","/","docs",
        "/notes","/data","/config","/data","/data",
        "/media","/media","/backup","/scripts","/logs"
    };
    int n = 20;
    srand((unsigned)time(NULL));
    for (int i = 0; i < n; i++) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s", dirs[i], names[i]);
        File *f = new_file(names[i], path, 512 + rand() % 100000);
        f->modified = time(NULL) - rand() % (60*60*24*365);
        insert(t, f);
    }
    printf("  [OK] Loaded %d sample files\n\n", n);
}

/* ============================================================
   MAIN MENU
   ============================================================ */

static void clear_input(void) {
    int c; while ((c = getchar()) != '\n' && c != EOF);
}

int main(void) {
    Tree *t = new_tree();
    load_samples(t);

    int choice;
    while (1) {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
        printf("=====================================================\n");
        printf("       B+ Tree File Management System\n");
        printf("       Files: %-6d", t->count);
        char sz[20]; fmt_size(t->total, sz, sizeof sz);
        printf("  Total size: %s\n", sz);
        printf("=====================================================\n\n");

        printf("  -- File Operations --\n");
        printf("  1  Add a file\n");
        printf("  2  Search for a file\n");
        printf("  3  Delete a file\n\n");

        printf("  -- Browse & Query --\n");
        printf("  4  List all files (sorted)\n");
        printf("  5  Filter by extension  (e.g. pdf, zip, mp4)\n");
        printf("  6  Filter by directory\n");
        printf("  7  Range search  (e.g. a... to m...)\n\n");

        printf("  -- Index --\n");
        printf("  8  Scan a real folder on your computer\n");
        printf("  9  Save index to file\n");
        printf(" 10  Load index from file\n\n");

        printf("  -- Info --\n");
        printf(" 11  Show tree structure\n");
        printf(" 12  Show statistics\n");
        printf(" 13  Run benchmark\n");
        printf(" 14  Reload sample data\n\n");
        printf("  -- Visualization --\n");
        printf(" 15  Show tree in terminal\n");
        printf(" 16  Export DOT file (Graphviz PNG)\n");
        printf(" 17  Export HTML (open in browser)\n\n");
        printf("  0  Exit\n");
        printf("\n  Choice: ");

        printf("  0  Exit\n");
        printf("\n  Choice: ");

        if (scanf("%d", &choice) != 1) { clear_input(); continue; }
        clear_input();
        printf("\n");

        char buf[512];

        switch (choice) {

        case 1: {
            char name[256], path[512]; long size;
            printf("  Filename      : "); fgets(name, 256, stdin); name[strcspn(name,"\n")]=0;
            printf("  Full path     : "); fgets(path, 512, stdin); path[strcspn(path,"\n")]=0;
            printf("  Size (bytes)  : "); scanf("%ld",&size); clear_input();
            File *f = new_file(name, path, size < 0 ? 0 : size);
            insert(t, f);
            printf("  [OK] '%s' added.\n", name);
            break;
        }

        case 2: {
            printf("  Search filename: "); fgets(buf,256,stdin); buf[strcspn(buf,"\n")]=0;
            clock_t t0 = clock();
            File *f = search(t, buf);
            double us = (double)(clock()-t0)/CLOCKS_PER_SEC*1000000;
            if (f) {
                char sz2[20]; fmt_size(f->size, sz2, sizeof sz2);
                char ts[20]; strftime(ts,sizeof ts,"%Y-%m-%d",localtime(&f->modified));
                printf("\n  [FOUND] in %.1f us\n", us);
                printf("  Name     : %s\n", f->name);
                printf("  Path     : %s\n", f->path);
                printf("  Size     : %s\n", sz2);
                printf("  Modified : %s\n", ts);
            } else {
                printf("  [NOT FOUND] '%.200s'  (%.1f us)\n", buf, us);
            }
            break;
        }

        case 3: {
            printf("  Delete filename: "); fgets(buf,256,stdin); buf[strcspn(buf,"\n")]=0;
            if (delete_file(t, buf)) printf("  [OK] Deleted '%s'\n", buf);
            else                     printf("  [NOT FOUND] '%s'\n", buf);
            break;
        }

        case 4: list_all(t);  break;

        case 5: {
            printf("  Extension (no dot): "); fgets(buf,32,stdin); buf[strcspn(buf,"\n")]=0;
            list_ext(t, buf);
            break;
        }

        case 6: {
            printf("  Directory prefix: "); fgets(buf,512,stdin); buf[strcspn(buf,"\n")]=0;
            list_dir(t, buf);
            break;
        }

        case 7: {
            char s[256], e[256];
            printf("  Start: "); fgets(s,256,stdin); s[strcspn(s,"\n")]=0;
            printf("  End  : "); fgets(e,256,stdin); e[strcspn(e,"\n")]=0;
            list_range(t, s, e);
            break;
        }

        case 8: {
            printf("  Folder path: "); fgets(buf,512,stdin); buf[strcspn(buf,"\n")]=0;
            index_directory(t, buf);
            break;
        }

        case 9: {
            printf("  Save to file: "); fgets(buf,256,stdin); buf[strcspn(buf,"\n")]=0;
            save_index(t, buf);
            break;
        }

        case 10: {
            printf("  Load from file: "); fgets(buf,256,stdin); buf[strcspn(buf,"\n")]=0;
            load_index(t, buf);
            break;
        }

        case 11: print_tree(t);   break;
        case 12: print_stats(t);  break;
        case 13: benchmark();     break;
        case 14: load_samples(t); break;

        case 0:
            printf("  Goodbye!\n\n");
            return 0;

        default:
            printf("  Invalid choice.\n");
        }

        printf("\n  Press Enter to continue...");
        getchar();
    }
}
