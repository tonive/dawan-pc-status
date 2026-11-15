# 主题格式

文件使用 UTF-8 JSON 和 `.wmtheme.json` 后缀，上限 8 KiB。格式 2 支持自绘图标与主卡布局，见 [星舰控制台](../assets/starship-console.wmtheme.json)。格式 1 的示例见 [极光终端](../assets/aurora-terminal.wmtheme.json)。

| 字段 | 支持的内容 |
| --- | --- |
| `format` | `dawan-pc-theme` |
| `version` | 新主题使用整数 `2`（上位机和设备均需 2.0.2 或更新），已有格式 `1` 继续支持 |
| `name`、`title` | 主题名称和首页标题。各 1–14 个字符，最多 36 UTF-8 字节，使用可打印 ASCII 或 U+3000–U+9FFF |
| `style` | `pixel` 像素景物，`panel` 简洁面板 |
| `layout` | `grid` 两行两列，`rows` 四条横排，`hero` 第一项为主卡、其余三项横排（格式 2） |
| `meter` | `segments` 分段进度条，`solid` 连续进度条 |
| `palette` | `background` 背景、`surface` 卡片底色、`ink` 正文、`muted` 次要文字、`outline` 描边 |
| `cards` | 四张卡片按屏幕顺序排列。每项含 `metric`、`icon`、`accent` 和 `banner` |

四种指标 `cpu`、`gpu`、`memory`、`disk` 各出现一次。颜色都使用 `#RRGGBB`，且正文、次要文字及高亮数值与对应底色的对比度须达到 4.5:1。上传前用 Skill 附带的验证脚本检查。

## 卡片图标

格式 2 的 `icon` 可填对象。其 `colors` 包含三种颜色，由 `1`、`2`、`3` 依次引用。其 `pixels` 包含八行，每行八个字符，`.` 表示透明。每行的连续非透明同色像素合并为一段，整张图标限 1–32 段。可以先用剪影建立轮廓，再用一个颜色补明显特征。

```json
{
  "colors": ["#4974A0", "#8AEAFD", "#EEF6FF"],
  "pixels": [
    "...11...", "..1221..", ".122221.", "11233211",
    "11233211", ".122221.", "..1221..", "...11..."
  ]
}
```

格式 1 和格式 2 都可使用字符串 `mushroom`、`flower`、`pipe`、`coin`、`none`。
