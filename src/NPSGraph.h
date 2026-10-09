#ifndef NPS_GRAPH_H
#define NPS_GRAPH_H

#include <vector>

#include "ActorMultiVertex.h"
#include "PlayerNumber.h"
#include "RageTypes.h"
#include "global.h"

class Steps;

/**
 * @brief An engine-side rendering of a chart's NPS-per-measure density graph.
 *
 * The chart data (Steps::GetNpsPerMeasure/GetPeakNps) is cached by the engine,
 * so the graph is built in C++ and rebuilt when the chart, size, color, or
 * desaturation changes. Scrolling only re-emits the visible window.
 *
 * Vertices are laid out with x in [0, graph width] and y in [-height, 0].
 */
class NPSGraph : public ActorMultiVertex {
 public:
  NPSGraph();
  virtual ~NPSGraph();
  virtual NPSGraph* Copy() const override;
  // Sets the chart to graph and which player's data to use.
  void SetSteps(Steps* pSteps, PlayerNumber pn);
  // The size of the visible graph area, in pixels.
  void SetSize(float width, float height);
  // The full width of the graph. Defaults to the width passed to SetSize.
  // Set this wider than the visible size to enable scrolling.
  void SetGraphWidth(float width);
  // Gradient endpoints. The gradient is interpolated by the height of each
  // measure, from the bottom color to the top color.
  void SetColors(const RageColor& bottom, const RageColor& top);
  // 0 = full color, 1 = fully desaturated.
  void SetDesaturation(float desaturation);
  // The left edge of the visible window, in graph pixels.
  void SetScrollOffset(float offset);

  // Lua
  virtual void PushSelf(lua_State* L) override;

 private:
  struct Vertex {
    float x;
    float y;
    RageColor color;
  };

  void Rebuild();
  bool CanBuild() const;
  void RebuildFull();
  void RebuildVisible();
  void Emit(const std::vector<Vertex>& verts);
  static Vertex Interpolate(const Vertex& v1, const Vertex& v2, float x);
  static RageColor Desaturate(RageColor color, float desaturation);

  Steps* m_pSteps;
  PlayerNumber m_Player;
  float m_ViewWidth;
  float m_ViewHeight;
  float m_GraphWidth;
  bool m_bGraphWidthSet;
  float m_ScrollOffset;
  RageColor m_BottomColor;
  RageColor m_TopColor;
  float m_Desaturation;
  std::vector<Vertex> m_AllVerts;
};

#endif

/*
 * (c) 2026 ITGmania contributors
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
