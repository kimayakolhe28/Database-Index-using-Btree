#include "../include/file_btree.h"

/*
 * B+ Tree insertion – top-down pre-splitting approach.
 * Before descending into any child we split it if it is full, which
 * guarantees the parent always has room for a promoted key.
 */

/* ─── Insert into a leaf that is NOT full ─────────────────────── */
static void leaf_insert(BPlusNode *leaf, FileEntry *file) {
    int pos = leaf->num_keys;
    while (pos > 0 && strcmp(file->filename, leaf->keys[pos-1]) < 0)
        pos--;

    /* update duplicate */
    if (pos < leaf->num_keys &&
        strcmp(file->filename, leaf->keys[pos]) == 0) {
        file_entry_free(leaf->files[pos]);
        leaf->files[pos] = file;
        return;
    }

    for (int i = leaf->num_keys; i > pos; i--) {
        strncpy(leaf->keys[i], leaf->keys[i-1], 255);
        leaf->files[i] = leaf->files[i-1];
    }
    strncpy(leaf->keys[pos], file->filename, 255);
    leaf->files[pos] = file;
    leaf->num_keys++;
}

/* ─── Split a full leaf (parent must NOT be full) ─────────────── */
void bpt_split_leaf(BPlusTree *tree, BPlusNode *parent, int idx, BPlusNode *leaf) {
    int t = tree->degree;
    BPlusNode *right = bpt_node_create(true);

    int split = t;
    right->num_keys = MAX_KEYS - split;
    for (int i = 0; i < right->num_keys; i++) {
        strncpy(right->keys[i], leaf->keys[split + i], 255);
        right->files[i]        = leaf->files[split + i];
        leaf->files[split + i] = NULL;
    }
    leaf->num_keys = split;

    right->next = leaf->next;
    leaf->next  = right;

    if (parent == NULL) {
        BPlusNode *new_root = bpt_node_create(false);
        strncpy(new_root->keys[0], right->keys[0], 255);
        new_root->children[0] = leaf;
        new_root->children[1] = right;
        new_root->num_keys    = 1;
        tree->root = new_root;
    } else {
        for (int i = parent->num_keys; i > idx; i--) {
            strncpy(parent->keys[i], parent->keys[i-1], 255);
            parent->children[i+1] = parent->children[i];
        }
        strncpy(parent->keys[idx], right->keys[0], 255);
        parent->children[idx+1] = right;
        parent->num_keys++;
    }
}

/* ─── Split a full internal node (parent must NOT be full) ─────── */
void bpt_split_internal(BPlusTree *tree, BPlusNode *parent, int idx, BPlusNode *node) {
    int t = tree->degree;
    BPlusNode *right = bpt_node_create(false);

    char mid[256];
    strncpy(mid, node->keys[t-1], 255);

    right->num_keys = MAX_KEYS - t;
    for (int i = 0; i < right->num_keys; i++) {
        strncpy(right->keys[i], node->keys[t + i], 255);
        right->children[i] = node->children[t + i];
    }
    right->children[right->num_keys] = node->children[MAX_KEYS];
    node->num_keys = t - 1;

    if (parent == NULL) {
        BPlusNode *new_root = bpt_node_create(false);
        strncpy(new_root->keys[0], mid, 255);
        new_root->children[0] = node;
        new_root->children[1] = right;
        new_root->num_keys    = 1;
        tree->root = new_root;
    } else {
        for (int i = parent->num_keys; i > idx; i--) {
            strncpy(parent->keys[i], parent->keys[i-1], 255);
            parent->children[i+1] = parent->children[i];
        }
        strncpy(parent->keys[idx], mid, 255);
        parent->children[idx+1] = right;
        parent->num_keys++;
    }
}

/* ─── Recursive descent with pre-splitting ───────────────────── */
static void insert_into_subtree(BPlusTree *tree, BPlusNode *node, FileEntry *file) {
    if (node->is_leaf) {
        leaf_insert(node, file);
        return;
    }

    int i = 0;
    while (i < node->num_keys && strcmp(file->filename, node->keys[i]) >= 0)
        i++;

    BPlusNode *child = node->children[i];

    if (child->num_keys == MAX_KEYS) {
        if (child->is_leaf)
            bpt_split_leaf(tree, node, i, child);
        else
            bpt_split_internal(tree, node, i, child);

        /* re-select child after split */
        if (i < node->num_keys && strcmp(file->filename, node->keys[i]) >= 0)
            i++;
        child = node->children[i];
    }

    insert_into_subtree(tree, child, file);
}

/* ─── Public insert ──────────────────────────────────────────── */
bool bpt_insert(BPlusTree *tree, FileEntry *file) {
    if (!tree || !file) return false;

    if (tree->root->num_keys == MAX_KEYS) {
        if (tree->root->is_leaf)
            bpt_split_leaf(tree, NULL, 0, tree->root);
        else
            bpt_split_internal(tree, NULL, 0, tree->root);
    }

    insert_into_subtree(tree, tree->root, file);
    tree->total_files++;
    tree->total_size += file->size;
    return true;
}
