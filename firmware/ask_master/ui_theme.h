#ifndef UI_THEME_H
#define UI_THEME_H

#include <Arduino.h>

// ============================================================================
// ask-master UI design system — "Modern Terminal + Mascot"
//
// 240 x 135 px TFT. Default font ~6x8. Treat as 40 cols x 16 rows.
//
// Layout grid:
//   y=0..7       status bar      (8 px,  dim mono, hairline at y=9)
//   y=11..118    body            (~108 px, ~13 rows)
//   y=120..134   keyhint footer  (15 px, accent fill, black text, hairline at y=119)
// ============================================================================

// Layout constants
static const int UI_STATUS_TOP    = 0;
static const int UI_STATUS_BOTTOM = 8;
static const int UI_BODY_TOP      = 11;
static const int UI_BODY_BOTTOM   = 118;
static const int UI_FOOTER_TOP    = 120;
static const int UI_FOOTER_BOTTOM = 134;

static const int UI_PAD_X         = 4;
static const int UI_HAIRLINE_TOP_Y    = 9;
static const int UI_HAIRLINE_BOT_Y    = 119;

// Mascot placement (top-left of body, 24x24 sprite)
static const int UI_MASCOT_CX     = 16;
static const int UI_MASCOT_CY     = UI_BODY_TOP + 14;
static const int UI_BODY_TEXT_X   = 34;          // body text starts right of mascot
static const int UI_BODY_TEXT_W   = 240 - 34 - 4;

// Colors — 16-bit RGB565 via canvas.color565(r,g,b)
// Helper to define a (bright, mid, dim) accent triple.
struct UIAccent {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

static const UIAccent ACC_ASK      = { 0x00, 0xCC, 0xFF }; // cyan
static const UIAccent ACC_CONFIRM  = { 0xFF, 0x5C, 0x3C }; // red-orange
static const UIAccent ACC_CHOOSE   = { 0xFF, 0xB0, 0x00 }; // amber
static const UIAccent ACC_ESCALATE = { 0xFF, 0x20, 0x20 }; // red
static const UIAccent ACC_SETUP    = { 0x00, 0xCC, 0x66 }; // green
static const UIAccent ACC_IDLE     = { 0xC0, 0xC0, 0xC0 }; // soft white

// Greys
#define UI_RGB_BG       0x0000              // black
#define UI_RGB_FG       0xFFFF              // white
// DIM and MUTED computed at runtime via canvas.color565

#endif // UI_THEME_H
