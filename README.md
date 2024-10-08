# JianziNote

## 简介

减字自动生成程序，使用单个减字通过运算符组成新的减字。

基本理念：
- 使用自定义运算符进行减字组合；同时提供缩写以简化常用减字的输入；
- 减字名称使用utf8编码的中文字符，无需额外记忆符号；
- 根据汉字框架结构特点均匀横画间隔；
- 配合不同风格的Styler可实现不同字形输出；
- 输出矢量描述，可开发不同输出端提供不同平台的功能包或插件，如LaTeX、Lilypond等；

## 使用

###  减字风格

`JianziStyler.hpp`定义了基本的减字风格所需的信息；`StylerFromDb.hpp`则为使用SQLite储存减字信息的Styler实现，即减字库。JianziNote项目内部统一使用以左下角为$(0,0)$、右上角为$(1,1)$的归一化坐标。

`bin`目录内提供基于“思源宋体”开发的减字库，可直接使用。出于外观风格统一、美观等各方面考虑，并未开放该减字库的开发工具。目前支持的减字及预览如下：

TODO

### 运算符

运算符优先级列表及功能描述如下：

TODO

### 输出描述

`JianziStyler.hpp`定义了输出的通用矢量描述方式，即以`MoveTo`、`LineTo`、`QuadTo`、`CubicTo`和`Close`为关键字的列表，以及对应的点坐标列表。此描述方式可轻松对接到Cairo、Skia、LaTeX、Lilypond等不同的绘制程序当中。

具体实现示例见Jianzi2LaTeX和Jianzi2Lilypond等项目。

### 库使用

TODO

## 编译

### 项目依赖

- [tiny-utf8](https://github.com/DuffsDevice/tiny-utf8)
- [magic_enum](https://github.com/Neargye/magic_enum)
- [SQlite3](https://www.sqlite.org/index.html)
- [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp)

### 兼容性

编译器要求C++17，以使用magic_enum的特性。

本项目在MacOS Ventura+Clang 12和Windows 11+MSVC 2019下编译通过。