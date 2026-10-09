#include "NPSGraph.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ActorUtil.h"
#include "LuaBinding.h"
#include "MeasureInfo.h"
#include "RageUtil.h"
#include "Song.h"
#include "Steps.h"
#include "TimingData.h"
#include "global.h"

namespace {
const RageColor kDefaultBottomColor(0.0f, 1.0f, 0.0f, 1.0f);
const RageColor kDefaultTopColor(1.0f, 0.0f, 0.0f, 1.0f);
}  // namespace

REGISTER_ACTOR_CLASS(NPSGraph);

NPSGraph::NPSGraph()
    : m_pSteps(nullptr),
      m_Player(PLAYER_1),
      m_ViewWidth(0.0f),
      m_ViewHeight(0.0f),
      m_GraphWidth(0.0f),
      m_bGraphWidthSet(false),
      m_ScrollOffset(0.0f),
      m_BottomColor(kDefaultBottomColor),
      m_TopColor(kDefaultTopColor),
      m_Desaturation(0.0f) {
  SetDrawState(DrawMode_QuadStrip, 0, -1);
}

NPSGraph::~NPSGraph() {}

void NPSGraph::SetSteps(Steps* pSteps, PlayerNumber pn) {
  if (m_pSteps == pSteps && m_Player == pn) {
    return;
  }
  m_pSteps = pSteps;
  m_Player = pn;
  Rebuild();
}

void NPSGraph::SetSize(float width, float height) {
  m_ViewWidth = width;
  m_ViewHeight = height;
  if (!m_bGraphWidthSet) {
    m_GraphWidth = width;
  }
  Rebuild();
}

void NPSGraph::SetGraphWidth(float width) {
  m_GraphWidth = width;
  m_bGraphWidthSet = true;
  Rebuild();
}

void NPSGraph::SetColors(const RageColor& bottom, const RageColor& top) {
  m_BottomColor = bottom;
  m_TopColor = top;
  Rebuild();
}

void NPSGraph::SetDesaturation(float desaturation) {
  m_Desaturation = desaturation;
  Rebuild();
}

void NPSGraph::SetScrollOffset(float offset) {
  if (m_ScrollOffset == offset) {
    return;
  }
  m_ScrollOffset = offset;
  RebuildVisible();
}

void NPSGraph::Rebuild() {
  if (CanBuild()) {
    RebuildFull();
    RebuildVisible();
  } else {
    m_AllVerts.clear();
    SetNumVertices(0);
    SetDrawState(DrawMode_QuadStrip, 0, 0);
  }
}

bool NPSGraph::CanBuild() const {
  return m_pSteps != nullptr && m_ViewWidth > 0.0f && m_ViewHeight > 0.0f &&
         m_GraphWidth > 0.0f;
}

RageColor NPSGraph::Desaturate(RageColor color, float desaturation) {
  const float luma = 0.3f * color.r + 0.59f * color.g + 0.11f * color.b;
  color.r += desaturation * (luma - color.r);
  color.g += desaturation * (luma - color.g);
  color.b += desaturation * (luma - color.b);
  return color;
}

NPSGraph::Vertex NPSGraph::Interpolate(
    const Vertex& v1, const Vertex& v2, float x) {
  const float dx = v2.x - v1.x;
  const float ratio = (dx != 0.0f) ? (x - v1.x) / dx : 0.0f;
  Vertex out;
  out.x = x;
  out.y = v1.y * (1.0f - ratio) + v2.y * ratio;
  lerp_rage_color(out.color, v1.color, v2.color, ratio);
  return out;
}

void NPSGraph::RebuildFull() {
  m_AllVerts.clear();

  const std::vector<float>& nps = m_pSteps->GetNpsPerMeasure(m_Player);
  const float peak = m_pSteps->GetPeakNps(m_Player);
  Song* pSong = m_pSteps->m_pSong;
  if (nps.empty() || peak <= 0.0f || pSong == nullptr) {
    return;
  }
  TimingData* pTiming = m_pSteps->GetTimingData();

  const float first = std::min(pTiming->GetElapsedTimeFromBeat(0.0f), 0.0f);
  const float last = pSong->GetLastSecond();
  if (last - first <= 0.0f) {
    return;
  }

  RageColor bottom = m_BottomColor;
  RageColor top = m_TopColor;
  if (m_Desaturation != 0.0f) {
    bottom = Desaturate(bottom, m_Desaturation);
    top = Desaturate(top, m_Desaturation);
  }

  const float width = m_GraphWidth;
  const float height = m_ViewHeight;

  bool started = false;
  float prev_x = 0.0f;
  for (std::size_t i = 0; i < nps.size(); ++i) {
    if (nps[i] > 0.0f) {
      started = true;
    }
    if (!started) {
      continue;
    }

    const float seconds = pTiming->GetElapsedTimeFromBeat(
        static_cast<float>(i) * static_cast<float>(BEATS_PER_MEASURE));
    // Keep x non-decreasing so the visible-window search can assume order.
    const float x = std::max(SCALE(seconds, first, last, 0.0f, width), prev_x);
    prev_x = x;
    // Quantize y to the nearest pixel so the run-length compression below
    // merges near-equal measures.
    const float y = std::round(-SCALE(nps[i], 0.0f, peak, 0.0f, height));

    // If the height of this measure matches the previous two, extend the
    // previous pair of vertices instead of adding a new pair. This noticeably
    // shrinks the vertex count for songs with long streams.
    const std::size_t count = m_AllVerts.size();
    if (count > 2 && m_AllVerts[count - 1].y == y &&
        m_AllVerts[count - 3].y == y) {
      m_AllVerts[count - 1].x = x;
      m_AllVerts[count - 2].x = x;
    } else {
      RageColor upper;
      lerp_rage_color(upper, bottom, top, std::abs(y / height));
      m_AllVerts.push_back({x, 0.0f, bottom});
      m_AllVerts.push_back({x, y, upper});
    }
  }

  // Add a 0 NPS point at the end so the last measure slopes down instead of
  // ending abruptly.
  if (!nps.empty() && nps.back() != 0.0f) {
    m_AllVerts.push_back({width, 0.0f, bottom});
    m_AllVerts.push_back({width, 0.0f, bottom});
  }
}

void NPSGraph::RebuildVisible() {
  const std::size_t pair_count = m_AllVerts.size() / 2;
  if (pair_count == 0) {
    SetNumVertices(0);
    return;
  }

  const float left = m_ScrollOffset;
  const float right = m_ScrollOffset + m_ViewWidth;

  // First pair at or past the left edge of the window.
  int a = 0;
  while (a < static_cast<int>(pair_count) &&
         m_AllVerts[static_cast<std::size_t>(a) * 2].x < left) {
    ++a;
  }
  // Last pair at or before the right edge of the window.
  int b = static_cast<int>(pair_count) - 1;
  while (b >= 0 && m_AllVerts[static_cast<std::size_t>(b) * 2].x > right) {
    --b;
  }

  if (a >= static_cast<int>(pair_count) || b < 0) {
    SetNumVertices(0);
    return;
  }

  std::vector<Vertex> visible;
  visible.reserve(static_cast<std::size_t>(b - a + 2) * 2);

  // Trim the partial quads at the window edges so the graph does not extend
  // past the visible area.
  if (a > 0) {
    visible.push_back(Interpolate(
        m_AllVerts[(static_cast<std::size_t>(a) - 1) * 2],
        m_AllVerts[static_cast<std::size_t>(a) * 2], left));
    visible.push_back(Interpolate(
        m_AllVerts[(static_cast<std::size_t>(a) - 1) * 2 + 1],
        m_AllVerts[static_cast<std::size_t>(a) * 2 + 1], left));
  }

  for (int k = a; k <= b; ++k) {
    visible.push_back(m_AllVerts[static_cast<std::size_t>(k) * 2]);
    visible.push_back(m_AllVerts[static_cast<std::size_t>(k) * 2 + 1]);
  }

  if (b + 1 < static_cast<int>(pair_count)) {
    visible.push_back(Interpolate(
        m_AllVerts[static_cast<std::size_t>(b) * 2],
        m_AllVerts[(static_cast<std::size_t>(b) + 1) * 2], right));
    visible.push_back(Interpolate(
        m_AllVerts[static_cast<std::size_t>(b) * 2 + 1],
        m_AllVerts[(static_cast<std::size_t>(b) + 1) * 2 + 1], right));
  }

  Emit(visible);
}

void NPSGraph::Emit(const std::vector<Vertex>& verts) {
  SetNumVertices(verts.size());
  for (std::size_t i = 0; i < verts.size(); ++i) {
    SetVertexPos(static_cast<int>(i), verts[i].x, verts[i].y, 0.0f);
    SetVertexColor(static_cast<int>(i), verts[i].color);
  }
  SetDrawState(DrawMode_QuadStrip, 0, -1);
}

// lua start
/** @brief Allow Lua to have access to the NPSGraph. */
class LunaNPSGraph : public Luna<NPSGraph> {
 public:
  static int SetSteps(T* p, lua_State* L) {
    Steps* pSteps = nullptr;
    if (!lua_isnoneornil(L, 1)) {
      pSteps = Luna<Steps>::check(L, 1);
    }
    PlayerNumber pn = PLAYER_1;
    if (!lua_isnoneornil(L, 2)) {
      pn = Enum::Check<PlayerNumber>(L, 2);
    }
    p->SetSteps(pSteps, pn);
    COMMON_RETURN_SELF;
  }

  static int SetSize(T* p, lua_State* L) {
    p->SetSize(FArg(1), FArg(2));
    COMMON_RETURN_SELF;
  }

  static int SetGraphWidth(T* p, lua_State* L) {
    p->SetGraphWidth(FArg(1));
    COMMON_RETURN_SELF;
  }

  static int SetColors(T* p, lua_State* L) {
    RageColor bottom;
    RageColor top;
    bottom.FromStackCompat(L, 1);
    top.FromStackCompat(L, 2);
    p->SetColors(bottom, top);
    COMMON_RETURN_SELF;
  }

  static int SetDesaturation(T* p, lua_State* L) {
    p->SetDesaturation(FArg(1));
    COMMON_RETURN_SELF;
  }

  static int SetScrollOffset(T* p, lua_State* L) {
    p->SetScrollOffset(FArg(1));
    COMMON_RETURN_SELF;
  }

  LunaNPSGraph() {
    ADD_METHOD(SetSteps);
    ADD_METHOD(SetSize);
    ADD_METHOD(SetGraphWidth);
    ADD_METHOD(SetColors);
    ADD_METHOD(SetDesaturation);
    ADD_METHOD(SetScrollOffset);
  }
};

LUA_REGISTER_DERIVED_CLASS(NPSGraph, ActorMultiVertex)
// lua end
