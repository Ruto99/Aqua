#include "AquaXml.h"

#include <charconv>
#include <cmath>
#include <format>
#include <utility>
#include <vector>

namespace AquaTool {

using Ruto::Json;

namespace {

constexpr std::string_view kRootName = "AquaSource";
constexpr std::string_view kFormat = "AquaSource";

// ---------------------------------------------------------------------------
// 書く
// ---------------------------------------------------------------------------

// 改行・タブなどの制御文字は文字参照にする（1 つの値が 1 行に収まり、改行コードの変換でも壊れない）
void AppendEscaped(std::string& out, std::string_view s) {
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default:
            if (c < 0x20 || c == 0x7f) out += std::format("&#{};", static_cast<int>(c));
            else out += ch;
            break;
        }
    }
}

// 読み直すと同じ double に戻る最短の表記
std::string FormatNumber(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v < 0 ? "-inf" : "inf";
    char buf[64];
    const auto r = std::to_chars(buf, buf + sizeof(buf), v);
    return std::string(buf, r.ptr);
}

const char* TypeTag(Json::Type t) {
    switch (t) {
    case Json::Type::Null: return "null";
    case Json::Type::Bool: return "bool";
    case Json::Type::Number: return "number";
    case Json::Type::String: return "string";
    case Json::Type::Array: return "array";
    case Json::Type::Object: return "object";
    }
    return "null";
}

void WriteValue(std::string& out, const Json& v, const std::string* key, int indent, int depth) {
    const bool pretty = indent > 0;
    if (pretty) out.append(static_cast<size_t>(indent * depth), ' ');
    const char* tag = TypeTag(v.GetType());
    out += '<';
    out += tag;
    if (key) {
        out += " key=\"";
        AppendEscaped(out, *key);
        out += '"';
    }

    std::string text;
    switch (v.GetType()) {
    case Json::Type::Null: break;
    case Json::Type::Bool: text = v.AsBool() ? "true" : "false"; break;
    case Json::Type::Number: text = FormatNumber(v.AsNumber()); break;
    case Json::Type::String: AppendEscaped(text, v.AsString()); break;
    case Json::Type::Array:
    case Json::Type::Object:
        if (v.Size() == 0) break;
        out += '>';
        if (pretty) out += '\n';
        if (v.IsArray()) {
            for (const Json& e : v.Elements()) WriteValue(out, e, nullptr, indent, depth + 1);
        } else {
            for (const auto& [k, e] : v.Items()) WriteValue(out, e, &k, indent, depth + 1);
        }
        if (pretty) out.append(static_cast<size_t>(indent * depth), ' ');
        out += std::format("</{}>", tag);
        if (pretty) out += '\n';
        return;
    }
    if (text.empty()) {
        out += "/>";
    } else {
        out += '>';
        out += text;
        out += std::format("</{}>", tag);
    }
    if (pretty) out += '\n';
}

// ---------------------------------------------------------------------------
// 読む（この形式に必要な分だけの小さな XML パーサー）
// ---------------------------------------------------------------------------

struct Node {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<Node> children;
    std::string text;
    size_t offset = 0; // エラーの行番号用

    const std::string* Attribute(std::string_view n) const {
        for (const auto& [k, v] : attributes) {
            if (k == n) return &v;
        }
        return nullptr;
    }
};

void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

bool IsNameChar(char c) {
    return !IsSpace(c) && c != '<' && c != '>' && c != '/' && c != '=' && c != '"' && c != '\'' && c != '&' && c != '\0';
}

std::string_view Trim(std::string_view s) {
    while (!s.empty() && IsSpace(s.front())) s.remove_prefix(1);
    while (!s.empty() && IsSpace(s.back())) s.remove_suffix(1);
    return s;
}

class Parser {
public:
    Parser(std::string_view text, std::string* error) : m_Text(text), m_Error(error) {
        if (m_Text.starts_with("\xEF\xBB\xBF")) m_Pos = 3; // BOM
    }

    // 文書の中のただ 1 つの要素を読む
    bool ParseDocument(Node& root) {
        if (!SkipMisc()) return false;
        if (!Peek('<')) return Fail("要素がありません");
        if (!ParseElement(root, 0)) return false;
        if (!SkipMisc()) return false;
        if (m_Pos != m_Text.size()) return Fail("ルートの要素の後ろに余分なものがあります");
        return true;
    }

    bool Fail(std::string_view message) { return FailAt(m_Pos, message); }
    bool FailAt(size_t offset, std::string_view message) {
        if (m_Error && m_Error->empty()) *m_Error = std::format("{} 行目: {}", LineOf(offset), message);
        return false;
    }

private:
    static constexpr int kMaxDepth = 512;

    size_t LineOf(size_t offset) const {
        size_t line = 1;
        for (size_t i = 0; i < offset && i < m_Text.size(); ++i) {
            if (m_Text[i] == '\n') ++line;
        }
        return line;
    }

    bool Peek(char c) const { return m_Pos < m_Text.size() && m_Text[m_Pos] == c; }
    bool StartsWith(std::string_view s) const { return m_Text.substr(m_Pos).starts_with(s); }
    void SkipSpaces() {
        while (m_Pos < m_Text.size() && IsSpace(m_Text[m_Pos])) ++m_Pos;
    }
    bool SkipPast(std::string_view end, std::string_view what) {
        const size_t found = m_Text.find(end, m_Pos);
        if (found == std::string_view::npos) return Fail(std::format("{}が閉じていません", what));
        m_Pos = found + end.size();
        return true;
    }

    // 空白・コメント・<?xml ...?>・<!DOCTYPE ...>
    bool SkipMisc() {
        for (;;) {
            SkipSpaces();
            if (StartsWith("<?")) {
                if (!SkipPast("?>", "処理命令")) return false;
            } else if (StartsWith("<!--")) {
                if (!SkipPast("-->", "コメント")) return false;
            } else if (StartsWith("<!DOCTYPE")) {
                if (!SkipPast(">", "DOCTYPE")) return false;
            } else {
                return true;
            }
        }
    }

    bool ParseName(std::string& name) {
        const size_t start = m_Pos;
        while (m_Pos < m_Text.size() && IsNameChar(m_Text[m_Pos])) ++m_Pos;
        if (m_Pos == start) return Fail("名前がありません");
        name.assign(m_Text.substr(start, m_Pos - start));
        return true;
    }

    // &amp; などを戻しながら out に足す
    bool Decode(std::string_view raw, size_t offset, std::string& out) {
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] != '&') {
                out += raw[i];
                continue;
            }
            const size_t semi = raw.find(';', i);
            if (semi == std::string_view::npos) return FailAt(offset + i, "& の後ろに ; がありません（& は &amp; と書きます）");
            const std::string_view ent = raw.substr(i + 1, semi - i - 1);
            if (ent == "lt") out += '<';
            else if (ent == "gt") out += '>';
            else if (ent == "amp") out += '&';
            else if (ent == "quot") out += '"';
            else if (ent == "apos") out += '\'';
            else if (ent.size() >= 2 && ent[0] == '#') {
                const bool hex = ent[1] == 'x' || ent[1] == 'X';
                const std::string_view digits = ent.substr(hex ? 2 : 1);
                uint32_t cp = 0;
                const auto r = std::from_chars(digits.data(), digits.data() + digits.size(), cp, hex ? 16 : 10);
                if (digits.empty() || r.ec != std::errc() || r.ptr != digits.data() + digits.size() || cp > 0x10FFFF)
                    return FailAt(offset + i, std::format("文字参照が正しくありません: &{};", ent));
                AppendUtf8(out, cp);
            } else {
                return FailAt(offset + i, std::format("知らない実体参照です: &{};", ent));
            }
            i = semi;
        }
        return true;
    }

    bool ParseElement(Node& node, int depth) {
        if (depth > kMaxDepth) return Fail("入れ子が深すぎます");
        node.offset = m_Pos;
        ++m_Pos; // '<'
        if (!ParseName(node.name)) return false;

        // 属性
        for (;;) {
            SkipSpaces();
            if (StartsWith("/>")) {
                m_Pos += 2;
                return true;
            }
            if (Peek('>')) {
                ++m_Pos;
                break;
            }
            std::string name;
            if (!ParseName(name)) return false;
            SkipSpaces();
            if (!Peek('=')) return Fail(std::format("属性 {} の = がありません", name));
            ++m_Pos;
            SkipSpaces();
            if (!Peek('"') && !Peek('\'')) return Fail(std::format("属性 {} の値が引用符で囲まれていません", name));
            const char quote = m_Text[m_Pos++];
            const size_t end = m_Text.find(quote, m_Pos);
            if (end == std::string_view::npos) return Fail(std::format("属性 {} の値が閉じていません", name));
            std::string value;
            if (!Decode(m_Text.substr(m_Pos, end - m_Pos), m_Pos, value)) return false;
            m_Pos = end + 1;
            if (node.Attribute(name)) return Fail(std::format("属性 {} が 2 回あります", name));
            node.attributes.emplace_back(std::move(name), std::move(value));
        }

        // 中身
        for (;;) {
            if (m_Pos >= m_Text.size()) return FailAt(node.offset, std::format("<{}> が閉じていません", node.name));
            if (StartsWith("</")) {
                m_Pos += 2;
                std::string name;
                if (!ParseName(name)) return false;
                if (name != node.name) return Fail(std::format("<{}> を </{}> で閉じています", node.name, name));
                SkipSpaces();
                if (!Peek('>')) return Fail(std::format("</{}> の > がありません", name));
                ++m_Pos;
                return true;
            }
            if (StartsWith("<!--")) {
                if (!SkipPast("-->", "コメント")) return false;
            } else if (StartsWith("<![CDATA[")) {
                m_Pos += 9;
                const size_t end = m_Text.find("]]>", m_Pos);
                if (end == std::string_view::npos) return Fail("CDATA が閉じていません");
                node.text.append(m_Text.substr(m_Pos, end - m_Pos));
                m_Pos = end + 3;
            } else if (StartsWith("<?")) {
                if (!SkipPast("?>", "処理命令")) return false;
            } else if (Peek('<')) {
                Node child;
                if (!ParseElement(child, depth + 1)) return false;
                node.children.push_back(std::move(child));
            } else {
                const size_t start = m_Pos;
                const size_t end = std::min(m_Text.find('<', m_Pos), m_Text.size());
                if (!Decode(m_Text.substr(start, end - start), start, node.text)) return false;
                m_Pos = end;
            }
        }
    }

    std::string_view m_Text;
    std::string* m_Error;
    size_t m_Pos = 0;
};

// 要素 → Json の値
bool ToJson(Parser& p, const Node& n, Json& out) {
    const bool container = n.name == "object" || n.name == "array";
    if (container) {
        if (!Trim(n.text).empty()) return p.FailAt(n.offset, std::format("<{}> の中に文字は書けません（値は <string> などの要素で書きます）", n.name));
    } else if (!n.children.empty()) {
        return p.FailAt(n.children.front().offset, std::format("<{}> の中に要素は書けません", n.name));
    }

    if (n.name == "object") {
        out = Json::MakeObject();
        for (const Node& c : n.children) {
            const std::string* key = c.Attribute("key");
            if (!key) return p.FailAt(c.offset, std::format("<object> の中の <{}> に key 属性がありません", c.name));
            if (out.Contains(*key)) return p.FailAt(c.offset, std::format("キー \"{}\" が 2 回あります", *key));
            Json v;
            if (!ToJson(p, c, v)) return false;
            out[*key] = std::move(v);
        }
    } else if (n.name == "array") {
        out = Json::MakeArray();
        for (const Node& c : n.children) {
            Json v;
            if (!ToJson(p, c, v)) return false;
            out.Push(std::move(v));
        }
    } else if (n.name == "string") {
        out = Json(n.text);
    } else if (n.name == "number") {
        const std::string_view s = Trim(n.text);
        double v = 0.0;
        const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
        if (s.empty() || r.ec != std::errc() || r.ptr != s.data() + s.size()) return p.FailAt(n.offset, std::format("数ではありません: {}", s));
        out = Json(v);
    } else if (n.name == "bool") {
        const std::string_view s = Trim(n.text);
        if (s == "true") out = Json(true);
        else if (s == "false") out = Json(false);
        else return p.FailAt(n.offset, std::format("<bool> は true か false です: {}", s));
    } else if (n.name == "null") {
        if (!Trim(n.text).empty()) return p.FailAt(n.offset, "<null> の中には何も書けません");
        out = Json();
    } else {
        return p.FailAt(n.offset, std::format("知らない要素です: <{}>（object / array / string / number / bool / null）", n.name));
    }
    return true;
}

} // namespace

std::string SourceToXml(const Json& source) {
    std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out += std::format("<{}", kRootName);
    // 短い情報はルートの属性に（format は要素の名前で分かる）
    for (const auto& [k, v] : source.Items()) {
        if (k == "format") continue;
        if (k == "version" && v.IsNumber()) out += std::format(" version=\"{}\"", FormatNumber(v.AsNumber()));
        if ((k == "output" || k == "generator") && v.IsString()) {
            out += std::format(" {}=\"", k);
            AppendEscaped(out, v.AsString());
            out += '"';
        }
    }
    std::string children;
    for (const auto& [k, v] : source.Items()) {
        if (k == "format") continue;
        if (k == "version" && v.IsNumber()) continue;
        if ((k == "output" || k == "generator") && v.IsString()) continue;
        WriteValue(children, v, &k, 2, 1);
    }
    if (children.empty()) {
        out += "/>\n";
    } else {
        out += ">\n" + children + std::format("</{}>\n", kRootName);
    }
    return out;
}

std::optional<Json> SourceFromXml(std::string_view text, std::string* error) {
    std::string e;
    Parser p(text, &e);
    Node root;
    Json out = Json::MakeObject();
    bool ok = p.ParseDocument(root);
    if (ok && root.name != kRootName) ok = p.FailAt(root.offset, std::format("ルートの要素が <{}> ではありません（<{}>）", kRootName, root.name));
    if (ok && !Trim(root.text).empty()) ok = p.FailAt(root.offset, std::format("<{}> の中に文字は書けません", kRootName));
    if (ok) {
        out["format"] = std::string(kFormat);
        for (const auto& [k, v] : root.attributes) {
            if (k == "version") {
                double n = 0.0;
                const auto r = std::from_chars(v.data(), v.data() + v.size(), n);
                if (v.empty() || r.ec != std::errc() || r.ptr != v.data() + v.size()) {
                    ok = p.FailAt(root.offset, std::format("version が数ではありません: {}", v));
                    break;
                }
                out[k] = n;
            } else {
                out[k] = v;
            }
        }
    }
    if (ok) {
        for (const Node& c : root.children) {
            const std::string* key = c.Attribute("key");
            if (!key) {
                ok = p.FailAt(c.offset, std::format("<{}> の中の <{}> に key 属性がありません", kRootName, c.name));
                break;
            }
            if (out.Contains(*key)) {
                ok = p.FailAt(c.offset, std::format("キー \"{}\" が 2 回あります", *key));
                break;
            }
            Json v;
            if (!(ok = ToJson(p, c, v))) break;
            out[*key] = std::move(v);
        }
    }
    if (!ok) {
        if (error) *error = e;
        return std::nullopt;
    }
    return out;
}

std::string JsonToXmlElement(const Json& value, int indent) {
    std::string out;
    WriteValue(out, value, nullptr, indent, 0);
    return out;
}

std::optional<Json> JsonFromXmlElement(std::string_view text, std::string* error) {
    std::string e;
    Parser p(text, &e);
    Node root;
    Json out;
    if (!p.ParseDocument(root) || !ToJson(p, root, out)) {
        if (error) *error = e;
        return std::nullopt;
    }
    return out;
}

} // namespace AquaTool
