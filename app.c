#include "dawan_streaming.h"
#include "crypto.h"
#include "theme.h"

__attribute__((section(".dawan"), used, retain))
static const dawan_module_info module = {
    DAWAN_MODULE_MAGIC, sizeof(dawan_module_info), DAWAN_ABI_STREAMING, 3, {0}
};

#define PORT ":47831"
#define API "/wm-pc/v1/"
#define PEER_LIMIT 5
#define INK 0x25243a
#define CREAM 0xfff4d9
#define PAPER 0xfff9e9
#define PANEL 0x3a5279
#define TEXT 0x25243a
#define MUTED 0x514958
#define ACCENT 0x146143
#define ACTION_GREEN 0x26794e
#define ACTION_GOLD 0xffcd5b
#define DANGER 0xa83932
#define UI_CAPACITY 288u
#ifndef PC_LAN_SEED
#define PC_LAN_SEED ""
#endif

enum page { DASHBOARD, CONNECT, DISCOVERY, PAIR, CONFIRM, THEMES, THEME_PREVIEW };
enum job_kind { NONE, ENVIRONMENT, LOAD_ADDRESS, SAVE_ADDRESS, PROBE, STATUS, CHALLENGE, SHUTDOWN,
                LOAD_THEME, CHECK_THEME, FETCH_THEME, SAVE_THEME };
enum action {
    EXIT = 1, SETTINGS, BACK, ERASE, DIRECT, SEARCH, MANUAL, SELECT_PEER = 16,
    KEY = 32, EDIT_CODE = 70, APPLY_CODE, ASK_SHUTDOWN, DO_SHUTDOWN,
    OPEN_THEMES, GET_THEME, INSTALL_THEME, RESET_THEME, PREVIEW_THEME
};
enum metric { CPU, MEMORY, USED, TOTAL, DISK, FREE, RX, TX, UPTIME, GPU, TEMP, CAN_SHUTDOWN, METRIC_COUNT };

typedef struct {
    char ip[16];
    char name[49];
} peer;

static struct {
    dawan_ui_node nodes[UI_CAPACITY];
    unsigned nodes_used, revision, tries, scan, probes, peers, input_length, key_length, target, submitted_target;
    unsigned canvas_width, status_failures;
    unsigned subnet[3];
    enum page screen, presented_screen, previous_screen;
    enum job_kind pending, theme_job;
    uint64_t serial, active, due, confirm_until, discover_due, theme_due;
    int64_t clock_epoch;
    bool presented, reported_metrics, dirty, stopping, have_address, ready, environment_error;
    bool load, save, scanning, auto_select, fresh, challenge, send_shutdown, control_sent;
    char ip[16], input[16], name[49], key[PC_KEY_LENGTH + 1];
    char client[PC_NONCE_LENGTH + 1], server[PC_NONCE_LENGTH + 1];
    char response[1025], date[11], time[9];
    const char *storage_key, *message, *theme_key, *theme_message;
    pc_theme installed_theme, preview_theme;
    char theme_wire[THEME_WIRE_MAX + 1], installed_wire[THEME_WIRE_MAX + 1];
    bool custom_theme, reset_theme, offered_theme;
    peer found[PEER_LIMIT];
    int metrics[METRIC_COUNT];
} state;

static const char alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static unsigned length(const char *text) {
    unsigned size = 0;
    while (text[size]) ++size;
    return size;
}

static char *append(char *out, const char *text) {
    while (*text) *out++ = *text++;
    *out = 0;
    return out;
}

static char *number(char *out, unsigned value) {
    char digits[10];
    unsigned used = 0;
    do {
        digits[used++] = '0' + value % 10;
        value /= 10;
    } while (value);
    while (used) *out++ = digits[--used];
    *out = 0;
    return out;
}

static bool starts(const char *text, const char *prefix) {
    while (*prefix) if (*text++ != *prefix++) return false;
    return true;
}

static bool same_theme_offer(void) {
    return state.offered_theme && length(state.theme_wire) == length(state.installed_wire) &&
           starts(state.theme_wire, state.installed_wire);
}

static dawan_ui_node *node(unsigned parent, unsigned kind, int x, int y, int w, int h) {
    dawan_ui_node *item = &state.nodes[state.nodes_used++];
    int width = (int)state.canvas_width;
    int left = x * width / 480;
    *item = (dawan_ui_node){
        .size = sizeof(*item), .parent = parent, .kind = kind, .x = left, .y = y,
        .width = (x + w) * width / 480 - left, .height = h, .opacity = 255
    };
    return item;
}

static unsigned box(unsigned parent, int x, int y, int w, int h, unsigned color, unsigned radius) {
    dawan_ui_node *item = node(parent, DAWAN_BOX, x, y, w, h);
    item->background = color;
    item->radius = radius;
    return state.nodes_used;
}

static void label(unsigned parent, int x, int y, int w, int h,
                  unsigned pixels, unsigned color, unsigned flags, const char *text) {
    dawan_ui_node *item = node(parent, DAWAN_LABEL, x, y, w, h);
    item->background = state.nodes[parent - 1].background;
    item->foreground = color;
    item->pixels = pixels;
    item->flags = flags;
    append(item->text, text);
}

static unsigned framed_box(unsigned parent, int x, int y, int w, int h, unsigned color) {
    unsigned id = box(parent, x, y, w, h, color, 0);
    state.nodes[id - 1].border = INK;
    state.nodes[id - 1].border_width = 2;
    return id;
}

static void button(int x, int y, int w, int h, const char *text, unsigned color, unsigned action) {
    unsigned id = framed_box(1, x, y, w, h, color);
    state.nodes[id - 1].flags = DAWAN_CLICKABLE;
    state.nodes[id - 1].action = action;
    box(id, 4, h - 9, w - 8, 5, INK, 0);
    unsigned foreground = color == ACTION_GOLD ? INK : PAPER;
    label(id, 11, 5, w - 20, h - 12, 21, foreground, DAWAN_TEXT_BOLD, text);
}

static void cloud(int x, int y) {
    box(1, x + 8, y + 6, 42, 15, 0xc9eff9, 0);
    box(1, x, y + 11, 57, 11, 0xc9eff9, 0);
    box(1, x + 10, y + 4, 35, 13, 0xf4fbfb, 0);
    box(1, x + 20, y, 15, 4, 0xf4fbfb, 0);
}

static void world(const pc_theme *theme) {
    box(0, 0, 0, 480, 480, theme->background, 0);
    if (theme->style == PANEL_STYLE) return;
    cloud(3, 75);
    cloud(419, 26);
    box(1, 0, 397, 480, 83, 0xb96a43, 0);
    box(1, 0, 397, 480, 8, 0x3d9d53, 0);
    box(1, 0, 405, 480, 3, INK, 0);
    box(1, 0, 465, 480, 2, 0x834331, 0);
    for (int x = 30; x < 480; x += 60)
        box(1, x, 467, 2, 13, 0x834331, 0);
}

static void title(const char *name, const char *back, unsigned action) {
    world(&builtin_theme);
    framed_box(1, 12, 72, 456, 395, CREAM);
    label(1, 22, 19, 323, 48, 29, TEXT, DAWAN_TEXT_BOLD, name);
    button(365, 19, 91, 46, back, PANEL, action);
}

static int metric(enum metric field) {
    return state.fresh ? state.metrics[field] : -1;
}

static void format_value(char *out, int value, const char *unit) {
    if (value < 0) append(out, "—");
    else append(number(out, (unsigned)value), unit);
}

static const char icons[][8][9] = {
    {
        "..kkkk..", ".krrrrk.", "krrwwrrk", "krwwrrrk",
        "krrrrwwk", ".kkkkkk.", "..kook..", "..kkkk.."
    },
    {
        "...kk...", "..krrk..", ".krwwrk.", "..krrk..",
        "...kk...", ".ggkg...", "..gkg.g.", "...ggg.."
    },
    {
        ".kkkkkk.", "kggggggk", "kgllgggk", ".kkkkkk.",
        ".kglggk.", ".kglggk.", ".kglggk.", ".kkkkkk."
    },
    {
        "..kkkk..", ".kyyyyk.", "kyywwyyk", "kywoyoyk",
        "kywoyoyk", "kywoooyk", ".kyyyyk.", "..kkkk.."
    },
};

static void tile_icon(unsigned parent, int x, int y, const theme_card *card) {
    if (card->icon == NO_ICON) return;
    bool custom = card->icon == CUSTOM_ICON;
    for (unsigned row = 0; row < 8; ++row) {
        const char *line = custom ? card->icon_pixels + row * 8 : icons[card->icon][row];
        for (unsigned first = 0, end; first < 8; first = end) {
            char pixel = line[first];
            end = first + 1;
            while (end < 8 && line[end] == pixel) ++end;
            if (pixel == '.') continue;
            unsigned color;
            if (custom) color = card->icon_colors[pixel - '1'];
            else switch (pixel) {
                case 'k': color = INK; break;
                case 'r': color = 0xe4503d; break;
                case 'g': color = 0x3ca654; break;
                case 'l': color = 0x9de56c; break;
                case 'w': color = 0xfff8da; break;
                case 'o': color = 0xe39a3f; break;
                case 'y': color = 0xffd259; break;
                default: continue;
            }
            box(parent, x + (int)first * 3, y + (int)row * 3, (int)(end - first) * 3, 3, color, 0);
        }
    }
}

static unsigned themed_box(const pc_theme *theme, int x, int y, int w, int h) {
    unsigned id = box(1, x, y, w, h, theme->surface, theme->style == PANEL_STYLE ? 10 : 0);
    state.nodes[id - 1].border = theme->outline;
    state.nodes[id - 1].border_width = theme->style == PANEL_STYLE ? 1 : 2;
    return id;
}

static void meter(unsigned parent, const pc_theme *theme, int x, int y, int width, int level, unsigned color) {
    unsigned track = theme == &builtin_theme ? 0xe7d9ba : theme->outline;
    if (theme->meter == SOLID_METER) {
        box(parent, x, y, width, 8, track, 2);
        if (level > 0 && width * level / 100 > 0)
            box(parent, x, y, width * level / 100, 8, color, 2);
        return;
    }
    int step = width / 10;
    for (int segment = 0; segment < 10; ++segment) {
        unsigned fill = level >= 0 && (level + 5) / 10 > segment ? color : track;
        box(parent, x + segment * step, y, step - 3, 10, fill, 0);
    }
}

static int readings(unsigned metric_id, const char **title_text, char value[44], char detail[147]) {
    static const enum metric fields[] = {CPU, GPU, MEMORY, DISK};
    static const char *const titles[] = {"CPU", "GPU", "内存", "系统盘"};
    int level = metric(fields[metric_id]);
    *title_text = titles[metric_id];
    format_value(value, level, "%");
    switch (metric_id) {
        case THEME_CPU: append(detail, "处理器使用率"); break;
        case THEME_GPU: format_value(append(detail, "温度 "), metric(TEMP), " °C"); break;
        case THEME_MEMORY:
            if (state.fresh) {
                char *end = append(number(detail, state.metrics[USED] / 1024u), " / ");
                append(number(end, state.metrics[TOTAL] / 1024u), " GiB");
            } else append(detail, "物理内存");
            break;
        case THEME_DISK:
            format_value(append(detail, "可用 "), state.fresh ? state.metrics[FREE] / 1024 : -1, " GiB");
            break;
    }
    return level;
}

static void metric_card(const pc_theme *theme, unsigned index) {
    const theme_card *item = &theme->cards[index];
    const char *title_text;
    char value[44], detail[147];
    int level = readings(item->metric, &title_text, value, detail);
    if (theme->layout != GRID_LAYOUT) {
        bool featured = theme->layout == HERO_LAYOUT && index == 0;
        int y = theme->layout == ROW_LAYOUT ? 99 + (int)index * 57
              : featured ? 99 : 174 + ((int)index - 1) * 52;
        int height = featured ? 67 : 50;
        unsigned row = themed_box(theme, 16, y, 448, height);
        box(row, 3, 3, featured ? 175 : 145, featured ? 38 : 29, item->banner, 0);
        tile_icon(row, 9, featured ? 9 : 5, item);
        label(row, 41, featured ? 7 : 4, featured ? 132 : 103, featured ? 32 : 27,
              featured ? 22 : 17, theme->ink, DAWAN_TEXT_BOLD, title_text);
        label(row, featured ? 186 : 157, featured ? 9 : 4, featured ? 153 : 180, 27, 15, theme->muted, 0, detail);
        label(row, 350, featured ? 5 : 3, 91, featured ? 43 : 34,
              featured ? 30 : 25, item->accent, DAWAN_TEXT_BOLD, value);
        meter(row, theme, 12, featured ? 50 : 35, 424, level, item->accent);
        return;
    }
    int x = 16 + (int)(index % 2) * 232, y = 99 + (int)(index / 2) * 116;
    if (theme->style == PIXEL_WORLD)
        box(1, x + 3, y + 4, 216, 104, 0x4596ba, 0);
    unsigned card = themed_box(theme, x, y, 216, 104);
    box(card, 3, 3, 210, 30, item->banner, 0);
    label(card, 12, 5, 154, 27, 18, theme->ink, DAWAN_TEXT_BOLD, title_text);
    tile_icon(card, 179, 6, item);
    label(card, 12, 35, 193, 32, 30, item->accent, DAWAN_TEXT_BOLD, value);
    label(card, 12, 67, 193, 18, 15, theme->muted, 0, detail);
    meter(card, theme, 12, 86, 190, level, item->accent);
}

static void dashboard(void) {
    bool preview = state.screen == THEME_PREVIEW;
    const pc_theme *theme = preview ? &state.preview_theme : state.custom_theme ? &state.installed_theme : &builtin_theme;
    char detail[147], context[112];
    world(theme);
    unsigned heading = themed_box(theme, 16, 15, 448, 73);
    if (theme->style == PIXEL_WORLD) {
        box(heading, 15, 10, 3, 33, theme->outline, 0);
        box(heading, 18, 11, 21, 13, 0xe4503d, 0);
        box(heading, 18, 24, 12, 3, 0xb53932, 0);
        box(heading, 11, 43, 11, 3, theme->outline, 0);
        box(heading, 403, 16, 22, 5, ACTION_GOLD, 0);
        box(heading, 399, 21, 30, 5, ACTION_GOLD, 0);
        box(heading, 403, 26, 22, 5, 0xe39a3f, 0);
    }
    label(heading, 50, 5, 340, 43, 27, theme->ink, DAWAN_TEXT_BOLD, theme->title);
    label(heading, 15, 46, 224, 21, 15, theme->muted, 0, state.date[0] ? state.date : "----/--/--");
    label(heading, 339, 46, 100, 21, 15, theme->muted, 0, state.time[0] ? state.time : "--:--:--");
    for (unsigned i = 0; i < THEME_CARD_COUNT; ++i) metric_card(theme, i);

    const char *status = "等待自动发现上位机";
    if (preview) {
        append(append(context, "预览 · "), theme->name);
        status = state.theme_message ? state.theme_message : context;
    } else if (state.environment_error) status = "当前系统缺少服务环境支持";
    else if (!state.ready) status = "读取万墨 OS 服务环境";
    else if (state.scanning) status = "正在搜索局域网上位机…";
    else if (state.fresh) {
        append(append(append(context, state.name), "  ·  "), state.ip);
        status = context;
    } else if (state.have_address && state.status_failures < 2) {
        append(append(context, "正在连接  "), state.ip);
        status = context;
    } else if (state.discover_due) status = "暂未发现电脑，稍后重试";
    unsigned footer = themed_box(theme, 16, 330, 448, 79);
    box(footer, 12, 12, 11, 11, state.fresh ? 0x339d50 : 0xca8a31, 0);
    label(footer, 31, 6, 406, 26, 18, theme->ink, DAWAN_TEXT_BOLD,
          !preview && state.message ? state.message : status);
    if (state.fresh) {
        format_value(append(detail, "下载 "), metric(RX), " KiB/s");
        format_value(append(detail + length(detail), "    上传 "), metric(TX), " KiB/s");
        label(footer, 12, 31, 420, 21, 15, theme->muted, 0, detail);
        unsigned minutes = state.metrics[UPTIME] / 60u;
        char *end = append(number(append(detail, "已运行 "), minutes / 60), "h ");
        append(number(end, minutes % 60), "m");
        label(footer, 12, 52, 420, 21, 15, theme->muted, 0, detail);
    } else label(footer, 12, 38, 420, 26, 15, theme->muted, 0, "请在电脑上启动 Windows 上位机");

    if (preview) {
        if (state.theme_job != SAVE_THEME && state.pending != SAVE_THEME) {
            button(16, 417, 216, 44, "安装", ACTION_GREEN, INSTALL_THEME);
            button(248, 417, 216, 44, "取消", PANEL, BACK);
        }
    } else if (state.fresh && metric(CAN_SHUTDOWN) == 1 && !state.challenge && !state.send_shutdown &&
        state.pending != CHALLENGE && state.pending != SHUTDOWN) {
        button(16, 417, 83, 44, "电脑", ACTION_GREEN, SETTINGS);
        button(107, 417, 89, 44, state.offered_theme && !same_theme_offer() ? "新主题" : "主题",
               PANEL, OPEN_THEMES);
        button(198, 417, 83, 44, "配对", ACTION_GOLD, EDIT_CODE);
        button(289, 417, 83, 44, "关机", DANGER, ASK_SHUTDOWN);
        button(380, 417, 84, 44, "退出", PANEL, EXIT);
    } else {
        button(16, 417, 141, 44, "电脑", ACTION_GREEN, SETTINGS);
        button(169, 417, 142, 44, state.offered_theme && !same_theme_offer() ? "新主题" : "主题",
               ACTION_GOLD, OPEN_THEMES);
        button(323, 417, 141, 44, "退出", PANEL, EXIT);
    }
}

static void themes(void) {
    char active[85];
    title("监控屏主题", "返回", BACK);
    append(append(active, "当前 · "), state.custom_theme ? state.installed_theme.name : builtin_theme.name);
    label(1, 26, 105, 428, 42, 24, TEXT, DAWAN_TEXT_BOLD, active);
    if (state.offered_theme) {
        append(append(active, "已获取 · "), state.preview_theme.name);
        label(1, 26, 165, 425, 32, 19, ACCENT, DAWAN_TEXT_BOLD, active);
    } else label(1, 26, 165, 425, 32, 17, MUTED, 0, "在 Windows 上位机上传主题文件。");
    label(1, 26, 210, 425, 32, 17, MUTED, 0, "预览后点「安装」即可保存到大万。");
    if (state.theme_message)
        label(1, 26, 268, 424, 36, 16, ACCENT, 0, state.theme_message);
    if (state.theme_job == NONE && state.pending != FETCH_THEME && state.pending != SAVE_THEME) {
        if (state.offered_theme) {
            button(26, 332, 208, 54, "预览主题", ACTION_GREEN, PREVIEW_THEME);
            button(26, 397, 208, 48, "重新获取", PANEL, GET_THEME);
        } else button(26, 332, 208, 54, "获取主题", ACTION_GREEN, GET_THEME);
        if (state.custom_theme) button(247, 332, 207, 54, "恢复内置", PANEL, RESET_THEME);
    }
}

static void connection_setup(void) {
    title("手动连接", "返回", BACK);
    label(1, 22, 76, 431, 32, 18, MUTED, 0, "输入电脑 IPv4，或输入网段的前三段");
    label(1, 22, 109, 430, 44, 26, ACCENT, DAWAN_TEXT_BOLD, state.input[0] ? state.input : "IPv4");
    static const char keys[] = "123456789.0";
    for (unsigned i = 0; i < sizeof(keys) - 1; ++i) {
        char key_text[2] = {keys[i], 0};
        unsigned key = keys[i] == '.' ? 10 : (unsigned)(keys[i] - '0');
        button(22 + (i % 4) * 111, 159 + (i / 4) * 61, 101, 51, key_text, PANEL, KEY + key);
    }
    button(355, 281, 101, 51, "删除", PANEL, ERASE);
    label(1, 22, 341, 430, 37, 17, MUTED, 0, state.message ? state.message : "地址可在 Windows 上位机窗口中查看");
    button(22, 396, 210, 55, "搜索网段", ACTION_GREEN, SEARCH);
    button(244, 396, 212, 55, "直连电脑", ACTION_GOLD, DIRECT);
}

static void discovery(void) {
    char progress[90], row[86];
    title("附近的电脑", "返回", BACK);
    if (state.scanning) {
        char *end = number(append(progress, "自动搜索中  "), state.probes);
        append(end, " / 254");
    } else append(progress, state.peers ? "点选要连接的电脑" : "暂未发现电脑");
    label(1, 22, 78, 432, 33, 19, MUTED, 0, progress);
    for (unsigned i = 0; i < state.peers; ++i) {
        append(append(append(row, state.found[i].name), "  ·  "), state.found[i].ip);
        button(22, 116 + i * 55, 434, 48, row, PANEL, SELECT_PEER + i);
    }
    button(22, 407, 205, 47, "重新搜索", ACTION_GREEN, SEARCH);
    button(239, 407, 217, 47, "手动输入", PANEL, MANUAL);
}

static void pair(void) {
    char prompt[78];
    title("上位机配对码", "返回", BACK);
    append(number(append(prompt, "已输入 "), state.key_length), " / 20 字符");
    label(1, 22, 78, 432, 34, 20, ACCENT, 0, prompt);
    for (unsigned i = 0; i < sizeof(alphabet) - 1; ++i) {
        char text[2] = {alphabet[i], 0};
        button(22 + (i % 8) * 55, 121 + (i / 8) * 57, 49, 49, text, PANEL, KEY + i);
    }
    if (state.key_length) button(22, 371, 123, 50, "删除", PANEL, ERASE);
    if (state.key_length == PC_KEY_LENGTH) button(275, 371, 181, 50, "使用配对码", ACTION_GOLD, APPLY_CODE);
}

static void confirmation(void) {
    title("确认关机", "取消", BACK);
    label(1, 24, 114, 421, 50, 32, TEXT, DAWAN_TEXT_BOLD, state.name);
    label(1, 24, 187, 425, 40, 21, MUTED, 0, "请先保存电脑上的工作。");
    label(1, 24, 235, 425, 40, 21, MUTED, 0, "上位机倒计时 15 秒，可取消。");
    button(24, 328, 192, 58, "确认关机", DANGER, DO_SHUTDOWN);
}

static void report(const dawan_host *base, const char *message) {
    if (base->report) base->report(base->instance, state.revision, message, length(message));
}

static int present(const dawan_app_host *host) {
    state.nodes_used = 0;
    switch (state.screen) {
        case DASHBOARD: dashboard(); break;
        case CONNECT: connection_setup(); break;
        case DISCOVERY: discovery(); break;
        case PAIR: pair(); break;
        case CONFIRM: confirmation(); break;
        case THEMES: themes(); break;
        case THEME_PREVIEW: dashboard(); break;
    }
    const dawan_host *base = &host->ui.base;
    int result = host->ui.present(base->instance, state.revision + 1, state.nodes, state.nodes_used);
    if (result == DAWAN_BUSY) return DAWAN_OK;
    if (result != DAWAN_OK) return result;
    ++state.revision;
    state.previous_screen = state.presented_screen;
    state.presented_screen = state.screen;
    state.dirty = false;
    if (!state.presented) {
        state.presented = true;
        report(base, "pc-status: first screen presented");
    }
    return DAWAN_OK;
}

static bool parse_ip(const char *text, unsigned count, unsigned parts[4]) {
    for (unsigned i = 0; i < count; ++i) {
        unsigned value = 0, digits = 0;
        while (*text >= '0' && *text <= '9' && digits < 3) {
            value = value * 10 + (unsigned)(*text++ - '0');
            ++digits;
        }
        if (!digits || value > 255 || *text++ != (i + 1 < count ? '.' : '\0')) return false;
        parts[i] = value;
    }
    return parts[0] == 10 || (parts[0] == 172 && parts[1] >= 16 && parts[1] <= 31) ||
           (parts[0] == 192 && parts[1] == 168);
}

static void format_ip(char *out, const unsigned parts[4]) {
    for (unsigned i = 0; i < 4; ++i) {
        out = number(out, parts[i]);
        if (i < 3) *out++ = '.';
    }
}

static void clear_key(void) {
    pc_wipe(state.key, sizeof(state.key));
    state.key_length = 0;
}

static void select_address(const char *ip) {
    ++state.target;
    append(state.ip, ip);
    state.have_address = state.save = true;
    state.fresh = state.scanning = false;
    state.due = state.discover_due = 0;
    state.status_failures = 0;
    state.theme_due = 0;
    state.offered_theme = false;
    state.screen = DASHBOARD;
    state.message = 0;
    clear_key();
}

static void start_scan(const unsigned parts[4], bool auto_select) {
    for (unsigned i = 0; i < 3; ++i) state.subnet[i] = parts[i];
    state.scan = parts[3] >= 1 && parts[3] <= 254 ? parts[3] : 1;
    ++state.target;
    state.probes = state.peers = 0;
    state.discover_due = 0;
    state.scanning = state.dirty = true;
    state.auto_select = auto_select;
}

static void search_known_network(bool auto_select, uint64_t now) {
    unsigned parts[4];
    if ((state.have_address && parse_ip(state.ip, 4, parts)) || parse_ip(PC_LAN_SEED, 4, parts))
        start_scan(parts, auto_select);
    else state.discover_due = now + 30000;
}

static const char *json_value(const char *text, const char *name) {
    for (; *text; ++text) {
        if (!starts(text, name)) continue;
        const char *value = text + length(name);
        while (*value == ' ' || *value == '\r' || *value == '\n' || *value == '\t') ++value;
        if (*value++ != ':') continue;
        while (*value == ' ' || *value == '\r' || *value == '\n' || *value == '\t') ++value;
        return value;
    }
    return "";
}

static bool json_is(const char *text, const char *name, const char *expected) {
    const char *value = json_value(text, name);
    if (!starts(value, expected)) return false;
    value += length(expected);
    while (*value == ' ' || *value == '\r' || *value == '\n' || *value == '\t') ++value;
    return *value == ',' || *value == '}';
}

/* Wire responses are short, ASCII line records, and must end on a field boundary. */
static const char *identity(const char *reply, char name[49]) {
    if (!starts(reply, "WMPC1\n")) return 0;
    reply += 6;
    unsigned size = 0;
    while (*reply && *reply != '\n' && size < 48) {
        char c = *reply++;
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9') &&
            c != '-' && c != '_' && c != '.') return 0;
        name[size++] = c;
    }
    name[size] = 0;
    return size && *reply == '\n' ? reply + 1 : 0;
}

static bool metrics(const char *reply) {
    char name[49];
    int values[METRIC_COUNT];
    const char *field = identity(reply, name);
    if (!field) return false;
    for (unsigned i = 0; i < METRIC_COUNT; ++i) {
        if (starts(field, "-1\n")) {
            if (i != RX && i != TX && i != GPU && i != TEMP) return false;
            values[i] = -1;
            field += 3;
            continue;
        }
        unsigned value = 0, digits = 0;
        while (*field >= '0' && *field <= '9') {
            if (value > 100000000u) return false;
            value = value * 10 + (unsigned)(*field++ - '0');
            ++digits;
        }
        if (!digits || *field++ != '\n') return false;
        if ((i == CPU || i == MEMORY || i == DISK || i == GPU) && value > 100) return false;
        if (i == TEMP && value > 200) return false;
        if (i == CAN_SHUTDOWN && value > 1) return false;
        values[i] = (int)value;
    }
    if (*field || values[USED] > values[TOTAL]) return false;
    append(state.name, name);
    for (unsigned i = 0; i < METRIC_COUNT; ++i) state.metrics[i] = values[i];
    return true;
}

static void scan_address(char out[16]) {
    int distance = (int)((state.probes + 1) / 2);
    int offset = state.probes % 2 ? distance : -distance;
    unsigned host = (unsigned)((int)state.scan - 1 + offset + 254) % 254u + 1;
    unsigned parts[4] = {state.subnet[0], state.subnet[1], state.subnet[2], host};
    format_ip(out, parts);
}

static bool hex_field(const char *value, unsigned size) {
    for (unsigned i = 0; i < size; ++i) {
        char c = value[i];
        if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f')) return false;
    }
    return value[size] == '\n';
}

static void receive(const dawan_app_host *host, uint64_t now) {
    const dawan_host *base = &host->ui.base;
    dawan_result result = {.size = sizeof(result)};
    if (host->jobs->poll(base->instance, &result, (uint8_t *)state.response, sizeof(state.response) - 1) != DAWAN_OK)
        return;
    if (result.kind != DAWAN_DONE || result.id != state.active || result.generation != base->instance) return;
    enum job_kind kind = state.pending;
    state.active = 0;
    state.pending = NONE;
    bool success = result.result == DAWAN_OK && result.bytes < sizeof(state.response);
    state.response[success ? result.bytes : 0] = 0;
    if (success) for (unsigned i = 0; i < result.bytes; ++i) if (!state.response[i]) success = false;
    bool http_ok = success && result.status == 200;
    char signature[PC_PROOF_LENGTH + 1];
    state.dirty = true;
    switch (kind) {
        case ENVIRONMENT:
            if (success && json_is(state.response, "\"environment_version\"", "1")) {
                if (json_is(state.response, "\"environment\"", "\"test\"")) {
                    state.storage_key = "pc_ip_test";
                    state.theme_key = "pc_theme_test";
                }
                if (json_is(state.response, "\"environment\"", "\"production\"")) {
                    state.storage_key = "pc_ip_prod";
                    state.theme_key = "pc_theme_prod";
                }
            }
            state.ready = state.storage_key != 0;
            state.environment_error = !state.ready;
            state.load = state.ready;
            if (state.ready) state.theme_job = LOAD_THEME;
            state.due = 0;
            report(base, state.ready ? "pc-status: service environment ready" : "pc-status: service environment unavailable");
            break;
        case LOAD_ADDRESS: {
            unsigned parts[4];
            if (success && !state.have_address && parse_ip(state.response, 4, parts)) {
                format_ip(state.ip, parts);
                state.have_address = true;
            }
            break;
        }
        case SAVE_ADDRESS:
            if (!success && !state.message) state.message = "无法保存地址，本次仍可连接";
            break;
        case LOAD_THEME:
            if (success && theme_parse(state.response, result.bytes, &state.installed_theme)) {
                append(state.installed_wire, state.response);
                state.custom_theme = true;
                report(base, "pc-status: installed theme restored");
            }
            break;
        case CHECK_THEME:
        case FETCH_THEME: {
            if (state.submitted_target != state.target || (kind == FETCH_THEME && state.screen != THEMES)) break;
            if (kind == CHECK_THEME && state.screen == THEME_PREVIEW) break;
            state.theme_due = now + 12000;
            bool changed = !state.offered_theme || !starts(state.response, state.theme_wire) ||
                           length(state.response) != length(state.theme_wire);
            if (http_ok && theme_parse(state.response, result.bytes, &state.preview_theme)) {
                append(state.theme_wire, state.response);
                state.offered_theme = true;
                if (changed) report(base, "pc-status: theme received and validated");
                if (kind == FETCH_THEME) {
                    state.theme_message = 0;
                    state.screen = THEME_PREVIEW;
                    report(base, "pc-status: theme preview ready");
                }
            } else if (kind == FETCH_THEME) {
                char info[90];
                char *end = number(append(info, "pc-status: theme fetch HTTP="), (unsigned)result.status);
                end = append(end, result.result < 0 ? " result=-" : " result=");
                end = number(end, result.result < 0 ? 0u - (unsigned)result.result : (unsigned)result.result);
                number(append(end, " bytes="), result.bytes);
                report(base, info);
                if (success && result.status == 404) {
                    state.offered_theme = false;
                    state.theme_message = "上位机未提供主题，请先上传。";
                } else state.theme_message = http_ok ? "主题格式无效。"
                                                     : "获取失败，请检查上位机连接。";
            } else if (success && result.status == 404) {
                state.offered_theme = false;
            }
            break;
        }
        case SAVE_THEME:
            if (!success) {
                state.theme_message = "保存失败，可重试。当前主题仍在使用。";
            } else {
                if (state.reset_theme) state.installed_wire[0] = 0;
                else {
                    state.installed_theme = state.preview_theme;
                    append(state.installed_wire, state.theme_wire);
                }
                state.custom_theme = !state.reset_theme;
                state.message = 0;
                state.screen = DASHBOARD;
                report(base, state.custom_theme ? "pc-status: theme installed"
                                                : "pc-status: builtin theme restored");
            }
            break;
        case PROBE:
            if (!state.scanning || state.submitted_target != state.target) break;
            if (http_ok) {
                peer *found = &state.found[state.peers];
                const char *end = identity(state.response, found->name);
                if (end && !*end) {
                    scan_address(found->ip);
                    ++state.peers;
                    if (state.auto_select) {
                        select_address(found->ip);
                        report(base, "pc-status: automatically discovered Windows companion");
                        break;
                    }
                }
            }
            if (++state.probes == 254 || state.peers == PEER_LIMIT) {
                state.scanning = false;
                state.discover_due = now + 30000;
            }
            break;
        case STATUS:
            if (state.submitted_target != state.target) break;
            state.fresh = http_ok && metrics(state.response);
            state.due = now + (state.fresh ? 2000 : 3000);
            if (state.fresh) state.status_failures = 0;
            else if (state.status_failures < 2) ++state.status_failures;
            if (state.fresh && !state.reported_metrics) {
                report(base, "pc-status: first Windows metrics received");
                state.reported_metrics = true;
            }
            break;
        case CHALLENGE:
            if (http_ok && result.bytes == 104 && starts(state.response, "WMPC1\n") &&
                hex_field(state.response + 6, PC_NONCE_LENGTH) && hex_field(state.response + 39, PC_PROOF_LENGTH)) {
                for (unsigned i = 0; i < PC_NONCE_LENGTH; ++i) state.server[i] = state.response[6 + i];
                pc_sign(signature, state.key, "challenge", state.client, state.server);
                state.send_shutdown = pc_equal(signature, state.response + 39, PC_PROOF_LENGTH);
            }
            if (!state.send_shutdown) {
                state.message = http_ok ? "配对码不匹配，请重新输入" : "上位机当前未受理关机";
                if (http_ok) clear_key();
            }
            break;
        case SHUTDOWN:
            pc_sign(signature, state.key, "scheduled", state.client, state.server);
            if (http_ok && result.bytes == 71 && starts(state.response, "WMPC1\n") &&
                hex_field(state.response + 6, PC_PROOF_LENGTH) &&
                pc_equal(signature, state.response + 6, PC_PROOF_LENGTH))
                state.message = "关机已预约，电脑端可取消";
            else state.message = "关机结果未知，请查看上位机";
            state.control_sent = false;
            break;
        default: break;
    }
    pc_wipe(signature, sizeof(signature));
}

static void submit(const dawan_app_host *host, enum job_kind kind, uint64_t now) {
    const dawan_host *base = &host->ui.base;
    char address[16], url[80] = "", body[THEME_WIRE_MAX + 1] = "";
    const char *path = 0;
    dawan_job job = {
        .size = sizeof(job), .kind = DAWAN_HTTP, .timeout_ms = 2200, .response_limit = 1024,
        .generation = base->instance, .id = ++state.serial, .method = "", .url = url,
        .header_name = "", .header_value = "", .key = "", .body = (const uint8_t *)body
    };
    append(address, state.ip);
    switch (kind) {
        case ENVIRONMENT: job.kind = DAWAN_HOST_INFO; break;
        case LOAD_ADDRESS:
            job.kind = DAWAN_KV_GET;
            job.key = state.storage_key;
            break;
        case SAVE_ADDRESS:
            job.kind = DAWAN_KV_PUT;
            job.key = state.storage_key;
            append(body, state.ip);
            break;
        case LOAD_THEME:
            job.kind = DAWAN_KV_GET;
            job.key = state.theme_key;
            break;
        case CHECK_THEME:
        case FETCH_THEME:
            job.response_limit = THEME_WIRE_MAX;
            break;
        case SAVE_THEME:
            job.kind = DAWAN_KV_PUT;
            job.key = state.theme_key;
            append(body, state.reset_theme ? THEME_DEFAULT_RECORD : state.theme_wire);
            break;
        case PROBE:
            path = "identify";
            scan_address(address);
            job.timeout_ms = 350;
            break;
        case STATUS: path = "status"; break;
        case CHALLENGE:
            path = "challenge";
            append(body, state.client);
            break;
        case SHUTDOWN:
            path = "shutdown";
            append(append(append(append(body, state.client), "\n"), state.server), "\n");
            pc_sign(body + 66, state.key, "shutdown", state.client, state.server);
            break;
        default: return;
    }
    job.bytes = length(body);
    if (path) {
        append(append(append(append(url, "http://"), address), PORT API), path);
        job.method = job.bytes ? "POST" : "GET";
        job.response_limit = 256;
    } else if (kind == CHECK_THEME || kind == FETCH_THEME) {
        append(append(append(url, "http://"), address), PORT "/wm-pc/v2/theme");
        job.method = "GET";
    }
    int result = host->jobs->submit(base->instance, &job);
    pc_wipe(body, sizeof(body));
    if (result == DAWAN_BUSY) return;
    if (kind == CHECK_THEME || kind == FETCH_THEME) state.theme_due = now + 12000;
    if (kind == LOAD_ADDRESS) state.load = false;
    if (kind == SAVE_ADDRESS) state.save = false;
    if (kind == state.theme_job) state.theme_job = NONE;
    if (kind == CHALLENGE) state.challenge = false;
    if (kind == SHUTDOWN) state.send_shutdown = false;
    if (result == DAWAN_OK) {
        state.active = job.id;
        state.pending = kind;
        state.submitted_target = state.target;
        state.control_sent = kind == SHUTDOWN;
        return;
    }
    switch (kind) {
        case ENVIRONMENT:
            state.environment_error = ++state.tries >= 3;
            state.due = now + 1500;
            break;
        case PROBE:
            state.scanning = false;
            state.discover_due = now + 30000;
            break;
        case STATUS:
            state.fresh = false;
            state.due = now + 3000;
            if (state.status_failures < 2) ++state.status_failures;
            break;
        case CHALLENGE: case SHUTDOWN: state.message = "关机请求提交失败"; break;
        case FETCH_THEME:
            state.theme_message = "获取主题失败，请重试。";
            report(base, "pc-status: theme request rejected by host");
            break;
        case SAVE_THEME: state.theme_message = "无法保存主题，请重试。"; break;
        default: break;
    }
    state.dirty = true;
}

static void touch(const dawan_streaming_host *host, unsigned action, uint64_t now) {
    unsigned parts[4];
    const dawan_host *base = &host->base.base.ui.base;
    state.dirty = true;
    switch (state.screen) {
        case DASHBOARD:
            if (action == OPEN_THEMES && state.ready && !state.control_sent && !state.challenge &&
                !state.send_shutdown && state.pending != CHALLENGE) {
                state.scanning = false;
                state.screen = THEMES;
                state.theme_message = 0;
                if (!state.offered_theme && state.have_address && state.pending != CHECK_THEME &&
                    state.theme_job == NONE) {
                    state.theme_job = FETCH_THEME;
                    state.theme_message = "正在获取主题…";
                }
            }
            if (action == SETTINGS && state.ready && !state.control_sent && !state.challenge &&
                !state.send_shutdown && state.pending != CHALLENGE) {
                state.screen = DISCOVERY;
                state.message = 0;
                search_known_network(false, now);
            }
            if (action == EDIT_CODE && state.fresh && metric(CAN_SHUTDOWN) == 1) {
                clear_key();
                state.screen = PAIR;
            }
            if (action == ASK_SHUTDOWN && state.fresh && metric(CAN_SHUTDOWN) == 1 && !state.challenge &&
                !state.send_shutdown && state.pending != CHALLENGE && !state.control_sent) {
                if (state.key_length != PC_KEY_LENGTH) state.screen = PAIR;
                else {
                    state.screen = CONFIRM;
                    state.confirm_until = now + 10000;
                }
            }
            break;
        case CONNECT:
            state.message = 0;
            if (action == BACK) state.screen = DISCOVERY;
            else if (action == ERASE && state.input_length) state.input[--state.input_length] = 0;
            else if (action >= KEY && action <= KEY + 10 && state.input_length < 15) {
                state.input[state.input_length++] = action == KEY + 10 ? '.' : (char)('0' + action - KEY);
                state.input[state.input_length] = 0;
            } else if (action == DIRECT) {
                if (!parse_ip(state.input, 4, parts)) state.message = "请输入完整的私有 IPv4";
                else {
                    char direct[16];
                    format_ip(direct, parts);
                    select_address(direct);
                }
            } else if (action == SEARCH) {
                if (!parse_ip(state.input, 3, parts)) state.message = "请输入 IPv4 的前三段";
                else {
                    parts[3] = 1;
                    start_scan(parts, false);
                    state.screen = DISCOVERY;
                }
            }
            break;
        case DISCOVERY:
            if (action == BACK) {
                state.scanning = false;
                state.screen = DASHBOARD;
            } else if (action == MANUAL) {
                state.scanning = false;
                state.screen = CONNECT;
                append(state.input, state.ip);
                state.input_length = length(state.input);
            } else if (action == SEARCH) search_known_network(false, now);
            else if (action >= SELECT_PEER && action - SELECT_PEER < state.peers)
                select_address(state.found[action - SELECT_PEER].ip);
            break;
        case PAIR:
            if (action == BACK) {
                clear_key();
                state.screen = DASHBOARD;
            } else if (action == ERASE && state.key_length) state.key[--state.key_length] = 0;
            else if (action == APPLY_CODE && state.key_length == PC_KEY_LENGTH) {
                state.screen = DASHBOARD;
                state.message = "已录入配对码，可发起关机";
            } else if (action >= KEY && action < KEY + sizeof(alphabet) - 1 && state.key_length < PC_KEY_LENGTH)
                state.key[state.key_length++] = alphabet[action - KEY];
            break;
        case CONFIRM:
            if (action == BACK) state.screen = DASHBOARD;
            else if (action == DO_SHUTDOWN && now < state.confirm_until && state.fresh) {
                uint8_t bytes[16];
                if (host->random_bytes(base->instance, bytes, sizeof(bytes)) == DAWAN_OK) {
                    pc_hex(state.client, bytes, sizeof(bytes));
                    state.challenge = true;
                    state.message = "正在验证配对并预约关机";
                } else state.message = "设备随机源暂不可用";
                state.screen = DASHBOARD;
            }
            break;
        case THEMES:
            if (state.pending == SAVE_THEME || state.theme_job == SAVE_THEME) break;
            if (action == BACK) {
                if (state.theme_job == FETCH_THEME) state.theme_job = NONE;
                if (state.pending == FETCH_THEME) ++state.target;
                state.screen = DASHBOARD;
            } else if (state.theme_job == NONE && state.pending != FETCH_THEME) {
                if (action == PREVIEW_THEME && state.offered_theme) {
                    state.screen = THEME_PREVIEW;
                    state.theme_message = 0;
                } else if (action == GET_THEME) {
                    if (state.have_address) {
                        state.theme_job = FETCH_THEME;
                        state.theme_message = "正在获取主题…";
                    } else state.theme_message = "请先连接电脑上位机。";
                } else if (action == RESET_THEME && state.custom_theme) {
                    state.reset_theme = true;
                    state.theme_job = SAVE_THEME;
                    state.theme_message = "正在恢复内置主题…";
                }
            }
            break;
        case THEME_PREVIEW:
            if (state.pending == SAVE_THEME || state.theme_job == SAVE_THEME) break;
            if (action == BACK) {
                state.screen = THEMES;
                state.theme_message = 0;
            } else if (action == INSTALL_THEME) {
                state.reset_theme = false;
                state.theme_job = SAVE_THEME;
                state.theme_message = "正在保存…";
            }
            break;
    }
}

static void clock_pair(char *out, int value) {
    out[0] = '0' + value / 10;
    out[1] = '0' + value % 10;
}

static void refresh_clock(const dawan_app_host *host, const dawan_system *system) {
    if (!(system->flags & 1u)) {
        if (state.time[0]) state.dirty = true;
        state.date[0] = state.time[0] = 0;
        return;
    }
    if (!host->runtime || host->runtime->size < sizeof(dawan_runtime) || !host->runtime->calendar) return;
    if (state.time[0] && state.clock_epoch == system->utc_seconds) return;
    dawan_calendar date;
    if (host->runtime->calendar(host->ui.base.instance, system->utc_seconds, &date) != DAWAN_OK ||
        date.year < 100 || date.year > 299 || date.month < 0 || date.month > 11 ||
        date.day < 1 || date.day > 31 || date.hour < 0 || date.hour > 23 ||
        date.minute < 0 || date.minute > 59 || date.second < 0 || date.second > 60) return;
    number(state.date, (unsigned)date.year + 1900);
    state.date[4] = state.date[7] = '-';
    clock_pair(state.date + 5, date.month + 1);
    clock_pair(state.date + 8, date.day);
    state.time[2] = state.time[5] = ':';
    clock_pair(state.time, date.hour);
    clock_pair(state.time + 3, date.minute);
    clock_pair(state.time + 6, date.second);
    state.clock_epoch = system->utc_seconds;
    state.dirty = true;
}

int32_t dawan_main(const dawan_host *base, uint32_t event) {
    if (!base || base->abi != DAWAN_ABI_STREAMING || base->size != sizeof(dawan_streaming_host) || !base->instance)
        return DAWAN_VERSION;
    const dawan_streaming_host *streaming = (const dawan_streaming_host *)base;
    const dawan_app_host *host = &streaming->base.base;
    if (!streaming->random_bytes || !host->jobs || host->jobs->size < sizeof(dawan_jobs) ||
        !host->jobs->submit || !host->jobs->poll || !host->jobs->cancel || !host->jobs->drained ||
        !host->retire_ui || !host->ui.present || !host->ui.poll || !host->system) return DAWAN_VERSION;
    if (event == DAWAN_STOP) {
        if (!state.stopping) {
            int result = host->jobs->cancel(base->instance, base->instance);
            if (result != DAWAN_OK) return result;
            state.stopping = true;
        }
        if (host->jobs->drained(base->instance, base->instance) != DAWAN_OK) return DAWAN_BUSY;
        clear_key();
        pc_wipe(state.response, sizeof(state.response));
        return host->retire_ui(base->instance);
    }
    if (event == DAWAN_START) {
        pc_wipe(&state, sizeof(state));
        state.canvas_width = 800;
        const dawan_extensions *extensions = streaming->base.extensions;
        if (extensions && extensions->size >= sizeof(dawan_extensions) && extensions->configure_scene &&
            extensions->configure_scene(base->instance, 480, 480) == DAWAN_OK) {
            state.canvas_width = 480;
            report(base, "pc-status: square canvas accepted");
        } else report(base, "pc-status: square design fitted to landscape SDK canvas");
        state.dirty = true;
        report(base, "pc-status: device version 2.0.2");
        return DAWAN_OK;
    }
    if (event != DAWAN_TICK || state.stopping) return DAWAN_OK;
    dawan_system system = {.size = sizeof(system)};
    if (host->system(base->instance, &system) != DAWAN_OK) return DAWAN_OK;
    uint64_t now = system.monotonic_ms;
    if (state.screen == DASHBOARD || state.screen == THEME_PREVIEW) refresh_clock(host, &system);
    if (state.screen == CONFIRM && now >= state.confirm_until) {
        state.screen = DASHBOARD;
        state.dirty = true;
    }
    dawan_ui_event click = {.size = sizeof(click)};
    for (unsigned n = 0; n < 8 && host->ui.poll(base->instance, &click) == DAWAN_OK; ++n) {
        bool theme_navigation = click.action == OPEN_THEMES || click.action == GET_THEME ||
                                click.action == PREVIEW_THEME;
        bool same_page = state.revision > 1 && click.revision == state.revision - 1 &&
                         state.previous_screen == state.presented_screen && state.screen == state.presented_screen;
        if (click.revision != state.revision && !(theme_navigation && same_page)) continue;
        if (state.screen == DASHBOARD && click.action == EXIT) return DAWAN_CANCELLED;
        enum page previous = state.screen;
        touch(streaming, click.action, now);
        if (state.screen != previous) break;
    }
    if (state.active) receive(host, now);
    if (!(system.flags & 2u)) {
        if (state.fresh) state.dirty = true;
        state.fresh = false;
        if (state.theme_job == FETCH_THEME) {
            state.theme_job = NONE;
            state.theme_message = "大万未联网，请连接网络后重试。";
            state.dirty = true;
        }
        if (state.challenge || state.send_shutdown) {
            state.challenge = state.send_shutdown = false;
            state.message = "大万已断网，关机请求未发送";
            state.dirty = true;
        }
    }
    if (!state.active) {
        if (!state.ready && !state.environment_error && now >= state.due) submit(host, ENVIRONMENT, now);
        else if (state.load) submit(host, LOAD_ADDRESS, now);
        else if (state.save) submit(host, SAVE_ADDRESS, now);
        else if (state.theme_job == LOAD_THEME || state.theme_job == SAVE_THEME)
            submit(host, state.theme_job, now);
        else if (system.flags & 2u) {
            if (state.send_shutdown) submit(host, SHUTDOWN, now);
            else if (state.challenge) submit(host, CHALLENGE, now);
            else if (state.theme_job == FETCH_THEME) submit(host, FETCH_THEME, now);
            else if (state.ready && (state.screen == DASHBOARD || state.screen == DISCOVERY)) {
                if (state.screen == DASHBOARD && !state.scanning &&
                    (!state.have_address || state.status_failures >= 2) && now >= state.discover_due)
                    search_known_network(true, now);
                if (state.scanning) submit(host, PROBE, now);
                else if (state.screen == DASHBOARD && state.have_address) {
                    if (now >= state.due) submit(host, STATUS, now);
                    else if (state.fresh && now >= state.theme_due) submit(host, CHECK_THEME, now);
                }
            } else if (state.screen == THEME_PREVIEW && state.have_address && now >= state.due)
                submit(host, STATUS, now);
        }
    }
    return state.dirty ? present(host) : DAWAN_OK;
}
