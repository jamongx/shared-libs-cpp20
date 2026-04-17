// inifile.h – INI file parser (C++17)
#pragma once

#include <string>
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

}  // namespace pdk
