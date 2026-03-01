#include "../include/file_btree.h"

static void clear_input(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

static void print_header(void) {
    system("cls");
    printf("=============================================================\n");
    printf("       B+ Tree File Management System  v1.0                 \n");
    printf("   High-Performance Hierarchical File Indexing              \n");
    printf("=============================================================\n");
}

static void print_menu(BPlusTree *tree) {
    printf("\n  Index: %d file(s)  |  Size: ", tree->total_files);
    if (tree->total_size >= 1024*1024)
        printf("%.1f MB", tree->total_size/(1024.0*1024.0));
    else if (tree->total_size >= 1024)
        printf("%.1f KB", tree->total_size/1024.0);
    else
        printf("%ld B", tree->total_size);
    printf("\n\n");

    printf("  +--- File Operations ------------------------------------+\n");
    printf("  |  1  Add / update a file                               |\n");
    printf("  |  2  Search for a file                                 |\n");
    printf("  |  3  Delete a file                                     |\n");
    printf("  +--- Directory Queries ----------------------------------+\n");
    printf("  |  4  List all files (sorted)                           |\n");
    printf("  |  5  List files by extension                           |\n");
    printf("  |  6  List files in directory                           |\n");
    printf("  |  7  Range search  [start ... end]                     |\n");
    printf("  +--- Index Management -----------------------------------+\n");
    printf("  |  8  Index a real directory                            |\n");
    printf("  |  9  Save index to file                                |\n");
    printf("  | 10  Load index from file                              |\n");
    printf("  +--- Analysis ------------------------------------------+\n");
    printf("  | 11  Show tree structure                               |\n");
    printf("  | 12  Show statistics                                   |\n");
    printf("  | 13  Run performance benchmark                         |\n");
    printf("  | 14  Load sample dataset                               |\n");
    printf("  +-------------------------------------------------------+\n");
    printf("  |  0  Exit                                              |\n");
    printf("  +-------------------------------------------------------+\n");
    printf("\n  Choice: ");
}

static void do_add(BPlusTree *tree) {
    char fname[256], path[512];
    long size;

    printf("\n  Filename : "); fgets(fname, 256, stdin);
    fname[strcspn(fname,"\n")] = 0;
    if (!fname[0]) { printf("  Cancelled.\n"); return; }

    printf("  Full path: "); fgets(path, 512, stdin);
    path[strcspn(path,"\n")] = 0;
    if (!path[0]) snprintf(path, 512, "/%s", fname);

    printf("  Size (bytes): "); scanf("%ld", &size); clear_input();
    if (size < 0) size = 0;

    FileEntry *f = file_entry_create(fname, path, size);
    bpt_insert(tree, f);
    printf("  [OK] '%s' added to index.\n", fname);
}

static void do_search(BPlusTree *tree) {
    char fname[256];
    printf("\n  Filename to search: "); fgets(fname, 256, stdin);
    fname[strcspn(fname,"\n")] = 0;

    long cmp = 0;
    clock_t t0 = clock();
    FileEntry *f = bpt_search(tree, fname, &cmp);
    double ms = (double)(clock()-t0)/CLOCKS_PER_SEC*1000000.0;

    if (f) {
        printf("\n  [FOUND] (%.1f us, %ld comparisons):\n\n", ms, cmp);
        printf("  Filename : %s\n", f->filename);
        printf("  Path     : %s\n", f->path);
        printf("  Size     : %ld bytes\n", f->size);
        printf("  Type     : %s\n", f->type[0] ? f->type : "-");
        char buf[32];
        strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", localtime(&f->modified));
        printf("  Modified : %s\n", buf);
    } else {
        printf("\n  [NOT FOUND] '%s'  (%.1f us, %ld comparisons)\n",
               fname, ms, cmp);
    }
}

static void do_delete(BPlusTree *tree) {
    char fname[256];
    printf("\n  Filename to delete: "); fgets(fname, 256, stdin);
    fname[strcspn(fname,"\n")] = 0;

    if (bpt_delete(tree, fname))
        printf("  [OK] '%s' removed from index.\n", fname);
    else
        printf("  [NOT FOUND] '%s'\n", fname);
}

static void do_range(BPlusTree *tree) {
    char s[256], e[256];
    printf("\n  Start of range: "); fgets(s, 256, stdin); s[strcspn(s,"\n")] = 0;
    printf("  End of range  : "); fgets(e, 256, stdin); e[strcspn(e,"\n")] = 0;
    bpt_range(tree, s, e);
}

static void do_list_type(BPlusTree *tree) {
    char ext[32];
    printf("\n  Extension (without dot, e.g. pdf): ");
    fgets(ext, 32, stdin); ext[strcspn(ext,"\n")] = 0;
    bpt_list_by_type(tree, ext);
}

static void do_list_dir(BPlusTree *tree) {
    char prefix[512];
    printf("\n  Directory prefix: ");
    fgets(prefix, 512, stdin); prefix[strcspn(prefix,"\n")] = 0;
    bpt_list_dir(tree, prefix);
}

static void do_index_dir(BPlusTree *tree) {
    char dirpath[512];
    printf("\n  Directory to index: ");
    fgets(dirpath, 512, stdin); dirpath[strcspn(dirpath,"\n")] = 0;
    bpt_index_directory(tree, dirpath);
}

static void do_save(BPlusTree *tree) {
    char fname[256];
    printf("\n  Save to file: ");
    fgets(fname, 256, stdin); fname[strcspn(fname,"\n")] = 0;
    bpt_save(tree, fname);
}

static BPlusTree *do_load(BPlusTree *tree) {
    char fname[256];
    printf("\n  Load from file: ");
    fgets(fname, 256, stdin); fname[strcspn(fname,"\n")] = 0;
    BPlusTree *loaded = bpt_load(fname);
    if (loaded) {
        bpt_destroy(tree);
        return loaded;
    }
    printf("  [ERROR] Could not load '%s'\n", fname);
    return tree;
}

static const char *SAMPLE_NAMES[] = {
    "main.c", "utils.c", "btree.c", "search.c", "insert.c",
    "delete.c", "bench.c", "main.h", "btree.h", "utils.h",
    "Makefile", "README.md", "design.md", "analysis.md",
    "report.pdf", "slides.pdf", "notes.txt", "todo.txt",
    "data.json", "config.json", "settings.ini", "app.conf",
    "users.csv", "products.csv", "sales.csv",
    "photo_001.jpg", "photo_002.jpg", "banner.png", "logo.svg",
    "backup.tar.gz", "archive.zip", "package.deb",
    "script.sh", "deploy.py", "test_suite.py", "setup.py",
    "database.db", "cache.db", "index.db",
    "server.log", "error.log", "access.log",
    "Dockerfile", "docker-compose.yml", ".gitignore",
    "CHANGELOG.md", "LICENSE", "CONTRIBUTING.md",
    "build.gradle", "pom.xml", "CMakeLists.txt",
};

static const char *SAMPLE_DIRS[] = {
    "/src", "/src", "/src", "/src/search", "/src/insert",
    "/src/delete", "/src/bench", "/include", "/include", "/include",
    "/", "/docs", "/docs", "/docs",
    "/docs", "/docs/slides", "/notes", "/notes",
    "/data", "/config", "/config", "/config",
    "/data", "/data", "/data",
    "/media", "/media", "/media", "/media",
    "/backup", "/backup", "/backup",
    "/scripts", "/scripts", "/tests", "/scripts",
    "/db", "/db", "/db",
    "/logs", "/logs", "/logs",
    "/docker", "/docker", "/",
    "/docs", "/", "/docs",
    "/build", "/build", "/build",
};

static void load_sample_data(BPlusTree *tree) {
    int n = (int)(sizeof SAMPLE_NAMES / sizeof SAMPLE_NAMES[0]);
    printf("\n  Loading %d sample files...\n", n);

    srand((unsigned)time(NULL));
    for (int i = 0; i < n; i++) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s", SAMPLE_DIRS[i], SAMPLE_NAMES[i]);
        long size = 512L + (rand() % (1024*512));
        FileEntry *f = file_entry_create(SAMPLE_NAMES[i], path, size);
        f->modified = time(NULL) - (rand() % (60*60*24*365));
        bpt_insert(tree, f);
    }
    printf("  [OK] Sample dataset loaded (%d files)\n", n);
}

int main(void) {
    srand((unsigned)time(NULL));
    BPlusTree *tree = bpt_create();

    load_sample_data(tree);

    int choice;
    while (1) {
        print_header();
        print_menu(tree);
        if (scanf("%d", &choice) != 1) { clear_input(); continue; }
        clear_input();

        printf("\n");

        switch (choice) {
            case  1: do_add(tree);                        break;
            case  2: do_search(tree);                     break;
            case  3: do_delete(tree);                     break;
            case  4: bpt_list_all(tree);                  break;
            case  5: do_list_type(tree);                  break;
            case  6: do_list_dir(tree);                   break;
            case  7: do_range(tree);                      break;
            case  8: do_index_dir(tree);                  break;
            case  9: do_save(tree);                       break;
            case 10: tree = do_load(tree);                break;
            case 11: bpt_print_tree(tree);                break;
            case 12: bpt_print_stats(tree);               break;
            case 13: bpt_benchmark(0);                    break;
            case 14: load_sample_data(tree);              break;
            case  0:
                printf("  Saving index...\n");
                bpt_save(tree, "output/autosave.idx");
                bpt_destroy(tree);
                printf("  Goodbye!\n\n");
                return 0;
            default:
                printf("  Invalid choice.\n");
        }
        printf("\n  Press Enter to continue...");
        getchar();
    }
}
