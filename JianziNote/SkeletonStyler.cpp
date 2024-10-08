#include "SkeletonStyler.hpp"

using namespace qin;

std::vector<SkeletonStyler::PathData> SkeletonStyler::RenderPath(const std::vector<Stroke> &strokes) const
{
    std::vector<PathData> ret;
    for (auto &s : strokes)
    {
        // 笔画开头必然是MoveTo
        ret.push_back(PathData{PathKey::MoveTo, {s.vertice[0].pt}});

        // skeleton类型直接用LineTo画所有的笔画
        for (int i = 1; i < s.vertice.size(); ++i)
        {
            ret.push_back(PathData{PathKey::LineTo, {s.vertice[i].pt}});
        }
    }
    return ret;
}

float SkeletonStyler::GetStrokeWidth() const
{
    return 0;
}
