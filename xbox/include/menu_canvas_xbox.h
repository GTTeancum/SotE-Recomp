#pragma once
#include <stdint.h>
/* Xbox-only adaptation of origin/main's src/menu_canvas.cpp (8016d37).
   Callers own strings for the duration of this call. No guest writes. */
enum xb_menu_page { XB_MENU_TITLE, XB_MENU_OPTIONS, XB_MENU_SCHEMES,
    XB_MENU_CHEATS, XB_MENU_PAUSE, XB_MENU_PROFILES, XB_MENU_BINDINGS };
typedef struct { const char *label, *value; int selected; } xb_menu_row;
typedef struct {
    enum xb_menu_page page;
    int pause_background, title_focus;
    const char *title, *tabs, *help, *navigation, *return_label;
    int return_selected, adjusting;
    xb_menu_row rows[16];
    unsigned row_count;
    xb_menu_row actions[2];
    /* Optional replacement only when caller suppresses native pause header. */
    const char *header[4]; /* Player, Difficulty, Time, Challenge Points */
    const char *preset;
    int scroll, total_rows;
    /* Optional controller capture/conflict modal; append-only snapshot fields. */
    const char *modal_title, *modal_detail, *modal_hint;
} xb_menu_snapshot;
/* Returns 1 if a complete canvas was submitted; 0 if original atlas invalid.
   PC 960x540 layout coordinates map to Xbox safe rectangle x48..592/y36..444
   (640x480 output). Glyph proportions stay intact; panels use the4:3 rectangle.
   Original glyphs only. Controls keyboard column is intentionally omitted. */
int xb_menu_canvas_render(const uint8_t *rdram, const xb_menu_snapshot *snapshot);
