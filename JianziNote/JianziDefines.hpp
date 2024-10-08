#pragma once

#include <vector>

#include "Point.hpp"

namespace qin
{
// 最大笔画宽度
// 此为极限值，并不会有笔画到达此宽度，用于分隔不同笔画
constexpr float MAX_STROKE_WIDTH = 0.14f;
// 最大笔画宽度的一半，方便计算表达
constexpr float HALF_STROKE_WIDTH = MAX_STROKE_WIDTH / 2;

// 笔画由端点类型及坐标、笔画类型组成
// 为了方便遍历，将其写到一起

// 端点类型
enum class VertexType
{
    None = -1,
    HengHead,      // 横头
    HengHeadNone,  // 无头横
    HengTail,      // 横尾
    HengTailNone,  // 无尾横
    HengTailRound, // 圆尾
    HengPie,       // 横撇

    ShuHead,      // 竖头
    ShuHeadNone,  // 无头竖（撇）
    ShuHeadJoint, // 接横竖
    ShuTail,      // 竖尾
    ShuTailNone,  // 无尾竖
    ShuGou,       // 竖钩
    ShuGouTip,    // 竖勾尖

    HengZhe, // 横折角

    PieHeadFlat,  // 平头撇
    PieHeadSlant, // 斜头撇
    PieHeadShu,   // 竖头撇
    PieHeadShort, // 短头撇
    PieHeadNone,  // 无头撇
    PieHeadNone2, // 无头撇，竖切
    PieTail,      // 撇尾
    PieTailShort, // 短撇尾
    PieControl,   // 撇控制点，与笔画同宽
    PieControl1,  // 撇控制点1，0.8笔画宽
    PieControl2,  // 撇控制点2，0.55笔画宽

    ShuPieNeck, // 竖撇连接处

    NaHead,     // 捺头
    NaControl1, // 捺控制点1，,0.3宽
    NaControl2, // 捺控制点2，1.0宽
    NaTail,     // 捺尾

    DianHead,      // 点头
    DianHeadShort, // 点短头
    DianTail,      // 点尾

    ShuWanControl, // 竖弯控制点
    ShuWanGou,     // 竖弯钩
    ShuWanGouTip,  // 竖弯钩尖

    TiHead, // 提头。其余共享撇的部件

    WanShuControl,  // 弯的竖钩控制点
    WanShuTail,     // 弯的竖尾
    WanShuTailNone, // 弯的竖无尾
    WanShuGou,      // 弯的竖钩
    WanShuGouTip,   // 弯竖勾尖

    WanGouHead,    // 弯钩头
    WanGouControl, // 弯钩控制点

    CeDianHead,     // 侧点头
    CeDianControl1, // 侧点控制点1
    CeDianControl2, // 侧点控制点2
    CeDianTail,     // 侧点尾

    DianShuiHead,   // 三点水最后一点头
    DianShuiDrop,   // 三点水最后一点垂露
    DianShuiCenter, // 三点水最后一点中心
    DianShuiTail,   // 三点水最后一点尾

    GaiDian, // 宝盖的侧点
};

enum class VertexBelong
{
    Other = -1,
    Top = 0,
    Medium,
    Bottom
};

// 笔画端点
struct StrokeVertex
{
    // int uid;
    VertexType type; // 端点类型
    Point2f pt;      // 端点坐标
    // EdgeType edge;   // 边标记
    // float weight;     // 笔画宽度权重
    VertexBelong belong; // 笔画所属位置（默认上半）
    // int next_uid;     // 下一个端点
};

// 笔画
struct Stroke
{
    float weight; // 一组笔画共享一个权重
    std::vector<StrokeVertex> vertice;
};

enum class JianziType
{
    Other,       // 其他分类
    Left,        // 左手（大、食、中、名、跪）
    LeftAlone,   // 左手无数字（散、就）
    Number,      // 数字（一至十）
    Main,        // 主字（抹、挑、勾、踢、轮、滚等）
    MainComplex, // 复杂主字（拨）
    MainShu,     // 中间带一竖的复杂主字（撮、剌、拨剌）
    GraceAbove,  // 上部装饰（泛、绰）
    GraceSide,   // 侧部装饰（注）
    Side,        // 旁字（进，复等）
};

} // namespace qin
