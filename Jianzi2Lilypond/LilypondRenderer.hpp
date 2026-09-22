#pragma once

#include <VectorPath.hpp>
#include <string>

namespace qin
{

class LilypondRenderer
{
public:
    LilypondRenderer();

public:
    std::string Render(const std::vector<qin::PathData> &path_data);
};

} // namespace qin
