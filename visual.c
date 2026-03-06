/*
 * ============================================================
 * FILE    : visual.c
 * PERSON  : Person 4
 * PURPOSE : B+ Tree Visualization
 *           - Option 15 : Terminal text view
 *           - Option 16 : DOT file  -> Graphviz PNG
 *           - Option 17 : HTML file -> open in browser
 *
 * HOW TO COMPILE WITH THE PROJECT:
 *   gcc -Wall -std=c11 -o fm file_btree_simple.c visual.c
 *
 * HOW TO USE GRAPHVIZ (Option 16):
 *   1. Download from https://graphviz.org/download/
 *   2. Install it
 *   3. Run: dot -Tpng btree.dot -o btree.png
 *   4. Open btree.png to see the tree image
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/* ── Same constants and structs as file_btree_simple.c ──────── */
/* Guard prevents redefinition when compiled together            */
#ifndef BTREE_STRUCTS_DEFINED
#define BTREE_STRUCTS_DEFINED

#define T        3
#define MAX_KEYS (2*T - 1)
#define MAX_CH   (2*T)

typedef struct {
    char   name[256];
    char   path[512];
    char   ext[16];
    long   size;
    time_t modified;
} File;

typedef struct Node {
    char         keys[MAX_KEYS][256];
    struct Node *child[MAX_CH];
    File        *data[MAX_KEYS];
    struct Node *next;
    int          n;
    bool         leaf;
} Node;

typedef struct {
    Node *root;
    int   count;
    long  total;
} Tree;

#endif /* BTREE_STRUCTS_DEFINED */

/* ============================================================
   OPTION 15 : TERMINAL TEXT VISUALIZATION
   Shows the tree level by level directly in the terminal
   ============================================================ */

void print_tree_terminal(Tree *t) {

    if (!t || !t->root || t->count == 0) {
        printf("  Tree is empty. Add some files first.\n");
        return;
    }

    printf("\n");
    printf("  ============================================================\n");
    printf("                    B+ TREE STRUCTURE\n");
    printf("  ============================================================\n");
    printf("  Min degree : %d\n", T);
    printf("  Max keys   : %d per node\n", MAX_KEYS);
    printf("  Total files: %d\n", t->count);
    printf("  ============================================================\n\n");

    /* BFS - level order traversal */
    Node *queue[50000];
    int   lvl[50000];
    int   head = 0, tail = 0;

    queue[tail] = t->root;
    lvl[tail]   = 0;
    tail++;

    int cur_lvl = -1;

    while (head < tail) {
        Node *nd = queue[head];
        int   lv = lvl[head];
        head++;

        /* print level heading when level changes */
        if (lv != cur_lvl) {
            if (cur_lvl >= 0) printf("\n\n");
            cur_lvl = lv;
            if (lv == 0)
                printf("  ROOT  (Level 0) :\n  ");
            else if (nd->leaf)
                printf("  LEAVES (Level %d) :\n  ", lv);
            else
                printf("  INTERNAL (Level %d) :\n  ", lv);
        }

        /* print the node as a box */
        if (nd->leaf)
            printf("[LEAF: ");
        else
            printf("[NODE: ");

        for (int i = 0; i < nd->n; i++) {
            if (nd->leaf)
                printf("%s", nd->data[i]->name);
            else
                printf("%s", nd->keys[i]);
            if (i < nd->n - 1) printf(" | ");
        }
        printf("]  ");

        /* add children to queue */
        if (!nd->leaf)
            for (int i = 0; i <= nd->n; i++)
                if (nd->child[i]) {
                    queue[tail] = nd->child[i];
                    lvl[tail]   = lv + 1;
                    tail++;
                }
    }

    /* print leaf linked list */
    printf("\n\n");
    printf("  ---- Leaf Chain (all files in sorted order) ----\n  ");
    Node *lf  = t->root;
    while (!lf->leaf) lf = lf->child[0];
    int count = 0;
    while (lf) {
        printf("[");
        for (int i = 0; i < lf->n; i++) {
            printf("%s", lf->data[i]->name);
            if (i < lf->n - 1) printf(", ");
        }
        printf("]");
        if (lf->next) printf(" --> ");
        lf = lf->next;
        count++;
        if (count % 3 == 0 && lf) printf("\n  ");
    }

    printf("\n\n");
    printf("  LEAF = stores actual file data\n");
    printf("  NODE = internal routing node\n");
    printf("  ============================================================\n\n");
}

/* ============================================================
   OPTION 16 : DOT FILE FOR GRAPHVIZ
   Saves btree.dot which Graphviz converts to a PNG image
   ============================================================ */

static int gv_id = 0;  /* global counter for unique node IDs */

static void dot_node(Node *nd, FILE *fp, int parent_id, int my_id) {
    if (!nd) return;

    /* write this node */
    if (nd->leaf) {
        /* leaf = green box, one key per line */
        fprintf(fp, "  n%d [label=\"", my_id);
        for (int i = 0; i < nd->n; i++) {
            fprintf(fp, "%s", nd->data[i]->name);
            if (i < nd->n - 1) fprintf(fp, "\\n");
        }
        fprintf(fp, "\" shape=box style=filled "
                    "fillcolor=\"#d4edda\" color=\"#28a745\" "
                    "fontsize=9 fontname=\"Arial\"];\n");
    } else {
        /* internal = blue record box */
        fprintf(fp, "  n%d [label=\"", my_id);
        for (int i = 0; i < nd->n; i++) {
            fprintf(fp, "%s", nd->keys[i]);
            if (i < nd->n - 1) fprintf(fp, " | ");
        }
        fprintf(fp, "\" shape=record style=filled "
                    "fillcolor=\"#cce5ff\" color=\"#004085\" "
                    "fontsize=9 fontname=\"Arial\"];\n");
    }

    /* edge from parent to this node */
    if (parent_id >= 0)
        fprintf(fp, "  n%d -> n%d;\n", parent_id, my_id);

    /* recurse into children */
    if (!nd->leaf)
        for (int i = 0; i <= nd->n; i++)
            if (nd->child[i]) {
                int cid = ++gv_id;
                dot_node(nd->child[i], fp, my_id, cid);
            }
}

void export_dot(Tree *t, const char *filename) {

    if (!t || !t->root || t->count == 0) {
        printf("  Tree is empty. Nothing to export.\n");
        return;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("  [ERROR] Cannot create file: %s\n", filename);
        return;
    }

    gv_id = 0;

    fprintf(fp, "/* B+ Tree - generated by visual.c */\n");
    fprintf(fp, "digraph BPlusTree {\n");
    fprintf(fp, "  rankdir=TB;\n");
    fprintf(fp, "  splines=ortho;\n");
    fprintf(fp, "  nodesep=0.5;\n");
    fprintf(fp, "  ranksep=0.8;\n");
    fprintf(fp, "  graph [label=\"B+ Tree File Index\\n"
                "Files: %d   Degree: %d\" "
                "fontsize=14 labelloc=t fontname=\"Arial\"];\n\n",
            t->count, T);

    dot_node(t->root, fp, -1, 0);

    fprintf(fp, "}\n");
    fclose(fp);

    printf("\n");
    printf("  [OK] DOT file saved : %s\n\n", filename);
    printf("  ---- How to generate PNG image ----\n");
    printf("  Step 1: Download Graphviz\n");
    printf("          https://graphviz.org/download/\n");
    printf("  Step 2: Install it\n");
    printf("  Step 3: Run this command:\n");
    printf("          dot -Tpng %s -o btree.png\n", filename);
    printf("  Step 4: Open btree.png to see the tree!\n\n");
    printf("  Other formats:\n");
    printf("          dot -Tsvg %s -o btree.svg\n", filename);
    printf("          dot -Tpdf %s -o btree.pdf\n\n", filename);
}

/* ============================================================
   OPTION 17 : HTML VISUALIZATION
   Saves btree.html - just double click to open in browser
   No extra software needed!
   ============================================================ */

void export_html(Tree *t, const char *filename) {

    if (!t || !t->root || t->count == 0) {
        printf("  Tree is empty. Nothing to export.\n");
        return;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("  [ERROR] Cannot create file: %s\n", filename);
        return;
    }

    /* collect all nodes with BFS for level display */
    Node *bfs[50000];
    int   bfs_lv[50000];
    int   bh = 0, bt = 0;
    bfs[bt]    = t->root;
    bfs_lv[bt] = 0;
    bt++;

    int max_lv = 0;
    while (bh < bt) {
        Node *nd = bfs[bh];
        int   lv = bfs_lv[bh];
        bh++;
        if (lv > max_lv) max_lv = lv;
        if (!nd->leaf)
            for (int i = 0; i <= nd->n; i++)
                if (nd->child[i]) {
                    bfs[bt]    = nd->child[i];
                    bfs_lv[bt] = lv + 1;
                    bt++;
                }
    }

    /* total size string */
    char tsz[32];
    if      (t->total >= 1024*1024*1024)
        snprintf(tsz, sizeof tsz, "%.1f GB", t->total/(1024.0*1024*1024));
    else if (t->total >= 1024*1024)
        snprintf(tsz, sizeof tsz, "%.1f MB", t->total/(1024.0*1024));
    else if (t->total >= 1024)
        snprintf(tsz, sizeof tsz, "%.1f KB", t->total/1024.0);
    else
        snprintf(tsz, sizeof tsz, "%ld B", t->total);

    /* ── HTML head + CSS ──────────────────────────────────────── */
    fprintf(fp,
"<!DOCTYPE html>\n"
"<html lang='en'>\n"
"<head>\n"
"<meta charset='UTF-8'>\n"
"<title>B+ Tree Visualization</title>\n"
"<style>\n"
"  *        { box-sizing:border-box; margin:0; padding:0; }\n"
"  body     { font-family:Arial,sans-serif; background:#f0f4f8; padding:20px; }\n"
"  h1       { text-align:center; color:#2c3e50; margin:20px 0 10px; }\n"
"  h2       { color:#34495e; margin:20px 0 10px; }\n"
"  .badges  { display:flex; justify-content:center; gap:15px; flex-wrap:wrap; margin:10px 0 30px; }\n"
"  .badge   { background:#3498db; color:#fff; padding:8px 20px; border-radius:20px; font-size:14px; }\n"
"  .section { background:#fff; border-radius:12px; padding:20px;\n"
"             box-shadow:0 2px 10px rgba(0,0,0,.08); margin:20px 0; overflow-x:auto; }\n"
"  /* tree levels */\n"
"  .lv-label{ text-align:center; font-size:12px; color:#888; margin:15px 0 5px; }\n"
"  .level   { display:flex; justify-content:center; gap:10px; flex-wrap:wrap; margin:5px 0; }\n"
"  .nd-int  { background:#cce5ff; border:2px solid #004085; border-radius:8px;\n"
"             padding:8px 12px; font-size:11px; text-align:center; min-width:80px; }\n"
"  .nd-leaf { background:#d4edda; border:2px solid #28a745; border-radius:8px;\n"
"             padding:8px 12px; font-size:11px; text-align:center; min-width:80px; }\n"
"  .key     { display:block; font-weight:bold; color:#2c3e50; padding:1px 0; }\n"
"  /* leaf chain */\n"
"  .chain   { display:flex; align-items:center; flex-wrap:wrap; gap:6px; }\n"
"  .ch-node { background:#d4edda; border:2px solid #28a745; border-radius:8px;\n"
"             padding:8px; font-size:11px; min-width:100px; text-align:center; }\n"
"  .arrow   { font-size:20px; color:#28a745; font-weight:bold; }\n"
"  /* file table */\n"
"  table    { width:100%%; border-collapse:collapse; }\n"
"  th       { background:#3498db; color:#fff; padding:10px; text-align:left; }\n"
"  td       { padding:8px 10px; border-bottom:1px solid #eee; font-size:13px; }\n"
"  tr:hover { background:#f8f9fa; }\n"
"  .ext     { background:#e9ecef; border-radius:4px; padding:2px 7px; font-size:11px; }\n"
"  footer   { text-align:center; color:#aaa; margin:30px 0; font-size:12px; }\n"
"  .legend  { display:flex; gap:20px; margin:10px 0; font-size:13px; }\n"
"  .leg-int { width:16px;height:16px;background:#cce5ff;border:2px solid #004085;display:inline-block;vertical-align:middle; }\n"
"  .leg-lf  { width:16px;height:16px;background:#d4edda;border:2px solid #28a745;display:inline-block;vertical-align:middle; }\n"
"</style>\n"
"</head>\n"
"<body>\n");

    /* ── header ───────────────────────────────────────────────── */
    fprintf(fp, "<h1>B+ Tree File Index Visualization</h1>\n");
    fprintf(fp, "<div class='badges'>\n");
    fprintf(fp, "  <span class='badge'>Total Files : %d</span>\n", t->count);
    fprintf(fp, "  <span class='badge'>Total Size  : %s</span>\n", tsz);
    fprintf(fp, "  <span class='badge'>Tree Height : %d</span>\n", max_lv + 1);
    fprintf(fp, "  <span class='badge'>Degree (t)  : %d</span>\n", T);
    fprintf(fp, "  <span class='badge'>Max Keys    : %d</span>\n", MAX_KEYS);
    fprintf(fp, "</div>\n");

    /* ── tree structure section ───────────────────────────────── */
    fprintf(fp, "<div class='section'>\n<h2>Tree Structure</h2>\n");
    fprintf(fp, "<div class='legend'>"
                "<span><span class='leg-int'></span> Internal node</span>"
                "<span><span class='leg-lf'></span> Leaf node</span>"
                "</div>\n");

    for (int lv = 0; lv <= max_lv; lv++) {
        const char *ltype = (lv == 0) ? "Root" :
                            (lv == max_lv) ? "Leaf Level" : "Internal";
        fprintf(fp, "<div class='lv-label'>Level %d &mdash; %s</div>\n", lv, ltype);
        fprintf(fp, "<div class='level'>\n");

        for (int i = 0; i < bt; i++) {
            if (bfs_lv[i] != lv) continue;
            Node *nd = bfs[i];
            fprintf(fp, "  <div class='%s'>\n", nd->leaf ? "nd-leaf" : "nd-int");
            for (int k = 0; k < nd->n; k++) {
                const char *kname = nd->leaf ? nd->data[k]->name : nd->keys[k];
                fprintf(fp, "    <span class='key'>%s</span>\n", kname);
            }
            fprintf(fp, "  </div>\n");
        }
        fprintf(fp, "</div>\n");
    }
    fprintf(fp, "</div>\n");

    /* ── leaf chain section ───────────────────────────────────── */
    fprintf(fp, "<div class='section'>\n<h2>Leaf Chain (All Files in Sorted Order)</h2>\n");
    fprintf(fp, "<div class='chain'>\n");

    Node *lf = t->root;
    while (!lf->leaf) lf = lf->child[0];
    bool first = true;
    while (lf) {
        if (!first) fprintf(fp, "<span class='arrow'>&#8594;</span>\n");
        first = false;
        fprintf(fp, "<div class='ch-node'>\n");
        for (int i = 0; i < lf->n; i++)
            fprintf(fp, "  <span class='key'>%s</span>\n", lf->data[i]->name);
        fprintf(fp, "</div>\n");
        lf = lf->next;
    }
    fprintf(fp, "</div>\n</div>\n");

    /* ── file table section ───────────────────────────────────── */
    fprintf(fp, "<div class='section'>\n<h2>All Indexed Files</h2>\n");
    fprintf(fp, "<table>\n");
    fprintf(fp, "<tr><th>#</th><th>Filename</th><th>Ext</th>"
                "<th>Size</th><th>Path</th><th>Modified</th></tr>\n");

    Node *lf2 = t->root;
    while (!lf2->leaf) lf2 = lf2->child[0];
    int row = 1;
    while (lf2) {
        for (int i = 0; i < lf2->n; i++) {
            File *f = lf2->data[i];

            char fsz[32];
            if      (f->size >= 1024*1024)
                snprintf(fsz, sizeof fsz, "%.1f MB", f->size/(1024.0*1024));
            else if (f->size >= 1024)
                snprintf(fsz, sizeof fsz, "%.1f KB", f->size/1024.0);
            else
                snprintf(fsz, sizeof fsz, "%ld B", f->size);

            char ts[20];
            strftime(ts, sizeof ts, "%Y-%m-%d", localtime(&f->modified));

            fprintf(fp,
                "<tr>"
                "<td>%d</td>"
                "<td><b>%s</b></td>"
                "<td><span class='ext'>%s</span></td>"
                "<td>%s</td>"
                "<td style='color:#888;font-size:11px'>%s</td>"
                "<td>%s</td>"
                "</tr>\n",
                row++, f->name,
                f->ext[0] ? f->ext : "-",
                fsz, f->path, ts);
        }
        lf2 = lf2->next;
    }
    fprintf(fp, "</table>\n</div>\n");

    /* ── footer ───────────────────────────────────────────────── */
    time_t now = time(NULL);
    char nts[32];
    strftime(nts, sizeof nts, "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(fp,
        "<footer>Generated by B+ Tree File Manager &mdash; %s</footer>\n"
        "</body>\n</html>\n", nts);

    fclose(fp);
    printf("\n  [OK] HTML saved : %s\n", filename);
    printf("  Just double-click the file to open it in your browser!\n\n");
}
