/*
 * ============================================================
 * FILE     : delete.c
 * CONTAINS : Full delete with borrow from sibling and merge
 * ============================================================
 */

#include "structs.h"

#define MAX_DEPTH 64

/* path entry - tracks how we descended the tree */
typedef struct {
    Node *nd;    /* the node at this level  */
    int   idx;   /* which child we went into */
} PathStep;

/* ── Remove one entry from a leaf at position pos ───────────── */
static void leaf_remove(Node *leaf, int pos) {
    free(leaf->data[pos]);
    for (int i = pos; i < leaf->n - 1; i++) {
        strncpy(leaf->keys[i], leaf->keys[i+1], 255);
        leaf->data[i] = leaf->data[i+1];
    }
    leaf->n--;
}

/* ── Recursive delete ───────────────────────────────────────── */
static bool delete_rec(Tree *t, Node *nd, const char *name,
                        PathStep path[], int depth) {
    if (nd->leaf) {

        /* find the file in this leaf */
        int pos = -1;
        for (int i = 0; i < nd->n; i++)
            if (strcmp(name, nd->data[i]->name) == 0) { pos = i; break; }
        if (pos < 0) return false;   /* not found */

        leaf_remove(nd, pos);

        /* check if underflow (too few keys) */
        int min = T - 1;
        if (nd == t->root || nd->n >= min) return true;

        Node *par = path[depth-1].nd;
        int   ci  = path[depth-1].idx;

        /* ── try borrow from RIGHT sibling ─────────────────── */
        if (ci < par->n) {
            Node *right = par->child[ci+1];
            if (right->n > min) {
                /* take first key from right */
                strncpy(nd->keys[nd->n], right->data[0]->name, 255);
                nd->data[nd->n++] = right->data[0];
                /* shift right sibling left */
                for (int i = 0; i < right->n - 1; i++) {
                    strncpy(right->keys[i], right->keys[i+1], 255);
                    right->data[i] = right->data[i+1];
                }
                right->n--;
                /* update separator in parent */
                strncpy(par->keys[ci], right->data[0]->name, 255);
                return true;
            }
        }

        /* ── try borrow from LEFT sibling ───────────────────── */
        if (ci > 0) {
            Node *left = par->child[ci-1];
            if (left->n > min) {
                /* shift current node right to make room */
                for (int i = nd->n; i > 0; i--) {
                    strncpy(nd->keys[i], nd->keys[i-1], 255);
                    nd->data[i] = nd->data[i-1];
                }
                /* take last key from left */
                strncpy(nd->keys[0], left->data[left->n-1]->name, 255);
                nd->data[0] = left->data[left->n-1];
                nd->n++;
                left->n--;
                /* update separator in parent */
                strncpy(par->keys[ci-1], nd->data[0]->name, 255);
                return true;
            }
        }

        /* ── merge with RIGHT sibling ───────────────────────── */
        if (ci < par->n) {
            Node *right = par->child[ci+1];
            /* copy all of right into nd */
            for (int i = 0; i < right->n; i++) {
                strncpy(nd->keys[nd->n+i], right->data[i]->name, 255);
                nd->data[nd->n+i] = right->data[i];
            }
            nd->n    += right->n;
            nd->next  = right->next;
            free(right);
            /* remove separator from parent */
            for (int i = ci; i < par->n - 1; i++) {
                strncpy(par->keys[i], par->keys[i+1], 255);
                par->child[i+1] = par->child[i+2];
            }
            par->n--;
            /* collapse root if empty */
            if (par == t->root && par->n == 0) {
                t->root = nd; free(par);
            }

        /* ── merge with LEFT sibling ────────────────────────── */
        } else if (ci > 0) {
            Node *left = par->child[ci-1];
            /* copy all of nd into left */
            for (int i = 0; i < nd->n; i++) {
                strncpy(left->keys[left->n+i], nd->data[i]->name, 255);
                left->data[left->n+i] = nd->data[i];
            }
            left->n  += nd->n;
            left->next = nd->next;
            free(nd);
            /* remove separator from parent */
            for (int i = ci-1; i < par->n - 1; i++) {
                strncpy(par->keys[i], par->keys[i+1], 255);
                par->child[i+1] = par->child[i+2];
            }
            par->n--;
            /* collapse root if empty */
            if (par == t->root && par->n == 0) {
                t->root = left; free(par);
            }
        }
        return true;
    }

    /* ── internal node: find correct child and descend ─────── */
    int i = 0;
    while (i < nd->n && strcmp(name, nd->keys[i]) >= 0) i++;
    path[depth].nd  = nd;
    path[depth].idx = i;
    return delete_rec(t, nd->child[i], name, path, depth + 1);
}

/* ── Public delete function ─────────────────────────────────── */
bool delete_file(Tree *t, const char *name) {
    File *f = search(t, name);
    if (!f) return false;

    long       sz   = f->size;
    PathStep   path[MAX_DEPTH] = {0};
    bool       ok   = delete_rec(t, t->root, name, path, 0);

    if (ok) {
        t->count--;
        t->total -= sz;
    }
    return ok;
}
