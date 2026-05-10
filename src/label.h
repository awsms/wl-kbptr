#ifndef __LABEL_H_INCLUDED__
#define __LABEL_H_INCLUDED__

#include <stdbool.h>

typedef struct {
    /*         data             data[num_symbols]
     *         |                |
     *  | 4 || 0 | 2 | 4 | 6 ||`a`| 0 |`b`| 0 |`c`| 0 |`d`| 0 |
     *    ^    ^-----------^    ^---------------------------^
     *    |       offsets               strings
     *    |
     *  number of symbols
     */
    unsigned char num_symbols;
    char          data[];
} label_symbols_t;

typedef struct {
    label_symbols_t *label_symbols;
    int              num_labels;
    int              max_len;
    int             *lens;
    unsigned char   *labels;
} label_layout_t;

typedef struct {
    label_symbols_t *label_symbols;
    int              num_labels;
    unsigned char    len;
    unsigned char    next;
    unsigned char    input[];
} label_selection_t;

// Create a `label_symbols_t` from a string of characters.
// Returns `NULL` upon error.
label_symbols_t *label_symbols_from_str(char *s);

// Free memory of a `label_symbols_t`.
void label_symbols_free(label_symbols_t *ls);

// Get pointer to string of the symbol at given index.
// Returns value <0 upon error.
char *label_symbols_idx_to_ptr(label_symbols_t *label_symbols, int idx);

// Find symbol index from given string.
// Returns value <0 upon error.
int label_symbols_find_idx(label_symbols_t *label_symbols, char *s);

// Create a label layout with the classic wl-kbptr labels.
label_layout_t *
label_layout_new(label_symbols_t *label_symbols, int num_labels);

// Create a label layout using top/bottom root hint pools. Empty pools fall back
// to the full label symbol set.
label_layout_t *label_layout_new_with_top_bottom(
    label_symbols_t *label_symbols, int num_labels, bool *is_top,
    char *top_hints, char *bottom_hints
);

// Returns the maximum number of symbols in any label.
int label_layout_max_len(label_layout_t *label_layout);

// Returns the maximum number of bytes needed to store a label string.
int label_layout_str_max_len(label_layout_t *label_layout);

// Returns true if label at `idx` starts with `start`.
bool label_layout_is_included(
    label_layout_t *label_layout, int idx, label_selection_t *start
);

// Returns true if any label starts with `start`.
bool label_layout_has_prefix(
    label_layout_t *label_layout, label_selection_t *start
);

// Returns associated label index when `selection` exactly matches a label.
int label_layout_to_idx(
    label_layout_t *label_layout, label_selection_t *selection
);

// Get label's string.
void label_layout_str(label_layout_t *label_layout, int idx, char *out);

// Get label string split at `cut`.
void label_layout_str_split(
    label_layout_t *label_layout, int idx, char *prefix, char *suffix, int cut
);

// Free memory of a `label_layout_t`.
void label_layout_free(label_layout_t *label_layout);

// Create a `label_selection_t`.
label_selection_t *
label_selection_new(label_symbols_t *label_symbols, int num_labels);

// Create a `label_selection_t` with an explicit maximum length.
label_selection_t *label_selection_new_with_len(
    label_symbols_t *label_symbols, int num_labels, int len
);

// Clear selection.
void label_selection_clear(label_selection_t *label_selection);

enum label_selection_append_ret {
    LABEL_SELECTION_APPEND_SUCCESS      = 0,
    LABEL_SELECTION_APPEND_IDX_OVERFLOW = 1,
    LABEL_SELECTION_APPEND_FULL         = 2
};
// Append to selection.
enum label_selection_append_ret
label_selection_append(label_selection_t *label_selection, int idx);

// Append to selection without applying numeric label overflow checks.
enum label_selection_append_ret
label_selection_append_raw(label_selection_t *label_selection, int idx);

// Erase last symbol.
// Returns true if it did else false.
bool label_selection_back(label_selection_t *label_selection);

// Returns true if `start` is the same as `reference` beginning else false.
bool label_selection_is_included(
    label_selection_t *reference, label_selection_t *start
);

// Returns associated label index.
int label_selection_to_idx(label_selection_t *label_selection);

// Set selection from associated index.
int label_selection_set_from_idx(label_selection_t *label_selection, int idx);

// Set to selection with incremented associated index.
int label_selection_incr(label_selection_t *label_selection);

// Get size of buffer needed to store label's string.
int label_selection_str_max_len(label_selection_t *label_selection);

// Get label's string.
void label_selection_str(label_selection_t *label_selection, char *out);

// Get label string split at `cut`.
void label_selection_str_split(
    label_selection_t *label_selection, char *prefix, char *suffix, int cut
);

// Free memory of `label_selection_t`.
void label_selection_free(label_selection_t *label_selection);

#endif
