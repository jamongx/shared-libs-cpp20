// list.h – C++17 container type aliases (replaces legacy MFC-style collections)
#pragma once

#include <algorithm>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>
#include "base.hpp"

namespace pdk {

// ── CArray<TYPE, ARG_TYPE> → std::vector<TYPE> ────────────────────────────────
template<class TYPE, class ARG_TYPE = const TYPE&>
using CArray = std::vector<TYPE>;

// ── CList<TYPE, ARG_TYPE> → std::list<TYPE> ───────────────────────────────────
template<class TYPE, class ARG_TYPE = const TYPE&>
using CList = std::list<TYPE>;

// ── CTypedPtrList<BASE_CLASS, TYPE> → std::list<TYPE> ─────────────────────────
template<class BASE_CLASS, class TYPE>
using CTypedPtrList = std::list<TYPE>;

// ── Concrete pointer list types ───────────────────────────────────────────────
using CPtrList = std::list<void*>;
using CObList = std::list<Object*>;
using CPtrArray = std::vector<void*>;
using CStringList = std::list<std::string>;

// ── Map types ─────────────────────────────────────────────────────────────────
using CMapPtrToPtr = std::unordered_map<void*, void*>;
using CMapStringToPtr = std::unordered_map<std::string, void*>;
using CMapStringToOb = std::unordered_map<std::string, Object*>;
using CMapStringToString = std::unordered_map<std::string, std::string>;

}  // namespace pdk
