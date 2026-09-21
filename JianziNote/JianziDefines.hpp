// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <vector>

#include "Point.hpp"

namespace qin {
// 通用路径描述。渲染器可直接消费该格式，不依赖减字的加载或绘制实现。
enum class PathKey {
    Close,
    MoveTo,
    LineTo,
    QuadTo,
    CubicTo,
};

struct PathData {
    PathKey key = PathKey::Close;
    std::vector<Point2f> pts;
};

// 标记点属于哪个区域，影响变形时是否跟随capsule
enum class VertexRegion { Other = -1, Top = 0, Medium, Bottom };

// 笔画端点
struct StrokeVertex {
    Point2f pt;          // 骨架节点坐标
    VertexRegion region;  // 笔画所属位置（默认上半）
};

// 笔画
struct Stroke {
    std::string desc;  // StrokeDesc 的字符串 ID
    float width = 0.1f;
    std::vector<StrokeVertex> vertice;
};

enum class JianziType {
    Other,        // 其他分类
    Left,         // 左手（大、食、中、名、跪）
    LeftAlone,    // 左手无数字（散、就）
    Number,       // 数字（一至十）
    Main,         // 主字（抹、挑、勾、踢、轮、滚等）
    MainComplex,  // 复杂主字（拨）
    MainShu,      // 中间带一竖的复杂主字（撮、剌、拨剌）
    GraceAbove,   // 上部装饰（泛、绰）
    GraceSide,    // 侧部装饰（注）
    Side,         // 旁字（进，复等）
};

}  // namespace qin
