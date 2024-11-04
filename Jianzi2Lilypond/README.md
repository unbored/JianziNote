# Jianzi2Lilypond

## 简介

将JianziNote库的`PathData`转化成lilypond path格式的输出，以便在Lilypond当中使用。

## 使用

定义了以下命令：
TODO

在Lilypond文档中添加`\"jz:"`等命令后，执行：
```
jianzi2lilypond <减字库文件> <ly文件>
```
程序将自动匹配文档中的`\"jz:"`等命令，在当前目录下生成`jianzidef.ly`文件。
将`jianzidef.ly`放在Lilypond文档同一目录下，文档当中加入一行`\include "jianzidef.ly"`，正常编译文档即可。

## 示例
`example`目录下的`example.ly`是五线谱的样例模板，可以参照该模板完成乐曲。
`example_jianpu.txt`是简谱的示例，需要配合`jianpu-ly.py`脚本使用，参见我参与的另一个项目[jianpu-ly](https://github.com/unbored/jianpu-ly/tree/Guqin)，Guqin分支。