// Temporary migration tool for JianziNote's legacy SQLite resource package.

#include "Cbor.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
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

// File names are ASCII-only yet derived reversibly from the UTF-8 glyph name.
// This avoids platform-specific Unicode path normalization while retaining a
// stable identity without introducing a separate numeric ID.
std::string GlyphFileStem(const std::string& utf8) {
  std::ostringstream result;
  result << "g";
  for (size_t i = 0; i < utf8.size();) {
    const auto byte = static_cast<std::uint8_t>(utf8[i++]);
    std::uint32_t codepoint = 0;
    size_t continuation_count = 0;
    if (byte < 0x80) {
      codepoint = byte;
    } else if ((byte & 0xE0) == 0xC0) {
      codepoint = byte & 0x1F;
      continuation_count = 1;
    } else if ((byte & 0xF0) == 0xE0) {
      codepoint = byte & 0x0F;
      continuation_count = 2;
    } else if ((byte & 0xF8) == 0xF0) {
      codepoint = byte & 0x07;
      continuation_count = 3;
    } else {
      throw std::runtime_error("Glyph name is not valid UTF-8.");
    }
    if (i + continuation_count > utf8.size()) {
      throw std::runtime_error("Glyph name is not valid UTF-8.");
    }
    for (size_t j = 0; j < continuation_count; ++j) {
      const auto next = static_cast<std::uint8_t>(utf8[i++]);
      if ((next & 0xC0) != 0x80) {
        throw std::runtime_error("Glyph name is not valid UTF-8.");
      }
      codepoint = (codepoint << 6) | (next & 0x3F);
    }
    result << "-u" << std::hex << std::nouppercase << std::setfill('0')
           << std::setw(codepoint <= 0xFFFF ? 4 : 6) << codepoint << std::dec;
  }
  return result.str();
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
                const std::string& target, const std::string& font_file,
                const std::string& font_name) {
  json styler = {{"format_version", 1},
                 {"name", name},
                 {"font", {{"name", font_name}, {"file", font_file}}},
                 {"vertices", json::array()}};
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
  fs::create_directories(output / "glyphs");
  fs::create_directories(output / "fonts");

  json manifest = {{"format_version", 1},
                   {"package_type", "jianzi-note-library"},
                   {"glyphs", json::array()},
                   {"aliases_file", "aliases.cbor"},
                   {"styler_file", "styler.cbor"}};
  SQLite::Statement glyph_list(db, "select name,type from jianzi order by name");
  while (glyph_list.executeStep()) {
    const std::string name = glyph_list.getColumn("name").getString();
    const std::string file = "glyphs/" + GlyphFileStem(name) + ".cbor";
    if (!WriteDocument(output / fs::path(file), LoadGlyph(db, name))) {
      return 1;
    }
    manifest["glyphs"].push_back(
        {{"name", name}, {"type", glyph_list.getColumn("type").getString()},
         {"file", file}});
  }

  json aliases = {{"format_version", 1}, {"aliases", json::array()}};
  SQLite::Statement alias_query(db, "select alias,jianzi,type from jianzi_alias order by rowid");
  while (alias_query.executeStep()) {
    aliases["aliases"].push_back(
        {{"alias", alias_query.getColumn("alias").getString()},
         {"glyph", alias_query.getColumn("jianzi").getString()},
         {"type", alias_query.getColumn("type").getString()}});
  }
  if (!WriteDocument(output / "aliases.cbor", aliases)) {
    return 1;
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
  const std::string font_file = "fonts/source-han-serif" + FontExtension(font);
  std::ofstream font_output(output / fs::path(font_file), std::ios::binary);
  font_output.write(reinterpret_cast<const char*>(font.data()),
                    static_cast<std::streamsize>(font.size()));
  if (!font_output) {
    throw std::runtime_error("Unable to write font file.");
  }
  manifest["font_file"] = font_file;

  if (!WriteDocument(output / "styler.cbor",
                     LoadStyler(db, styler_name, styler_target, font_file,
                                font_name)) ||
      !WriteDocument(output / "manifest.cbor", manifest)) {
    return 1;
  }

  // Exercise the public reader as an integrity check for the freshly emitted
  // package before reporting success.
  const auto loaded_manifest = qin::cbor::Read(output / "manifest.cbor");
  if (!loaded_manifest) {
    std::cerr << loaded_manifest.error->path.string() << ": "
              << loaded_manifest.error->message << '\n';
    return 1;
  }
  if (!loaded_manifest.document.contains("glyphs") ||
      loaded_manifest.document["glyphs"].size() != manifest["glyphs"].size()) {
    std::cerr << "Migrated manifest did not pass its integrity check.\n";
    return 1;
  }

  std::cout << "Migrated " << manifest["glyphs"].size() << " glyphs to "
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
