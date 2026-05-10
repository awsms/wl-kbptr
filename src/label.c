#include "label.h"

#include "log.h"
#include "utils.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

label_symbols_t *label_symbols_from_str(char *s) {
    char *c = s;

    uint32_t r;
    int      c_len;

    int len         = sizeof(label_symbols_t);
    int num_symbols = 0;

    while ((c_len = str_to_rune(c, &r)) > 0) {
        // One byte for the indices and one of the end of string (`\0`).
        c           += c_len;
        len         += c_len + 2;
        num_symbols += 1;
    }

    if (c_len < 0) {
        LOG_ERR("Invalid UTF-8 input.");
        return NULL;
    }

    if (num_symbols < 2) {
        LOG_ERR(
            "Not enough characters (%d). Must have at least 2.", num_symbols
        );
        return NULL;
    }

    if (num_symbols >= 255) {
        LOG_ERR("Too many characters (%d).", num_symbols);
        return NULL;
    }

    label_symbols_t *label_symbols = malloc(len);

    label_symbols->num_symbols = num_symbols;
    unsigned char *indices     = (unsigned char *)label_symbols->data;
    char          *str         = &label_symbols->data[num_symbols];

    c              = s;
    int str_offset = 0;
    for (int i = 0; i < num_symbols; i++) {
        c_len      = str_to_rune(c, &r);
        indices[i] = str_offset;
        memcpy(str + str_offset, c, c_len);
        str[str_offset + c_len] = '\0';

        str_offset += c_len + 1;
        c          += c_len;
    }

    return label_symbols;
}

void label_symbols_free(label_symbols_t *ls) {
    free(ls);
}

char *label_symbols_idx_to_ptr(label_symbols_t *label_symbols, int idx) {
    if (idx < 0 || idx >= label_symbols->num_symbols) {
        LOG_ERR("Label symbols index (%d) out of bound.", idx);
        return NULL;
    }

    return ((char *)label_symbols->data) + label_symbols->num_symbols +
           ((unsigned char *)label_symbols->data)[idx];
}

int label_symbols_find_idx(label_symbols_t *label_symbols, char *s) {
    for (int i = 0; i < label_symbols->num_symbols; i++) {
        if (strcmp(label_symbols_idx_to_ptr(label_symbols, i), s) == 0) {
            return i;
        }
    }

    return -1;
}

static int label_symbols_max_str_len(label_symbols_t *label_symbols);

static unsigned char *
label_layout_label_ptr(label_layout_t *label_layout, int idx) {
    return &label_layout->labels[idx * label_layout->max_len];
}

static int label_selection_len_for_labels(
    label_symbols_t *label_symbols, int num_labels
) {
    int len = 0;
    while (num_labels > 0) {
        len++;
        num_labels /= label_symbols->num_symbols;
    }

    return len;
}

label_layout_t *
label_layout_new(label_symbols_t *label_symbols, int num_labels) {
    label_layout_t *l = malloc(sizeof(*l));

    l->label_symbols = label_symbols;
    l->num_labels    = num_labels;
    l->max_len       = label_selection_len_for_labels(label_symbols, num_labels);
    l->lens          = malloc(sizeof(*l->lens) * num_labels);
    l->labels =
        calloc(max(1, num_labels * l->max_len), sizeof(*l->labels));

    const int num_symbols = label_symbols->num_symbols;
    for (int idx = 0; idx < num_labels; idx++) {
        int            remaining = idx;
        unsigned char *label     = label_layout_label_ptr(l, idx);

        l->lens[idx] = l->max_len;
        for (int pos = 0; pos < l->max_len; pos++) {
            label[pos]  = remaining % num_symbols;
            remaining  /= num_symbols;
        }
    }

    return l;
}

struct label_pool {
    unsigned char *symbols;
    int            len;
};

struct generated_hint {
    unsigned char *symbols;
    int            len;
};

struct generated_hints {
    struct generated_hint *hints;
    int                    count;
};

struct label_pool_group {
    struct label_pool      pool;
    int                    count;
    int                    next;
    struct generated_hints generated;
};

static void label_pool_free(struct label_pool *pool) {
    free(pool->symbols);
    pool->symbols = NULL;
    pool->len     = 0;
}

static bool label_pool_set_default(
    struct label_pool *pool, label_symbols_t *label_symbols
) {
    pool->len     = label_symbols->num_symbols;
    pool->symbols = malloc(pool->len);
    for (int i = 0; i < pool->len; i++) {
        pool->symbols[i] = i;
    }

    return true;
}

static bool label_pool_contains(struct label_pool *pool, int symbol) {
    for (int i = 0; i < pool->len; i++) {
        if (pool->symbols[i] == symbol) {
            return true;
        }
    }

    return false;
}

static bool label_pool_from_str(
    struct label_pool *pool, label_symbols_t *label_symbols, char *s
) {
    pool->symbols = malloc(label_symbols->num_symbols);
    pool->len     = 0;

    char    *c = s;
    uint32_t r;
    int      c_len;

    while ((c_len = str_to_rune(c, &r)) > 0) {
        char symbol[c_len + 1];
        memcpy(symbol, c, c_len);
        symbol[c_len] = '\0';

        const int symbol_idx = label_symbols_find_idx(label_symbols, symbol);
        if (symbol_idx < 0) {
            LOG_ERR("Hint pool symbol '%s' is not in label_symbols.", symbol);
            label_pool_free(pool);
            return false;
        }

        if (!label_pool_contains(pool, symbol_idx)) {
            pool->symbols[pool->len++] = symbol_idx;
        }

        c += c_len;
    }

    if (c_len < 0) {
        LOG_ERR("Invalid UTF-8 input.");
        label_pool_free(pool);
        return false;
    }

    if (pool->len == 0) {
        label_pool_free(pool);
        return label_pool_set_default(pool, label_symbols);
    }

    return true;
}

static bool label_pool_clone(
    struct label_pool *dest, const struct label_pool *src
) {
    dest->len     = src->len;
    dest->symbols = malloc(src->len);
    memcpy(dest->symbols, src->symbols, src->len);
    return true;
}

static bool label_pool_eq(
    const struct label_pool *a, const struct label_pool *b
) {
    if (a->len != b->len) {
        return false;
    }

    for (int i = 0; i < a->len; i++) {
        if (a->symbols[i] != b->symbols[i]) {
            return false;
        }
    }

    return true;
}

static void generated_hint_free(struct generated_hint *hint) {
    free(hint->symbols);
    hint->symbols = NULL;
    hint->len     = 0;
}

static void generated_hints_free(struct generated_hints *hints) {
    for (int i = 0; i < hints->count; i++) {
        generated_hint_free(&hints->hints[i]);
    }
    free(hints->hints);
    hints->hints = NULL;
    hints->count = 0;
}

static struct generated_hint generated_hint_from_symbol(int symbol) {
    struct generated_hint hint;
    hint.len        = 1;
    hint.symbols    = malloc(1);
    hint.symbols[0] = symbol;
    return hint;
}

static struct generated_hint
generated_hint_child(struct generated_hint *parent, int symbol) {
    struct generated_hint hint;
    hint.len     = parent->len + 1;
    hint.symbols = malloc(hint.len);
    memcpy(hint.symbols, parent->symbols, parent->len);
    hint.symbols[parent->len] = symbol;
    return hint;
}

static void generated_hint_queue_push(
    struct generated_hint **queue, int *tail, int *cap,
    struct generated_hint hint
) {
    if (*tail >= *cap) {
        *cap  *= 2;
        *queue = realloc(*queue, sizeof(**queue) * *cap);
    }

    (*queue)[(*tail)++] = hint;
}

static struct generated_hints generate_prefix_free_hints(
    int count, const struct label_pool *pool, int num_symbols
) {
    struct generated_hints out = {
        .hints = calloc(max(1, count), sizeof(*out.hints)),
        .count = count,
    };

    if (count == 0) {
        return out;
    }

    int                    cap   = max(count + num_symbols, pool->len + 1);
    int                    head  = 0;
    int                    tail  = 0;
    struct generated_hint *queue = malloc(sizeof(*queue) * cap);

    for (int i = 0; i < pool->len; i++) {
        generated_hint_queue_push(
            &queue, &tail, &cap, generated_hint_from_symbol(pool->symbols[i])
        );
    }

    while (tail - head < count) {
        struct generated_hint to_expand = queue[head++];
        for (int i = 0; i < num_symbols; i++) {
            generated_hint_queue_push(
                &queue, &tail, &cap, generated_hint_child(&to_expand, i)
            );
        }
        generated_hint_free(&to_expand);
    }

    for (int i = 0; i < count; i++) {
        out.hints[i] = queue[head + i];
    }

    for (int i = head + count; i < tail; i++) {
        generated_hint_free(&queue[i]);
    }

    free(queue);
    return out;
}

static void label_pool_group_free(struct label_pool_group *group) {
    label_pool_free(&group->pool);
    generated_hints_free(&group->generated);
}

label_layout_t *label_layout_new_with_top_bottom(
    label_symbols_t *label_symbols, int num_labels, bool *is_top,
    char *top_hints, char *bottom_hints
) {
    const bool has_top_hints    = top_hints != NULL && top_hints[0] != '\0';
    const bool has_bottom_hints =
        bottom_hints != NULL && bottom_hints[0] != '\0';

    if (!has_top_hints && !has_bottom_hints) {
        return label_layout_new(label_symbols, num_labels);
    }

    struct label_pool default_pool, top_pool, bottom_pool;
    if (!label_pool_set_default(&default_pool, label_symbols)) {
        return NULL;
    }
    if (has_top_hints) {
        if (!label_pool_from_str(&top_pool, label_symbols, top_hints)) {
            label_pool_free(&default_pool);
            return NULL;
        }
    } else {
        label_pool_clone(&top_pool, &default_pool);
    }
    if (has_bottom_hints) {
        if (!label_pool_from_str(&bottom_pool, label_symbols, bottom_hints)) {
            label_pool_free(&default_pool);
            label_pool_free(&top_pool);
            return NULL;
        }
    } else {
        label_pool_clone(&bottom_pool, &default_pool);
    }

    struct label_pool_group groups[3] = {0};
    int                     num_groups = 0;
    int                    *label_group =
        malloc(sizeof(*label_group) * max(1, num_labels));

    for (int i = 0; i < num_labels; i++) {
        struct label_pool *pool = is_top[i] ? &top_pool : &bottom_pool;

        int group_idx = -1;
        for (int j = 0; j < num_groups; j++) {
            if (label_pool_eq(&groups[j].pool, pool)) {
                group_idx = j;
                break;
            }
        }

        if (group_idx < 0) {
            group_idx = num_groups++;
            label_pool_clone(&groups[group_idx].pool, pool);
        }

        groups[group_idx].count++;
        label_group[i] = group_idx;
    }

    int max_len = 0;
    for (int i = 0; i < num_groups; i++) {
        groups[i].generated = generate_prefix_free_hints(
            groups[i].count, &groups[i].pool, label_symbols->num_symbols
        );

        for (int j = 0; j < groups[i].generated.count; j++) {
            if (groups[i].generated.hints[j].len > max_len) {
                max_len = groups[i].generated.hints[j].len;
            }
        }
    }

    label_layout_t *layout = malloc(sizeof(*layout));
    layout->label_symbols  = label_symbols;
    layout->num_labels     = num_labels;
    layout->max_len        = max_len;
    layout->lens           = malloc(sizeof(*layout->lens) * num_labels);
    layout->labels =
        calloc(max(1, num_labels * max_len), sizeof(*layout->labels));

    for (int i = 0; i < num_labels; i++) {
        struct label_pool_group *group = &groups[label_group[i]];
        struct generated_hint   *hint  = &group->generated.hints[group->next++];
        unsigned char           *label = label_layout_label_ptr(layout, i);

        layout->lens[i] = hint->len;
        memcpy(label, hint->symbols, hint->len);
    }

    for (int i = 0; i < num_groups; i++) {
        label_pool_group_free(&groups[i]);
    }
    label_pool_free(&default_pool);
    label_pool_free(&top_pool);
    label_pool_free(&bottom_pool);
    free(label_group);

    return layout;
}

int label_layout_max_len(label_layout_t *label_layout) {
    return label_layout->max_len;
}

int label_layout_str_max_len(label_layout_t *label_layout) {
    return label_symbols_max_str_len(label_layout->label_symbols) *
           label_layout->max_len;
}

bool label_layout_is_included(
    label_layout_t *label_layout, int idx, label_selection_t *start
) {
    if (idx < 0 || idx >= label_layout->num_labels ||
        label_layout->label_symbols != start->label_symbols ||
        label_layout->lens[idx] < start->next) {
        return false;
    }

    unsigned char *label = label_layout_label_ptr(label_layout, idx);
    for (int i = 0; i < start->next; i++) {
        if (label[i] != start->input[i]) {
            return false;
        }
    }

    return true;
}

bool label_layout_has_prefix(
    label_layout_t *label_layout, label_selection_t *start
) {
    for (int i = 0; i < label_layout->num_labels; i++) {
        if (label_layout_is_included(label_layout, i, start)) {
            return true;
        }
    }

    return false;
}

int label_layout_to_idx(
    label_layout_t *label_layout, label_selection_t *selection
) {
    for (int i = 0; i < label_layout->num_labels; i++) {
        if (label_layout->lens[i] == selection->next &&
            label_layout_is_included(label_layout, i, selection)) {
            return i;
        }
    }

    return -1;
}

void label_layout_str(label_layout_t *label_layout, int idx, char *out) {
    label_symbols_t *label_symbols = label_layout->label_symbols;
    unsigned char   *label         = label_layout_label_ptr(label_layout, idx);
    for (int i = 0; i < label_layout->lens[idx]; i++) {
        out = stpcpy(out, label_symbols_idx_to_ptr(label_symbols, label[i]));
    }

    *out = '\0';
}

void label_layout_str_split(
    label_layout_t *label_layout, int idx, char *prefix, char *suffix, int cut
) {
    if (cut < 0) {
        cut = 0;
    } else if (cut >= label_layout->lens[idx]) {
        cut = label_layout->lens[idx];
    }

    label_symbols_t *label_symbols = label_layout->label_symbols;
    unsigned char   *label         = label_layout_label_ptr(label_layout, idx);
    for (int i = 0; i < cut; i++) {
        prefix =
            stpcpy(prefix, label_symbols_idx_to_ptr(label_symbols, label[i]));
    }
    *prefix = '\0';

    for (int i = cut; i < label_layout->lens[idx]; i++) {
        suffix =
            stpcpy(suffix, label_symbols_idx_to_ptr(label_symbols, label[i]));
    }
    *suffix = '\0';
}

void label_layout_free(label_layout_t *label_layout) {
    if (label_layout == NULL) {
        return;
    }

    free(label_layout->lens);
    free(label_layout->labels);
    free(label_layout);
}

label_selection_t *
label_selection_new(label_symbols_t *label_symbols, int num_labels) {
    const int len = label_selection_len_for_labels(label_symbols, num_labels);
    return label_selection_new_with_len(label_symbols, num_labels, len);
}

label_selection_t *label_selection_new_with_len(
    label_symbols_t *label_symbols, int num_labels, int len
) {
    label_selection_t *l = malloc(sizeof(*l) + max(1, len));

    l->num_labels    = num_labels;
    l->len           = len;
    l->next          = 0;
    l->label_symbols = label_symbols;
    return l;
}

void label_selection_clear(label_selection_t *label_selection) {
    label_selection->next = 0;
}

static int label_selection_to_partial_idx(label_selection_t *label_selection) {
    int idx         = 0;
    int factor      = 1;
    int num_symbols = label_selection->label_symbols->num_symbols;

    for (int i = 0; i < label_selection->next; i++) {
        idx    += label_selection->input[i] * factor;
        factor *= num_symbols;
    }

    return idx;
}

enum label_selection_append_ret
label_selection_append(label_selection_t *label_selection, int idx) {
    if (label_selection->next >= label_selection->len) {
        return LABEL_SELECTION_APPEND_FULL;
    }

    label_selection->input[label_selection->next++] = idx;

    if (label_selection_to_partial_idx(label_selection) >=
        label_selection->num_labels) {
        label_selection->next--;
        return LABEL_SELECTION_APPEND_IDX_OVERFLOW;
    }

    return LABEL_SELECTION_APPEND_SUCCESS;
}

enum label_selection_append_ret
label_selection_append_raw(label_selection_t *label_selection, int idx) {
    if (label_selection->next >= label_selection->len) {
        return LABEL_SELECTION_APPEND_FULL;
    }

    label_selection->input[label_selection->next++] = idx;
    return LABEL_SELECTION_APPEND_SUCCESS;
}

bool label_selection_back(label_selection_t *label_selection) {
    if (label_selection->next == 0) {
        return false;
    }

    label_selection->next--;
    return true;
}

bool label_selection_is_included(
    label_selection_t *reference, label_selection_t *start
) {
    if (reference->label_symbols != start->label_symbols ||
        reference->len != start->len || reference->next < start->next) {
        return false;
    }

    for (int i = 0; i < start->next; i++) {
        if (reference->input[i] != start->input[i]) {
            return false;
        }
    }

    return true;
}

int label_selection_to_idx(label_selection_t *label_selection) {
    if (label_selection->next != label_selection->len) {
        return -1;
    }

    return label_selection_to_partial_idx(label_selection);
}

int label_selection_set_from_idx(label_selection_t *label_selection, int idx) {
    int num_symbols = label_selection->label_symbols->num_symbols;

    for (label_selection->next = 0;
         label_selection->next < label_selection->len;
         label_selection->next++) {
        label_selection->input[label_selection->next]  = idx % num_symbols;
        idx                                           /= num_symbols;
    }

    return idx == 0;
}

int label_selection_incr(label_selection_t *label_selection) {
    int num_symbols = label_selection->label_symbols->num_symbols;

    for (int i = 0; i < label_selection->len; i++) {
        label_selection->input[i] += 1;

        if (label_selection->input[i] < num_symbols) {
            return 1;
        }

        label_selection->input[i] %= num_symbols;
    }

    return 0;
}

static int label_symbols_max_str_len(label_symbols_t *label_symbols) {
    unsigned char *indices = (unsigned char *)label_symbols->data;
    int            i;

    int max_len  = 0;
    int curr_len = 0;

    for (i = 1; i < label_symbols->num_symbols; i++) {
        curr_len = indices[i] - indices[i - 1] - 1;
        if (curr_len > max_len) {
            max_len = curr_len;
        }
    }

    curr_len = strlen(
        label_symbols_idx_to_ptr(label_symbols, label_symbols->num_symbols - 1)
    );
    if (curr_len > max_len) {
        max_len = curr_len;
    }

    return max_len;
}

int label_selection_str_max_len(label_selection_t *label_selection) {
    return label_symbols_max_str_len(label_selection->label_symbols) *
           label_selection->len;
}

void label_selection_str(label_selection_t *label_selection, char *out) {
    label_symbols_t *label_symbols = label_selection->label_symbols;
    for (int i = 0; i < label_selection->next; i++) {
        out = stpcpy(
            out,
            label_symbols_idx_to_ptr(label_symbols, label_selection->input[i])
        );
    }

    *out = '\0';
}

void label_selection_str_split(
    label_selection_t *label_selection, char *prefix, char *suffix, int cut
) {
    if (cut < 0) {
        cut = 0;
    } else if (cut >= label_selection->next) {
        cut = label_selection->next;
    }

    label_symbols_t *label_symbols = label_selection->label_symbols;
    for (int i = 0; i < cut; i++) {
        prefix = stpcpy(
            prefix,
            label_symbols_idx_to_ptr(label_symbols, label_selection->input[i])
        );
    }
    *prefix = '\0';

    for (int i = cut; i < label_selection->next; i++) {
        suffix = stpcpy(
            suffix,
            label_symbols_idx_to_ptr(label_symbols, label_selection->input[i])
        );
    }
    *suffix = '\0';
}

void label_selection_free(label_selection_t *label_selection) {
    free(label_selection);
}
