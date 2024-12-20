# JianziNote

## 简介

古琴谱减字自动生成程序，使用基本减字通过组合得到新的减字。

基本理念：

- 使用自定义运算符进行减字组合；同时提供自然表述以简化常用减字的输入；
- 减字名称使用utf8编码的中文字符，无需额外记忆符号；
- 根据汉字框架结构特点均匀横画间隔，使减字间架比例自然；
- 自动计算笔画粗细权重，使笔画粗细自然；
- 配合不同的Styler可实现不同风格的字形输出；
- 输出矢量描述，可开发不同输出端提供不同平台的功能包或插件，如LaTeX、Lilypond等；

## 使用

### 减字库

`Jianzi.hpp`使用SQLite储存已定义的减字名称和框架信息，即为减字库。只有框架信息的减字库无法直接使用，需要配合减字风格库。`SkeletonStyler.hpp`为仅将关键控制点进行连线的风格库。

JianziNote项目内部统一使用以左上角为(0,0)、右下角为(1,1)的归一化坐标。

### 减字风格

`JianziStyler.hpp`定义了基本的减字风格所需的信息，`StylerFromDb.hpp`则为使用SQLite储存减字信息的Styler实现，即减字风格库。

`bin`目录内提供基于“思源宋体”旧字形风格开发的减字库，可直接使用。出于外观风格统一等方面考虑，并未开放该减字库的开发工具，有添加减字的需求可提issue。目前支持的基础减字及预览如下：

#### 数字

| 一                                              | 二                                              | 三                                              | 四                                              | 五                                              | 六                                              | 七                                              | 八                                              | 九                                              | 十                                              | 外                                              | 半                                              |
| ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- |
| <img src="images/一.png" alt="一" width="30" /> | <img src="images/二.png" alt="二" width="30" /> | <img src="images/三.png" alt="三" width="30" /> | <img src="images/四.png" alt="四" width="30" /> | <img src="images/五.png" alt="五" width="30" /> | <img src="images/六.png" alt="六" width="30" /> | <img src="images/七.png" alt="七" width="30" /> | <img src="images/八.png" alt="八" width="30" /> | <img src="images/九.png" alt="九" width="30" /> | <img src="images/十.png" alt="十" width="30" /> | <img src="images/外.png" alt="外" width="30" /> | <img src="images/半.png" alt="半" width="30" /> |

#### 左手

| 大                                              | 食                                              | 中                                              | 名                                              | 跪                                              | 散                                              |
| ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- |
| <img src="images/大.png" alt="大" width="30" /> | <img src="images/食.png" alt="食" width="30" /> | <img src="images/中.png" alt="中" width="30" /> | <img src="images/名.png" alt="名" width="30" /> | <img src="images/跪.png" alt="跪" width="30" /> | <img src="images/散.png" alt="散" width="30" /> |

#### 单弦指法

| 擘                                              | 托                                              | 抹                                              | 挑                                              | 勾                                              | 剔                                              | 打                                              | 摘                                              |
| ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- |
| <img src="images/擘.png" alt="擘" width="30" /> | <img src="images/托.png" alt="托" width="30" /> | <img src="images/抹.png" alt="抹" width="30" /> | <img src="images/挑.png" alt="挑" width="30" /> | <img src="images/勾.png" alt="勾" width="30" /> | <img src="images/剔.png" alt="剔" width="30" /> | <img src="images/打.png" alt="打" width="30" /> | <img src="images/摘.png" alt="摘" width="30" /> |

| 抹挑                                               | 勾剔                                               | 打摘                                               | 按                                              | 轮                                              | 涓                                              | 叠                                              | 锁                                              | 罨                                              |
| ------------------------------------------------ | ------------------------------------------------ | ------------------------------------------------ | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- |
| <img src="images/抹挑.png" alt="抹挑" width="30" /> | <img src="images/勾剔.png" alt="勾剔" width="30" /> | <img src="images/打摘.png" alt="打摘" width="30" /> | <img src="images/按.png" alt="按" width="30" /> | <img src="images/轮.png" alt="轮" width="30" /> | <img src="images/涓.png" alt="涓" width="30" /> | <img src="images/叠.png" alt="叠" width="30" /> | <img src="images/锁.png" alt="锁" width="30" /> | <img src="images/罨.png" alt="罨" width="30" /> |

#### 多弦指法

| 拨                                              | 剌                                              | 拨剌                                               | 撮                                              | 弹                                              | 滚                                              | 拂                                              | 至                                              | 历                                              |
| ---------------------------------------------- | ---------------------------------------------- | ------------------------------------------------ | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- |
| <img src="images/拨.png" alt="拨" width="30" /> | <img src="images/剌.png" alt="剌" width="30" /> | <img src="images/拨剌.png" alt="拨剌" width="30" /> | <img src="images/撮.png" alt="撮" width="30" /> | <img src="images/弹.png" alt="弹" width="30" /> | <img src="images/滚.png" alt="滚" width="30" /> | <img src="images/拂.png" alt="拂" width="30" /> | <img src="images/至.png" alt="至" width="30" /> | <img src="images/历.png" alt="历" width="30" /> |

#### 修饰

| 绰                                              | 注                                              | 泛                                              | 反                                              | 急                                              | 就                                              | 掐                                              |
| ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- |
| <img src="images/绰.png" alt="绰" width="30" /> | <img src="images/注.png" alt="注" width="30" /> | <img src="images/泛.png" alt="泛" width="30" /> | <img src="images/反.png" alt="反" width="30" /> | <img src="images/急.png" alt="急" width="30" /> | <img src="images/就.png" alt="就" width="30" /> | <img src="images/掐.png" alt="掐" width="30" /> |

#### 其他

| 背                                              | 不动                                               | 次                                              | 从                                              | 打圆                                               | 带                                              | 荡                                              | 逗                                              | 度                                              | 放                                              |
| ---------------------------------------------- | ------------------------------------------------ | ---------------------------------------------- | ---------------------------------------------- | ------------------------------------------------ | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- |
| <img src="images/北.png" alt="北" width="30" /> | <img src="images/不动.png" alt="不动" width="30" /> | <img src="images/次.png" alt="次" width="30" /> | <img src="images/从.png" alt="从" width="30" /> | <img src="images/打圆.png" alt="打圆" width="30" /> | <img src="images/带.png" alt="带" width="30" /> | <img src="images/荡.png" alt="荡" width="30" /> | <img src="images/逗.png" alt="逗" width="30" /> | <img src="images/度.png" alt="度" width="30" /> | <img src="images/放.png" alt="放" width="30" /> |

| 分开                                               | 伏                                              | 复                                              | 合                                              | 浒                                              | 缓                                              | 唤                                              | 节                                              | 进                                              | 连                                              |
| ------------------------------------------------ | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- |
| <img src="images/分开.png" alt="分开" width="30" /> | <img src="images/伏.png" alt="伏" width="30" /> | <img src="images/复.png" alt="复" width="30" /> | <img src="images/合.png" alt="合" width="30" /> | <img src="images/浒.png" alt="浒" width="30" /> | <img src="images/缓.png" alt="缓" width="30" /> | <img src="images/唤.png" alt="唤" width="30" /> | <img src="images/节.png" alt="节" width="30" /> | <img src="images/进.png" alt="进" width="30" /> | <img src="images/连.png" alt="连" width="30" /> |

| 落指                                               | 慢                                              | 猱                                              | 撇                                              | 起                                              | 轻                                              | 曲                                              | 如                                              | 入                                              | 软                                              |
| ------------------------------------------------ | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- |
| <img src="images/落指.png" alt="落指" width="30" /> | <img src="images/慢.png" alt="慢" width="30" /> | <img src="images/猱.png" alt="猱" width="30" /> | <img src="images/撇.png" alt="撇" width="30" /> | <img src="images/起.png" alt="起" width="30" /> | <img src="images/轻.png" alt="轻" width="30" /> | <img src="images/曲.png" alt="曲" width="30" /> | <img src="images/如.png" alt="如" width="30" /> | <img src="images/入.png" alt="入" width="30" /> | <img src="images/软.png" alt="软" width="30" /> |

| 上                                              | 少                                              | 少息                                               | 声                                              | 双                                              | 索铃                                               | 淌                                              | 同                                              | 头                                              | 推出                                               |
| ---------------------------------------------- | ---------------------------------------------- | ------------------------------------------------ | ---------------------------------------------- | ---------------------------------------------- | ------------------------------------------------ | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ------------------------------------------------ |
| <img src="images/上.png" alt="上" width="30" /> | <img src="images/少.png" alt="少" width="30" /> | <img src="images/少息.png" alt="少息" width="30" /> | <img src="images/声.png" alt="声" width="30" /> | <img src="images/双.png" alt="双" width="30" /> | <img src="images/索铃.png" alt="索铃" width="30" /> | <img src="images/淌.png" alt="淌" width="30" /> | <img src="images/同.png" alt="同" width="30" /> | <img src="images/头.png" alt="头" width="30" /> | <img src="images/推出.png" alt="推出" width="30" /> |

| 退                                              | 往来                                               | 细                                              | 下                                              | 虚                                              | 许                                              | 音                                              | 吟                                              | 应合                                               | 游                                              |
| ---------------------------------------------- | ------------------------------------------------ | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ------------------------------------------------ | ---------------------------------------------- |
| <img src="images/退.png" alt="退" width="30" /> | <img src="images/往来.png" alt="往来" width="30" /> | <img src="images/细.png" alt="细" width="30" /> | <img src="images/下.png" alt="下" width="30" /> | <img src="images/虚.png" alt="虚" width="30" /> | <img src="images/许.png" alt="许" width="30" /> | <img src="images/音.png" alt="音" width="30" /> | <img src="images/吟.png" alt="吟" width="30" /> | <img src="images/应合.png" alt="应合" width="30" /> | <img src="images/游.png" alt="游" width="30" /> |

| 又                                              | 圆搂                                               | 再                                              | 长                                              | 止                                              | 指                                              | 终                                              | 抓                                              | 撞                                              | 作                                              |
| ---------------------------------------------- | ------------------------------------------------ | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- | ---------------------------------------------- |
| <img src="images/又.png" alt="又" width="30" /> | <img src="images/圆搂.png" alt="圆搂" width="30" /> | <img src="images/再.png" alt="再" width="30" /> | <img src="images/长.png" alt="长" width="30" /> | <img src="images/止.png" alt="止" width="30" /> | <img src="images/指.png" alt="指" width="30" /> | <img src="images/终.png" alt="终" width="30" /> | <img src="images/抓.png" alt="抓" width="30" /> | <img src="images/撞.png" alt="撞" width="30" /> | <img src="images/作.png" alt="作" width="30" /> |

| 左反复                                                | 右反复                                                |
| -------------------------------------------------- | -------------------------------------------------- |
| <img src="images/左反复.png" alt="左反复" width="30" /> | <img src="images/右反复.png" alt="右反复" width="30" /> |

其中也支持一些常用的别称，列表如下：

| 减字  | 别称  |
| --- | --- |
| 擘   | 劈   |
| 掐   | 搯   |
| 剔   | 踢   |
| 拨   | 泼   |

在没有特殊规则的情况下，减字将以上下堆叠的方式组合，因此如“泛起”、“泛止”、“掐起”、“双弹”等减字无需特别定义即可输入。

减字库也提供了一些常用但需要特别运算规则的固定减字组合，以减少输入复杂度（见“算式”一节），列表如下：

| 组合    | 表达式             |
| ----- | --------------- |
| 掐起    | `(掐/起)`         |
| 掐撮三声  | `(掐/撮*(声\|三))`  |
| 掐拨剌三声 | `(掐/拨剌*(声\|三))` |
| 同声    | `(同*声)`         |
| 同起    | `(同*起)`         |
| 虚罨    | `(虚*罨)`         |
| 游吟    | `(游*吟)`         |
| 游猱    | `(游*猱)`         |
| 放合    | `(放&合)`         |

### 基本用法——文字表述

常用减字可通过文字表述直接得到。文字表述仅使用减字库当中有的减字名称，按照从左到右、从上到下的方式排列，不加额外字符。示例如下：

| 文字表述      | 结果                                                         |
| --------- | ---------------------------------------------------------- |
| `大九挑七`    | <img src="images/大九挑七.png" alt="大九挑七" width="60" />       |
| `名十八注抹五`  | <img src="images/名十八注抹五.png" alt="名十八注抹五" width="60" />   |
| `中九绰勾剔一`  | <img src="images/中九绰勾踢一.png" alt="中九绰勾踢一" width="60" />   |
| `拨大七六七散六` | <img src="images/拨大七六七散六.png" alt="拨大七六七散六" width="60" /> |
| `撮名十八三散五` | <img src="images/撮名十八三散五.png" alt="撮名十八三散五" width="60" /> |
| `泛撮六一`    | <img src="images/泛撮六一.png" alt="泛撮六一" width="60" />       |
| `拨剌一`     | <img src="images/拨剌一.png" alt="拨剌一" width="60" />         |

文字表述使用贪婪匹配，在算法库内部按照一定规律转化成为算式，见“算式”一节。文字表述模式可基本满足明末以后的琴谱减字输入需求。更早的琴谱由于记谱方式尚未足够统一，由于减字组合位置不同而产生的差异可通过算式模式解决，而基本减字的写法不同产生的差异只能通过切换减字库解决。

### 高阶用法——算式

使用基本减字加运算符形成算式，可以任意组合减字，实现复杂的效果。在最外层使用一对圆括号来指示使用算式；而算式当中也可以通过一对单引号来使用字符串，以简化部分输入。示例如下：

| 表达式                         | 结果                                                             |
| --------------------------- | -------------------------------------------------------------- |
| `(大&九^挑*七)`                 | <img src="images/大九挑七.png" alt="大九挑七" width="60" />           |
| `((名&(十/一))/(中&食)/拂/滚/一/二)` | <img src="images/名十一中食拂滚一二.png" alt="名十一中食拂滚一二" width="60" /> |
| `(食<(中&名)^打*五)`             | <img src="images/食中名打五.png" alt="食中名打五" width="60" />         |
| `(右反复*'大九挑七')`              | <img src="images/右反复大九挑七.png" alt="右反复大九挑七" width="60" />     |

同级运算符按从左到右进行运算。运算符优先级列表及功能描述如下：

| 优先级 | 运算符  | 功能描述                                                             |
| --- | ---- | ---------------------------------------------------------------- |
| 0   | `()` | 括号，用于调整运算顺序，具有最高优先级                                              |
| 0   | `'`  | 单引号，用于从算式模式进入文字表述模式，简化输入                                         |
| 4   | `*`  | 将后一个减字置于前一个减字内；如果前一个减字并无内置空间，则效果同`/`运算符。                         |
| 3   | `&`  | 将两个减字左右均分。                                                       |
| 3   | `<`  | 将两个减字按0.3:0.7比例左右缩放。基本用于指法“注”。                                   |
| 3   | `/`  | 将两个减字根据各自所占的纵向间隔比例缩放。                                            |
| 2   | `\|` | 用于将两个减字左右均分，且中间留出一竖的空间。用在中间带竖的双弦指法的下半部分，如“撮”、“剌”等减字。             |
| 1   | `^`  | 如果前一个减字纵向间隔比例小于后一个，则效果同`/`；否则，将两个减字上下均分。用于调整减字重心，一般在减字的左右手分界处使用。 |

### 输出描述

`JianziStyler.hpp`定义了输出的通用矢量描述方式，即以`MoveTo`、`LineTo`、`QuadTo`、`CubicTo`和`Close`为关键字的列表，以及对应的点坐标列表。此描述方式方便对接到Cairo、Skia、LaTeX、Lilypond等不同的应用当中。

具体实现示例见Jianzi2LaTeX和Jianzi2Lilypond等子项目。

### 库使用

```cpp
#include <Jianzi.hpp>
#include <StylerFromDb.hpp>
#include <string>

// 假定的某个绘制矢量路径的renderer
#include <SomeRenderer.hpp>

using namespace qin;

// 假定的减字库路径
std::string db_file = "JianziNote.db";

// 减字
std::string jianzi_str = "大九挑七";

// 加载减字信息
Jianzi::OpenDb(db_file.c_str());

// 读取styler列表
auto styler_names = StylerFromDb::GetStylerList(db_file);
// 加载第一个styler
// 可以输入别的笔画粗细，比如: `StylerFromDb styler(0.07f)`
StylerFromDb styler;
styler.Load(db_file, styler_names[0]);

// 将一个自然表述字串转化为算式，然后进行解析
auto jianzi = Jianzi::Parse(Jianzi::ParseNatural(jianzi_str.c_str()).c_str());

// 获得减字路径描述
auto path_data = jianzi.RenderPath(styler);

// 进行路径绘制
SomeRenderer renderer;
auto result = renderer.Render(path_data);
```

## 编译

### 项目依赖

- [tiny-utf8](https://github.com/DuffsDevice/tiny-utf8)
- [magic_enum](https://github.com/Neargye/magic_enum)
- [SQlite3](https://www.sqlite.org/index.html)
- [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp)
- [FreeType2](https://freetype.org/)

### 兼容性

编译器要求C++17，以使用magic_enum的特性。

本项目在MacOS Ventura+Clang 12和Windows 11+MSVC 2019下编译通过。

### 预编译包
可以在Actions当中找列表的最上方下到最新的成功编译结果。