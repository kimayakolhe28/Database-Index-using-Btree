/*
 * ============================================================
 * FILE     : operations.c
 * CONTAINS : save, load, directory scan, benchmark,
 *            sample data loader
 * ============================================================
 */

#include "structs.h"

/* ============================================================
   SAVE INDEX TO FILE
   Format per line: name|path|size|modified
   ============================================================ */
void save_index(Tree *t, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("  [ERROR] Cannot save to %s\n", filename);
        return;
    }

    /* walk entire leaf chain */
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

/* ============================================================
   LOAD INDEX FROM FILE
   ============================================================ */
void load_index(Tree *t, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("  [ERROR] Cannot open %s\n", filename);
        return;
    }

    char line[1024];
    int  count = 0;

    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\n")] = 0;

        char name[256], path[512];
        long sz, mod;

        if (sscanf(line, "%255[^|]|%511[^|]|%ld|%ld",
                   name, path, &sz, &mod) == 4) {
            File *f     = new_file(name, path, sz);
            f->modified = (time_t)mod;
            insert(t, f);
            count++;
        }
    }
    fclose(fp);
    printf("  [OK] Loaded %d files from %s\n\n", count, filename);
}

/* ============================================================
   REAL DIRECTORY SCANNING
   Windows uses FindFirstFile / FindNextFile
   Linux/Mac uses opendir / readdir
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
        if (!strcmp(fd.cFileName, ".") ||
            !strcmp(fd.cFileName, "..")) continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof fullpath,
                 "%s\\%s", dirpath, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* recurse into subdirectory */
            count += scan_dir(t, fullpath);
        } else {
            long sz = (long)((ULONGLONG)fd.nFileSizeHigh << 32
                           | fd.nFileSizeLow);
            File *f = new_file(fd.cFileName, fullpath, sz);

            /* convert Windows FILETIME to Unix time_t */
            ULONGLONG ft =
                ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32)
               | fd.ftLastWriteTime.dwLowDateTime;
            f->modified =
                (time_t)((ft - 116444736000000000ULL) / 10000000ULL);

            insert(t, f);
            count++;
            if (count % 100 == 0)
                printf("  ...%d files indexed\n", count);
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
        snprintf(fullpath, sizeof fullpath,
                 "%s/%s", dirpath, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) continue;

        if (S_ISREG(st.st_mode)) {
            File *f     = new_file(entry->d_name, fullpath,
                                   st.st_size);
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

/* ── Public: clean up path then scan ────────────────────────── */
int index_directory(Tree *t, const char *raw_path) {
    char path[512];
    strncpy(path, raw_path, 511);

    /* strip surrounding quotes */
    int len = (int)strlen(path);
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
   BENCHMARK : B+ Tree vs Linear Search
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

        /* build tree and linear array */
        Tree  *t   = new_tree();
        File **arr = malloc(n * sizeof(File*));

        for (int i = 0; i < n; i++) {
            char nm[64];
            snprintf(nm, 64, "file_%06d.dat", i);
            arr[i] = new_file(nm, "/bench", i * 512L);
            File *f2 = new_file(nm, "/bench", i * 512L);
            insert(t, f2);
        }

        int queries = 1000;

        /* B+ tree search timing */
        clock_t t0 = clock();
        for (int q = 0; q < queries; q++) {
            char nm[64];
            snprintf(nm, 64, "file_%06d.dat", rand() % n);
            search(t, nm);
        }
        double bt = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;

        /* linear search timing */
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

        for (int i = 0; i < n; i++) free(arr[i]);
        free(arr);
    }
    printf("\n");
}

/* ============================================================
   LOAD SAMPLE DATA  (20 files for quick demo)
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
        "/include","/include","/","/","/docs",
        "/notes","/data","/config","/data","/data",
        "/media","/media","/backup","/scripts","/logs"
    };
    int n = 20;
    srand((unsigned)time(NULL));
    for (int i = 0; i < n; i++) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s", dirs[i], names[i]);
        File *f     = new_file(names[i], path,
                               512 + rand() % 100000);
        f->modified = time(NULL) - rand() % (60*60*24*365);
        insert(t, f);
    }
    printf("  [OK] Loaded %d sample files\n\n", n);
}
