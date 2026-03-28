module;
#include <string>
#include <vector>
#include <utility>
#include <fstream>
#include <sstream>

export module preset;

export struct PresetNode {
    int id;
    std::string type;
    std::vector<std::pair<std::string, float>> params;
};

export struct PresetConnection {
    int from;
    int to;        // -1 means "output"
    bool toOutput;
};

// ── helpers (not exported) ──────────────────────────────────────────────────

namespace {

std::string escapeJson(const std::string& s)
{
    std::string out;
    for (char c : s)
    {
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else                out += c;
    }
    return out;
}

void skipWhitespace(const std::string& json, size_t& pos)
{
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                  json[pos] == '\n' || json[pos] == '\r'))
        ++pos;
}

bool expect(const std::string& json, size_t& pos, char c)
{
    skipWhitespace(json, pos);
    if (pos < json.size() && json[pos] == c) { ++pos; return true; }
    return false;
}

std::string parseString(const std::string& json, size_t& pos)
{
    skipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '"') return {};
    ++pos; // skip opening quote
    std::string result;
    while (pos < json.size() && json[pos] != '"')
    {
        if (json[pos] == '\\' && pos + 1 < json.size())
        {
            ++pos;
            result += json[pos];
        }
        else
        {
            result += json[pos];
        }
        ++pos;
    }
    if (pos < json.size()) ++pos; // skip closing quote
    return result;
}

// Parses a number or the string "output".
// Sets isOutput=true if the value was "output", otherwise parses as float.
float parseNumberOrOutput(const std::string& json, size_t& pos, bool& isOutput)
{
    skipWhitespace(json, pos);
    isOutput = false;
    if (pos < json.size() && json[pos] == '"')
    {
        std::string s = parseString(json, pos);
        if (s == "output") isOutput = true;
        return 0.0f;
    }
    // parse number
    size_t start = pos;
    if (pos < json.size() && (json[pos] == '-' || json[pos] == '+')) ++pos;
    while (pos < json.size() && (json[pos] >= '0' && json[pos] <= '9')) ++pos;
    if (pos < json.size() && json[pos] == '.') { ++pos; }
    while (pos < json.size() && (json[pos] >= '0' && json[pos] <= '9')) ++pos;
    // handle exponent
    if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E'))
    {
        ++pos;
        if (pos < json.size() && (json[pos] == '-' || json[pos] == '+')) ++pos;
        while (pos < json.size() && (json[pos] >= '0' && json[pos] <= '9')) ++pos;
    }
    std::string numStr = json.substr(start, pos - start);
    return std::stof(numStr);
}

float parseNumber(const std::string& json, size_t& pos)
{
    bool dummy;
    return parseNumberOrOutput(json, pos, dummy);
}

int parseInt(const std::string& json, size_t& pos)
{
    skipWhitespace(json, pos);
    size_t start = pos;
    if (pos < json.size() && (json[pos] == '-' || json[pos] == '+')) ++pos;
    while (pos < json.size() && (json[pos] >= '0' && json[pos] <= '9')) ++pos;
    std::string numStr = json.substr(start, pos - start);
    return std::stoi(numStr);
}

} // anon namespace

// ── serialization ───────────────────────────────────────────────────────────

export std::string serializePreset(const std::vector<PresetNode>& nodes,
                                   const std::vector<PresetConnection>& connections)
{
    std::ostringstream out;
    out << "{\n  \"nodes\": [\n";

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto& n = nodes[i];
        out << "    { \"id\": " << n.id
            << ", \"type\": \"" << escapeJson(n.type) << "\"";

        if (!n.params.empty())
        {
            out << ", \"params\": { ";
            for (size_t j = 0; j < n.params.size(); ++j)
            {
                if (j > 0) out << ", ";
                out << "\"" << escapeJson(n.params[j].first) << "\": "
                    << n.params[j].second;
            }
            out << " }";
        }

        out << " }";
        if (i + 1 < nodes.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"connections\": [\n";

    for (size_t i = 0; i < connections.size(); ++i)
    {
        const auto& c = connections[i];
        out << "    { \"from\": " << c.from << ", \"to\": ";
        if (c.toOutput)
            out << "\"output\"";
        else
            out << c.to;
        out << " }";
        if (i + 1 < connections.size()) out << ",";
        out << "\n";
    }

    out << "  ]\n}";
    return out.str();
}

// ── deserialization ─────────────────────────────────────────────────────────

export bool parsePreset(const std::string& json,
                        std::vector<PresetNode>& outNodes,
                        std::vector<PresetConnection>& outConnections)
{
    outNodes.clear();
    outConnections.clear();

    size_t pos = 0;
    if (!expect(json, pos, '{')) return false;

    // Parse top-level keys
    while (pos < json.size())
    {
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '}') { ++pos; break; }

        // skip comma between top-level entries
        if (json[pos] == ',') { ++pos; continue; }

        std::string key = parseString(json, pos);
        if (!expect(json, pos, ':')) return false;

        if (key == "nodes")
        {
            if (!expect(json, pos, '[')) return false;
            while (pos < json.size())
            {
                skipWhitespace(json, pos);
                if (json[pos] == ']') { ++pos; break; }
                if (json[pos] == ',') { ++pos; continue; }

                // parse node object
                if (!expect(json, pos, '{')) return false;
                PresetNode node{};
                while (pos < json.size())
                {
                    skipWhitespace(json, pos);
                    if (json[pos] == '}') { ++pos; break; }
                    if (json[pos] == ',') { ++pos; continue; }

                    std::string nkey = parseString(json, pos);
                    if (!expect(json, pos, ':')) return false;

                    if (nkey == "id")
                        node.id = parseInt(json, pos);
                    else if (nkey == "type")
                        node.type = parseString(json, pos);
                    else if (nkey == "params")
                    {
                        if (!expect(json, pos, '{')) return false;
                        while (pos < json.size())
                        {
                            skipWhitespace(json, pos);
                            if (json[pos] == '}') { ++pos; break; }
                            if (json[pos] == ',') { ++pos; continue; }
                            std::string pname = parseString(json, pos);
                            if (!expect(json, pos, ':')) return false;
                            float pval = parseNumber(json, pos);
                            node.params.push_back({pname, pval});
                        }
                    }
                }
                outNodes.push_back(std::move(node));
            }
        }
        else if (key == "connections")
        {
            if (!expect(json, pos, '[')) return false;
            while (pos < json.size())
            {
                skipWhitespace(json, pos);
                if (json[pos] == ']') { ++pos; break; }
                if (json[pos] == ',') { ++pos; continue; }

                if (!expect(json, pos, '{')) return false;
                PresetConnection conn{};
                conn.to = -1;
                conn.toOutput = false;
                while (pos < json.size())
                {
                    skipWhitespace(json, pos);
                    if (json[pos] == '}') { ++pos; break; }
                    if (json[pos] == ',') { ++pos; continue; }

                    std::string ckey = parseString(json, pos);
                    if (!expect(json, pos, ':')) return false;

                    if (ckey == "from")
                        conn.from = parseInt(json, pos);
                    else if (ckey == "to")
                    {
                        bool isOut = false;
                        float val = parseNumberOrOutput(json, pos, isOut);
                        if (isOut)
                        {
                            conn.toOutput = true;
                            conn.to = -1;
                        }
                        else
                        {
                            conn.to = static_cast<int>(val);
                        }
                    }
                }
                outConnections.push_back(conn);
            }
        }
    }

    return true;
}

// ── file I/O ────────────────────────────────────────────────────────────────

export bool savePresetToFile(const std::string& path, const std::string& json)
{
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f.is_open()) return false;
    f << json;
    return f.good();
}

export std::string loadPresetFromFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
