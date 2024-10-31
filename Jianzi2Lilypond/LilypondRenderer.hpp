#pragma once

#include <JianziStyler.hpp>
#include <string>

namespace qin
{

class LilypondRenderer
{
public:
    LilypondRenderer();

public:
    std::string Render(const std::vector<qin::JianziStyler::PathData> &path_data);
};

} // namespace qin
