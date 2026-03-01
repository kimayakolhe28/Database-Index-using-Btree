#include "../include/file_btree.h"

/* --- Linear search for comparison ---------------------------- */

static FileEntry *linear_search(FileEntry **arr, int n, const char *name) {
    for (int i = 0; i < n; i++)
        if (strcmp(arr[i]->filename, name) == 0)
            return arr[i];
    return NULL;
}

/* --- Benchmark suite ------------------------------------------ */

void bpt_benchmark(int num_files) {
    printf("\n============================================================\n");
    printf("       B+ Tree vs Linear Search Benchmark\n");
    printf("============================================================\n\n");

    /* -- dataset sizes -- */
    int sizes[] = {500, 1000, 5000, 10000, 50000};
    int ns      = (int)(sizeof sizes / sizeof sizes[0]);

    /* cap at num_files if caller supplied one */
    if (num_files > 0) {
        ns = 1;
        sizes[0] = num_files;
    }

    printf("  %-12s | %-14s | %-14s | %-8s | %-10s\n",
           "Dataset", "B+ Tree (ms)", "Linear (ms)", "Speedup", "Avg Cmp");
    printf("  %s\n",
           "--------------|----------------|----------------|----------|-----------");

    for (int s = 0; s < ns; s++) {
        int n = sizes[s];

        /* -- build B+ tree -- */
        BPlusTree  *tree    = bpt_create();
        FileEntry **lin_arr = malloc(n * sizeof(FileEntry*));

        for (int i = 0; i < n; i++) {
            char fname[256];
            /* pad numbers so filenames sort numerically as well */
            snprintf(fname, sizeof fname, "file_%07d.dat", i);
            FileEntry *f  = file_entry_create(fname, "/bench", (i+1)*512L);
            lin_arr[i]    = file_entry_create(fname, "/bench", (i+1)*512L);
            bpt_insert(tree, f);
        }

        /* -- benchmark searches -- */
        int  queries = 2000;
        long total_comp = 0;

        /* B+ tree */
        clock_t t0 = clock();
        for (int q = 0; q < queries; q++) {
            char fname[256];
            int  idx = rand() % n;
            snprintf(fname, sizeof fname, "file_%07d.dat", idx);
            long cmp = 0;
            bpt_search(tree, fname, &cmp);
            total_comp += cmp;
        }
        double bt_ms = (double)(clock()-t0)/CLOCKS_PER_SEC * 1000.0;

        /* linear */
        t0 = clock();
        for (int q = 0; q < queries; q++) {
            char fname[256];
            int  idx = rand() % n;
            snprintf(fname, sizeof fname, "file_%07d.dat", idx);
            linear_search(lin_arr, n, fname);
        }
        double lin_ms = (double)(clock()-t0)/CLOCKS_PER_SEC * 1000.0;

        double speedup  = (bt_ms > 0.0) ? (lin_ms / bt_ms) : 0.0;
        double avg_cmp  = (double)total_comp / queries;

        printf("  %-12d | %-14.3f | %-14.3f | %-8.1fx | %-10.2f\n",
               n, bt_ms, lin_ms, speedup, avg_cmp);

        /* -- range query benchmark -- */
        /* (printed separately for the largest size) */

        /* cleanup */
        bpt_destroy(tree);
        for (int i = 0; i < n; i++) file_entry_free(lin_arr[i]);
        free(lin_arr);
    }

    /* -- height growth table -- */
    printf("\n  Tree Height Growth:\n");
    printf("  %-12s | %-8s | %-10s\n", "Files", "Height", "Nodes");
    printf("  %s\n", "--------------|----------|-----------");

    int milestones[] = {10, 50, 100, 500, 1000, 5000, 10000, 50000};
    int nm = (int)(sizeof milestones / sizeof milestones[0]);

    BPlusTree *gtree = bpt_create();
    int mi = 0;
    for (int i = 1; i <= milestones[nm-1] && mi < nm; i++) {
        char fname[256];
        snprintf(fname, sizeof fname, "g_%07d.dat", i);
        FileEntry *f = file_entry_create(fname, "/g", i*100L);
        bpt_insert(gtree, f);
        if (i == milestones[mi]) {
            TreeStats st = bpt_get_stats(gtree);
            printf("  %-12d | %-8d | %-10d\n", i, st.height, st.total_nodes);
            mi++;
        }
    }
    bpt_destroy(gtree);

    /* -- export CSV -- */
    FILE *fp = fopen("output/benchmark_results.csv", "w");
    if (fp) {
        fprintf(fp, "Dataset_Size,BTree_ms,Linear_ms,Speedup,Avg_Comparisons\n");

        for (int s = 0; s < (int)(sizeof sizes / sizeof sizes[0]); s++) {
            int n = sizes[s];
            BPlusTree  *tree    = bpt_create();
            FileEntry **lin_arr = malloc(n * sizeof(FileEntry*));
            for (int i = 0; i < n; i++) {
                char fname[256];
                snprintf(fname, sizeof fname, "file_%07d.dat", i);
                FileEntry *f = file_entry_create(fname, "/bench", (i+1)*512L);
                lin_arr[i]   = file_entry_create(fname, "/bench", (i+1)*512L);
                bpt_insert(tree, f);
            }
            int queries = 1000;
            long total_comp = 0;
            clock_t t0 = clock();
            for (int q = 0; q < queries; q++) {
                char fname[256];
                snprintf(fname, sizeof fname, "file_%07d.dat", rand()%n);
                long c = 0;
                bpt_search(tree, fname, &c);
                total_comp += c;
            }
            double bt_ms = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
            t0 = clock();
            for (int q = 0; q < queries; q++) {
                char fname[256];
                snprintf(fname, sizeof fname, "file_%07d.dat", rand()%n);
                linear_search(lin_arr, n, fname);
            }
            double lin_ms = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
            double sp = bt_ms > 0 ? lin_ms/bt_ms : 0;
            fprintf(fp, "%d,%.3f,%.3f,%.2f,%.2f\n",
                    n, bt_ms, lin_ms, sp, (double)total_comp/queries);
            bpt_destroy(tree);
            for (int i = 0; i < n; i++) file_entry_free(lin_arr[i]);
            free(lin_arr);
        }
        fclose(fp);
        printf("\n  [OK] Results saved to output/benchmark_results.csv\n");
    }
    printf("\n");
}
