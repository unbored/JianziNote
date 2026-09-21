#include "StrokeDescRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace qin {
namespace {

constexpr double kEpsilon = 1e-9;

struct Vec {
  double x = 0;
  double y = 0;
};

Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.y + b.y}; }
Vec operator-(Vec a, Vec b) { return {a.x - b.x, a.y - b.y}; }
Vec operator*(Vec a, double scale) { return {a.x * scale, a.y * scale}; }
double Length(Vec value) { return std::hypot(value.x, value.y); }
double Dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y; }
Vec Unit(Vec value, Vec fallback) {
  const auto length = Length(value);
  return length > kEpsilon ? value * (1.0 / length) : fallback;
}

struct GeometrySegment {
  Vec origin;
  Vec tangent;
  Vec normal;
  double length = 0;
};

struct Geometry {
  std::vector<Vec> nodes;
  std::vector<GeometrySegment> segments;
  double width = 0;
};

struct Command {
  PathKey key = PathKey::Close;
  std::vector<Vec> points;
};

using Contour = std::vector<Command>;
using Outline = std::vector<Contour>;

struct PointSemantic {
  bool present = false;
  size_t node_index = 0;
  double influence = 0;
  double orientation_lock = 0;
};

struct CompiledDescription {
  std::string type;
  std::vector<double> means;
  std::vector<double> inverse_scales;
  std::vector<double> weights;
  size_t input_count = 0;
  size_t output_count = 0;
  bool bounded_output = false;
  std::vector<double> output_center;
  std::vector<double> residual_limits;
  std::vector<std::vector<PathKey>> outline_template;
  std::vector<size_t> point_segment_bindings;
  std::vector<PointSemantic> point_semantics;
  bool semantic_coordinates = false;
  std::vector<std::vector<bool>> smooth_joins;
};

size_t PointCount(PathKey key) {
  if (key == PathKey::MoveTo || key == PathKey::LineTo) return 1;
  if (key == PathKey::QuadTo) return 2;
  if (key == PathKey::CubicTo) return 3;
  return 0;
}

PathKey ParsePathKey(const std::string& type) {
  if (type == "M") return PathKey::MoveTo;
  if (type == "L") return PathKey::LineTo;
  if (type == "Q") return PathKey::QuadTo;
  if (type == "C") return PathKey::CubicTo;
  if (type == "Z") return PathKey::Close;
  throw std::runtime_error("Unknown StrokeDesc outline command: " + type);
}

CompiledDescription CompileDescription(const nlohmann::json& source) {
  CompiledDescription result;
  result.type = source.at("type").get<std::string>();
  if (result.type.empty()) throw std::runtime_error("StrokeDesc type must not be empty.");
  if (source.value("closed", false)) {
    throw std::runtime_error("Closed StrokeDesc skeletons are no longer supported: " + result.type);
  }
  const auto& fit = source.at("fit");
  const auto& model = fit.at("model");
  result.means = model.at("means").get<std::vector<double>>();
  const auto scales = model.at("scales").get<std::vector<double>>();
  if (scales.size() != result.means.size()) {
    throw std::runtime_error("StrokeDesc means and scales have different lengths: " + result.type);
  }
  result.inverse_scales.reserve(scales.size());
  for (const auto scale : scales) {
    if (!std::isfinite(scale)) throw std::runtime_error("StrokeDesc scale is not finite: " + result.type);
    result.inverse_scales.push_back(scale == 0 ? 1.0 : 1.0 / scale);
  }

  const auto& matrix = model.at("weights");
  result.input_count = result.means.size() + 1;
  if (!matrix.is_array() || matrix.size() != result.input_count || matrix.empty() || !matrix.at(0).is_array()) {
    throw std::runtime_error("StrokeDesc weight matrix dimensions are invalid: " + result.type);
  }
  result.output_count = matrix.at(0).size();
  result.weights.reserve(result.input_count * result.output_count);
  for (const auto& row : matrix) {
    if (!row.is_array() || row.size() != result.output_count) {
      throw std::runtime_error("StrokeDesc weight matrix is not rectangular: " + result.type);
    }
    for (const auto& value : row) {
      const auto number = value.get<double>();
      if (!std::isfinite(number)) throw std::runtime_error("StrokeDesc weight is not finite: " + result.type);
      result.weights.push_back(number);
    }
  }

  const auto& outline = model.at("template_outline_local");
  if (!outline.is_array()) throw std::runtime_error("StrokeDesc outline template must be an array: " + result.type);
  size_t coordinate_count = 0;
  size_t point_count = 0;
  for (const auto& contour_source : outline) {
    if (!contour_source.is_array()) throw std::runtime_error("StrokeDesc contour must be an array: " + result.type);
    std::vector<PathKey> contour;
    for (const auto& command : contour_source) {
      const auto key = ParsePathKey(command.at("type").get<std::string>());
      contour.push_back(key);
      point_count += PointCount(key);
      coordinate_count += PointCount(key) * 2;
    }
    result.outline_template.push_back(std::move(contour));
  }
  if (coordinate_count != result.output_count) {
    throw std::runtime_error("StrokeDesc output dimensions do not match its outline template: " + result.type);
  }

  result.point_segment_bindings =
      model.value("point_segment_bindings", std::vector<size_t>(point_count, size_t{0}));
  if (result.point_segment_bindings.size() != point_count) {
    throw std::runtime_error("StrokeDesc point binding count does not match its outline: " + result.type);
  }
  const auto coordinate_system = model.value("semantic_coordinate_system", std::string{});
  result.semantic_coordinates = coordinate_system == "node-width-blend-v1" ||
                                coordinate_system == "node-width-orientation-lock-v2";
  result.point_semantics.resize(point_count);
  if (const auto semantics = model.find("point_semantics"); semantics != model.end()) {
    if (!semantics->is_array() || semantics->size() > point_count) {
      throw std::runtime_error("StrokeDesc point semantics have invalid dimensions: " + result.type);
    }
    for (size_t index = 0; index < semantics->size(); ++index) {
      if (semantics->at(index).is_null()) continue;
      const auto& value = semantics->at(index);
      auto& semantic = result.point_semantics[index];
      semantic.present = true;
      semantic.node_index = value.at("node_index").get<size_t>();
      semantic.influence = std::clamp(value.value("influence", 0.0), 0.0, 1.0);
      semantic.orientation_lock = std::clamp(value.value("orientation_lock", 0.0), 0.0, 1.0);
    }
  }

  result.output_center = model.value("output_center", std::vector<double>{});
  result.residual_limits = model.value("residual_limits", std::vector<double>{});
  result.bounded_output = model.value("output_link", std::string{}) == "centered-tanh-v1" &&
                          result.output_center.size() == result.output_count &&
                          result.residual_limits.size() == result.output_count;

  const auto topology = fit.value("topology", nlohmann::json::object());
  const auto contours = topology.value("contours", nlohmann::json::array());
  result.smooth_joins.resize(result.outline_template.size());
  for (size_t contour_index = 0; contour_index < contours.size() && contour_index < result.smooth_joins.size();
       ++contour_index) {
    const auto joins = contours.at(contour_index).value("joins", nlohmann::json::array());
    auto& compiled = result.smooth_joins[contour_index];
    compiled.reserve(joins.size());
    for (const auto& join : joins) compiled.push_back(join.value("continuity", std::string{}) == "smooth");
  }
  return result;
}

Geometry MakeGeometry(const Stroke& stroke) {
  if (!std::isfinite(stroke.width) || stroke.width <= 0) {
    throw std::runtime_error("StrokeDesc stroke width must be positive.");
  }
  Geometry result;
  result.width = stroke.width;
  for (const auto& vertex : stroke.vertice) {
    if (!std::isfinite(vertex.pt.x) || !std::isfinite(vertex.pt.y)) {
      throw std::runtime_error("StrokeDesc skeleton contains a non-finite node.");
    }
    result.nodes.push_back({vertex.pt.x, vertex.pt.y});
  }
  if (result.nodes.size() < 2) throw std::runtime_error("StrokeDesc requires at least two skeleton nodes.");
  for (size_t index = 1; index < result.nodes.size(); ++index) {
    const auto delta = result.nodes[index] - result.nodes[index - 1];
    const auto length = Length(delta);
    if (length < kEpsilon) throw std::runtime_error("StrokeDesc contains coincident adjacent skeleton nodes.");
    const auto tangent = delta * (1.0 / length);
    result.segments.push_back({result.nodes[index - 1], tangent, {-tangent.y, tangent.x}, length});
  }
  return result;
}

std::vector<double> FeatureVector(const Geometry& geometry) {
  std::vector<double> result;
  result.reserve(geometry.segments.size() * 4 + 1);
  for (const auto& segment : geometry.segments) {
    result.push_back(segment.length);
    result.push_back(geometry.width / segment.length);
  }
  for (size_t index = 1; index < geometry.segments.size(); ++index) {
    const auto previous = geometry.segments[index - 1].tangent;
    const auto current = geometry.segments[index].tangent;
    result.push_back(previous.x * current.y - previous.y * current.x);
    result.push_back(Dot(previous, current));
  }
  result.push_back(geometry.width);
  return result;
}

std::vector<double> PredictVector(const CompiledDescription& desc, const Geometry& geometry) {
  const auto raw = FeatureVector(geometry);
  if (raw.size() != desc.means.size()) {
    throw std::runtime_error("StrokeDesc feature count does not match the skeleton: " + desc.type);
  }
  std::vector<double> output(desc.output_count, 0);
  for (size_t column = 0; column < desc.output_count; ++column) output[column] = desc.weights[column];
  for (size_t row = 0; row < raw.size(); ++row) {
    const auto input = (raw[row] - desc.means[row]) * desc.inverse_scales[row];
    const auto offset = (row + 1) * desc.output_count;
    for (size_t column = 0; column < desc.output_count; ++column) {
      output[column] += input * desc.weights[offset + column];
    }
  }
  if (desc.bounded_output) {
    for (size_t index = 0; index < output.size(); ++index) {
      output[index] = desc.output_center[index] + desc.residual_limits[index] * std::tanh(output[index]);
    }
  }
  for (const auto value : output) {
    if (!std::isfinite(value)) throw std::runtime_error("StrokeDesc predicted a non-finite outline: " + desc.type);
  }
  return output;
}

Outline OutlineFromVector(const CompiledDescription& desc, const std::vector<double>& vector) {
  Outline result;
  size_t offset = 0;
  for (const auto& source : desc.outline_template) {
    Contour contour;
    for (const auto key : source) {
      Command command{key, {}};
      for (size_t point = 0; point < PointCount(key); ++point) {
        command.points.push_back({vector[offset], vector[offset + 1]});
        offset += 2;
      }
      contour.push_back(std::move(command));
    }
    result.push_back(std::move(contour));
  }
  return result;
}

struct Frame {
  Vec origin;
  Vec x;
  Vec y;
};

Frame SemanticFrame(const Geometry& geometry, size_t segment_index, const PointSemantic& semantic) {
  const auto& segment = geometry.segments.at(segment_index);
  Frame frame{segment.origin, segment.tangent * segment.length, segment.normal * geometry.width};
  if (!semantic.present || semantic.influence <= 0) return frame;
  if (semantic.node_index >= geometry.nodes.size()) {
    throw std::runtime_error("StrokeDesc semantic node binding is out of range.");
  }
  Vec tangent;
  if (semantic.node_index == 0) {
    tangent = geometry.segments.front().tangent;
  } else if (semantic.node_index + 1 >= geometry.nodes.size()) {
    tangent = geometry.segments.back().tangent;
  } else {
    tangent = Unit(geometry.segments[semantic.node_index - 1].tangent +
                       geometry.segments[semantic.node_index].tangent,
                   segment.tangent);
  }
  const Vec normal{-tangent.y, tangent.x};
  const auto mix = [&](Vec a, Vec b) { return a + (b - a) * semantic.influence; };
  frame.origin = mix(frame.origin, geometry.nodes[semantic.node_index]);
  frame.x = mix(frame.x, tangent * geometry.width);
  frame.y = mix(frame.y, normal * geometry.width);
  if (semantic.orientation_lock > 0) {
    const auto angle = -std::atan2(frame.x.y, frame.x.x) * semantic.orientation_lock;
    const auto cosine = std::cos(angle);
    const auto sine = std::sin(angle);
    const auto rotate = [&](Vec value) {
      return Vec{value.x * cosine - value.y * sine, value.x * sine + value.y * cosine};
    };
    frame.x = rotate(frame.x);
    frame.y = rotate(frame.y);
  }
  return frame;
}

void GlobalizeOutline(Outline& outline, const Geometry& geometry, const CompiledDescription& desc) {
  size_t point_index = 0;
  for (auto& contour : outline) {
    for (auto& command : contour) {
      for (auto& point : command.points) {
        const auto segment_index = desc.point_segment_bindings[point_index];
        if (segment_index >= geometry.segments.size()) {
          throw std::runtime_error("StrokeDesc point segment binding is out of range: " + desc.type);
        }
        if (desc.semantic_coordinates) {
          const auto frame = SemanticFrame(geometry, segment_index, desc.point_semantics[point_index]);
          point = frame.origin + frame.x * point.x + frame.y * point.y;
        } else {
          const auto& segment = geometry.segments[segment_index];
          point = segment.origin + segment.tangent * (point.x * segment.length) +
                  segment.normal * (point.y * geometry.width);
        }
        ++point_index;
      }
    }
  }
}

struct SegmentRef {
  PathKey key = PathKey::LineTo;
  Vec start;
  Vec end;
  size_t command_index = 0;
  bool implicit = false;
};

std::vector<SegmentRef> ContourSegments(const Contour& contour) {
  std::vector<SegmentRef> result;
  Vec current;
  Vec first;
  bool has_current = false;
  for (size_t index = 0; index < contour.size(); ++index) {
    const auto& command = contour[index];
    if (command.key == PathKey::MoveTo && !command.points.empty()) {
      current = first = command.points.back();
      has_current = true;
    } else if ((command.key == PathKey::LineTo || command.key == PathKey::QuadTo ||
                command.key == PathKey::CubicTo) &&
               has_current && !command.points.empty()) {
      result.push_back({command.key, current, command.points.back(), index, false});
      current = command.points.back();
    } else if (command.key == PathKey::Close && has_current && Length(current - first) > kEpsilon) {
      result.push_back({PathKey::LineTo, current, first, index, true});
    }
  }
  return result;
}

Vec StartTangent(const SegmentRef& segment, const Contour& contour) {
  if (!segment.implicit) {
    const auto& command = contour[segment.command_index];
    if ((segment.key == PathKey::CubicTo || segment.key == PathKey::QuadTo) && !command.points.empty()) {
      return Unit(command.points.front() - segment.start, {1, 0});
    }
  }
  return Unit(segment.end - segment.start, {1, 0});
}

Vec EndTangent(const SegmentRef& segment, const Contour& contour) {
  if (!segment.implicit) {
    const auto& command = contour[segment.command_index];
    if (segment.key == PathKey::CubicTo && command.points.size() >= 2) {
      return Unit(segment.end - command.points[1], Unit(segment.end - segment.start, {1, 0}));
    }
    if (segment.key == PathKey::QuadTo && !command.points.empty()) {
      return Unit(segment.end - command.points.front(), {1, 0});
    }
  }
  return Unit(segment.end - segment.start, {1, 0});
}

bool SmoothAt(const CompiledDescription& desc, size_t contour, size_t join) {
  return contour < desc.smooth_joins.size() && join < desc.smooth_joins[contour].size() &&
         desc.smooth_joins[contour][join];
}

void EnforceHandleProgress(Outline& outline, const CompiledDescription& desc) {
  for (size_t contour_index = 0; contour_index < outline.size(); ++contour_index) {
    auto& contour = outline[contour_index];
    const auto segments = ContourSegments(contour);
    size_t segment_index = 0;
    Vec current;
    bool has_current = false;
    for (auto& command : contour) {
      if (command.key == PathKey::MoveTo && !command.points.empty()) {
        current = command.points.back();
        has_current = true;
      } else if (command.key == PathKey::CubicTo && has_current && command.points.size() == 3) {
        const auto end = command.points.back();
        const auto chord = Unit(end - current, {1, 0});
        const auto start = Dot(command.points[0] - current, chord);
        const auto finish = Dot(end - command.points[1], chord);
        if (SmoothAt(desc, contour_index, segment_index) && start < 0) {
          command.points[0] = command.points[0] - chord * start;
        }
        if (!segments.empty() && SmoothAt(desc, contour_index, (segment_index + 1) % segments.size()) &&
            finish < 0) {
          command.points[1] = command.points[1] + chord * finish;
        }
        current = end;
        ++segment_index;
      } else if ((command.key == PathKey::LineTo || command.key == PathKey::QuadTo) &&
                 !command.points.empty()) {
        current = command.points.back();
        has_current = true;
        ++segment_index;
      }
    }
  }
}

void EnforceContinuity(Outline& outline, const CompiledDescription& desc) {
  for (size_t contour_index = 0; contour_index < outline.size(); ++contour_index) {
    auto& contour = outline[contour_index];
    auto segments = ContourSegments(contour);
    if (segments.empty() || contour_index >= desc.smooth_joins.size() ||
        desc.smooth_joins[contour_index].size() != segments.size()) {
      continue;
    }
    for (size_t index = 0; index < segments.size(); ++index) {
      if (!desc.smooth_joins[contour_index][index]) continue;
      auto& incoming = segments[(index + segments.size() - 1) % segments.size()];
      auto& outgoing = segments[index];
      if (incoming.key != PathKey::CubicTo && outgoing.key != PathKey::CubicTo) continue;
      const auto a = EndTangent(incoming, contour);
      const auto b = StartTangent(outgoing, contour);
      const auto tangent = Unit(a + b, a);
      const auto point = outgoing.start;
      if (incoming.key == PathKey::CubicTo && !incoming.implicit) {
        auto& control = contour[incoming.command_index].points[1];
        control = point - tangent * Length(point - control);
      }
      if (outgoing.key == PathKey::CubicTo && !outgoing.implicit) {
        auto& control = contour[outgoing.command_index].points[0];
        control = point + tangent * Length(control - point);
      }
    }
    if (segments.back().implicit) {
      const auto close = contour.back().key == PathKey::Close ? contour.back() : Command{PathKey::Close, {}};
      if (contour.back().key == PathKey::Close) contour.pop_back();
      contour.push_back({PathKey::LineTo, {segments.back().end}});
      contour.push_back(close);
    }
  }
}

std::vector<PathData> RenderOne(const CompiledDescription& desc, const Stroke& stroke) {
  const auto geometry = MakeGeometry(stroke);
  auto outline = OutlineFromVector(desc, PredictVector(desc, geometry));
  GlobalizeOutline(outline, geometry, desc);
  EnforceHandleProgress(outline, desc);
  EnforceContinuity(outline, desc);
  std::vector<PathData> result;
  for (const auto& contour : outline) {
    for (const auto& command : contour) {
      PathData path{command.key, {}};
      for (const auto point : command.points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
          throw std::runtime_error("StrokeDesc produced a non-finite path point: " + desc.type);
        }
        path.pts.push_back({static_cast<float>(point.x), static_cast<float>(point.y)});
      }
      result.push_back(std::move(path));
    }
  }
  return result;
}

}  // namespace

struct StrokeDescRenderer::Impl {
  std::unordered_map<std::string, CompiledDescription> descriptions;
};

StrokeDescRenderer::StrokeDescRenderer(const nlohmann::json& descriptions) : impl_(std::make_unique<Impl>()) {
  if (!descriptions.is_array()) throw std::runtime_error("stroke_descs must be an array.");
  for (const auto& source : descriptions) {
    auto description = CompileDescription(source);
    const auto type = description.type;
    if (!impl_->descriptions.emplace(type, std::move(description)).second) {
      throw std::runtime_error("Duplicate StrokeDesc type: " + type);
    }
  }
}

StrokeDescRenderer::~StrokeDescRenderer() = default;

bool StrokeDescRenderer::Contains(std::string_view name) const {
  return impl_->descriptions.find(std::string(name)) != impl_->descriptions.end();
}

std::vector<PathData> StrokeDescRenderer::Render(const std::vector<Stroke>& strokes) const {
  std::vector<PathData> result;
  for (const auto& stroke : strokes) {
    const auto found = impl_->descriptions.find(stroke.desc);
    if (found == impl_->descriptions.end()) throw std::runtime_error("Unknown StrokeDesc: " + stroke.desc);
    auto path = RenderOne(found->second, stroke);
    result.insert(result.end(), std::make_move_iterator(path.begin()), std::make_move_iterator(path.end()));
  }
  return result;
}

}  // namespace qin
