/* ThemeMetric - Theme specific data. */

#ifndef THEME_METRIC_H
#define THEME_METRIC_H

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "LuaManager.h"
#include "LuaReference.h"
#include "ThemeManager.h"
#include "global.h"

/** @brief The general interface for reading ThemeMetrics. */
class IThemeMetric {
 protected:
  /** @brief the metric's group.
   *
   * In metrics.ini, it is usually done as such: [GroupName] */
  std::string m_sGroup;
  /** @brief the metric's name. */
  std::string m_sName;
  /** @brief the metric's value. */
  LuaReference m_Value;
  bool m_bCallEachTime;

  IThemeMetric(
      const std::string& sGroup = "", const std::string& sName = "");
  IThemeMetric(const IThemeMetric& cpy);

  Lua* BeginRead(bool bCallable);
  Lua* BeginCallEachTime() const;
  void EndRead(Lua* L) const;

 public:
  virtual ~IThemeMetric();
  virtual void Read() = 0;
  void Clear();

  /**
   * @brief Load the chosen metric from the .ini file.
   * @param sGroup the group the metric is in.
   * @param sName the name of the metric. */
  void Load(const std::string& sGroup, const std::string& sName);
  void PushSelf(Lua* L);

  /**
   * @brief Retrieve the metric's name.
   * @return the metric's name. */
  const std::string& GetName() const { return m_sName; }

  bool IsLoaded() const { return m_Value.IsSet(); }
};

template <class T>
struct ThemeMetricTypeTraits {
  enum { Callable = 1 };
};

/* LuaReference and apActorCommands return the function directly without calling
 * it. */
template <>
struct ThemeMetricTypeTraits<LuaReference> {
  enum { Callable = 0 };
};
template <>
struct ThemeMetricTypeTraits<apActorCommands> {
  enum { Callable = 0 };
};

/** @brief The theme specific data.
 *
 * Each piece of data is to correspond to a type. */
template <class T>
class ThemeMetric : public IThemeMetric {
 protected:
  mutable T m_currentValue;

 public:
  /* Initializing with no group and name is allowed; if you do this, you must
   * call Load() to set them.  This is done to allow initializing cached metrics
   * in one place for classes that don't receive their m_sName in the
   * constructor (everything except screens). */
  ThemeMetric(const std::string& sGroup = "", const std::string& sName = "")
      : IThemeMetric(sGroup, sName), m_currentValue(T()) {
    ThemeManager::Subscribe(this);
  }

  ThemeMetric(const ThemeMetric<T>& cpy)
      : IThemeMetric(cpy), m_currentValue(cpy.m_currentValue) {
    ThemeManager::Subscribe(this);
  }

  ~ThemeMetric() override = default;

  /**
   * @brief Actually read the metric and get its data. */
  void Read() override {
    if (Lua* L = BeginRead(ThemeMetricTypeTraits<T>::Callable)) {
      LuaHelpers::FromStack(L, m_currentValue, -1);
      EndRead(L);
    }
  }

  /**
   * @brief Retrieve the metric's value.
   * @return the metric's value. */
  const T& GetValue() const {
    if (unlikely(!m_Value.IsSet() || m_bCallEachTime)) {
      if (Lua* L = BeginCallEachTime()) {
        LuaHelpers::FromStack(L, m_currentValue, -1);
        EndRead(L);
      }
    }
    return m_currentValue;
  }

  operator const T&() const { return GetValue(); }

  // Hacks for VC6 for all boolean operators.
  // These three no longer appear to be required:
  // bool operator ! () const { return !GetValue(); }
  // bool operator && ( const T& input ) const { return GetValue() && input; }
  // bool operator || ( const T& input ) const { return GetValue() || input; }

  // This one is still required in at least Visual Studio 2008:
  bool operator==(const T& input) const { return GetValue() == input; }
};

typedef std::string (*MetricName1D)(size_t N);

template <class T>
class ThemeMetric1D {
  typedef ThemeMetric<T> ThemeMetricT;
  std::vector<ThemeMetricT> m_metric;

 public:
  ThemeMetric1D(const std::string& sGroup, MetricName1D pfn, size_t N) {
    Load(sGroup, pfn, N);
  }
  ThemeMetric1D() { Load(std::string(), nullptr, 0); }
  void Load(const std::string& sGroup, MetricName1D pfn, size_t N) {
    m_metric.resize(N);
    for (unsigned i = 0; i < N; i++) {
      m_metric[i].Load(sGroup, pfn(i));
    }
  }
  void Clear() {
    for (unsigned i = 0; i < m_metric.size(); i++) {
      m_metric[i].Clear();
    }
  }
  const T& GetValue(size_t i) const { return m_metric[i].GetValue(); }
};

typedef std::string (*MetricName2D)(size_t N, size_t M);

template <class T>
class ThemeMetric2D {
  typedef ThemeMetric<T> ThemeMetricT;
  typedef std::vector<ThemeMetricT> ThemeMetricTVector;
  std::vector<ThemeMetricTVector> m_metric;

 public:
  ThemeMetric2D(
      const std::string& sGroup = "", MetricName2D pfn = nullptr, size_t N = 0,
      size_t M = 0) {
    Load(sGroup, pfn, N, M);
  }
  void Load(const std::string& sGroup, MetricName2D pfn, size_t N, size_t M) {
    m_metric.resize(N);
    for (unsigned i = 0; i < N; i++) {
      m_metric[i].resize(M);
      for (unsigned j = 0; j < M; j++) {
        m_metric[i][j].Load(sGroup, pfn(i, j));
      }
    }
  }
  const T& GetValue(size_t i, size_t j) const {
    return m_metric[i][j].GetValue();
  }
};

typedef std::string (*MetricNameMap)(std::string s);

template <class T>
class ThemeMetricMap {
  typedef ThemeMetric<T> ThemeMetricT;
  std::map<std::string, ThemeMetricT> m_metric;

 public:
  ThemeMetricMap(
      const std::string& sGroup = "", MetricNameMap pfn = nullptr,
      const std::vector<std::string> vsValueNames =
          std::vector<std::string>()) {
    Load(sGroup, pfn, vsValueNames);
  }
  void Load(
      const std::string& sGroup, MetricNameMap pfn,
      const std::vector<std::string> vsValueNames) {
    m_metric.clear();
    for (const std::string& s : vsValueNames) {
      m_metric[s].Load(sGroup, pfn(s));
    }
  }
  const T& GetValue(std::string s) const {
    // HACK: GCC (3.4) takes this and pretty much nothing else.
    // I don't know why.
    typename std::map<std::string, ThemeMetric<T> >::const_iterator iter =
        m_metric.find(s);
    ASSERT(iter != m_metric.end());
    return iter->second.GetValue();
  }
};

#endif

/**
 * @file
 * @author Chris Danford, Chris Gomez (c) 2001-2004
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
