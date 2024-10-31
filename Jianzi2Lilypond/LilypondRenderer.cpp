#include "LilypondRenderer.hpp"

namespace qin
{

LilypondRenderer::LilypondRenderer()
{
}

std::string LilypondRenderer::Render(const std::vector<JianziStyler::PathData> &path_data)
{
    std::string ret = "\\override #'(filled . #t)\n"
                      "\\path #0 #'(\n";
    // 注意y轴转换
    for (auto &p : path_data)
    {
        switch (p.key)
        {
        case JianziStyler::PathKey::MoveTo:
            ret += "(moveto " + std::to_string(p.pts[0].x) + " " + std::to_string(-p.pts[0].y) + ")";
            break;
        case JianziStyler::PathKey::LineTo:
            ret += "(lineto " + std::to_string(p.pts[0].x) + " " + std::to_string(-p.pts[0].y) + ")";
            break;
        case JianziStyler::PathKey::QuadTo: {
            // lilypond不提供二阶曲线的命令，升成三阶
            // 需要获得前一个路径点的最后一个点
            Point2f pt0 = (&p)[-1].pts.back();
            Point2f pt1 = 1.0f / 3.0f * pt0 + 2.0f / 3.0f * p.pts[0];
            Point2f pt2 = 2.0f / 3.0f * p.pts[0] + 1.0f / 3.0f * p.pts[1];
            Point2f pt3 = p.pts[1];
            ret += "(curveto " + std::to_string(pt1.x) + " " + std::to_string(-pt1.y) + " " + std::to_string(pt2.x) +
                   " " + std::to_string(-pt2.y) + " " + std::to_string(pt3.x) + " " + std::to_string(-pt3.y) + ")";
        }
        break;
        case JianziStyler::PathKey::CubicTo:
            ret += "(curveto " + std::to_string(p.pts[0].x) + " " + std::to_string(-p.pts[0].y) + " " +
                   std::to_string(p.pts[1].x) + " " + std::to_string(-p.pts[1].y) + " " + std::to_string(p.pts[2].x) +
                   " " + std::to_string(-p.pts[2].y) + ")";
            break;
        case JianziStyler::PathKey::Close:
            ret += "(closepath)";
        default:
            break;
        }
    }

    ret += ")\n";

    return ret;
}

} // namespace qin
