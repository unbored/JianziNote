// Temporary migration tool for JianziNote's legacy SQLite resource package.

#include "Cbor.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using nlohmann::json;
namespace fs = std::filesystem;

json BorderFlags(const SQLite::Statement& query) {
  return {{"top", query.getColumn("border_t").getInt() != 0},
          {"bottom", query.getColumn("border_b").getInt() != 0},
          {"left", query.getColumn("border_l").getInt() != 0},
          {"right", query.getColumn("border_r").getInt() != 0}};
}

bool WriteDocument(const fs::path& path, const json& document) {
  if (const auto error = qin::cbor::Write(path, document)) {
    std::cerr << error->path.string() << ": " << error->message << '\n';
    return false;
  }
  return true;
}

json LoadGlyph(SQLite::Database& db, const std::string& name) {
  json glyph = {{"format_version", 1}, {"name", name}};

  SQLite::Statement glyph_query(
      db, "select border_t,border_b,border_l,border_r,v_segments,type "
          "from jianzi where name = ?");
  glyph_query.bind(1, name);
  if (!glyph_query.executeStep()) {
    throw std::runtime_error("Missing glyph while exporting: " + name);
  }
  glyph["type"] = glyph_query.getColumn("type").getString();
  glyph["border_flags"] = BorderFlags(glyph_query);
  glyph["vertical_segments"] = glyph_query.getColumn("v_segments").getInt();

  SQLite::Statement capsule_query(
      db, "select border_t,border_b,border_l,border_r,v_segments,tl_x,tl_y,"
          "br_x,br_y from capsule where name = ?");
  capsule_query.bind(1, name);
  if (capsule_query.executeStep()) {
    glyph["capsule"] = {
        {"border_flags", BorderFlags(capsule_query)},
        {"vertical_segments", capsule_query.getColumn("v_segments").getInt()},
        {"top_left", {capsule_query.getColumn("tl_x").getDouble(),
                       capsule_query.getColumn("tl_y").getDouble()}},
        {"bottom_right", {capsule_query.getColumn("br_x").getDouble(),
                            capsule_query.getColumn("br_y").getDouble()}}};
  }

  glyph["strokes"] = json::array();
  SQLite::Statement stroke_query(
      db, "select weight,vertice from stroke where name = ? order by rowid");
  stroke_query.bind(1, name);
  SQLite::Statement vertex_query(
      db, "select json_extract(value, '$.type') as type, "
          "json_extract(value, '$.x') as x, json_extract(value, '$.y') as y, "
          "json_extract(value, '$.belong') as belong from json_each(?) "
          "order by cast(key as integer)");
  while (stroke_query.executeStep()) {
    json stroke = {{"weight", stroke_query.getColumn("weight").getDouble()},
                   {"vertices", json::array()}};
    vertex_query.reset();
    vertex_query.clearBindings();
    vertex_query.bind(1, stroke_query.getColumn("vertice").getString());
    while (vertex_query.executeStep()) {
      stroke["vertices"].push_back(
          {{"type", vertex_query.getColumn("type").getString()},
           {"x", vertex_query.getColumn("x").getDouble()},
           {"y", vertex_query.getColumn("y").getDouble()},
           {"belong", vertex_query.getColumn("belong").getString()}});
    }
    glyph["strokes"].push_back(std::move(stroke));
  }
  return glyph;
}

json LoadStyler(SQLite::Database& db, const std::string& name,
                const std::string& target) {
  json styler = {{"name", name}, {"vertices", json::array()}};
  SQLite::Statement query(
      db, "select type,pre_rotate,dir,forward,backward from " + target +
              " order by rowid");
  while (query.executeStep()) {
    const std::string forward = query.getColumn("forward").getString();
    const std::string backward = query.getColumn("backward").getString();
    styler["vertices"].push_back(
        {{"type", query.getColumn("type").getString()},
         {"pre_rotate", query.getColumn("pre_rotate").getDouble()},
         {"direction", query.getColumn("dir").getString()},
         {"forward", forward.empty() ? json::array() : json::parse(forward)},
         {"backward", backward.empty() ? json::array() : json::parse(backward)}});
  }
  return styler;
}

std::string FontExtension(const std::vector<std::uint8_t>& data) {
  if (data.size() >= 4 && data[0] == 'O' && data[1] == 'T' &&
      data[2] == 'T' && data[3] == 'O') {
    return ".otf";
  }
  return ".ttf";
}

int Migrate(const fs::path& source, const fs::path& output) {
  if (!fs::exists(source)) {
    std::cerr << "Database does not exist: " << source.string() << '\n';
    return 1;
  }
  if (fs::exists(output) && !fs::is_empty(output)) {
    std::cerr << "Output directory must not already contain files: "
              << output.string() << '\n';
    return 1;
  }

  SQLite::Database db(source.string(), SQLite::OPEN_READONLY);
  fs::create_directories(output);

  json library = {{"format_version", 1},
                  {"package_type", "jianzi-note-library"},
                  {"glyphs", json::object()},
                  {"aliases", json::array()}};
  SQLite::Statement glyph_list(db, "select name,type from jianzi order by name");
  while (glyph_list.executeStep()) {
    const std::string name = glyph_list.getColumn("name").getString();
    library["glyphs"][name] = LoadGlyph(db, name);
  }

  SQLite::Statement alias_query(db, "select alias,jianzi,type from jianzi_alias order by rowid");
  while (alias_query.executeStep()) {
    library["aliases"].push_back(
        {{"alias", alias_query.getColumn("alias").getString()},
         {"glyph", alias_query.getColumn("jianzi").getString()},
         {"type", alias_query.getColumn("type").getString()}});
  }

  SQLite::Statement styler_list(db, "select name,target from styler_list order by rowid");
  if (!styler_list.executeStep()) {
    throw std::runtime_error("The database contains no styler.");
  }
  const std::string styler_name = styler_list.getColumn("name").getString();
  const std::string styler_target = styler_list.getColumn("target").getString();
  if (styler_list.executeStep()) {
    throw std::runtime_error("The database contains multiple stylers; this "
                             "migration format accepts one.");
  }

  SQLite::Statement font_query(db, "select name,data,length(data) from font_data");
  if (!font_query.executeStep()) {
    throw std::runtime_error("The database contains no font data.");
  }
  const std::string font_name = font_query.getColumn("name").getString();
  const auto size = font_query.getColumn("length(data)").getUInt();
  const auto* blob = static_cast<const std::uint8_t*>(font_query.getColumn("data").getBlob());
  if (blob == nullptr || size <= 0) {
    throw std::runtime_error("The database contains empty font data.");
  }
  const std::vector<std::uint8_t> font(blob, blob + size);
  const std::string font_file = "source-han-serif" + FontExtension(font);
  std::ofstream font_output(output / fs::path(font_file), std::ios::binary);
  font_output.write(reinterpret_cast<const char*>(font.data()),
                    static_cast<std::streamsize>(font.size()));
  if (!font_output) {
    throw std::runtime_error("Unable to write font file.");
  }
  library["font"] = {{"name", font_name}, {"file", font_file}};
  library["styler"] = LoadStyler(db, styler_name, styler_target);
  if (!WriteDocument(output / "library.cbor", library)) {
    return 1;
  }

  // Exercise the public reader as an integrity check for the freshly emitted
  // package before reporting success.
  const auto loaded_library = qin::cbor::Read(output / "library.cbor");
  if (!loaded_library) {
    std::cerr << loaded_library.error->path.string() << ": "
              << loaded_library.error->message << '\n';
    return 1;
  }
  if (!loaded_library.document.contains("glyphs") ||
      loaded_library.document["glyphs"].size() != library["glyphs"].size()) {
    std::cerr << "Migrated library did not pass its integrity check.\n";
    return 1;
  }

  std::cout << "Migrated " << library["glyphs"].size() << " glyphs to "
            << output.string() << '\n';
  return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: jianzi-sqlite-to-cbor <source.db> <empty-output-dir>\n";
    return 1;
  }
  try {
    return Migrate(argv[1], argv[2]);
  } catch (const std::exception& error) {
    std::cerr << "Migration failed: " << error.what() << '\n';
    return 1;
  }
}
