#include "theme.h"

const pc_theme builtin_theme = {
    .name = "像素关卡", .title = "电脑状态",
    .background = 0x82dafa, .surface = 0xfff9e9, .ink = 0x25243a, .muted = 0x514958, .outline = 0x25243a,
    .cards = {
        {.metric=THEME_CPU, .icon=MUSHROOM, .accent=0xa73b2d, .banner=0xffd3af},
        {.metric=THEME_GPU, .icon=FLOWER, .accent=0x985013, .banner=0xffde9d},
        {.metric=THEME_MEMORY, .icon=PIPE, .accent=0x216c3b, .banner=0xcce7a1},
        {.metric=THEME_DISK, .icon=COIN, .accent=0x875a16, .banner=0xffe0a0},
    }
};

static unsigned line(const char **cursor, const char *end, const char **start) {
    *start = *cursor;
    while (*cursor < end && **cursor != '\n') ++*cursor;
    if (*cursor == end) return 0;
    unsigned bytes = (unsigned)(*cursor - *start);
    ++*cursor;
    return bytes;
}

static bool text(const char *field, unsigned bytes, char *out) {
    if (!bytes || bytes > THEME_NAME_MAX) return false;
    unsigned chars = 0;
    bool visible = false;
    for (unsigned i = 0; i < bytes; ++chars) {
        unsigned c = (uint8_t)field[i++];
        if (c >= 32 && c <= 126) {
            visible |= c != 32;
            continue;
        }
        if (i + 1 >= bytes || c < 0xe3 || c > 0xe9) return false;
        unsigned second = (uint8_t)field[i++], third = (uint8_t)field[i++];
        if (second < 0x80 || second > 0xbf || third < 0x80 || third > 0xbf) return false;
        unsigned code = ((c & 15) << 12) | ((second & 63) << 6) | (third & 63);
        if (code < 0x3000 || code > 0x9fff) return false;
        visible |= code != 0x3000;
    }
    if (!visible || chars > 14) return false;
    for (unsigned i = 0; i < bytes; ++i) out[i] = field[i];
    out[bytes] = 0;
    return true;
}

static bool color(const char *field, unsigned *out) {
    unsigned value = 0;
    for (unsigned i = 0; i < 6; ++i) {
        char c = field[i];
        if (c >= '0' && c <= '9') value = value * 16 + (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') value = value * 16 + (unsigned)(c - 'a' + 10);
        else return false;
    }
    *out = value;
    return true;
}

static bool sprite(const char *field, theme_card *card) {
    for (unsigned i = 0; i < 3; ++i)
        if (!color(field + i * 6, &card->icon_colors[i])) return false;
    const char *pixels = field + 18;
    unsigned runs = 0;
    for (unsigned i = 0; i < 64; ++i) {
        char c = pixels[i];
        if (c != '.') {
            if (c < '1' || c > '3') return false;
            if (i % 8 == 0 || c != pixels[i - 1]) ++runs;
        }
        card->icon_pixels[i] = c;
    }
    return runs >= 1 && runs <= 32;
}

bool theme_parse(const char *wire, unsigned bytes, pc_theme *out) {
    if (bytes < 5 || bytes > THEME_WIRE_MAX || wire[0] != 'W' || wire[1] != 'M' ||
        wire[2] != 'T' || (wire[3] != '1' && wire[3] != '2') || wire[4] != '\n') return false;
    bool extended = wire[3] == '2';
    const char *end = wire + bytes, *cursor = wire + 5, *field = cursor;
    pc_theme candidate = {0};
    unsigned count = line(&cursor, end, &field);
    if (!text(field, count, candidate.name)) return false;
    count = line(&cursor, end, &field);
    if (!text(field, count, candidate.title)) return false;
    if (line(&cursor, end, &field) != 3) return false;
    if (field[0] < '0' || field[0] > '1' || field[1] < '0' ||
        field[1] > (extended ? '2' : '1') || field[2] < '0' || field[2] > '1') return false;
    candidate.style = field[0] - '0';
    candidate.layout = field[1] - '0';
    candidate.meter = field[2] - '0';
    if (line(&cursor, end, &field) != 30 || !color(field, &candidate.background) ||
        !color(field + 6, &candidate.surface) || !color(field + 12, &candidate.ink) ||
        !color(field + 18, &candidate.muted) || !color(field + 24, &candidate.outline)) return false;
    unsigned seen = 0;
    for (unsigned i = 0; i < THEME_CARD_COUNT; ++i) {
        count = line(&cursor, end, &field);
        if (count < 14 || field[0] < '0' || field[0] > '3' || field[1] < '0' ||
            field[1] > (extended ? '5' : '4')) return false;
        theme_card *card = &candidate.cards[i];
        card->metric = (unsigned)(field[0] - '0');
        card->icon = (unsigned)(field[1] - '0');
        unsigned bit = 1u << card->metric;
        if ((seen & bit) || !color(field + 2, &card->accent) || !color(field + 8, &card->banner)) return false;
        if (card->icon == CUSTOM_ICON) {
            if (count != 96 || !sprite(field + 14, card)) return false;
        } else if (count != 14) return false;
        seen |= bit;
    }
    if (cursor != end) return false;
    *out = candidate;
    return true;
}
