#include "log.h"
#include "src/label.h"

#include <string.h>

int main() {
    label_symbols_t *label_symbols = label_symbols_from_str("abcdé");
    if (!label_symbols) {
        LOG_ERR("`label_symbolss_from_str` should not have returned null.");
        return 1;
    }

    if (label_symbols->num_symbols != 5) {
        LOG_ERR("Expected 5 symbols, got %d.", label_symbols->num_symbols);
        return 2;
    }

    char *s = label_symbols_idx_to_ptr(label_symbols, 0);
    if (strcmp(s, "a")) {
        LOG_ERR("No match");
        LOG_ERR("Given string: '%s'", s);
        return 3;
    }

    char *symbols[] = {
        "a", "b", "c", "d", "é",
    };

    for (int i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
        int symbol_idx = label_symbols_find_idx(label_symbols, symbols[i]);
        if (symbol_idx != i) {
            LOG_ERR(
                "Wrong index %d (expected %d) for symbol '%s'", symbol_idx, i,
                symbols[i]
            );
            return 4;
        }
    }

    label_selection_t *label_selection =
        label_selection_new(label_symbols, 100);
    int label_selection_str_buf_size =
        label_selection_str_max_len(label_selection) + 1;
    if (label_selection_str_buf_size != 7) {
        LOG_ERR(
            "Wrong label_selection_str_buffer_size = %d",
            label_selection_str_buf_size
        );

        return 5;
    }

    char label_selection_str_buf[label_selection_str_buf_size];
    int  idx;

    for (int i = 0; i < 100; i++) {
        label_selection_set_from_idx(label_selection, i);
        idx = label_selection_to_idx(label_selection);
        if (idx != i) {
            LOG_ERR("Wrong index %d, expected %d.", idx, i);
            label_selection_str(label_selection, label_selection_str_buf);
            LOG_ERR("Selection: '%s'", label_selection_str_buf);
            return 6;
        }
    }

    label_selection_clear(label_selection);
    label_selection_append(label_selection, 4);
    label_selection_append(label_selection, 2);

    label_selection_str(label_selection, label_selection_str_buf);
    if (strcmp(label_selection_str_buf, "éc")) {
        LOG_ERR("Wrong selection string '%s'", label_selection_str_buf);
        return 7;
    }

    label_selection_append(label_selection, 0);
    if ((idx = label_selection_to_idx(label_selection)) != 14) {
        LOG_ERR("Wrong index %d", idx);
        return 8;
    }

    if (!label_selection_incr(label_selection)) {
        LOG_ERR("We should not be overflowing here.");
        return 9;
    }

    if ((idx = label_selection_to_idx(label_selection)) != 15) {
        LOG_ERR("Wrong index %d", idx);
        return 10;
    }

    char label_selection_str_buf_1[label_selection_str_buf_size];

    label_selection_str_split(
        label_selection, label_selection_str_buf, label_selection_str_buf_1, 1
    );
    if (strcmp(label_selection_str_buf, "a")) {
        LOG_ERR("Wrong prefix '%s'", label_selection_str_buf);
        return 11;
    }
    if (strcmp(label_selection_str_buf_1, "da")) {
        LOG_ERR("Wrong suffix '%s'", label_selection_str_buf_1);
        return 12;
    }

    // Tests with the unicode character not at end-of-string

    label_symbols_t *alt_label_symbols = label_symbols_from_str("abcdéfghi");
    label_selection_t *alt_selection =
        label_selection_new(alt_label_symbols, 100);
    int alt_selection_str_buf_size =
        label_selection_str_max_len(alt_selection) + 1;
    if (alt_selection_str_buf_size != 7) {
        LOG_ERR(
            "Wrong label_selection_str_buffer_size = %d",
            alt_selection_str_buf_size
        );

        return 13;
    }

    char *alt_symbols[] = {
        "a", "b", "c", "d", "é", "f", "g", "h", "i",
    };

    for (int i = 0; i < 9; i++) {
        int symbol_idx =
            label_symbols_find_idx(alt_label_symbols, alt_symbols[i]);
        if (symbol_idx != i) {
            LOG_ERR(
                "Wrong index %d (expected %d) for symbol '%s'", symbol_idx, i,
                symbols[i]
            );
            return 14;
        }
    }

    label_layout_t *legacy_layout = label_layout_new(label_symbols, 100);
    int             legacy_buf_size =
        label_layout_str_max_len(legacy_layout) + 1;
    char legacy_buf[legacy_buf_size];

    for (int i = 0; i < 100; i++) {
        label_layout_str(legacy_layout, i, legacy_buf);
        label_selection_set_from_idx(label_selection, i);
        label_selection_str(label_selection, label_selection_str_buf);
        if (strcmp(legacy_buf, label_selection_str_buf)) {
            LOG_ERR(
                "Legacy layout label mismatch for %d: '%s' != '%s'", i,
                legacy_buf, label_selection_str_buf
            );
            return 15;
        }
    }

    label_symbols_t *region_symbols = label_symbols_from_str("abc");
    bool             is_top[]       = {true, true, false};
    label_layout_t  *region_layout  = label_layout_new_with_top_bottom(
        region_symbols, 3, is_top, "a", "b"
    );
    int  region_buf_size = label_layout_str_max_len(region_layout) + 1;
    char region_buf[region_buf_size];

    label_layout_str(region_layout, 0, region_buf);
    if (strcmp(region_buf, "aa")) {
        LOG_ERR("Wrong top region label 0 '%s'", region_buf);
        return 16;
    }

    label_layout_str(region_layout, 1, region_buf);
    if (strcmp(region_buf, "ab")) {
        LOG_ERR("Wrong top region label 1 '%s'", region_buf);
        return 17;
    }

    label_layout_str(region_layout, 2, region_buf);
    if (strcmp(region_buf, "b")) {
        LOG_ERR("Wrong bottom region label '%s'", region_buf);
        return 18;
    }

    label_selection_t *region_selection = label_selection_new_with_len(
        region_symbols, 3, label_layout_max_len(region_layout)
    );
    label_selection_append_raw(
        region_selection, label_symbols_find_idx(region_symbols, "a")
    );
    if (!label_layout_has_prefix(region_layout, region_selection)) {
        LOG_ERR("Expected region layout to have prefix 'a'.");
        return 19;
    }
    if (label_layout_to_idx(region_layout, region_selection) >= 0) {
        LOG_ERR("Prefix 'a' should not exactly match a top hint.");
        return 20;
    }

    char region_prefix[region_buf_size];
    label_layout_str_split(
        region_layout, 1, region_prefix, region_buf, region_selection->next
    );
    if (strcmp(region_prefix, "a") || strcmp(region_buf, "b")) {
        LOG_ERR(
            "Wrong split for region label: prefix='%s' suffix='%s'",
            region_prefix, region_buf
        );
        return 21;
    }

    label_selection_append_raw(
        region_selection, label_symbols_find_idx(region_symbols, "b")
    );
    if (label_layout_to_idx(region_layout, region_selection) != 1) {
        LOG_ERR("Expected 'ab' to select label 1.");
        return 22;
    }

    label_selection_clear(region_selection);
    label_selection_append_raw(
        region_selection, label_symbols_find_idx(region_symbols, "b")
    );
    if (label_layout_to_idx(region_layout, region_selection) != 2) {
        LOG_ERR("Expected 'b' to select bottom label.");
        return 23;
    }

    label_selection_free(region_selection);
    label_layout_free(region_layout);
    label_symbols_free(region_symbols);
    label_layout_free(legacy_layout);
    label_selection_free(alt_selection);
    label_symbols_free(alt_label_symbols);
    label_selection_free(label_selection);
    label_symbols_free(label_symbols);
    return 0;
}
