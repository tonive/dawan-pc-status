# 大万状态监控屏 2.0 自制主题

## 安装主题 Skill

可以使用仓库的 [完整 Skill 文件夹](../skills/dawan-pc-theme/) 或从 [2.0.2 发行页](https://github.com/tonive/dawan-pc-status/releases/tag/v2.0.2) 下载 `dawan-pc-theme.zip`。包按 [Agent Skills 规范](https://agentskills.io/specification) 组织，各平台使用自己的导入入口。

```text
请安装大万监控屏的 dawan-pc-theme Skill。
来源：https://github.com/tonive/dawan-pc-status/tree/v2.0.2/skills/dawan-pc-theme
请获取完整文件夹，保留 SKILL.md、scripts、references 和 assets，并按你所在平台支持的方式安装。如果需要我手动导入，请告诉我导入入口。
```

- **支持导入 ZIP 或文件夹的平台。** 在平台的 Skill 管理界面上传 `dawan-pc-theme.zip`，或选择解压后的完整文件夹。
- **支持从对话安装的平台。** 发上面的安装提示词。Codex 也可以在对话里输入 `$skill-installer install https://github.com/tonive/dawan-pc-status/tree/v2.0.2/skills/dawan-pc-theme`。
- **仅支持对话附件的平台。** 将包里的 `SKILL.md`、`references/theme-format.md` 和 `assets/starship-console.wmtheme.json` 附到对话，让它依照格式和示例生成 `.wmtheme.json`。Windows 上位机在导入时会校验内容。

装好 Skill 后，可要求智能体设计主题。例如：

> 请使用 dawan-pc-theme 做一个太空主题，生成可上传的 .wmtheme.json。

智能体会按题材设计像素图标。布局未指定时，可从四宫格、四条横排、主卡加三条横排中随机选择。也可以指定必须保留的布局、指标顺序、配色或进度条。屏幕使用大万自带字体。

## 上传和安装

1. 将大万应用和 Windows 上位机升级到 2.0.2，以使用自绘图标和主卡布局。此前保存的格式 1 主题可以继续使用。
2. 点击上位机的「上传主题」，选择 Agent 生成的 `.wmtheme.json`。可以先用 [星舰控制台示例](../skills/dawan-pc-theme/assets/starship-console.wmtheme.json) 或 [漫威四宫格示例](https://github.com/tonive/dawan-pc-status/releases/download/v2.0.2/caramel-arcade.wmtheme.json) 验证流程。
3. 确认大万监控屏连接了这台电脑。设备会自动检查主题，首页出现「新主题」后点进去选择「预览主题」，也可在主题页手动获取。预览使用当前监控数据，并显示主题名称。
4. 点「安装」保存，或点「取消」离开预览。安装后的主题保存在大万当前开发实例中，重新打开应用会自动恢复。想切回像素关卡时点「主题 → 恢复内置」。

Windows 上位机窗口显示「已上传」表示文件已在局域网中可获取。设备上的「安装」完成后才会替换当前主题。上位机关闭后，已经安装到大万的主题仍可显示。主题上传使用已连接的私有局域网，不要求开启关机权限。

## 常见问题

| 现象 | 操作 |
| --- | --- |
| 文件被上位机拒绝 | 将报错和原主题文件交给 Agent，使用 Skill 自带的脚本修正并复查。 |
| 大万提示上位机未提供主题 | 检查上位机是否为 2.0、是否已启动和上传，以及大万连接的 IP 是否正确。 |
| 预览后决定换一套 | 点「取消」返回。上位机重新上传后，在大万再次点「获取主题」。 |
| 设备保存失败 | 当前已安装的主题继续生效。可检查设备存储状态后重新安装。 |
