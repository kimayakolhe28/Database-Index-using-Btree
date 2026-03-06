/*
 * ============================================================
 * FILE     : main.c
 * CONTAINS : Full interactive menu — all 17 options
 *
 * COMPILE  :
 *   gcc -Wall -std=c11 -o fm \
 *       main.c insert.c search.c delete.c operations.c visual.c
 *
 * RUN      : ./fm
 * ============================================================
 */

#include "structs.h"

/* ── clear terminal input buffer ────────────────────────────── */
static void clear_input(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* ── print the main menu ────────────────────────────────────── */
static void print_menu(Tree *t) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif

    char sz[20];
    fmt_size(t->total, sz, sizeof sz);

    printf("=====================================================\n");
    printf("       B+ Tree File Management System\n");
    printf("       Files: %-6d  Total size: %s\n", t->count, sz);
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
    printf(" 16  Export DOT file  (Graphviz PNG)\n");
    printf(" 17  Export HTML      (open in browser)\n\n");

    printf("  0  Exit\n");
    printf("\n  Choice: ");
}

/* ============================================================
   MAIN
   ============================================================ */
int main(void) {
    srand((unsigned)time(NULL));

    Tree *t = new_tree();
    load_samples(t);   /* auto-load 20 sample files on startup */

    int  choice;
    char buf[512];

    while (1) {
        print_menu(t);

        if (scanf("%d", &choice) != 1) { clear_input(); continue; }
        clear_input();
        printf("\n");

        switch (choice) {

        /* ── 1. Add a file ────────────────────────────────── */
        case 1: {
            char name[256], path[512];
            long size;
            printf("  Filename      : ");
            fgets(name, 256, stdin);
            name[strcspn(name, "\n")] = 0;

            printf("  Full path     : ");
            fgets(path, 512, stdin);
            path[strcspn(path, "\n")] = 0;

            printf("  Size (bytes)  : ");
            scanf("%ld", &size);
            clear_input();

            File *f = new_file(name, path, size < 0 ? 0 : size);
            insert(t, f);
            printf("  [OK] '%s' added.\n", name);
            break;
        }

        /* ── 2. Search for a file ─────────────────────────── */
        case 2: {
            printf("  Search filename: ");
            fgets(buf, 256, stdin);
            buf[strcspn(buf, "\n")] = 0;

            clock_t t0 = clock();
            File   *f  = search(t, buf);
            double  us = (double)(clock()-t0)/CLOCKS_PER_SEC*1000000;

            if (f) {
                char sz[20], ts[20];
                fmt_size(f->size, sz, sizeof sz);
                strftime(ts, sizeof ts, "%Y-%m-%d",
                         localtime(&f->modified));
                printf("\n  [FOUND] in %.1f us\n", us);
                printf("  Name     : %s\n", f->name);
                printf("  Path     : %s\n", f->path);
                printf("  Size     : %s\n", sz);
                printf("  Modified : %s\n", ts);
            } else {
                printf("  [NOT FOUND] '%s'  (%.1f us)\n", buf, us);
            }
            break;
        }

        /* ── 3. Delete a file ─────────────────────────────── */
        case 3: {
            printf("  Delete filename: ");
            fgets(buf, 256, stdin);
            buf[strcspn(buf, "\n")] = 0;

            if (delete_file(t, buf))
                printf("  [OK] Deleted '%s'\n", buf);
            else
                printf("  [NOT FOUND] '%s'\n", buf);
            break;
        }

        /* ── 4. List all ──────────────────────────────────── */
        case 4:
            list_all(t);
            break;

        /* ── 5. Filter by extension ───────────────────────── */
        case 5:
            printf("  Extension (no dot): ");
            fgets(buf, 32, stdin);
            buf[strcspn(buf, "\n")] = 0;
            list_ext(t, buf);
            break;

        /* ── 6. Filter by directory ───────────────────────── */
        case 6:
            printf("  Directory prefix: ");
            fgets(buf, 512, stdin);
            buf[strcspn(buf, "\n")] = 0;
            list_dir(t, buf);
            break;

        /* ── 7. Range search ──────────────────────────────── */
        case 7: {
            char s[256], e[256];
            printf("  Start: ");
            fgets(s, 256, stdin);
            s[strcspn(s, "\n")] = 0;
            printf("  End  : ");
            fgets(e, 256, stdin);
            e[strcspn(e, "\n")] = 0;
            list_range(t, s, e);
            break;
        }

        /* ── 8. Scan real folder ──────────────────────────── */
        case 8:
            printf("  Folder path: ");
            fgets(buf, 512, stdin);
            buf[strcspn(buf, "\n")] = 0;
            index_directory(t, buf);
            break;

        /* ── 9. Save index ────────────────────────────────── */
        case 9:
            printf("  Save to file: ");
            fgets(buf, 256, stdin);
            buf[strcspn(buf, "\n")] = 0;
            save_index(t, buf);
            break;

        /* ── 10. Load index ───────────────────────────────── */
        case 10:
            printf("  Load from file: ");
            fgets(buf, 256, stdin);
            buf[strcspn(buf, "\n")] = 0;
            load_index(t, buf);
            break;

        /* ── 11. Show tree structure (basic) ──────────────── */
        case 11:
            print_tree_terminal(t);
            break;

        /* ── 12. Show statistics ──────────────────────────── */
        case 12: {
            /* BFS to count nodes and height */
            Node *stk[50000]; int lv[50000], h=0, tl=0;
            stk[tl]=t->root; lv[tl]=0; tl++;
            int nodes=0, leaves=0, height=0;
            while (h<tl) {
                Node *nd=stk[h]; int l=lv[h]; h++;
                nodes++;
                if (l>height) height=l;
                if (nd->leaf) { leaves++; continue; }
                for (int i=0;i<=nd->n;i++) {
                    stk[tl]=nd->child[i]; lv[tl]=l+1; tl++;
                }
            }
            char sz[20]; fmt_size(t->total, sz, sizeof sz);
            printf("\n  ============================================\n");
            printf("              Index Statistics\n");
            printf("  ============================================\n");
            printf("  Total files    : %d\n",   t->count);
            printf("  Total size     : %s\n",   sz);
            printf("  Tree height    : %d\n",   height+1);
            printf("  Total nodes    : %d\n",   nodes);
            printf("  Leaf nodes     : %d\n",   leaves);
            printf("  Internal nodes : %d\n",   nodes-leaves);
            printf("  ============================================\n\n");
            break;
        }

        /* ── 13. Benchmark ────────────────────────────────── */
        case 13:
            benchmark();
            break;

        /* ── 14. Reload samples ───────────────────────────── */
        case 14:
            load_samples(t);
            break;

        /* ── 15. Terminal tree visualization ──────────────── */
        case 15:
            print_tree_terminal(t);
            break;

        /* ── 16. Export DOT for Graphviz ──────────────────── */
        case 16:
            export_dot(t, "btree.dot");
            break;

        /* ── 17. Export HTML ──────────────────────────────── */
        case 17:
            export_html(t, "btree.html");
            break;

        /* ── 0. Exit ──────────────────────────────────────── */
        case 0:
            printf("  Goodbye!\n\n");
            return 0;

        default:
            printf("  Invalid choice. Try again.\n");
        }

        printf("\n  Press Enter to continue...");
        getchar();
    }
}
