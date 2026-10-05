#ifndef ENUM_HELPER_H
#define ENUM_HELPER_H

#include <string>
#include <type_traits>

#include "LocalizedString.h"
#include "LuaManager.h"
#include "LuaReference.h"
#include "RageUtil.h"

#ifndef _MSC_VER
extern "C" {
#endif
#include "../extern/lua-5.1/src/lua.h"
#ifndef _MSC_VER
}
#endif

/** @brief A general foreach loop for enumerators. */
#define FOREACH_ENUM(e, var) \
  for (e var = (e)0; var < NUM_##e; enum_add<e>(var, +1))

int CheckEnum(
    lua_State* L, LuaReference& table, int iPos, int iInvalid,
    const char* szType, bool bAllowInvalid, bool bAllowAnything = false);

template <typename T>
struct EnumTraits {
  static LuaReference StringToEnum;
  static LuaReference EnumToString;
  static T Invalid;
  static const char* szName;
};
template <typename T>
LuaReference EnumTraits<T>::StringToEnum;
template <typename T>
LuaReference EnumTraits<T>::EnumToString;
/** @brief Lua helpers for Enumerators. */
namespace Enum {
template <typename T>
static T Check(
    lua_State* L, int iPos, bool bAllowInvalid = false,
    bool bAllowAnything = false) {
  return (T)CheckEnum(
      L, EnumTraits<T>::StringToEnum, iPos, EnumTraits<T>::Invalid,
      EnumTraits<T>::szName, bAllowInvalid, bAllowAnything);
}
template <typename T>
static void Push(lua_State* L, T iVal) {
  /* Enum_Invalid values are nil in Lua. */
  if (iVal == EnumTraits<T>::Invalid) {
    lua_pushnil(L);
    return;
  }

  /* Look up the string value. */
  EnumTraits<T>::EnumToString.PushSelf(L);
  lua_rawgeti(L, -1, iVal + 1);
  lua_remove(L, -2);
}

void InitLuaEnum(
    lua_State* L, LuaReference& EnumToString, LuaReference& StringToEnum,
    const char* szName, int iMax, std::string (*pfnToString)(int));

template <typename E>
constexpr auto to_integral(E e) {
  return static_cast<typename std::underlying_type<E>::type>(e);
}
};  // namespace Enum

const std::string& EnumToString(
    int iVal, int iMax, const char** szNameArray,
    std::string* pNameCache);  // XToString helper

int StringToEnum(const std::string& s, int iMax, const char** szNameArray);

#define XToString(X)                                         \
  const std::string& X##ToString(X x);                       \
  static_assert(NUM_##X == ARRAYLEN(X##Names));              \
  const std::string& X##ToString(X x) {                      \
    static std::string as_##X##Name[NUM_##X + 2];            \
    return EnumToString(x, NUM_##X, X##Names, as_##X##Name); \
  }                                                          \
  namespace StringConversion {                               \
  template <>                                                \
  std::string ToString<X>(const X& value) {                  \
    return X##ToString(value);                               \
  }                                                          \
  }

#define XToLocalizedString(X)                       \
  const std::string& X##ToLocalizedString(X x);     \
  const std::string& X##ToLocalizedString(X x) {    \
    static LocalizedString g_##X##Name[NUM_##X];    \
    static bool bInitialized = false;               \
    if (unlikely(!bInitialized)) {                  \
      for (unsigned i = 0; i < NUM_##X; ++i) {      \
        g_##X##Name[i].Load(#X, X##ToString((X)i)); \
      }                                             \
      bInitialized = true;                          \
    }                                               \
    return g_##X##Name[x].GetValue();               \
  }

#define StringToX(X)                                        \
  X StringTo##X(const std::string&);                        \
  static_assert(                                            \
      NUM_##X == ARRAYLEN(X##Names) &&                      \
      X##_Invalid == NUM_##X + 1);                          \
  X StringTo##X(const std::string& s) {                     \
    return (X)StringToEnum(s, ARRAYLEN(X##Names), X##Names); \
  }                                                         \
  namespace StringConversion {                              \
  template <>                                               \
  bool FromString<X>(const std::string& sValue, X& out) {   \
    out = StringTo##X(sValue);                              \
    return out != X##_Invalid;                              \
  }                                                         \
  }

// currently unused
#define LuaDeclareType(X)

#define LuaXType(X)                                                      \
  template struct EnumTraits<X>;                                         \
  static void Lua##X(lua_State* L) {                                     \
    Enum::InitLuaEnum(                                                   \
        L, EnumTraits<X>::EnumToString, EnumTraits<X>::StringToEnum, #X, \
        NUM_##X, +[](int i) -> std::string { return X##ToString((X)i); }); \
  }                                                                      \
  REGISTER_WITH_LUA_FUNCTION(Lua##X);                                    \
  template <>                                                             \
  X EnumTraits<X>::Invalid = X##_Invalid;                                 \
  template <>                                                             \
  const char* EnumTraits<X>::szName = #X;                                 \
  namespace LuaHelpers {                                                  \
  template <>                                                             \
  bool FromStack<X>(lua_State * L, X& Object, int iOffset) {              \
    Object = Enum::Check<X>(L, iOffset, true);                            \
    return Object != EnumTraits<X>::Invalid;                              \
  }                                                                       \
  }                                                                       \
  namespace LuaHelpers {                                                  \
  template <>                                                             \
  void Push<X>(lua_State * L, const X& Object) {                          \
    Enum::Push<X>(L, Object);                                             \
  }                                                                       \
  }

#endif

/**
 * @file
 * @author Chris Danford, Glenn Maynard (c) 2004-2006
 * @section LICENSE
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
