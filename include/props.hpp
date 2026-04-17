// props.h – Property classes (C++17)
#pragma once

#include <list>
#include <string>
#include <unordered_map>
#include "base.hpp"
#include "logger.hpp"

namespace pdk {

enum PROPTYPE {
    PT_NONE = 0,
    PT_TIME = 1,
    PT_CHAR = 2,
    PT_BOOL = 3,
    PT_INT = 4,
    PT_STRING = 5,
    PT_USER = 6
};

// ── PropBase ─────────────────────────────────────────────────────────────────
class PropBase : public Object {
public:
    PropBase() : type_(PT_NONE), logger_(Logger::get()) {}
    PropBase(PROPTYPE nType, const char* key)
        : type_(nType),
          key_(key ? key : ""),
          logger_(Logger::get())
    {}
    virtual ~PropBase() { remove_all(); }

    void set_type(PROPTYPE nType) { type_ = nType; }
    void SetKey(const char* pszKey) { key_ = pszKey ? pszKey : ""; }
    virtual bool set_value(const char* pszValue, bool bAdd = true) = 0;

    PROPTYPE type() const { return type_; }
    const char* GetKey() const { return key_.c_str(); }
    const char* get_value() const;
    bool remove_value();
    void remove_all();

    virtual int time_value() { return 0; }
    virtual char char_value() { return '\0'; }
    virtual bool bool_value() { return false; }
    virtual int int_value() { return 0; }
    virtual const char* string_value() { return ""; }

protected:
    PROPTYPE type_;
    std::string key_;
    std::list<std::string> values_;
    Logger* logger_;
};

// ── PropTime ─────────────────────────────────────────────────────────────────
class PropTime : public PropBase {
public:
    explicit PropTime(const char* key) : PropBase(PT_TIME, key), time_val_(0) {}
    ~PropTime() override = default;

    bool set_value(const char* pszValue, bool bAdd = true) override;
    void set_time_value(int t) { time_val_ = t; }
    int time_value() override { return time_val_; }

protected:
    int time_val_;
};

// ── PropChar ─────────────────────────────────────────────────────────────────
class PropChar : public PropBase {
public:
    explicit PropChar(const char* key) : PropBase(PT_CHAR, key), char_val_(0) {}
    ~PropChar() override = default;

    bool set_value(const char* pszValue, bool bAdd = true) override;
    void set_char_value(char c) { char_val_ = c; }
    char char_value() override { return char_val_; }

protected:
    char char_val_;
};

// ── PropBool ─────────────────────────────────────────────────────────────────
class PropBool : public PropBase {
public:
    explicit PropBool(const char* key) : PropBase(PT_BOOL, key), bool_val_(false) {}
    ~PropBool() override = default;

    bool set_value(const char* pszValue, bool bAdd = true) override;
    void set_bool_value(bool b) { bool_val_ = b; }
    bool bool_value() override { return bool_val_; }

protected:
    bool bool_val_;
};

// ── PropInt ──────────────────────────────────────────────────────────────────
class PropInt : public PropBase {
public:
    explicit PropInt(const char* key) : PropBase(PT_INT, key), int_val_(0) {}
    ~PropInt() override = default;

    bool set_value(const char* pszValue, bool bAdd = true) override;
    void set_int_value(int n) { int_val_ = n; }
    int int_value() override { return int_val_; }

protected:
    int int_val_;
};

// ── PropString ───────────────────────────────────────────────────────────────
class PropString : public PropBase {
public:
    explicit PropString(const char* key) : PropBase(PT_STRING, key) {}
    ~PropString() override = default;

    bool set_value(const char* pszValue, bool bAdd = true) override;
    const char* string_value() override { return str_val_.c_str(); }

protected:
    std::string str_val_;
};

// ── Properties ───────────────────────────────────────────────────────────────
using PropBaseList = std::list<PropBase*>;

class Properties : public Object {
public:
    Properties();
    ~Properties();

    bool init();
    void remove_all();

    PropBase* new_prop(PROPTYPE nType, const char* key);
    bool add_to_map(const char* key, PropBase* pProp);
    PropBase* find(const char* key);
    bool add_value(const char* key, const char* pszValue);
    bool update_value(const char* key, const char* pszValue);
    bool restore_value(const char* key);

protected:
    PropBaseList props_;
    std::unordered_map<std::string, PropBase*> prop_map_;
    Logger* logger_;
};

}  // namespace pdk
