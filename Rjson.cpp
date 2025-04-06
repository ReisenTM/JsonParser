#include "print.h"
#include <charconv>
#include <cstddef>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
struct JsonObj;
using jsonObjcet = std::unordered_map<std::string, JsonObj>;
using jsonArray = std::vector<JsonObj>;

struct JsonObj {
  // json对象
  std::variant<std::monostate, // null
               int,            // 35
               bool,           // true
               double,         // 3.14
               std::string,    //"reisen "
               jsonArray,      //[3,4,"dsad"]  Json数组
               jsonObjcet      //{"hello":"world","PI",3.14}
               >
      inner;
  void do_print() const { printnl(inner); }
  template <class T> bool is() const {
    // 判断文件类型
    return std::holds_alternative<T>(inner);
  }

  template <class T> T &get() { return std::get<T>(inner); }

  template <class T> T const &get() const { return std::get<T>(inner); }
};
template <class T> static std::optional<T> try_parse_num(std::string_view str) {
  T value;
  // 将str转为数字
  auto res = std::from_chars(str.data(), str.data() + str.size(), value);
  if (res.ec == std::errc() && res.ptr == str.data() + str.size()) {
    // 这里不仅要判定是否正常返回，还要判定是否全部转换
    // 因为如果在对double调用该函数时错选择了int模板，会导致只转换整数部分
    return value;
  }
  return std::nullopt;
}
static char unescape_char(const char &ch) {
  switch (ch) {
  case 'r':
    return '\r';
  case 'n':
    return '\n';
  case '0':
    return '\0';
  case 't':
    return '\t';
  case 'v':
    return '\v';
  case 'f':
    return '\f';
  case 'b':
    return '\b';
  case 'a':
    return '\a';
  default:
    return ch;
  }
}
// 使用pair是为了让它在递归时自述长度
std::pair<JsonObj, size_t> parse(std::string_view json) {
  // TODO
  if (json.empty()) {
    return {JsonObj{std::monostate{}}, 0};
  } else if (size_t off = json.find_first_not_of(" \n\r\t\v\f\0");
             off != 0 && off != json.npos) {
    // 要解析的json可能是格式化过的，需要过滤
    auto [obj, eaten] = parse(json.substr(off));
    return {std::move(obj), off + eaten};
  } else if (json[0] >= '0' && json[0] <= '9' || json[0] == '+' ||
             json[0] == '-') {

    // 通过第一个字符初步判定json对象的内容
    // 如果是int或者double -------------
    std::regex reg{R"([+-]?[0-9]+(\.[0-9]*)?([eE][+-]?[0-9]+)?)"};
    // 设置正则表达式，匹配数字型
    std::cmatch match;
    if (std::regex_search(json.data(), json.data() + json.size(), match, reg)) {
      // 如果能匹配到，匹配结果存放到match中
      // 只取出匹配到的部分,为什么不用regex_match()？见数组解析部分
      std::string str = match.str();
      if (auto num = try_parse_num<int>(str); num.has_value()) {
        // return JsonObj{num.value()};
        // 已经进行过判断，不需要optional内部再判断是否有值
        return {JsonObj{*num}, str.size()};
      }
#ifdef CPP_VERSION_20
      if (auto num = try_parse_num<double>(str)) {
        // return JsonObj{num.value()};
        return {JsonObj{*num}, str.size()};
      }
#endif
    }
  } else if (json[0] == '"') {
#ifdef DEBUG_MODE
    std::cout << json.data() << "是个字符串！" << std::endl;
#endif
    // 如果是字符串 ----------------
    // 定义字符串追加状态
    enum {
      ESCAPE, // 识别到转义字符的模式
      RAW,    // 原文模式
    } mode = RAW;
    // 跳过第一个“ ，开始解析字符串
    std::string resStr;
    size_t i;
    for (i = 1; i < json.size(); i++) {
      char ch = json[i];
      if (mode == RAW) {
        // 原文模式
        if (ch == '\\') {
          // 发现转义符号，切换模式
          mode = ESCAPE;
          continue;
        } else if (ch == '"') {
          // 识别到结束引号，直接退出
          i += 1;
          break;
        } else {
          // 如果是其他正常字符，直接追加
          resStr += ch;
        }
      } else if (mode == ESCAPE) {
        // 转义模式
        resStr += unescape_char(ch);
        // 转义后切回正常模式
        mode = RAW;
      }
    }
    return {JsonObj{std::move(resStr)}, i};
  } else if (json[0] == '[') {
    // 如果是json数组
    // vector用来向数组内追加json对象
    std::vector<JsonObj> res;
    size_t i;
    for (i = 1; i < json.size();) {
      // 逐个解析对象
      if (json[i] == ']') {
        // 是数组的结尾(可能是该数组，也可能是内嵌的数组),i+=1为了跳过内嵌数组的右侧括号
        i += 1;
        break;
      }
      // 不确定每个对象的类型，递归判断
      auto [obj, handled] = parse(json.substr(i));
      if (handled == 0) {
        i = 0;
        break;
      }
      res.push_back(std::move(obj));
      i += handled;
      if (json[i] == ',') {
        i += 1;
      }
    }
    return {JsonObj{std::move(res)}, i};
  } else if (json[0] == '{') {
    // 如果是json对象
    // unordered_map用来存放
    std::unordered_map<std::string, JsonObj> res;
    size_t i;
    for (i = 1; i < json.size();) {
      if (json[i] == '}') {
        i += 1;
        break;
      }
      // 先处理key
      auto [keyobj, keyhandled] = parse(json.substr(i));
      if (keyhandled == 0) {
        i = 0;
        break;
      }
      i += keyhandled;
      if (!std::holds_alternative<std::string>(keyobj.inner)) {
        // 检测下key值是否是string，不是直接退出
        i = 0;
        break;
      }
      // 跳过:
      if (json[i] == ':') {
        i += 1;
      }
      // 保存key值
      std::string key = std::move(std::get<std::string>(keyobj.inner));
      // 再处理value
      auto [valobj, valhandled] = parse(json.substr(i));
      if (valhandled == 0) {
        i = 0;
        break;
      }
      i += valhandled;
      res.try_emplace(std::move(key), std::move(valobj));
      if (json[i] == ',') {
        i += 1;
      }
    }
    return {JsonObj{std::move(res)}, i};
  }
  return {JsonObj{std::monostate{}}, 0};
}
#ifdef _TEST_JSON_PARSE_
#define _TEST_JSON_PARSE_
int main(int argc, char *argv[]) {

  std::cout << "integer parsed:" << std::get<int>(parse("100").first.inner)
            << std::endl;
  std::cout << "string parsed:"
            << std::get<std::string>(parse("\"sada\"").first.inner)
            << std::endl;
  std::string_view str = R"JSON([32,[42,16,22],"test",13,2])JSON";
  auto [obj, eaten] = parse(str);
  print(obj);
  std::string_view str2 =
      R"JSON(
    {
        "school":"huel",
        "salary":30000
    })JSON";
  auto [jsonobj, Objeaten] = parse(str2);
  print(jsonobj);
  std::string_view schoolStr =
      R"JSON({
        "school":["huel",["华水","农大"]],
        "salary":30000
    })JSON";
  auto [res, whatever] = parse(schoolStr);
  auto const &dict = res.get<jsonObjcet>();
  auto const &school = dict.at("school");
  auto dovisit = [&](auto &dovisit, JsonObj const &school) -> void {
    std::visit(
        [&](auto const &school) {
          // decay使jsonobj暴露出内部的类型
          if constexpr (std::is_same_v<std::decay_t<decltype(school)>,
                                       jsonArray>) {
            // 如果是数组类型，就递归展开
            for (auto &subschool : school) {
              dovisit(dovisit, subschool);
            }
          } else {
            print("I have never heared of such a bullshit school called",
                  school);
          }
        },
        school.inner);
  };
  dovisit(dovisit, school);
  return 0;
}
#endif
