// inifile.cpp – INI file parser implementation (C++17)
#include "inifile.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pdk {

static void TrimLeft(std::string& s, char c = ' ')
{
    auto pos = s.find_first_not_of(c);
    s = (pos == std::string::npos) ? "" : s.substr(pos);
}

static void TrimRight(std::string& s, char c = ' ')
{
    auto pos = s.find_last_not_of(c);
    s = (pos == std::string::npos) ? "" : s.substr(0, pos + 1);
}

static void Trim(std::string& s, char c = ' ')
{
    TrimLeft(s, c);
    TrimRight(s, c);
}

// ── Constructor ───────────────────────────────────────────────────────────────
IniFile::IniFile(const std::string& inipath) : m_path(inipath) {}

void IniFile::set_path(const std::string& newpath)
{
    m_path = newpath;
}

// ── read_file ──────────────────────────────────────────────────────────────────
bool IniFile::read_file()
{
    FILE* fp = fopen(m_path.c_str(), "r");
    if (!fp) {
        error = "Unable to open ini file.";
        return false;
    }

    std::string keyname;
    char buf[2048];
    while (fgets(buf, sizeof(buf), fp)) {
        std::string line(buf);
        TrimRight(line, '\n');
        TrimRight(line, '\r');
        if (line.empty() || line[0] == '#')
            continue;

        if (line[0] == '[' && line.back() == ']') {
            keyname = line.substr(1, line.size() - 2);
            Trim(keyname);
        } else {
            auto eq = line.find('=');
            std::string valuename, value;
            if (eq == std::string::npos) {
                valuename = line;
                value = "";
            } else {
                valuename = line.substr(0, eq);
                value = line.substr(eq + 1);
            }
            Trim(valuename);
            // strip inline comment
            auto comment = value.find('#');
            if (comment != std::string::npos)
                value = value.substr(0, comment);
            Trim(value);

            if (!valuename.empty())
                (void)set_value(keyname, valuename, value);
        }
    }
    fclose(fp);
    return true;
}

// ── write_file ─────────────────────────────────────────────────────────────────
void IniFile::write_file()
{
    FILE* fp = fopen(m_path.c_str(), "wb");
    if (!fp) {
        error = "Unable to open ini file.";
        return;
    }

    for (int k = 0; k < static_cast<int>(names_.size()); k++) {
        if (keys_[k].names.empty())
            continue;
        fprintf(fp, "[%s]\n", names_[k].c_str());
        for (int v = 0; v < static_cast<int>(keys_[k].names.size()); v++) {
            fprintf(fp, "%-10s = %s\n", keys_[k].names[v].c_str(), keys_[k].values[v].c_str());
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
}

// ── Reset ─────────────────────────────────────────────────────────────────────
void IniFile::reset()
{
    keys_.clear();
    names_.clear();
}

// ── num_keys / num_values ─────────────────────────────────────────────────
int IniFile::num_keys()
{
    return static_cast<int>(keys_.size());
}

int IniFile::num_values(const std::string& keyname)
{
    int keynum = find_key(keyname);
    if (keynum == -1)
        return -1;
    return static_cast<int>(keys_[keynum].names.size());
}

// ── get_value / get_value_i / get_value_f ─────────────────────────────────────────
std::string IniFile::get_value(const std::string& keyname, const std::string& valuename)
{
    int keynum = find_key(keyname);
    int valuenum = find_value(keynum, valuename);
    if (keynum == -1) {
        error = "Key not found.";
        return "";
    }
    if (valuenum == -1) {
        error = "Value not found.";
        return "";
    }
    return keys_[keynum].values[valuenum];
}

int IniFile::get_value_i(const std::string& keyname, const std::string& valuename)
{
    return atoi(get_value(keyname, valuename).c_str());
}

double IniFile::get_value_f(const std::string& keyname, const std::string& valuename)
{
    return atof(get_value(keyname, valuename).c_str());
}

bool IniFile::get_value(const std::string& keyname,
                        int valuenum,
                        std::string& valuename_out,
                        std::string& value_out)
{
    int keynum = find_key(keyname);
    if (keynum == -1)
        return false;
    if (valuenum < 0 || valuenum >= static_cast<int>(keys_[keynum].names.size()))
        return false;
    valuename_out = keys_[keynum].names[valuenum];
    value_out = get_value(keyname, valuename_out);
    return true;
}

bool IniFile::get_values(const std::string& keyname,
                         std::unordered_map<std::string, std::string>& valuemap)
{
    int keynum = find_key(keyname);
    if (keynum == -1)
        return false;
    valuemap.clear();
    for (int i = 0; i < static_cast<int>(keys_[keynum].names.size()); i++)
        valuemap[keys_[keynum].names[i]] = get_value(keyname, keys_[keynum].names[i]);
    return true;
}

// ── set_value / set_value_i / set_value_f ─────────────────────────────────────────
bool IniFile::set_value(const std::string& keyname,
                        const std::string& valuename,
                        const std::string& value,
                        bool create)
{
    int keynum = find_key(keyname);
    if (keynum == -1) {
        if (!create)
            return false;
        names_.push_back(keyname);
        keys_.push_back({});
        keynum = static_cast<int>(names_.size()) - 1;
    }
    int valuenum = find_value(keynum, valuename);
    if (valuenum == -1) {
        if (!create)
            return false;
        keys_[keynum].names.push_back(valuename);
        keys_[keynum].values.emplace_back("");
        valuenum = static_cast<int>(keys_[keynum].names.size()) - 1;
    }
    keys_[keynum].values[valuenum] = value;
    return true;
}

bool IniFile::set_value_i(const std::string& key,
                          const std::string& valuename,
                          int value,
                          bool create)
{
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%d", value);
    return set_value(key, valuename, tmp, create);
}

bool IniFile::set_value_f(const std::string& key,
                          const std::string& valuename,
                          double value,
                          bool create)
{
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%e", value);
    return set_value(key, valuename, tmp, create);
}

// ── delete_value / delete_key ───────────────────────────────────────────────────
bool IniFile::delete_value(const std::string& keyname, const std::string& valuename)
{
    int keynum = find_key(keyname);
    int valuenum = find_value(keynum, valuename);
    if (keynum == -1 || valuenum == -1)
        return false;
    keys_[keynum].names.erase(keys_[keynum].names.begin() + valuenum);
    keys_[keynum].values.erase(keys_[keynum].values.begin() + valuenum);
    return true;
}

bool IniFile::delete_key(const std::string& keyname)
{
    int keynum = find_key(keyname);
    if (keynum == -1)
        return false;
    keys_.erase(keys_.begin() + keynum);
    names_.erase(names_.begin() + keynum);
    return true;
}

// ── find_key / find_value ───────────────────────────────────────────────────────
int IniFile::find_key(const std::string& keyname) const
{
    for (int i = 0; i < static_cast<int>(names_.size()); i++)
        if (names_[i] == keyname)
            return i;
    return -1;
}

int IniFile::find_value(int keynum, const std::string& valuename) const
{
    if (keynum < 0)
        return -1;
    const auto& names = keys_[keynum].names;
    for (int i = 0; i < static_cast<int>(names.size()); i++)
        if (names[i] == valuename)
            return i;
    return -1;
}

// ── Modern accessors ─────────────────────────────────────────────────────────

std::optional<std::string> IniFile::try_get_string(std::string_view section,
                                                   std::string_view key)
{
    std::string s_section{section};
    std::string s_key{key};
    int keynum = find_key(s_section);
    if (keynum < 0) return std::nullopt;
    int valnum = find_value(keynum, s_key);
    if (valnum < 0) return std::nullopt;
    return keys_[keynum].values[valnum];
}

namespace {
// Trim trailing/leading whitespace so callers can ignore formatting drift.
std::string trim(const std::string& s)
{
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}
}  // namespace

std::optional<int> IniFile::try_get_int(std::string_view section, std::string_view key)
{
    auto s = try_get_string(section, key);
    if (!s) return std::nullopt;
    const std::string trimmed = trim(*s);
    if (trimmed.empty()) return std::nullopt;
    try {
        std::size_t consumed = 0;
        int v = std::stoi(trimmed, &consumed);
        // Reject partial parses like "12abc" — the entire trimmed value
        // must be consumed for the result to be trustworthy.
        if (consumed != trimmed.size()) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> IniFile::try_get_double(std::string_view section, std::string_view key)
{
    auto s = try_get_string(section, key);
    if (!s) return std::nullopt;
    const std::string trimmed = trim(*s);
    if (trimmed.empty()) return std::nullopt;
    try {
        std::size_t consumed = 0;
        double v = std::stod(trimmed, &consumed);
        if (consumed != trimmed.size()) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

std::string IniFile::get_string_or(std::string_view section, std::string_view key,
                                   std::string_view default_value)
{
    auto v = try_get_string(section, key);
    return v ? *v : std::string{default_value};
}

int IniFile::get_int_or(std::string_view section, std::string_view key, int default_value)
{
    auto v = try_get_int(section, key);
    return v.value_or(default_value);
}

double IniFile::get_double_or(std::string_view section, std::string_view key, double default_value)
{
    auto v = try_get_double(section, key);
    return v.value_or(default_value);
}

namespace {
bool parse_bool(const std::string& v) {
    std::string s;
    s.reserve(v.size());
    for (char c : v) s.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
    if (s == "1" || s == "true" || s == "yes" || s == "on" || s == "y") return true;
    return false;  // anything else (incl. explicit "false/no/off/0") → false
}
bool is_truthy(const std::string& trimmed) {
    return parse_bool(trimmed);
}
bool is_explicitly_false(const std::string& trimmed) {
    std::string s;
    s.reserve(trimmed.size());
    for (char c : trimmed) s.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
    return s == "0" || s == "false" || s == "no" || s == "off" || s == "n";
}
}  // namespace

bool IniFile::get_bool_or(std::string_view section, std::string_view key, bool default_value)
{
    auto v = try_get_string(section, key);
    if (!v) return default_value;
    const std::string trimmed = trim(*v);
    if (trimmed.empty()) return default_value;
    if (is_truthy(trimmed)) return true;
    if (is_explicitly_false(trimmed)) return false;
    return default_value;
}

// ── Templated get<T>() specializations ────────────────────────────────────────

template<>
std::optional<std::string> IniFile::get<std::string>(std::string_view section,
                                                     std::string_view key) {
    return try_get_string(section, key);
}

template<>
std::optional<int> IniFile::get<int>(std::string_view section, std::string_view key) {
    return try_get_int(section, key);
}

template<>
std::optional<double> IniFile::get<double>(std::string_view section, std::string_view key) {
    return try_get_double(section, key);
}

template<>
std::optional<bool> IniFile::get<bool>(std::string_view section, std::string_view key) {
    auto v = try_get_string(section, key);
    if (!v) return std::nullopt;
    const std::string trimmed = trim(*v);
    if (trimmed.empty()) return std::nullopt;
    if (is_truthy(trimmed)) return true;
    if (is_explicitly_false(trimmed)) return false;
    return std::nullopt;  // unknown literal — refuse to guess
}

}  // namespace pdk
