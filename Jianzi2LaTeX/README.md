# Jianzi2LaTeX

## 简介

将JianziNote库的`PathData`转化成TikZ格式的输出，以便在LaTeX当中使用。

## 使用

需配合`jianzinote.sty`使用（见example目录）。`jianzinote.sty`内定义了以下命令：
TODO

在LaTeX文档中添加`\jzs{}`等命令后，执行：
```
jianzi2latex <减字库文件> <tex文件>
```
程序将自动匹配文档中的`\jzs{}`等命令，在当前目录下生成`jianzilut.sty`文件。
将`jianzinote.sty`、`jianzilut.sty`放在LaTeX文档同一目录下，正常编译文档即可。

TODO：`jianzilut.sty`仅根据当前文档生成，处理不同文件后将被覆盖。暂未实现多文档处理功能。

## 示例
`example`目录下是两个例子：`example.tex`是普通横排的例子，`exapmle_classic.tex`是线装竖排的例子。
线装竖排模板`zhvt-classic.cls`来自chianjin的[zhvt-classic](https://github.com/chianjin/zhvt-classic)。