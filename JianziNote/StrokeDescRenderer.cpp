#include "StrokeDescRenderer.hpp"

#include <algorithm>
#include <cmath>
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

Result<CompiledDescription> DescriptionError(std::string message) {
  return Result<CompiledDescription>::Failure(
      {JianziErrorCode::InvalidStrokeDesc, std::move(message)});
}

const nlohmann::json* Member(const nlohmann::json& object, const char* name) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(name);
  return found == object.end() ? nullptr : &*found;
}

bool NumberArray(const nlohmann::json& source, std::vector<double>& output) {
  if (!source.is_array()) return false;
  output.clear();
  output.reserve(source.size());
  for (const auto& value : source) {
    if (!value.is_number()) return false;
    const auto number = value.get<double>();
    if (!std::isfinite(number)) return false;
    output.push_back(number);
  }
  return true;
}

bool IndexArray(const nlohmann::json& source, std::vector<size_t>& output) {
  if (!source.is_array()) return false;
  output.clear();
  output.reserve(source.size());
  for (const auto& value : source) {
    if (!value.is_number_unsigned() && !value.is_number_integer()) return false;
    if (value.is_number_integer() && value.get<std::int64_t>() < 0) return false;
    output.push_back(value.get<size_t>());
  }
  return true;
}

bool ParsePathKey(const std::string& type, PathKey& output) {
  if (type == "M") output = PathKey::MoveTo;
  else if (type == "L") output = PathKey::LineTo;
  else if (type == "Q") output = PathKey::QuadTo;
  else if (type == "C") output = PathKey::CubicTo;
  else if (type == "Z") output = PathKey::Close;
  else return false;
  return true;
}

Result<CompiledDescription> CompileDescription(const nlohmann::json& source) {
  CompiledDescription result;
  const auto type = Member(source, "type");
  if (!type || !type->is_string()) return DescriptionError("StrokeDesc type must be a string.");
  result.type = type->get<std::string>();
  if (result.type.empty()) return DescriptionError("StrokeDesc type must not be empty.");
  if (const auto closed = Member(source, "closed")) {
    if (!closed->is_boolean()) return DescriptionError("StrokeDesc closed must be boolean: " + result.type);
    if (closed->get<bool>()) {
      return DescriptionError("Closed StrokeDesc skeletons are no longer supported: " + result.type);
    }
  }
  const auto fit = Member(source, "fit");
  const auto model = fit ? Member(*fit, "model") : nullptr;
  if (!fit || !fit->is_object() || !model || !model->is_object()) {
    return DescriptionError("StrokeDesc fit.model is missing: " + result.type);
  }
  const auto means = Member(*model, "means");
  const auto scales_source = Member(*model, "scales");
  std::vector<double> scales;
  if (!means || !NumberArray(*means, result.means) || !scales_source ||
      !NumberArray(*scales_source, scales)) {
    return DescriptionError("StrokeDesc means and scales must be finite number arrays: " + result.type);
  }
  if (scales.size() != result.means.size()) {
    return DescriptionError("StrokeDesc means and scales have different lengths: " + result.type);
  }
  result.inverse_scales.reserve(scales.size());
  for (const auto scale : scales) {
    result.inverse_scales.push_back(scale == 0 ? 1.0 : 1.0 / scale);
  }

  const auto matrix = Member(*model, "weights");
  result.input_count = result.means.size() + 1;
  if (!matrix || !matrix->is_array() || matrix->size() != result.input_count || matrix->empty() ||
      !matrix->front().is_array()) {
    return DescriptionError("StrokeDesc weight matrix dimensions are invalid: " + result.type);
  }
  result.output_count = matrix->front().size();
  result.weights.reserve(result.input_count * result.output_count);
  for (const auto& row : *matrix) {
    if (!row.is_array() || row.size() != result.output_count) {
      return DescriptionError("StrokeDesc weight matrix is not rectangular: " + result.type);
    }
    for (const auto& value : row) {
      if (!value.is_number()) return DescriptionError("StrokeDesc weight is not numeric: " + result.type);
      const auto number = value.get<double>();
      if (!std::isfinite(number)) return DescriptionError("StrokeDesc weight is not finite: " + result.type);
      result.weights.push_back(number);
    }
  }

  const auto outline = Member(*model, "template_outline_local");
  if (!outline || !outline->is_array()) {
    return DescriptionError("StrokeDesc outline template must be an array: " + result.type);
  }
  size_t coordinate_count = 0;
  size_t point_count = 0;
  for (const auto& contour_source : *outline) {
    if (!contour_source.is_array()) return DescriptionError("StrokeDesc contour must be an array: " + result.type);
    std::vector<PathKey> contour;
    for (const auto& command : contour_source) {
      const auto command_type = Member(command, "type");
      if (!command_type || !command_type->is_string()) {
        return DescriptionError("StrokeDesc outline command has no type: " + result.type);
      }
      PathKey key;
      if (!ParsePathKey(command_type->get<std::string>(), key)) {
        return DescriptionError("Unknown StrokeDesc outline command in " + result.type);
      }
      contour.push_back(key);
      point_count += PointCount(key);
      coordinate_count += PointCount(key) * 2;
    }
    result.outline_template.push_back(std::move(contour));
  }
  if (coordinate_count != result.output_count) {
    return DescriptionError("StrokeDesc output dimensions do not match its outline template: " + result.type);
  }

  result.point_segment_bindings.assign(point_count, size_t{0});
  if (const auto bindings = Member(*model, "point_segment_bindings")) {
    if (!IndexArray(*bindings, result.point_segment_bindings)) {
      return DescriptionError("StrokeDesc point bindings must be non-negative integers: " + result.type);
    }
  }
  if (result.point_segment_bindings.size() != point_count) {
    return DescriptionError("StrokeDesc point binding count does not match its outline: " + result.type);
  }
  std::string coordinate_system;
  if (const auto system = Member(*model, "semantic_coordinate_system")) {
    if (!system->is_string()) return DescriptionError("Invalid semantic coordinate system: " + result.type);
    coordinate_system = system->get<std::string>();
  }
  result.semantic_coordinates = coordinate_system == "node-width-blend-v1" ||
                                coordinate_system == "node-width-orientation-lock-v2";
  result.point_semantics.resize(point_count);
  if (const auto semantics = Member(*model, "point_semantics")) {
    if (!semantics->is_array() || semantics->size() > point_count) {
      return DescriptionError("StrokeDesc point semantics have invalid dimensions: " + result.type);
    }
    for (size_t index = 0; index < semantics->size(); ++index) {
      const auto& value = (*semantics)[index];
      if (value.is_null()) continue;
      const auto node_index = Member(value, "node_index");
      if (!node_index || (!node_index->is_number_unsigned() && !node_index->is_number_integer()) ||
          (node_index->is_number_integer() && node_index->get<std::int64_t>() < 0)) {
        return DescriptionError("StrokeDesc semantic node index is invalid: " + result.type);
      }
      auto& semantic = result.point_semantics[index];
      semantic.present = true;
      semantic.node_index = node_index->get<size_t>();
      if (const auto influence = Member(value, "influence")) {
        if (!influence->is_number()) return DescriptionError("Invalid semantic influence: " + result.type);
        semantic.influence = std::clamp(influence->get<double>(), 0.0, 1.0);
      }
      if (const auto lock = Member(value, "orientation_lock")) {
        if (!lock->is_number()) return DescriptionError("Invalid orientation lock: " + result.type);
        semantic.orientation_lock = std::clamp(lock->get<double>(), 0.0, 1.0);
      }
    }
  }

  if (const auto center = Member(*model, "output_center"); center && !NumberArray(*center, result.output_center)) {
    return DescriptionError("Invalid StrokeDesc output center: " + result.type);
  }
  if (const auto limits = Member(*model, "residual_limits"); limits && !NumberArray(*limits, result.residual_limits)) {
    return DescriptionError("Invalid StrokeDesc residual limits: " + result.type);
  }
  std::string output_link;
  if (const auto link = Member(*model, "output_link")) {
    if (!link->is_string()) return DescriptionError("Invalid StrokeDesc output link: " + result.type);
    output_link = link->get<std::string>();
  }
  result.bounded_output = output_link == "centered-tanh-v1" &&
                          result.output_center.size() == result.output_count &&
                          result.residual_limits.size() == result.output_count;

  result.smooth_joins.resize(result.outline_template.size());
  if (const auto topology = Member(*fit, "topology")) {
    const auto contours = Member(*topology, "contours");
    if (!topology->is_object() || (contours && !contours->is_array())) {
      return DescriptionError("Invalid StrokeDesc topology: " + result.type);
    }
    if (contours) {
      for (size_t contour_index = 0;
           contour_index < contours->size() && contour_index < result.smooth_joins.size(); ++contour_index) {
        const auto joins = Member((*contours)[contour_index], "joins");
        if (!joins) continue;
        if (!joins->is_array()) return DescriptionError("Invalid StrokeDesc joins: " + result.type);
        auto& compiled = result.smooth_joins[contour_index];
        compiled.reserve(joins->size());
        for (const auto& join : *joins) {
          const auto continuity = Member(join, "continuity");
          compiled.push_back(continuity && continuity->is_string() &&
                             continuity->get<std::string>() == "smooth");
        }
      }
    }
  }
  return Result<CompiledDescription>::Success(std::move(result));
}

Result<Geometry> MakeGeometry(const Stroke& stroke) {
  if (!std::isfinite(stroke.width) || stroke.width <= 0) {
    return Result<Geometry>::Failure(
        {JianziErrorCode::InvalidGeometry, "StrokeDesc stroke width must be positive."});
  }
  Geometry result;
  result.width = stroke.width;
  for (const auto& vertex : stroke.vertice) {
    if (!std::isfinite(vertex.pt.x) || !std::isfinite(vertex.pt.y)) {
      return Result<Geometry>::Failure(
          {JianziErrorCode::InvalidGeometry, "StrokeDesc skeleton contains a non-finite node."});
    }
    result.nodes.push_back({vertex.pt.x, vertex.pt.y});
  }
  if (result.nodes.size() < 2) {
    return Result<Geometry>::Failure(
        {JianziErrorCode::InvalidGeometry, "StrokeDesc requires at least two skeleton nodes."});
  }
  for (size_t index = 1; index < result.nodes.size(); ++index) {
    const auto delta = result.nodes[index] - result.nodes[index - 1];
    const auto length = Length(delta);
    if (length < kEpsilon) {
      return Result<Geometry>::Failure(
          {JianziErrorCode::InvalidGeometry, "StrokeDesc contains coincident adjacent skeleton nodes."});
    }
    const auto tangent = delta * (1.0 / length);
    result.segments.push_back({result.nodes[index - 1], tangent, {-tangent.y, tangent.x}, length});
  }
  return Result<Geometry>::Success(std::move(result));
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

Result<std::vector<double>> PredictVector(const CompiledDescription& desc, const Geometry& geometry) {
  const auto raw = FeatureVector(geometry);
  if (raw.size() != desc.means.size()) {
    return Result<std::vector<double>>::Failure(
        {JianziErrorCode::InvalidGeometry,
         "StrokeDesc feature count does not match the skeleton: " + desc.type});
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
    if (!std::isfinite(value)) {
      return Result<std::vector<double>>::Failure(
          {JianziErrorCode::InvalidGeometry,
           "StrokeDesc predicted a non-finite outline: " + desc.type});
    }
  }
  return Result<std::vector<double>>::Success(std::move(output));
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

Result<Frame> SemanticFrame(const Geometry& geometry, size_t segment_index, const PointSemantic& semantic) {
  if (segment_index >= geometry.segments.size()) {
    return Result<Frame>::Failure(
        {JianziErrorCode::InvalidGeometry, "StrokeDesc segment binding is out of range."});
  }
  const auto& segment = geometry.segments[segment_index];
  Frame frame{segment.origin, segment.tangent * segment.length, segment.normal * geometry.width};
  if (!semantic.present || semantic.influence <= 0) return Result<Frame>::Success(frame);
  if (semantic.node_index >= geometry.nodes.size()) {
    return Result<Frame>::Failure(
        {JianziErrorCode::InvalidGeometry, "StrokeDesc semantic node binding is out of range."});
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
  return Result<Frame>::Success(frame);
}

Result<void> GlobalizeOutline(Outline& outline, const Geometry& geometry, const CompiledDescription& desc) {
  size_t point_index = 0;
  for (auto& contour : outline) {
    for (auto& command : contour) {
      for (auto& point : command.points) {
        const auto segment_index = desc.point_segment_bindings[point_index];
        if (segment_index >= geometry.segments.size()) {
          return Result<void>::Failure(
              {JianziErrorCode::InvalidGeometry,
               "StrokeDesc point segment binding is out of range: " + desc.type});
        }
        if (desc.semantic_coordinates) {
          const auto frame = SemanticFrame(geometry, segment_index, desc.point_semantics[point_index]);
          if (!frame) return Result<void>::Failure(*frame.GetError());
          point = frame.GetValue()->origin + frame.GetValue()->x * point.x + frame.GetValue()->y * point.y;
        } else {
          const auto& segment = geometry.segments[segment_index];
          point = segment.origin + segment.tangent * (point.x * segment.length) +
                  segment.normal * (point.y * geometry.width);
        }
        ++point_index;
      }
    }
  }
  return Result<void>::Success();
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

Result<std::vector<PathData>> RenderOne(const CompiledDescription& desc, const Stroke& stroke) {
  const auto geometry = MakeGeometry(stroke);
  if (!geometry) return Result<std::vector<PathData>>::Failure(*geometry.GetError());
  const auto predicted = PredictVector(desc, *geometry.GetValue());
  if (!predicted) return Result<std::vector<PathData>>::Failure(*predicted.GetError());
  auto outline = OutlineFromVector(desc, *predicted.GetValue());
  const auto globalized = GlobalizeOutline(outline, *geometry.GetValue(), desc);
  if (!globalized) return Result<std::vector<PathData>>::Failure(*globalized.GetError());
  EnforceHandleProgress(outline, desc);
  EnforceContinuity(outline, desc);
  std::vector<PathData> result;
  for (const auto& contour : outline) {
    for (const auto& command : contour) {
      PathData path{command.key, {}};
      for (const auto point : command.points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
          return Result<std::vector<PathData>>::Failure(
              {JianziErrorCode::InvalidGeometry,
               "StrokeDesc produced a non-finite path point: " + desc.type});
        }
        path.pts.push_back({static_cast<float>(point.x), static_cast<float>(point.y)});
      }
      result.push_back(std::move(path));
    }
  }
  return Result<std::vector<PathData>>::Success(std::move(result));
}

}  // namespace

struct StrokeDescRenderer::Impl {
  std::unordered_map<std::string, CompiledDescription> descriptions;
};

StrokeDescRenderer::StrokeDescRenderer(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Result<std::unique_ptr<StrokeDescRenderer>> StrokeDescRenderer::Create(
    const nlohmann::json& descriptions) {
  if (!descriptions.is_array()) {
    return Result<std::unique_ptr<StrokeDescRenderer>>::Failure(
        {JianziErrorCode::InvalidStrokeDesc, "stroke_descs must be an array."});
  }
  auto impl = std::make_unique<Impl>();
  for (const auto& source : descriptions) {
    auto description = CompileDescription(source);
    if (!description) {
      return Result<std::unique_ptr<StrokeDescRenderer>>::Failure(*description.GetError());
    }
    const auto type = description.GetValue()->type;
    if (!impl->descriptions.emplace(type, std::move(*description.GetValue())).second) {
      return Result<std::unique_ptr<StrokeDescRenderer>>::Failure(
          {JianziErrorCode::InvalidStrokeDesc, "Duplicate StrokeDesc type: " + type});
    }
  }
  return Result<std::unique_ptr<StrokeDescRenderer>>::Success(
      std::unique_ptr<StrokeDescRenderer>(new StrokeDescRenderer(std::move(impl))));
}

StrokeDescRenderer::~StrokeDescRenderer() = default;

bool StrokeDescRenderer::Contains(std::string_view name) const {
  return impl_->descriptions.find(std::string(name)) != impl_->descriptions.end();
}

Result<std::vector<PathData>> StrokeDescRenderer::Render(const std::vector<Stroke>& strokes) const {
  std::vector<PathData> result;
  for (const auto& stroke : strokes) {
    const auto found = impl_->descriptions.find(stroke.desc);
    if (found == impl_->descriptions.end()) {
      return Result<std::vector<PathData>>::Failure(
          {JianziErrorCode::InvalidStrokeDesc, "Unknown StrokeDesc: " + stroke.desc});
    }
    auto path = RenderOne(found->second, stroke);
    if (!path) return Result<std::vector<PathData>>::Failure(*path.GetError());
    result.insert(result.end(), std::make_move_iterator(path.GetValue()->begin()),
                  std::make_move_iterator(path.GetValue()->end()));
  }
  return Result<std::vector<PathData>>::Success(std::move(result));
}

}  // namespace qin
