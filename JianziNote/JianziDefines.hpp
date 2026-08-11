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

enum class RotateDir { Prev, Next };

struct PathOffset {
    float x_rel;
    float x_abs;
    float y_rel;
    float y_abs;
    float x_len;
    float y_len;
};

struct PathGroup {
    PathKey key;
    std::vector<PathOffset> offsets;
};

// 一个 Vertex 字符串 ID 对应的一套路径展开规则。
struct VertexDesc {
    std::string type;
    float pre_rotate;
    RotateDir dir;
    std::vector<PathGroup> forward;
    std::vector<PathGroup> backward;
};

// 最大笔画宽度
// 此为极限值，并不会有笔画到达此宽度，用于分隔不同笔画
constexpr float MAX_STROKE_WIDTH = 0.14f;
// 最大笔画宽度的一半，方便计算表达
constexpr float HALF_STROKE_WIDTH = MAX_STROKE_WIDTH / 2;

struct StrokeProfileSlot {
    Point2f default_pt;
    std::vector<std::string> vertex_types;
};
// 标记笔画种类，限定可用顶点类型
struct StrokeProfile {
    std::string type;
    std::vector<StrokeProfileSlot> slots;
};

// 标记点属于哪个区域，影响变形时是否跟随capsule
enum class VertexRegion { Other = -1, Top = 0, Medium, Bottom };

// 笔画端点
struct StrokeVertex {
    // int uid;
    std::string type;  // VertexDesc 的字符串 ID
    Point2f pt;        // 端点坐标
    // EdgeType edge;   // 边标记
    // float weight;     // 笔画宽度权重
    VertexRegion region;  // 笔画所属位置（默认上半）
                          // int next_uid;     // 下一个端点
};

// 笔画
struct Stroke {
    // StrokeProfile 的字符串 ID；空字符串表示旧数据中尚未归类的笔画。
    std::string profile;
    float weight;  // 一组笔画共享一个权重
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
