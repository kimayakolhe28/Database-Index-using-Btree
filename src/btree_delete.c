#include "../include/file_btree.h"

/*
 * B+ Tree deletion strategy used here:
 *
 *   We keep a parent stack while descending.  When we reach the leaf
 *   and remove the key we check for underflow (num_keys < MIN_DEGREE-1).
 *   If underflow occurs we:
 *     1. Try to borrow from the right sibling.
 *     2. Try to borrow from the left sibling.
 *     3. Merge with a sibling (prefer right).
 *   Parent separator keys are updated accordingly.
 *
 * For simplicity the root is allowed to have 0 keys (empty tree).
 */

#define MAX_PATH 64

typedef struct {
    BPlusNode *node;
    int        child_idx; /* index in node->children[] that we descended into */
} PathEntry;

/* --- Remove entry from leaf (assumes present) ----------------- */
static void leaf_remove(BPlusNode *leaf, int pos) {
    file_entry_free(leaf->files[pos]);
    for (int i = pos; i < leaf->num_keys - 1; i++) {
        strncpy(leaf->keys[i], leaf->keys[i+1], 255);
        leaf->files[i] = leaf->files[i+1];
    }
    leaf->num_keys--;
}

/* --- Main recursive delete ----------------------------------- */

static bool delete_rec(BPlusTree *tree,
                        BPlusNode *node,
                        const char *filename,
                        PathEntry  path[],
                        int        depth) {
    if (node->is_leaf) {
        /* find key */
        int pos = -1;
        for (int i = 0; i < node->num_keys; i++) {
            if (strcmp(filename, node->files[i]->filename) == 0) {
                pos = i; break;
            }
        }
        if (pos < 0) return false;   /* not found */

        leaf_remove(node, pos);

        /* check underflow */
        int min_keys = MIN_DEGREE - 1;
        if (node == tree->root) return true;  /* root leaf - ok */
        if (node->num_keys >= min_keys) return true;

        /* need to fix underflow - find sibling through parent */
        if (depth == 0) return true;
        BPlusNode *parent   = path[depth-1].node;
        int        cidx     = path[depth-1].child_idx;

        /* try right sibling */
        if (cidx < parent->num_keys) {
            BPlusNode *right = parent->children[cidx+1];
            if (right->num_keys > min_keys) {
                /* borrow first entry of right sibling */
                strncpy(node->keys[node->num_keys], right->files[0]->filename, 255);
                node->files[node->num_keys] = right->files[0];
                node->num_keys++;

                /* remove from right */
                for (int i = 0; i < right->num_keys-1; i++) {
                    strncpy(right->keys[i], right->keys[i+1], 255);
                    right->files[i] = right->files[i+1];
                }
                right->num_keys--;

                /* update separator in parent */
                strncpy(parent->keys[cidx], right->files[0]->filename, 255);
                return true;
            }
        }

        /* try left sibling */
        if (cidx > 0) {
            BPlusNode *left = parent->children[cidx-1];
            if (left->num_keys > min_keys) {
                /* borrow last entry of left sibling */
                for (int i = node->num_keys; i > 0; i--) {
                    strncpy(node->keys[i], node->keys[i-1], 255);
                    node->files[i] = node->files[i-1];
                }
                strncpy(node->keys[0], left->files[left->num_keys-1]->filename, 255);
                node->files[0] = left->files[left->num_keys-1];
                node->num_keys++;
                left->num_keys--;

                /* update separator in parent */
                strncpy(parent->keys[cidx-1], node->files[0]->filename, 255);
                return true;
            }
        }

        /* merge with right sibling */
        if (cidx < parent->num_keys) {
            BPlusNode *right = parent->children[cidx+1];
            /* copy right into node */
            for (int i = 0; i < right->num_keys; i++) {
                strncpy(node->keys[node->num_keys+i], right->files[i]->filename, 255);
                node->files[node->num_keys+i] = right->files[i];
            }
            node->num_keys += right->num_keys;
            node->next      = right->next;
            free(right);  /* don't free files - now owned by node */

            /* remove separator from parent */
            for (int i = cidx; i < parent->num_keys-1; i++) {
                strncpy(parent->keys[i], parent->keys[i+1], 255);
                parent->children[i+1] = parent->children[i+2];
            }
            parent->num_keys--;

            /* collapse root if empty */
            if (parent == tree->root && parent->num_keys == 0) {
                tree->root = node;
                free(parent);
            }
            return true;
        }

        /* merge with left sibling */
        if (cidx > 0) {
            BPlusNode *left = parent->children[cidx-1];
            for (int i = 0; i < node->num_keys; i++) {
                strncpy(left->keys[left->num_keys+i], node->files[i]->filename, 255);
                left->files[left->num_keys+i] = node->files[i];
            }
            left->num_keys += node->num_keys;
            left->next      = node->next;
            free(node);

            for (int i = cidx-1; i < parent->num_keys-1; i++) {
                strncpy(parent->keys[i], parent->keys[i+1], 255);
                parent->children[i+1] = parent->children[i+2];
            }
            parent->num_keys--;

            if (parent == tree->root && parent->num_keys == 0) {
                tree->root = left;
                free(parent);
            }
        }
        return true;
    }

    /* internal node - descend */
    int i = 0;
    while (i < node->num_keys && strcmp(filename, node->keys[i]) >= 0)
        i++;

    path[depth].node      = node;
    path[depth].child_idx = i;

    return delete_rec(tree, node->children[i], filename, path, depth+1);
}

/* --- Public delete -------------------------------------------- */

bool bpt_delete(BPlusTree *tree, const char *filename) {
    if (!tree || !filename) return false;

    /* find the file first to record its size */
    FileEntry *f = bpt_search(tree, filename, NULL);
    if (!f) return false;
    long fsize = f->size;

    PathEntry path[MAX_PATH];
    memset(path, 0, sizeof path);

    bool ok = delete_rec(tree, tree->root, filename, path, 0);
    if (ok) {
        tree->total_files--;
        tree->total_size -= fsize;
    }
    return ok;
}
