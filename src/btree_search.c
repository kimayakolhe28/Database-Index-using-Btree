#include "../include/file_btree.h"

/* --- Exact search --------------------------------------------- */

FileEntry *bpt_search(BPlusTree *tree, const char *filename, long *comparisons) {
    if (!tree || !filename) return NULL;
    if (comparisons) *comparisons = 0;

    BPlusNode *cur = tree->root;

    /* navigate internal nodes */
    while (!cur->is_leaf) {
        int i = 0;
        while (i < cur->num_keys) {
            if (comparisons) (*comparisons)++;
            if (strcmp(filename, cur->keys[i]) < 0) break;
            i++;
        }
        cur = cur->children[i];
    }

    /* search leaf */
    for (int i = 0; i < cur->num_keys; i++) {
        if (comparisons) (*comparisons)++;
        int cmp = strcmp(filename, cur->files[i]->filename);
        if (cmp == 0) return cur->files[i];
        if (cmp < 0)  break;
    }
    return NULL;
}

/* --- Range search [start, end] -------------------------------- */

int bpt_range(BPlusTree *tree, const char *start, const char *end) {
    if (!tree) return 0;

    /* find the first leaf that could contain 'start' */
    BPlusNode *leaf = bpt_find_leaf(tree, start);

    int count = 0;
    printf("\n  %-30s  %-10s  %-5s  %s\n",
           "Filename", "Size", "Type", "Modified");
    printf("  %s\n", "----------------------------------------------------------------");

    while (leaf) {
        for (int i = 0; i < leaf->num_keys; i++) {
            const char *fn = leaf->files[i]->filename;
            if (strcmp(fn, start) < 0)  continue;
            if (strcmp(fn, end)   > 0)  goto done;
            file_entry_print(leaf->files[i]);
            count++;
        }
        leaf = leaf->next;
    }
done:
    printf("\n  %d file(s) found in range [%s ... %s]\n\n", count, start, end);
    return count;
}

/* --- List all files (sorted, via leaf scan) ------------------- */

int bpt_list_all(BPlusTree *tree) {
    if (!tree) return 0;

    /* find leftmost leaf */
    BPlusNode *leaf = tree->root;
    while (!leaf->is_leaf) leaf = leaf->children[0];

    int count = 0;
    printf("\n  %-30s  %-10s  %-5s  %s\n",
           "Filename", "Size", "Type", "Modified");
    printf("  %s\n", "----------------------------------------------------------------");

    while (leaf) {
        for (int i = 0; i < leaf->num_keys; i++) {
            file_entry_print(leaf->files[i]);
            count++;
        }
        leaf = leaf->next;
    }
    printf("\n  Total: %d file(s)\n\n", count);
    return count;
}

/* --- List files by extension ---------------------------------- */

int bpt_list_by_type(BPlusTree *tree, const char *ext) {
    if (!tree || !ext) return 0;

    BPlusNode *leaf = tree->root;
    while (!leaf->is_leaf) leaf = leaf->children[0];

    int count = 0;
    printf("\n  Files with extension .%s:\n", ext);
    printf("  %-30s  %-10s  %s\n", "Filename", "Size", "Path");
    printf("  %s\n", "----------------------------------------------------------------");

    while (leaf) {
        for (int i = 0; i < leaf->num_keys; i++) {
            FileEntry *f = leaf->files[i];
            if (strcmp(f->type, ext) == 0) {
                char sz[32];
                if (f->size >= 1024*1024)
                    snprintf(sz, sizeof sz, "%.1f MB", f->size/(1024.0*1024.0));
                else if (f->size >= 1024)
                    snprintf(sz, sizeof sz, "%.1f KB", f->size/1024.0);
                else
                    snprintf(sz, sizeof sz, "%ld B", f->size);
                printf("  %-30s  %-10s  %s\n", f->filename, sz, f->path);
                count++;
            }
        }
        leaf = leaf->next;
    }
    printf("\n  Found %d .%s file(s)\n\n", count, ext);
    return count;
}

/* --- List files under a directory prefix ---------------------- */

int bpt_list_dir(BPlusTree *tree, const char *dir_prefix) {
    if (!tree || !dir_prefix) return 0;

    BPlusNode *leaf = tree->root;
    while (!leaf->is_leaf) leaf = leaf->children[0];

    int count = 0;
    printf("\n  Files under '%s':\n", dir_prefix);
    printf("  %-30s  %-10s  %s\n", "Filename", "Size", "Full Path");
    printf("  %s\n", "----------------------------------------------------------------");

    while (leaf) {
        for (int i = 0; i < leaf->num_keys; i++) {
            FileEntry *f = leaf->files[i];
            if (strncmp(f->path, dir_prefix, strlen(dir_prefix)) == 0) {
                char sz[32];
                if (f->size >= 1024*1024)
                    snprintf(sz, sizeof sz, "%.1f MB", f->size/(1024.0*1024.0));
                else if (f->size >= 1024)
                    snprintf(sz, sizeof sz, "%.1f KB", f->size/1024.0);
                else
                    snprintf(sz, sizeof sz, "%ld B",   f->size);
                printf("  %-30s  %-10s  %s\n", f->filename, sz, f->path);
                count++;
            }
        }
        leaf = leaf->next;
    }
    printf("\n  Found %d file(s) in '%s'\n\n", count, dir_prefix);
    return count;
}

/* --- Index a real directory ----------------------------------- */

static int scan_dir(BPlusTree *tree, const char *base, const char *rel) {
    char fullpath[1024];
    snprintf(fullpath, sizeof fullpath, "%s/%s", base, rel[0] ? rel : "");

    DIR *d = opendir(fullpath);
    if (!d) { perror(fullpath); return 0; }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char childpath[1024];
        snprintf(childpath, sizeof childpath, "%s/%s",
                 fullpath, entry->d_name);

        struct stat st;
        if (stat(childpath, &st) != 0) continue;

        if (S_ISREG(st.st_mode)) {
            /* build relative path for storage */
            char relpath[1024];
            if (rel[0])
                snprintf(relpath, sizeof relpath, "%s/%s/%s",
                         base, rel, entry->d_name);
            else
                snprintf(relpath, sizeof relpath, "%s/%s",
                         base, entry->d_name);

            FileEntry *f = file_entry_create(entry->d_name, relpath, st.st_size);
            f->modified = st.st_mtime;
            f->created  = st.st_ctime;
            bpt_insert(tree, f);
            count++;
        } else if (S_ISDIR(st.st_mode)) {
            char newrel[1024];
            if (rel[0])
                snprintf(newrel, sizeof newrel, "%s/%s", rel, entry->d_name);
            else
                snprintf(newrel, sizeof newrel, "%s", entry->d_name);
            count += scan_dir(tree, base, newrel);
        }
    }
    closedir(d);
    return count;
}

int bpt_index_directory(BPlusTree *tree, const char *dir_path) {
    printf("  Scanning: %s\n", dir_path);
    clock_t t0 = clock();
    int n = scan_dir(tree, dir_path, "");
    double ms = (double)(clock()-t0) / CLOCKS_PER_SEC * 1000.0;
    printf("  [OK] Indexed %d file(s) in %.1f ms\n\n", n, ms);
    return n;
}
