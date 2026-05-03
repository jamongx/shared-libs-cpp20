// inifile.h – INI file parser (C++17)
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "base.hpp"
#include "list.hpp"

namespace pdk {

class IniFile {
private:
    std::string m_path;

    struct Key {
        std::vector<std::string> values;
        std::vector<std::string> names;
    };

    std::vector<Key> keys_;
    std::vector<std::string> names_;

    [[nodiscard]] int find_value(int keynum, const std::string& valuename) const;
    [[nodiscard]] int find_key(const std::string& keyname) const;

public:
    std::string error;

    IniFile() = default;
    explicit IniFile(const std::string& inipath);
    virtual ~IniFile() = default;

    void set_path(const std::string& newpath);

    [[nodiscard]] bool read_file();
    void write_file();
    void reset();

    int num_keys();
    int num_values(const std::string& keyname);

    [[nodiscard]] std::string get_value(const std::string& keyname, const std::string& valuename);
    int get_value_i(const std::string& keyname, const std::string& valuename);
    double get_value_f(const std::string& keyname, const std::string& valuename);

    // ── Modern accessors (preferred) ─────────────────────────────────────
    // Return std::nullopt when the section/key is missing or the value
    // cannot be parsed as the requested type. Plain get_value_i / get_value_f
    // return 0 / 0.0 in those cases, which is ambiguous with legitimate zero.
    [[nodiscard]] std::optional<std::string> try_get_string(std::string_view section,
                                                            std::string_view key);
    [[nodiscard]] std::optional<int> try_get_int(std::string_view section,
                                                 std::string_view key);
    [[nodiscard]] std::optional<double> try_get_double(std::string_view section,
                                                       std::string_view key);

    // Convenience: return value or fall back to `defaultValue`. Avoids the
    // get_value_i==0-means-missing trap.
    [[nodiscard]] std::string get_string_or(std::string_view section,
                                            std::string_view key,
                                            std::string_view default_value);
    [[nodiscard]] int get_int_or(std::string_view section, std::string_view key,
                                 int default_value);
    [[nodiscard]] double get_double_or(std::string_view section, std::string_view key,
                                       double default_value);
    [[nodiscard]] bool get_bool_or(std::string_view section, std::string_view key,
                                   bool default_value);

    // ── Generic templated accessor (preferred for new code) ─────────────
    // Single API, type-safe via tag dispatch. Returns std::nullopt if the
    // key is missing or cannot be parsed as T. Supported T: int, double,
    // std::string, bool. Booleans accept "true/false/yes/no/on/off/1/0"
    // (case-insensitive).
    //
    // Example:
    //     auto port = ini.get<int>("server", "port").value_or(9100);
    //     auto name = ini.get<std::string>("user", "name").value_or("guest");
    //
    // Unsupported T: a clear compile-time diagnostic is emitted via the
    // primary template's static_assert below — better than an opaque link
    // error from a missing specialization.
    template<class T>
    [[nodiscard]] std::optional<T> get(std::string_view section,
                                       std::string_view key) {
        static_assert(sizeof(T) == 0,
                      "IniFile::get<T>() supports only int, double, "
                      "std::string, bool — supply one of these as T.");
        (void)section; (void)key;
        return std::nullopt;
    }

    template<class T>
    [[nodiscard]] T get_or(std::string_view section, std::string_view key, T default_value) {
        auto v = get<T>(section, key);
        return v ? *std::move(v) : std::move(default_value);
    }

    [[nodiscard]] bool set_value(const std::string& key,
                   const std::string& valuename,
                   const std::string& value,
                   bool create = true);
    [[nodiscard]] bool set_value_i(const std::string& key,
                     const std::string& valuename,
                     int value,
                     bool create = true);
    [[nodiscard]] bool set_value_f(const std::string& key,
                     const std::string& valuename,
                     double value,
                     bool create = true);

    [[nodiscard]] bool get_value(const std::string& keyname,
                   int valuenum,
                   std::string& valuename_out,
                   std::string& value_out);
    [[nodiscard]] bool get_values(const std::string& keyname,
                    std::unordered_map<std::string, std::string>& valuemap);

    [[nodiscard]] bool is_key(const std::string& keyname) const { return find_key(keyname) != -1; }

    bool delete_value(const std::string& keyname, const std::string& valuename);
    bool delete_key(const std::string& keyname);
};

// Explicit specialization declarations — definitions live in inifile.cpp.
// Without these declarations, consumers would silently bind to the primary
// template (which static_asserts) instead of picking up the specialised
// implementation. The `inline` keyword would not work for member templates;
// declaring here + defining in the .cpp file is the canonical pattern.
template<> std::optional<std::string> IniFile::get<std::string>(std::string_view, std::string_view);
template<> std::optional<int>         IniFile::get<int>(std::string_view, std::string_view);
template<> std::optional<double>      IniFile::get<double>(std::string_view, std::string_view);
template<> std::optional<bool>        IniFile::get<bool>(std::string_view, std::string_view);

}  // namespace pdk
