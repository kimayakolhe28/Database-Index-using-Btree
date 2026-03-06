/*
 * ============================================================
 * FILE     : insert.c
 * CONTAINS : Tree/Node/File creation + full insert with
 *            top-down pre-splitting
 * ============================================================
 */

#include "structs.h"

/* ── Create a new empty tree ────────────────────────────────── */
Tree *new_tree(void) {
    Tree *t = calloc(1, sizeof(Tree));
    t->root  = new_node(true);   /* start with one empty leaf  */
    return t;
}

/* ── Create a new empty node ────────────────────────────────── */
Node *new_node(bool leaf) {
    Node *nd = calloc(1, sizeof(Node));
    nd->leaf = leaf;
    return nd;
}

/* ── Create a new file record ───────────────────────────────── */
File *new_file(const char *name, const char *path, long size) {
    File *f = calloc(1, sizeof(File));
    strncpy(f->name, name, 255);
    strncpy(f->path, path, 511);
    f->size     = size;
    f->modified = time(NULL);

    /* extract extension from filename */
    const char *dot = strrchr(name, '.');
    if (dot && dot != name)
        strncpy(f->ext, dot + 1, 15);

    return f;
}

/* ── Insert into a leaf that has room ───────────────────────── */
static void leaf_put(Node *leaf, File *f) {
    int pos = leaf->n;

    /* find correct sorted position */
    while (pos > 0 && strcmp(f->name, leaf->keys[pos-1]) < 0)
        pos--;

    /* if duplicate filename → just update the record */
    if (pos < leaf->n && strcmp(f->name, leaf->keys[pos]) == 0) {
        free(leaf->data[pos]);
        leaf->data[pos] = f;
        return;
    }

    /* shift existing keys right to make room */
    for (int i = leaf->n; i > pos; i--) {
        strncpy(leaf->keys[i], leaf->keys[i-1], 255);
        leaf->data[i] = leaf->data[i-1];
    }

    /* insert at correct position */
    strncpy(leaf->keys[pos], f->name, 255);
    leaf->data[pos] = f;
    leaf->n++;
}

/* ── Split a full leaf node ─────────────────────────────────── */
static void split_leaf(Tree *t, Node *parent, int idx, Node *leaf) {
    Node *right = new_node(true);

    int split  = T;
    right->n   = MAX_KEYS - split;

    /* move right half of keys to new node */
    for (int i = 0; i < right->n; i++) {
        strncpy(right->keys[i], leaf->keys[split + i], 255);
        right->data[i]        = leaf->data[split + i];
        leaf->data[split + i] = NULL;
    }
    leaf->n     = split;

    /* link leaves together */
    right->next = leaf->next;
    leaf->next  = right;

    if (!parent) {
        /* leaf was root → create new root */
        Node *nr = new_node(false);
        strncpy(nr->keys[0], right->keys[0], 255);
        nr->child[0] = leaf;
        nr->child[1] = right;
        nr->n        = 1;
        t->root      = nr;
    } else {
        /* push separator key up into parent */
        for (int i = parent->n; i > idx; i--) {
            strncpy(parent->keys[i], parent->keys[i-1], 255);
            parent->child[i+1] = parent->child[i];
        }
        strncpy(parent->keys[idx], right->keys[0], 255);
        parent->child[idx+1] = right;
        parent->n++;
    }
}

/* ── Split a full internal node ─────────────────────────────── */
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
        /* internal node was root → create new root */
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

/* ── Recursive insert with pre-splitting ────────────────────── */
static void insert_rec(Tree *t, Node *nd, File *f) {
    if (nd->leaf) {
        leaf_put(nd, f);
        return;
    }

    /* find which child to descend into */
    int i = 0;
    while (i < nd->n && strcmp(f->name, nd->keys[i]) >= 0) i++;

    Node *ch = nd->child[i];

    /* pre-split child if full before descending */
    if (ch->n == MAX_KEYS) {
        if (ch->leaf) split_leaf(t, nd, i, ch);
        else          split_internal(t, nd, i, ch);

        /* re-check which child after split */
        if (strcmp(f->name, nd->keys[i]) >= 0) i++;
        ch = nd->child[i];
    }

    insert_rec(t, ch, f);
}

/* ── Public insert function ─────────────────────────────────── */
void insert(Tree *t, File *f) {
    /* pre-split root if it is full */
    if (t->root->n == MAX_KEYS) {
        if (t->root->leaf) split_leaf(t, NULL, 0, t->root);
        else               split_internal(t, NULL, 0, t->root);
    }
    insert_rec(t, t->root, f);
    t->count++;
    t->total += f->size;
}
