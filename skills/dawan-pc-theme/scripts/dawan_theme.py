"""Validate a DaWan PC Monitor theme and encode its bounded device format."""

import argparse
from itertools import groupby
import json
from pathlib import Path
import re
import secrets


COLOR = re.compile(r"#[0-9A-Fa-f]{6}")
METRICS = ("cpu", "gpu", "memory", "disk")
ICONS = ("mushroom", "flower", "pipe", "coin", "none")
LAYOUTS = ("grid", "rows", "hero")
STYLES = ("pixel", "panel")
METERS = ("segments", "solid")
PALETTE = ("background", "surface", "ink", "muted", "outline")
MAX_FILE = 8192
MAX_WIRE = 512
SPRITE_ROW = re.compile(r"[.123]{8}")
MAX_SPRITE_RUNS = 32


def fields(value, required, location):
    if not isinstance(value, dict) or set(value) != set(required):
        raise ValueError(f"{location} 应包含且只包含 {', '.join(required)}")


def unique(pairs):
    obj = {}
    for key, value in pairs:
        if key in obj:
            raise ValueError(f"重复字段 {key}")
        obj[key] = value
    return obj


def name(value, field):
    if (not isinstance(value, str) or not value.strip(" \u3000") or len(value) > 14
            or any(not (32 <= ord(c) <= 126 or 0x3000 <= ord(c) <= 0x9FFF) for c in value)
            or len(value.encode("utf-8")) > 36):
        raise ValueError(f"{field} 须为 1–14 个中日韩字符或可打印 ASCII，最多 36 UTF-8 字节")
    return value


def color(value, location):
    if not isinstance(value, str) or not COLOR.fullmatch(value):
        raise ValueError(f"{location} 须为 #RRGGBB")
    return value[1:].lower()


def luminance(rgb):
    channels = [int(rgb[i:i+2], 16) / 255 for i in range(0, 6, 2)]
    linear = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in channels]
    return sum(c * weight for c, weight in zip(linear, (0.2126, 0.7152, 0.0722)))


def contrast(left, right, location):
    dark, light = sorted((luminance(left), luminance(right)))
    if (light + 0.05) / (dark + 0.05) < 4.5:
        raise ValueError(f"{location} 的文字对比度须达到 4.5:1")


def encode_icon(icon, version):
    if icon in ICONS:
        return str(ICONS.index(icon)), ""
    if version != 2:
        raise ValueError("格式 1 须使用预置图标名称")
    fields(icon, ("colors", "pixels"), "自定义图标")
    colors, pixels = icon["colors"], icon["pixels"]
    if not isinstance(colors, list) or len(colors) != 3:
        raise ValueError("自定义图标须提供三种颜色")
    palette = "".join(color(value, "图标颜色") for value in colors)
    if (not isinstance(pixels, list) or len(pixels) != 8
            or any(not isinstance(row, str) or not SPRITE_ROW.fullmatch(row) for row in pixels)):
        raise ValueError("自定义图标须有八行八列，使用 .、1、2、3")
    runs = sum(pixel != "." for row in pixels for pixel, _ in groupby(row))
    if not 1 <= runs <= MAX_SPRITE_RUNS:
        raise ValueError(f"图标每行连续色块合计须为 1–{MAX_SPRITE_RUNS} 段")
    return "5", palette + "".join(pixels)


def encode_theme(value):
    fields(value, ("format", "version", "name", "title", "style", "layout", "meter", "palette", "cards"), "主题")
    version = value["version"]
    if value["format"] != "dawan-pc-theme" or type(version) is not int or version not in (1, 2):
        raise ValueError("需要 dawan-pc-theme 格式版本 1 或 2")
    theme_name = name(value["name"], "name")
    title = name(value["title"], "title")
    options = ""
    layouts = LAYOUTS[:2] if version == 1 else LAYOUTS
    for key, choices in (("style", STYLES), ("layout", layouts), ("meter", METERS)):
        if value[key] not in choices:
            raise ValueError(f"{key} 只支持 {', '.join(choices)}")
        options += str(choices.index(value[key]))
    fields(value["palette"], PALETTE, "palette")
    palette = {key: color(value["palette"][key], f"palette.{key}") for key in PALETTE}
    contrast(palette["ink"], palette["surface"], "正文")
    contrast(palette["muted"], palette["surface"], "次要文字")
    cards = value["cards"]
    if not isinstance(cards, list) or len(cards) != len(METRICS):
        raise ValueError("cards 须有四项")
    rows, seen = [], set()
    for card in cards:
        fields(card, ("metric", "icon", "accent", "banner"), "卡片")
        metric, icon = card["metric"], card["icon"]
        if metric not in METRICS or metric in seen:
            raise ValueError("每项指标须出现一次")
        seen.add(metric)
        icon_id, sprite = encode_icon(icon, version)
        accent, banner = color(card["accent"], "accent"), color(card["banner"], "banner")
        contrast(accent, palette["surface"], f"{metric} 数值")
        contrast(palette["ink"], banner, f"{metric} 标题")
        rows.append(f"{METRICS.index(metric)}{icon_id}{accent}{banner}{sprite}")
    fields_out = (f"WMT{version}", theme_name, title, options, "".join(palette.values()), *rows)
    wire = ("\n".join(fields_out) + "\n").encode("utf-8")
    if len(wire) > MAX_WIRE:
        raise ValueError("主题超出设备长度上限")
    return wire


def load_theme(path):
    try:
        with Path(path).open("rb") as file:
            data = file.read(MAX_FILE + 1)
        if len(data) > MAX_FILE:
            raise ValueError("主题文件最大 8 KiB")
        value = json.loads(data.decode("utf-8-sig"), object_pairs_hook=unique)
        return value, encode_theme(value)
    except (UnicodeDecodeError, RecursionError) as error:
        raise ValueError("主题需要有效的 UTF-8 JSON") from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("theme", nargs="?", type=Path)
    parser.add_argument("--suggest-layout", action="store_true", help="Pick a layout for an open-ended design")
    parser.add_argument("--exclude-layout", choices=LAYOUTS, action="append", default=[])
    args = parser.parse_args()
    if args.suggest_layout:
        choices = tuple(layout for layout in LAYOUTS if layout not in args.exclude_layout)
        if args.theme or not choices:
            parser.error("--suggest-layout requires available layouts and no theme file")
        print(secrets.choice(choices))
        return
    if not args.theme or args.exclude_layout:
        parser.error("Provide a theme file, or use --suggest-layout")
    try:
        value, _ = load_theme(args.theme)
    except (OSError, ValueError) as error:
        parser.exit(1, f"主题校验失败：{error}\n")
    print(f"主题可用于大万状态监控屏 2.0：{value['name']}")


if __name__ == "__main__":
    main()
