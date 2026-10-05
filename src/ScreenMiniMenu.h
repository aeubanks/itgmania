/* ScreenMiniMenu - Displays a simple menu over the top of another screen. */

#ifndef SCREEN_MINI_MENU_H
#define SCREEN_MINI_MENU_H

#include <initializer_list>
#include <string>
#include <vector>

#include "GameConstantsAndTypes.h"
#include "PlayerNumber.h"
#include "ScreenMessage.h"
#include "ScreenOptions.h"

typedef bool (*MenuRowUpdateEnabled)();

struct MenuRowDef {
  int iRowCode;
  std::string sName;
  bool bEnabled;
  MenuRowUpdateEnabled pfnEnabled;  // if ! nullptr, used instead of bEnabled
  EditMode emShowIn;
  int iDefaultChoice;
  std::vector<std::string> choices;
  bool bThemeTitle;
  bool bThemeItems;

  MenuRowDef(const MenuRowDef&);
  MenuRowDef(MenuRowDef&&) noexcept;
  MenuRowDef& operator=(const MenuRowDef&);
  MenuRowDef& operator=(MenuRowDef&&) noexcept;
  ~MenuRowDef();

  MenuRowDef(
      int r, const char* n, bool e, MenuRowUpdateEnabled pe, EditMode s,
      bool bTT, bool bTI, int d,
      std::initializer_list<const char*> choices_list);

  MenuRowDef(
      int r, const std::string& n, bool e, EditMode s, bool bTT, bool bTI,
      int d, const std::vector<std::string>& options);

  template <typename... Choices>
  MenuRowDef(
      int r, const char* n, MenuRowUpdateEnabled pe, EditMode s, bool bTT,
      bool bTI, int d, const char* c0, Choices... c)
      : MenuRowDef(r, n, true, pe, s, bTT, bTI, d, {c0, c...}) {}

  template <typename... Choices>
  MenuRowDef(
      int r, const char* n, bool e, EditMode s, bool bTT, bool bTI, int d,
      const char* c0, Choices... c)
      : MenuRowDef(r, n, e, nullptr, s, bTT, bTI, d, {c0, c...}) {}

  template <typename... Choices>
  MenuRowDef(
      int r, const std::string& n, bool e, EditMode s, bool bTT, bool bTI,
      int d, const char* c0, Choices... c)
      : MenuRowDef(r, n.c_str(), e, nullptr, s, bTT, bTI, d, {c0, c...}) {}

  void SetOneUnthemedChoice(const std::string& sChoice) {
    choices.resize(1);
    choices[0] = "|" + sChoice;
  }

  bool SetDefaultChoiceIfPresent(std::string sChoice);
};

struct MenuDef {
  std::string sClassName;
  std::vector<MenuRowDef> rows;

  MenuDef(
      const char* c, std::initializer_list<const MenuRowDef*> rows_list);
  ~MenuDef();

  template <typename... Rows>
  MenuDef(const char* c, const Rows&... r) : MenuDef(c, {&r...}) {}
};

class ScreenMiniMenu : public ScreenOptions {
 public:
  static void MiniMenu(
      const MenuDef* pDef, ScreenMessage smSendOnOK,
      ScreenMessage smSendOnCancel = SM_None, float fX = 0, float fY = 0);

  void Init();
  void BeginScreen();
  void HandleScreenMessage(const ScreenMessage SM);

 protected:
  virtual void AfterChangeValueOrRow(PlayerNumber pn);
  virtual void ImportOptions(int iRow, const std::vector<PlayerNumber>& vpns);
  virtual void ExportOptions(int iRow, const std::vector<PlayerNumber>& vpns);

  virtual bool FocusedItemEndsScreen(PlayerNumber pn) const;

  void LoadMenu(const MenuDef* pDef);

  ScreenMessage m_SMSendOnOK;
  ScreenMessage m_SMSendOnCancel;

  std::vector<MenuRowDef> m_vMenuRows;

 public:
  ScreenMiniMenu() : m_SMSendOnOK(), m_SMSendOnCancel(), m_vMenuRows() {}

  static bool s_bCancelled;
  static int s_iLastRowCode;
  static std::vector<int> s_viLastAnswers;
};

#endif

/*
 * (c) 2003-2004 Chris Danford
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
