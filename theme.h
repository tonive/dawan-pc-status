#ifndef PC_STATUS_THEME_H
#define PC_STATUS_THEME_H

#include <stdbool.h>
#include <stdint.h>

#define THEME_WIRE_MAX 512u
#define THEME_NAME_MAX 36u
#define THEME_DEFAULT_RECORD "WMT0\n"

enum theme_metric { THEME_CPU, THEME_GPU, THEME_MEMORY, THEME_DISK, THEME_CARD_COUNT };
enum theme_icon { MUSHROOM, FLOWER, PIPE, COIN, NO_ICON, CUSTOM_ICON };
enum theme_style { PIXEL_WORLD, PANEL_STYLE };
enum theme_layout { GRID_LAYOUT, ROW_LAYOUT, HERO_LAYOUT };
enum theme_meter { SEGMENT_METER, SOLID_METER };

typedef struct {
    unsigned metric, icon, accent, banner;
    unsigned icon_colors[3];
    char icon_pixels[64];
} theme_card;

typedef struct {
    char name[THEME_NAME_MAX + 1], title[THEME_NAME_MAX + 1];
    unsigned style, layout, meter, background, surface, ink, muted, outline;
    theme_card cards[THEME_CARD_COUNT];
} pc_theme;

extern const pc_theme builtin_theme;
bool theme_parse(const char *wire, unsigned bytes, pc_theme *out);

#endif
