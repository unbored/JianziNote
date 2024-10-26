// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "Jianzi.hpp"

#include <tinyutf8/tinyutf8.h>

#include <algorithm>
#include <cfloat>
#include <functional>
#include <iostream>
#include <magic_enum.hpp>
#include <set>
#include <stack>
#include <string>

#define JE(key) "json_extract(value, '$." #key "') as " #key

namespace qin

{

std::unique_ptr<SQLite::Database> Jianzi::s_db;
std::vector<Jianzi::JianziInfo> Jianzi::s_alias_list;
std::vector<Jianzi::JianziInfo> Jianzi::s_jianzi_list;

void Jianzi::OpenDb(const char *file) {
  // 打开数据库
  try {
    s_db = std::make_unique<SQLite::Database>(file);
  } catch (SQLite::Exception e) {
    std::cout << e.what() << std::endl;
    throw e;
  }
  // 加载减字和别名列表
  try {
    SQLite::Statement jianzi_query(*s_db, "select name,type from jianzi");
    while (jianzi_query.executeStep()) {
      JianziInfo info;
      info.name = jianzi_query.getColumn("name").getString();
      info.type = magic_enum::enum_cast<JianziType>(
                      jianzi_query.getColumn("type").getString())
                      .value_or(JianziType::Other);
      s_jianzi_list.push_back(info);
    }
    SQLite::Statement alias_query(*s_db, "select alias,type from jianzi_alias");
    while (alias_query.executeStep()) {
      JianziInfo info;
      info.name = alias_query.getColumn("alias").getString();
      info.type = magic_enum::enum_cast<JianziType>(
                      alias_query.getColumn("type").getString())
                      .value_or(JianziType::Other);
      s_alias_list.push_back(info);
    }
  } catch (SQLite::Exception e) {
    std::cout << e.what() << std::endl;
    throw e;
  }

  // 优先根据字符长短排序
  auto JianziInfoComp = [](const JianziInfo &j1, const JianziInfo &j2) {
    if (j1.name.length() != j2.name.length()) {
      return j1.name.length() > j2.name.length();
    } else {
      return j1.name > j2.name;
    }
  };

  std::sort(s_jianzi_list.begin(), s_jianzi_list.end(), JianziInfoComp);
  std::sort(s_alias_list.begin(), s_alias_list.end(), JianziInfoComp);
}

Jianzi::Jianzi(const char *u8_ch) {
  m_name = u8_ch;

  if (m_name == "") {
    // 空名字，无需加载
    *this = Jianzi();
    return;
  }

  tiny_utf8::string ch(u8_ch);

  // std::cout << "New Jianzi: " << ch << std::endl;

  ch = "\"" + ch + "\"";

  try {
    // 查询指定名称的减字
    SQLite::Statement jianzi_query(
        *s_db, ("select * from jianzi where name = " + ch).c_str());
    if (jianzi_query.executeStep()) {
      m_border_flags.t = jianzi_query.getColumn("border_t").getInt();
      m_border_flags.b = jianzi_query.getColumn("border_b").getInt();
      m_border_flags.l = jianzi_query.getColumn("border_l").getInt();
      m_border_flags.r = jianzi_query.getColumn("border_r").getInt();

      m_v_segments = jianzi_query.getColumn("v_segments").getInt();

      m_type = magic_enum::enum_cast<JianziType>(
                   jianzi_query.getColumn("type").getString())
                   .value_or(JianziType::Other);
    } else {
      return;
    }
  } catch (SQLite::Exception e) {
    std::cout << "Error querying jianzi: " << e.what() << std::endl;
    return;
  }

  try {
    // 查询指定名称的capsule，如果存在，则该减字具有可嵌入空间
    SQLite::Statement capsule_query(
        *s_db, ("select * from capsule where name = " + ch).c_str());
    if (capsule_query.executeStep()) {
      m_capsule = std::make_unique<Capsule>();
      m_capsule->border_flags.t = capsule_query.getColumn("border_t").getInt();
      m_capsule->border_flags.b = capsule_query.getColumn("border_b").getInt();
      m_capsule->border_flags.l = capsule_query.getColumn("border_l").getInt();
      m_capsule->border_flags.r = capsule_query.getColumn("border_r").getInt();

      m_capsule->v_segments = capsule_query.getColumn("v_segments").getInt();

      m_capsule->tl.x = capsule_query.getColumn("tl_x").getDouble();
      m_capsule->tl.y = capsule_query.getColumn("tl_y").getDouble();
      m_capsule->br.x = capsule_query.getColumn("br_x").getDouble();
      m_capsule->br.y = capsule_query.getColumn("br_y").getDouble();
    }
  } catch (SQLite::Exception e) {
    std::cout << "Error querying capsule: " << e.what() << std::endl;
  }

  try {
    m_node = std::make_unique<Node>();

    // 查询指定名称的stroke，得到笔画列表
    SQLite::Statement stroke_query(
        *s_db, ("select * from stroke where name = " + ch).c_str());
    while (stroke_query.executeStep()) {
      Stroke stroke;
      stroke.weight = stroke_query.getColumn("weight").getDouble();
      std::string vertice = stroke_query.getColumn("vertice").getString();

      // 根据vertice的json字串，获得所有的顶点数据
      SQLite::Statement vertex_query(
          *s_db, "select " JE(type) ", " JE(x) ", " JE(y) ", " JE(
                     belong) " from json_each( ? )");
      vertex_query.bind(1, vertice);
      while (vertex_query.executeStep()) {
        StrokeVertex v;
        v.type = magic_enum::enum_cast<VertexType>(
                     vertex_query.getColumn("type").getString())
                     .value_or(VertexType::None);
        v.pt.x = vertex_query.getColumn("x").getDouble();
        v.pt.y = vertex_query.getColumn("y").getDouble();
        v.belong = magic_enum::enum_cast<VertexBelong>(
                       vertex_query.getColumn("belong").getString())
                       .value_or(VertexBelong::Top);
        stroke.vertice.push_back(v);
      }
      // 查找完成记得将笔画存入
      m_node->strokes.push_back(stroke);
    }
  } catch (SQLite::Exception e) {
    std::cout << "Error querying stroke: " << e.what() << std::endl;
  }
}

Jianzi::Jianzi(const Jianzi &other) { *this = other; }

Jianzi &Jianzi::operator=(const Jianzi &other) {
  if (this == &other) {
    return *this;
  }
  m_name = other.m_name;
  m_type = other.m_type;
  // m_strokes = other.m_strokes;
  if (other.m_node) {
    m_node = Node::Clone(*other.m_node);
  } else {
    m_node.reset();
  }
  m_border_flags = other.m_border_flags;
  m_v_segments = other.m_v_segments;
  if (other.m_capsule) {
    m_capsule = std::make_unique<Capsule>();
    *m_capsule = *other.m_capsule;
  } else {
    m_capsule.reset();
  }
  return *this;
}

struct JianziOperator {
  std::function<Jianzi(const Jianzi &, const Jianzi &)> op;
  size_t priority;
};

const std::map<char32_t, JianziOperator> c_jianzi_operators = {
    {U'(', {{}, 0}},
    {U')', {{}, 0}},
    {U'\'', {{}, 0}},

    {U'^', {[](const Jianzi &j1, const Jianzi &j2) { return j1 ^ j2; }, 1}},

    {U'|', {[](const Jianzi &j1, const Jianzi &j2) { return j1 | j2; }, 2}},

    {U'&', {[](const Jianzi &j1, const Jianzi &j2) { return j1 & j2; }, 3}},
    {U'<', {[](const Jianzi &j1, const Jianzi &j2) { return j1 < j2; }, 3}},
    {U'/', {[](const Jianzi &j1, const Jianzi &j2) { return j1 / j2; }, 3}},

    {U'*', {[](const Jianzi &j1, const Jianzi &j2) { return j1 * j2; }, 4}},
};

Jianzi Jianzi::Parse(const char *u8_str) {
  Jianzi result;

  using tiny_utf8::string;
  string str(u8_str);

  // 清除所有空格
  auto ws_pos = str.find(U' ');
  while (ws_pos != str.npos) {
    str = str.erase(ws_pos);
    ws_pos = str.find(U' ');
  }

  // 特殊处理：!标记紧跟单个字符，给出名字后直接返回
  if (!str.empty() && str[0] == U'!') {
    result.m_name = str.cpp_str();
    return result;
  }

  // 前后添加括号，有利于后续处理
  str = "(" + str + ")";

  std::stack<Jianzi> values;
  std::stack<char32_t> operators;

  // 定义使用运算符op进行运算的函数
  auto Calc = [&values](char32_t op) {
    // std::cout << "Doing operation " << string(op) << std::endl;
    if (values.size() < 2) {
      // 减字已不足两个，算式有误，返回空
      return false;
    }

    auto v2 = values.top();
    values.pop();
    auto v1 = values.top();
    values.pop();

    values.push(c_jianzi_operators.at(op).op(v1, v2));

    return true;
  };

  while (str.length() > 0) {
    // std::cout << str << std::endl;
    // 寻找运算符的位置，获得运算及优先级
    auto sub_pos = std::find_if(str.begin(), str.end(), [](char32_t c) {
      return c_jianzi_operators.find(c) != c_jianzi_operators.end();
    });
    if (sub_pos != str.end()) {
      // 找到一个运算符，开始处理
      if (sub_pos != str.begin()) {
        // 不在字符串开头，先处理值
        string sub = str.substr(0, sub_pos - str.begin());

        // 确认减字存在
        try {
          SQLite::Statement jianzi_query(
              *s_db, ("select 1 from jianzi where name = '" + sub + "' limit 1")
                         .c_str());
          if (jianzi_query.executeStep()) {
            // 减字存在，直接存入
            values.push(Jianzi(sub.c_str()));
          } else {
            SQLite::Statement alias_query(
                *s_db, ("select jianzi from jianzi_alias where alias = '" +
                        sub + "' limit 1")
                           .c_str());
            if (alias_query.executeStep()) {
              // 有别名，将别名替换本体后重新检索
              string alias = alias_query.getColumn("jianzi").getString();
              // 补个括号，保证正常运行
              str = "(" + alias + ")" + str.substr(sub_pos - str.begin());
              continue;
            } else {
              // 都找不到，直接返回空
              return Jianzi();
            }
          }
        } catch (...) {
        }
      }

      // 处理运算符
      char32_t c = *sub_pos;  // 当前字符

      if (c == U'(') {
        // 左括号，直接入栈
        operators.push(c);
      } else if (c == U')') {
        // 右括号，直到遇到左括号为止，进行计算
        char32_t op = 0;
        while (!operators.empty()) {
          op = operators.top();
          operators.pop();
          if (op == U'(') {
            // 遇到左括号停止
            break;
          } else {
            // 进行运算
            if (!Calc(op)) {
              return Jianzi();
            }
          }
        }
        if (op != U'(') {
          // 最终没有停在左括号上，算式有误，返回空
          return Jianzi();
        }
      } else if (c == U'\'') {
        // 引用，寻找配对引用并置换
        size_t curr_quote = sub_pos - str.begin();
        size_t next_quote = str.find(U'\'', curr_quote + 1);
        if (next_quote == str.npos) {
          // 没有配对，算式有误，返回空
          return Jianzi();
        }
        // 生成算式
        string sub = str.substr(curr_quote + 1, next_quote - 1);
        string rep = Jianzi::ParseNatural(sub.c_str());
        // 替换内容
        str = "(" + rep + ")" + str.substr(next_quote + 1);
        continue;
      } else {
        // 普通算符

        if (operators.empty() ||
            c_jianzi_operators.at(c).priority >
                c_jianzi_operators.at(operators.top()).priority) {
          // 无旧运算符，或新运算符优先级更高
          operators.push(c);
        } else {
          // 旧运算符优先级更高或平级
          // 先将旧运算符出栈进行运算
          char32_t op = operators.top();
          operators.pop();
          // 进行运算
          if (!Calc(op)) {
            return Jianzi();
          }
          // 再将新运算符入栈
          operators.push(c);
        }
      }
      // 仅保留找到的运算符之后的字符串
      str = str.substr(sub_pos - str.begin() + 1);
    } else {
      break;
    }
  }
  if (str.length() > 0) {
    // 已经没有运算符，剩下的是值
    values.push(Jianzi(str.c_str()));
  }

  // 将剩下的运算符算完
  while (!operators.empty()) {
    char32_t op = operators.top();
    operators.pop();
    // 进行运算
    if (!Calc(op)) {
      return Jianzi();
    }
  }

  // values里剩下的唯一一个值就是结果
  // assert(values.size() <= 1);
  if (!values.empty()) {
    result = values.top();
  }
  return result;
}

std::string Jianzi::ParseNatural(const char *u8_str) {
  using namespace tiny_utf8;

  // 初始化输入
  string input(u8_str);
  // 清除所有空格
  auto ws_pos = input.find(U' ');
  while (ws_pos != input.npos) {
    input = input.erase(ws_pos);
    ws_pos = input.find(U' ');
  }
  size_t input_length = input.length();

  if (input_length == 0) {
    // 无字符
    return std::string();
  } else if (input.front() == U'(' && input.back() == U')') {
    // 内容为公式，原样返回
    return std::string(u8_str);
  }

  // 检索标记
  std::vector<bool> input_marks(input_length, false);
  std::vector<JianziInfo> info_list(input_length);

  auto MarkInput = [&input, &input_marks,
                    &info_list](const std::vector<JianziInfo> &input_list) {
    for (auto &info : input_list) {
      size_t info_length = info.name.length();
      // 寻找所有点位
      auto pos = input.find(info.name);
      while (pos != input.npos) {
        // 找到一个，先确认所有位置为空
        bool occupied = false;
        for (size_t i = pos; i < pos + info_length; ++i) {
          if (input_marks[i]) {
            occupied = true;
            break;
          }
        }
        if (occupied) {
          // 位置已被占用，寻找下一个
          pos = input.find(info.name, pos + info_length);
          continue;
        }
        // 标记占用
        for (size_t i = pos; i < pos + info_length; ++i) {
          input_marks[i] = true;
        }
        info_list[pos] = info;
        // 寻找下一个
        pos = input.find(info.name, pos + info_length);
      }
    }
  };

  // 先对别名进行标记
  MarkInput(s_alias_list);
  // 再标记减字
  MarkInput(s_jianzi_list);

  if (input_length == 1 && !input_marks[0]) {
    // 如果输入为单个字且不在减字列表当中，标记!符号并在后续处理
    return "!" + std::string(u8_str);
  } else {
    // 检查是否所有位置均已处理
    for (auto m : input_marks) {
      if (!m) {
        // 输入有问题，返回空
        return std::string();
      }
    }
  }

  // 把标记的空位移除，虽然可能比较耗时，但省去后续处理的麻烦
  auto ws_iter = info_list.begin();
  while (ws_iter != info_list.end()) {
    if (ws_iter->name.empty()) {
      ws_iter = info_list.erase(ws_iter);
    } else {
      ++ws_iter;
    }
  }

  // 开始根据规则进行运算符号拼装
  string ret;

  // 处理连串数字。左闭右开区间
  auto ProcessNumbers = [&](size_t start_pos, size_t end_pos) {
    size_t p = start_pos;
    ret += "(" + info_list[p++].name;
    while (p < end_pos) {
      if (info_list[p].type != JianziType::Number) {
        // 后方已不是数字，终止
        break;
      }
      ret += "/" + info_list[p++].name;
    }
    ret += ")";

    return p;
  };

  // 当该字为左手指法时，向后合并数字
  // auto ProcessLeft = [&](size_t start_pos, size_t end_pos) {
  //     size_t p = start_pos;
  //     ret += info_list[p++].name;

  //     if (info_list[start_pos].type == JianziType::LeftAlone)
  //     {
  //         // 单独指法，返回
  //         return p;
  //     }

  //     if (p > end_pos || info_list[p].type != JianziType::Number)
  //     {
  //         // 后方无内容或不是数字则返回
  //         return p;
  //     }
  //     // 堆数字
  //     ret += "&";
  //     p = ProcessNumbers(p, end_pos);

  //     return p;
  // };

  size_t pos = 0;
  JianziType prev_type;
  while (pos < info_list.size()) {
    if (pos > 0) {
      // 跟着前面的指法，加一个运算符
      if (prev_type == JianziType::Left || prev_type == JianziType::LeftAlone) {
        // if (info_list[pos].type == JianziType::GraceAbove)
        // {
        //     // 把修饰符归于上半，比例会更好看一些
        //     ret += "/";
        // }
        // else
        // {
        ret += "^";
        // }
      }
      // else if (prev_type == JianziType::GraceAbove)
      // {
      //     ret += "^";
      // }
      else if (prev_type == JianziType::GraceSide) {
        ret += "<";
      } else {
        ret += "/";
      }
    }
    prev_type = info_list[pos].type;  // 为下一个字做准备

    ret += info_list[pos++].name;

    // 左手指法
    if (prev_type == JianziType::Left) {
      // 后续是数字，处理数字
      if (pos < info_list.size() && info_list[pos].type == JianziType::Number) {
        ret += "&";
        pos = ProcessNumbers(pos, info_list.size());
      }
    }

    // 主字
    else if (prev_type == JianziType::Main) {
      if (pos < info_list.size() && info_list[pos].type == JianziType::Number) {
        // 后续跟的是数字，全部放入
        ret += "*";
        pos = ProcessNumbers(pos, info_list.size());
      }
    }

    // 复杂减字
    else if (prev_type == JianziType::MainComplex ||
             prev_type == JianziType::MainShu) {
      // 后续还有内容
      if (pos < info_list.size()) {
        // 确定运算符
        string op = "&";
        if (prev_type == JianziType::MainShu) {
          op = "|";
        }

        // 后续跟的是数字，为弦号
        if (info_list[pos].type == JianziType::Number) {
          // 对于有竖笔的减字，单个数字应当放在下方。因此需要先计算数字个数
          size_t number_count = 0;
          size_t n = pos;
          while (n < info_list.size()) {
            if (info_list[n++].type == JianziType::Number) {
              ++number_count;
            } else {
              break;
            }
          }
          if (number_count == 1 && prev_type == JianziType::MainShu) {
            ret += "/(" + info_list[pos++].name;
          } else {
            ret += "*(" + info_list[pos++].name;
          }
          // 后续继续跟的是数字，再取一个
          if (pos < info_list.size() &&
              info_list[pos].type == JianziType::Number) {
            ret += op + info_list[pos++].name;
          }
          ret += ")";
        }
        // 后续跟的是左手指法
        else if (info_list[pos].type == JianziType::Left ||
                 info_list[pos].type == JianziType::LeftAlone) {
          // 多加一个括号便于处理
          ret += "*((" + info_list[pos++].name;
          // 后续为左手指法，先统计指法后的数字个数，应当留出一个数字作为弦
          size_t number_count = 0;
          size_t n = pos;
          while (n < info_list.size()) {
            if (info_list[n++].type == JianziType::Number) {
              ++number_count;
            } else {
              break;
            }
          }
          if (number_count > 1) {
            ret += "&";
            // 留下最后一个数字
            pos = ProcessNumbers(pos, pos + number_count - 1);
            ret += "/" + info_list[pos++].name;
          } else if (number_count == 1) {
            ret += "/" + info_list[pos++].name;
          }
          ret += ")";

          // 后续再接一个左手指法
          if (pos < info_list.size() &&
              (info_list[pos].type == JianziType::Left ||
               info_list[pos].type == JianziType::LeftAlone)) {
            ret += op + "(" + info_list[pos++].name;
            // 再来一次：后续为左手指法，先统计指法后的数字个数，应当留出一个数字作为弦
            size_t number_count = 0;
            size_t n = pos;
            while (n < info_list.size()) {
              if (info_list[n++].type == JianziType::Number) {
                ++number_count;
              } else {
                break;
              }
            }
            if (number_count > 1) {
              ret += "&";
              // 留下最后一个数字
              pos = ProcessNumbers(pos, pos + number_count - 1);
              ret += "/" + info_list[pos++].name;
            } else if (number_count == 1) {
              ret += "/" + info_list[pos++].name;
            }
            ret += ")";
          }
          // 把括号对上
          ret += ")";
        }
      }
    }
  }

  return ret.cpp_str();
}

float CalculateZoom(size_t stroke_num) {
  // return 1 - 0.013f * ((int)stroke_num - 1);
  return 0.4f * std::pow(1.1f, (1 - (float)stroke_num)) + 0.6f;
}

Jianzi Jianzi::operator&(const Jianzi &right) const {
  Jianzi ret;
  // 名称：公式
  ret.m_name = "(" + m_name + "&" + right.m_name + ")";

  float border_flag = 0.25f;  // 默认退0.25个
  if (m_border_flags.r != right.m_border_flags.l) {
    // 一方有边界，各退半个笔画宽
    border_flag = 0.5f;
  } else if (m_border_flags.r && right.m_border_flags.l) {
    // 两方有边界，各退一个笔画宽
    border_flag = 1.0f;
  }

  ret.m_node = std::make_unique<Node>();
  // 当前减字
  {
    ret.m_node->first = Node::Clone(*m_node);
    Node &node = *ret.m_node->first;
    // 将右侧归一化
    Normalize(node, NormalizeDirection::Right);
    // 先处理边界
    node.outer_border.r = border_flag;
    // 每个box均压缩至一半
    node.box.w = 0.5f;
  }
  // 右侧减字
  {
    ret.m_node->second = Node::Clone(*right.m_node);
    Node &node = *ret.m_node->second;
    // 将左侧归一化
    Normalize(node, NormalizeDirection::Left);
    // 先处理边界
    node.outer_border.l = border_flag;
    // 压缩至一半后加到右侧
    node.box.x = 0.5f;
    node.box.w = 0.5f;
  }

  // 处理整个减字边界
  ret.m_border_flags.t = m_border_flags.t || right.m_border_flags.t;
  ret.m_border_flags.b = m_border_flags.b || right.m_border_flags.b;
  ret.m_border_flags.l = m_border_flags.l;
  ret.m_border_flags.r = right.m_border_flags.r;

  // 分段数取大者
  ret.m_v_segments =
      m_v_segments > right.m_v_segments ? m_v_segments : right.m_v_segments;

  return ret;
}

Jianzi Jianzi::operator|(const Jianzi &right) const {
  Jianzi ret;
  // 名称：公式
  ret.m_name = "(" + m_name + "|" + right.m_name + ")";

  ret.m_node = std::make_unique<Node>();

  float border_flag = 0.0f;
  if (m_border_flags.r) {
    // 有边界，退2个笔画宽
    border_flag = 2.0f;
  } else {
    // 无边界，也得退一个笔画
    border_flag = 1.0f;
  }
  // 当前减字
  {
    ret.m_node->first = Node::Clone(*m_node);
    Node &node = *ret.m_node->first;
    // 将右侧归一化
    Normalize(node, NormalizeDirection::Right);
    // 先处理边界
    node.outer_border.r = border_flag;
    // 每个box均压缩至一半
    node.box.w = 0.5f;
  }

  if (right.m_border_flags.l) {
    // 有边界，退2个笔画宽
    border_flag = 2.0f;
  } else {
    // 无边界，也得退一个笔画
    border_flag = 1.0f;
  }
  // 右侧减字
  {
    ret.m_node->second = Node::Clone(*right.m_node);
    Node &node = *ret.m_node->second;
    // 将左侧归一化
    Normalize(node, NormalizeDirection::Left);
    // 先处理边界
    node.outer_border.l = border_flag;
    // 压缩至一半后加到右侧
    node.box.x = 0.5f;
    node.box.w = 0.5f;
  }

  // 处理整个减字边界
  ret.m_border_flags.t = m_border_flags.t || right.m_border_flags.t;
  ret.m_border_flags.b = m_border_flags.b || right.m_border_flags.b;
  ret.m_border_flags.l = m_border_flags.l;
  ret.m_border_flags.r = right.m_border_flags.r;

  // 分段数取大者
  ret.m_v_segments =
      m_v_segments > right.m_v_segments ? m_v_segments : right.m_v_segments;

  return ret;
}

Jianzi Jianzi::operator<(const Jianzi &right) const {
  // 基本与&相同，除了比例
  Jianzi ret;
  // 名称：公式
  ret.m_name = "(" + m_name + "<" + right.m_name + ")";

  float border_flag = 0.25f;  // 默认退0.25个
  if (m_border_flags.r != right.m_border_flags.l) {
    // 一方有边界，各退半个笔画宽
    border_flag = 0.5f;
  } else if (m_border_flags.r && right.m_border_flags.l) {
    // 两方有边界，各退一个笔画宽
    border_flag = 1.0f;
  }

  ret.m_node = std::make_unique<Node>();
  // 当前减字
  {
    ret.m_node->first = Node::Clone(*m_node);
    Node &node = *ret.m_node->first;
    // 考虑到此类减字较为特殊，将左右侧都归一化
    Normalize(node, NormalizeDirection::Left);
    Normalize(node, NormalizeDirection::Right);
    // 先处理边界
    node.outer_border.r = border_flag;
    // 每个box均压缩至一半
    node.box.w = 0.3f;
  }
  // 右侧减字
  {
    ret.m_node->second = Node::Clone(*right.m_node);
    Node &node = *ret.m_node->second;
    // 将左侧归一化
    Normalize(node, NormalizeDirection::Left);
    // 先处理边界
    node.outer_border.l = border_flag;
    // 压缩至一半后加到右侧
    node.box.x = 0.3f;
    node.box.w = 0.7f;
  }

  // 处理整个减字边界
  ret.m_border_flags.t = m_border_flags.t || right.m_border_flags.t;
  ret.m_border_flags.b = m_border_flags.b || right.m_border_flags.b;
  ret.m_border_flags.l = m_border_flags.l;
  ret.m_border_flags.r = right.m_border_flags.r;

  // 分段数取大者
  ret.m_v_segments =
      m_v_segments > right.m_v_segments ? m_v_segments : right.m_v_segments;

  return ret;
}

Jianzi Jianzi::operator/(const Jianzi &below) const {
  Jianzi ret;
  // 名称：公式
  ret.m_name = "(" + m_name + "/" + below.m_name + ")";

  // 根据分段数确定是否需要加一个segment
  int add_segment = 0;
  float border_flag = 0.25f;  // 默认退0.25个
  if (m_border_flags.b != below.m_border_flags.t) {
    // 一方有边界，各退半个笔画宽
    border_flag = 0.5f;
  } else if (m_border_flags.b && below.m_border_flags.t) {
    // 两方有边界，插入一个新的分段
    add_segment = 1;
    // 插入新分段后不必再退
    border_flag = 0;
  }
  int total_segment = m_v_segments + below.m_v_segments + add_segment;

  ret.m_node = std::make_unique<Node>();
  // 当前减字
  {
    ret.m_node->first = Node::Clone(*m_node);
    Node &node = *ret.m_node->first;
    // 将下侧归一化
    Normalize(node, NormalizeDirection::Bottom);
    // 先处理边界
    node.outer_border.b = border_flag;
    // 每个box均压缩
    node.box.h = (float)m_v_segments / (float)total_segment;
  }
  // 下方减字
  {
    ret.m_node->second = Node::Clone(*below.m_node);
    Node &node = *ret.m_node->second;
    // 将上侧归一化
    Normalize(node, NormalizeDirection::Top);
    // 先处理边界
    node.outer_border.t = border_flag;
    // 压缩
    node.box.y = (float)(m_v_segments + add_segment) / (float)total_segment;
    node.box.h = (float)(below.m_v_segments) / (float)total_segment;
  }

  // 处理整个减字边界
  ret.m_border_flags.t = m_border_flags.t;
  ret.m_border_flags.b = below.m_border_flags.b;
  ret.m_border_flags.l = m_border_flags.l || below.m_border_flags.l;
  ret.m_border_flags.r = m_border_flags.r || below.m_border_flags.r;

  // 分段数取和
  ret.m_v_segments = total_segment;

  return ret;
}

Jianzi Jianzi::operator^(const Jianzi &below) const {
  // 令上方减字不大于下方
  Jianzi above = *this;
  if (above.m_v_segments > below.m_v_segments) {
    above.m_v_segments = below.m_v_segments;
  }
  // 上方减字也不能太小
  else if (above.m_v_segments < below.m_v_segments * 0.3f) {
    above.m_v_segments = std::round(below.m_v_segments * 0.3f);
  }
  Jianzi ret = above / below;
  // 名称：公式
  ret.m_name = "(" + m_name + "^" + below.m_name + ")";

  return ret;
}

Jianzi Jianzi::operator*(const Jianzi &content) const {
  // 没有capsule的情况下返回"/"的结果
  if (!m_capsule) {
    return *this / content;
  }

  Jianzi ret;
  // 名称：公式
  ret.m_name = "(" + m_name + "*" + content.m_name + ")";

  // 计算内容放进减字后实际占用的segment
  int content_segments = content.m_v_segments;
  if (m_capsule->border_flags.t && content.m_border_flags.t) {
    content_segments++;
  }
  if (m_capsule->border_flags.b && content.m_border_flags.b) {
    content_segments++;
  }
  // 根据填充减字的segment不同，本体可能需要缩放也可能不需要
  if (m_capsule->v_segments > content_segments) {
    content_segments = m_capsule->v_segments;
  }
  int final_segments = m_v_segments + content_segments - m_capsule->v_segments;

  // 计算包围框缩放后的位置
  // 包围框上方点跟随外减字上方走
  Point2f tl = m_capsule->tl;
  tl.y *= (float)m_v_segments / (float)final_segments;
  // 下方点则跟随下方走
  Point2f br = m_capsule->br;
  br.y = 1.0f - br.y;
  br.y *= (float)m_v_segments / (float)final_segments;
  br.y = 1.0f - br.y;

  ret.m_node = std::make_unique<Node>();
  // 当前减字
  {
    // 后续的笔画粗细调整并不适用于*运算的本字。在此预先计算
    // 调整面积与segment的调整有关。但不应缩放得太厉害
    float area = (float)m_v_segments / (float)final_segments;
    float weight = 0.5f * std::sqrt(area) + 0.5f;

    ret.m_node->first = Node::Clone(*m_node);
    Node &node = *ret.m_node->first;
    for (auto &s : node.strokes) {
      s.weight *= weight;
      for (auto &v : s.vertice) {
        switch (v.belong) {
          case VertexBelong::Top:
            v.pt.y *= (float)m_v_segments / (float)final_segments;
            break;
          case VertexBelong::Bottom:
            // 上下颠倒后再计算
            v.pt.y = 1.0f - v.pt.y;
            v.pt.y *= (float)m_v_segments / (float)final_segments;
            // 记得倒回来
            v.pt.y = 1.0f - v.pt.y;
            break;
          case VertexBelong::Medium:
            // 计算中间点坐标
            // 中间点坐标实际上跟着包围框变化
            v.pt.y = (v.pt.y - m_capsule->tl.y) /
                         (m_capsule->br.y - m_capsule->tl.y) * (br.y - tl.y) +
                     tl.y;
            break;
          default:
            break;
        }
      }
    }
  }

  // 处理包围框内的笔画
  // 根据segment的情况调整包围框大小
  if (m_capsule->border_flags.t && content.m_border_flags.t) {
    tl.y += 1.0f / (float)final_segments;
  }
  if (m_capsule->border_flags.b && content.m_border_flags.b) {
    br.y -= 1.0f / (float)final_segments;
  }

  // 把content的box扔到capsule范围内
  {
    ret.m_node->second = Node::Clone(*content.m_node);
    Node &node = *ret.m_node->second;
    // 根据情况把content四周归一化
    if (m_capsule->border_flags.t || tl.y > 0) {
      Normalize(node, NormalizeDirection::Top);
    }
    if (m_capsule->border_flags.b || br.y < 1.0f) {
      Normalize(node, NormalizeDirection::Bottom);
    }
    if (m_capsule->border_flags.l || tl.x > 0) {
      Normalize(node, NormalizeDirection::Left);
    }
    if (m_capsule->border_flags.r || br.x < 1.0f) {
      Normalize(node, NormalizeDirection::Right);
    }

    // 先关注capsule的边界情况。
    if (m_capsule->border_flags.l) {
      if (content.m_border_flags.l) {
        node.outer_border.l = 2.0f;
      } else {
        node.outer_border.l = 1.0f;
      }
    }
    if (m_capsule->border_flags.r) {
      if (content.m_border_flags.r) {
        node.outer_border.r = 2.0f;
      } else {
        node.outer_border.r = 1.0f;
      }
    }

    if (m_capsule->border_flags.t) {
      if (content.m_border_flags.t) {
        // 已经处理过间隔
        node.outer_border.t = 0;
      } else {
        node.outer_border.t = 1.0f;
      }
    }
    if (m_capsule->border_flags.b) {
      if (content.m_border_flags.b) {
        // 已经处理过间隔
        node.outer_border.b = 0;
      } else {
        node.outer_border.b = 1.0f;
      }
    }

    // 再缩放
    node.box.x = tl.x;
    node.box.y = tl.y;
    node.box.w = br.x - tl.x;
    node.box.h = br.y - tl.y;
  }

  // 处理边界
  ret.m_border_flags.l = m_border_flags.l;
  ret.m_border_flags.r = m_border_flags.r;
  ret.m_border_flags.t = m_border_flags.t;
  ret.m_border_flags.b = m_border_flags.b;
  // 根据content的边界情况更新
  if (tl.x == 0) {
    ret.m_border_flags.l |= content.m_border_flags.l;
  }
  if (std::abs(br.x - 1.0f) < FLT_EPSILON) {
    ret.m_border_flags.r |= content.m_border_flags.r;
  }
  if (tl.y == 0) {
    ret.m_border_flags.t |= content.m_border_flags.t;
  }
  if (std::abs(br.y - 1.0f) < FLT_EPSILON) {
    ret.m_border_flags.b |= content.m_border_flags.b;
  }

  // 处理间距
  ret.m_v_segments = final_segments;

  return ret;
}

std::vector<JianziStyler::PathData> qin::Jianzi::RenderPath(
    const JianziStyler &styler) const {
  tiny_utf8::string name = m_name;
  if (name.length() == 2 && name[0] == U'!') {
    return styler.RenderChar(name[1]);
  }
  std::vector<Stroke> strokes;

  float stroke_width = styler.GetStrokeWidth();

  if (m_node) {
    CollectStrokes(
        *m_node,
        BoundingBox{HALF_STROKE_WIDTH, HALF_STROKE_WIDTH,
                    1.0f - MAX_STROKE_WIDTH, 1.0f - MAX_STROKE_WIDTH},
        stroke_width, strokes);
  }

  return styler.RenderPath(strokes);
}

const char *Jianzi::GetName() const { return m_name.c_str(); }

Jianzi::BorderFlags Jianzi::GetBorderFlags() const { return m_border_flags; }

int Jianzi::GetSegments() const { return m_v_segments; }

BoundingBox Jianzi::TightBoundingBox(const Node &node) {
  // 获得所有笔画
  std::vector<Stroke> strokes;
  CollectStrokes(node, BoundingBox(), 0.1f, strokes);

  // 获取坐标最大最小值
  float x_min = 1.0f, x_max = 0.0f, y_min = 1.0f, y_max = 0.0f;
  for (auto &s : strokes) {
    for (auto &v : s.vertice) {
      if (v.pt.x < x_min) {
        x_min = v.pt.x;
      }
      if (v.pt.x > x_max) {
        x_max = v.pt.x;
      }
      if (v.pt.y < y_min) {
        y_min = v.pt.y;
      }
      if (v.pt.y > y_max) {
        y_max = v.pt.y;
      }
    }
  }

  // 生成包围盒
  BoundingBox box;
  if (x_min < x_max) {
    box.x = x_min;
    box.w = x_max - x_min;
  }
  if (y_min < y_max) {
    box.y = y_min;
    box.h = y_max - y_min;
  }

  return box;
}

void Jianzi::CollectStrokes(const Node &node, const BoundingBox &parent_box,
                            float stroke_width, std::vector<Stroke> &strokes) {
  // 求实际box大小
  BoundingBox box = parent_box * node.box;

  // TODO: 计算box面积，求缩放系数
  float area = box.w * box.h;
  // 如果有子节点，合理预估实际计算area时按一半来计算
  if (node.first || node.second) {
    area *= 0.5f;
  }
  float weight = 0.7f * std::sqrt(area) + 0.3f;

  box.x += node.outer_border.l * stroke_width * weight;
  box.y += node.outer_border.t * stroke_width * weight;
  box.w -= (node.outer_border.l + node.outer_border.r) * stroke_width * weight;
  box.h -= (node.outer_border.t + node.outer_border.b) * stroke_width * weight;

  // 计算笔画实际大小
  for (auto &s : node.strokes) {
    Stroke ns = s;
    // 权重
    ns.weight *= weight;
    // 笔画缩放
    for (auto &v : ns.vertice) {
      v.pt = box * v.pt;
    }
    strokes.push_back(ns);
  }

  // 获取各个子node的笔画
  if (node.first) {
    CollectStrokes(*node.first, box, stroke_width, strokes);
  }
  if (node.second) {
    CollectStrokes(*node.second, box, stroke_width, strokes);
  }
}

void Jianzi::Normalize(Node &node, NormalizeDirection dir) {
  // 获取当前最小包围盒
  auto tight_box = TightBoundingBox(node);
  float xa = tight_box.x;
  float xb = tight_box.x + tight_box.w;
  float ya = tight_box.y;
  float yb = tight_box.y + tight_box.h;

  // 根据最小包围盒反算归一化所需包围盒
  // 包围盒计算方式：px'=w*px+x, py'=h*py+y
  BoundingBox norm_box;

  if ((int)dir & (int)NormalizeDirection::Top) {
    BoundingBox box;
    // 向上归一化：h*ya+y=0, h*yb+y=yb
    box.h = yb / (yb - ya);
    box.y = -box.h * ya;

    norm_box = norm_box * box;
  }
  if ((int)dir & (int)NormalizeDirection::Bottom) {
    BoundingBox box;
    // 向下归一化：h*ya+y=ya, h*yb+y=1
    box.h = (1.0f - ya) / (yb - ya);
    box.y = 1.0f - box.h * yb;

    norm_box = norm_box * box;
  }
  if ((int)dir & (int)NormalizeDirection::Left) {
    BoundingBox box;
    // 向上归一化：w*xa+x=0, w*xb+x=xb
    box.w = xb / (xb - xa);
    box.x = -box.w * xa;

    norm_box = norm_box * box;
  }
  if ((int)dir & (int)NormalizeDirection::Right) {
    BoundingBox box;
    // 向下归一化：w*xa+x=xa, w*xb+x=1
    box.w = (1.0f - xa) / (xb - xa);
    box.x = 1.0f - box.w * xb;

    norm_box = norm_box * box;
  }

  NormalizeFunc(node, norm_box);
}

void Jianzi::NormalizeFunc(Node &node, const BoundingBox &box) {
  for (auto &s : node.strokes) {
    // 笔画缩放
    for (auto &v : s.vertice) {
      v.pt = box * v.pt;
    }
  }

  // 修改各个子node的笔画
  if (node.first) {
    NormalizeFunc(*node.first, box);
  }
  if (node.second) {
    NormalizeFunc(*node.second, box);
  }
}

std::unique_ptr<Jianzi::Node> Jianzi::Node::Clone(const Jianzi::Node &node) {
  std::unique_ptr<Node> ret = std::make_unique<Node>();
  ret->box = node.box;
  ret->strokes = node.strokes;
  ret->outer_border = node.outer_border;
  if (node.first) {
    ret->first = Clone(*node.first);
  }
  if (node.second) {
    ret->second = Clone(*node.second);
  }
  return std::move(ret);
}

}  // namespace qin
