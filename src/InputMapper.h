#ifndef INPUT_MAPPER_H
#define INPUT_MAPPER_H

#include <initializer_list>
#include <string>
#include <vector>

#include "GameInput.h"
#include "PlayerNumber.h"
#include "RageInputDevice.h"

struct Game;

const int NUM_GAME_TO_DEVICE_SLOTS =
    5;  // five device inputs may map to one game input
const int NUM_SHOWN_GAME_TO_DEVICE_SLOTS = 3;
const int NUM_USER_GAME_TO_DEVICE_SLOTS = 2;
extern const std::string DEVICE_INPUT_SEPARATOR;

struct AutoMappingEntry {
  AutoMappingEntry(int i, DeviceButton db, GameButton gb, bool b)
      : m_iSlotIndex(i), m_deviceButton(db), m_gb(gb), m_bSecondController(b) {}

  int m_iSlotIndex;
  DeviceButton m_deviceButton;
  GameButton m_gb;  // GameButton_Invalid means unmap this button
  /* If this is true, this is an auxilliary mapping assigned to the second
   * player. If two of the same device are found, and the device has secondary
   * entries, the later entries take precedence. This way, if a Pump pad is
   * found, it'll map P1 to the primary pad and P2 to the secondary pad. (We
   * can't tell if a slave pad is actually there.) Then, if a second primary
   * is found (DEVICE_PUMP2), 2P will be mapped to it. */
  bool m_bSecondController;
};

struct AutoMappings {
  AutoMappings(
      std::string s1, std::string s2, std::string s3,
      std::initializer_list<AutoMappingEntry> maps_list = {});

  template <typename... Entries>
  AutoMappings(
      const char* s1, const char* s2, const char* s3, const Entries&... im)
      : AutoMappings(s1, s2, s3, {im...}) {}

  // Strings used by automatic joystick mappings.
  std::string m_sGame;            // only used
  std::string m_sDriverRegex;     // reported by InputHandler
  std::string m_sControllerName;  // the product name of the controller

  std::vector<AutoMappingEntry> m_vMaps;
};

class InputScheme {
 public:
  const char* m_szName;
  int m_iButtonsPerController;
  struct GameButtonInfo {
    const char* m_szName;  // The name used by the button graphics system.  e.g.
                           // "left", "right", "middle C", "snare"
    GameButton m_SecondaryMenuButton;
  };
  // Data for each Game-specific GameButton. This starts at GAME_BUTTON_NEXT.
  GameButtonInfo m_GameButtonInfo[NUM_GameButton];
  const AutoMappings* m_pAutoMappings;

  GameButton ButtonNameToIndex(const std::string& sButtonName) const;
  GameButton GameButtonToMenuButton(GameButton gb) const;
  void MenuButtonToGameInputs(
      GameButton MenuI, PlayerNumber pn,
      std::vector<GameInput>& GameIout) const;
  void MenuButtonToGameButtons(
      GameButton MenuI, std::vector<GameButton>& aGameButtons) const;
  const GameButtonInfo* GetGameButtonInfo(GameButton gb) const;
  const char* GetGameButtonName(GameButton gb) const;
};
/** @brief A special foreach loop to handle the various GameButtons. */
#define FOREACH_GameButtonInScheme(s, var)                               \
  for (GameButton var = (GameButton)0; var < s->m_iButtonsPerController; \
       enum_add<GameButton>(var, +1))

class InputMappings {
 public:
  // only filled for automappings
  std::string m_sDeviceRegex;
  std::string m_sDescription;

  // map from a GameInput to multiple DeviceInputs
  DeviceInput m_GItoDI[NUM_GameController][NUM_GameButton]
                      [NUM_GAME_TO_DEVICE_SLOTS];

  void Clear();
  void Unmap(InputDevice id);
  void WriteMappings(const InputScheme* pInputScheme, std::string sFilePath);
  void ReadMappings(
      const InputScheme* pInputScheme, std::string sFilePath,
      bool bIsAutoMapping);
  void SetInputMap(
      const DeviceInput& DeviceI, const GameInput& GameI, int iSlotIndex);

  void ClearFromInputMap(const DeviceInput& DeviceI);
  bool ClearFromInputMap(const GameInput& GameI, int iSlotIndex);
};
/** @brief Holds user-chosen input preferences and saves it between sessions. */
class InputMapper {
 public:
  InputMapper();
  ~InputMapper();

  void SetInputScheme(const InputScheme* pInputScheme);
  const InputScheme* GetInputScheme() const;
  void SetJoinControllers(PlayerNumber pn);

  void ReadMappingsFromDisk();
  void SaveMappingsToDisk();
  void ResetMappingsToDefault();
  void CheckButtonAndAddToReason(
      GameButton menu, std::vector<std::string>& full_reason,
      const std::string& sub_reason);
  void SanityCheckMappings(std::vector<std::string>& reason);

  void SetInputMap(
      const DeviceInput& DeviceI, const GameInput& GameI, int iSlotIndex);
  void ClearFromInputMap(const DeviceInput& DeviceI);
  bool ClearFromInputMap(const GameInput& GameI, int iSlotIndex);

  void AddDefaultMappingsForCurrentGameIfUnmapped();
  void AutoMapJoysticksForCurrentGame();
  bool CheckForChangedInputDevicesAndRemap(std::string& sMessageOut);

  bool IsMapped(const DeviceInput& DeviceI) const;

  bool DeviceToGame(const DeviceInput& DeviceI, GameInput& GameI)
      const;  // return true if there is a mapping from device to pad
  bool GameToDevice(const GameInput& GameI, int iSlotNum, DeviceInput& DeviceI)
      const;  // return true if there is a mapping from pad to device

  GameButton GameButtonToMenuButton(GameButton gb) const;
  void MenuToGame(
      GameButton MenuI, PlayerNumber pn,
      std::vector<GameInput>& GameIout) const;
  PlayerNumber ControllerToPlayerNumber(GameController controller) const;

  float GetSecsHeld(
      const GameInput& GameI, MultiPlayer mp = MultiPlayer_Invalid) const;

  bool IsBeingPressed(
      const GameInput& GameI, MultiPlayer mp = MultiPlayer_Invalid,
      const DeviceInputList* pButtonState = nullptr) const;
  bool IsBeingPressed(GameButton MenuI, PlayerNumber pn) const;
  bool IsBeingPressed(
      const std::vector<GameInput>& GameI, MultiPlayer mp = MultiPlayer_Invalid,
      const DeviceInputList* pButtonState = nullptr) const;

  void ResetKeyRepeat(const GameInput& GameI);
  void ResetKeyRepeat(GameButton MenuI, PlayerNumber pn);

  void RepeatStopKey(const GameInput& GameI);
  void RepeatStopKey(GameButton MenuI, PlayerNumber pn);

  float GetLevel(const GameInput& GameI) const;
  float GetLevel(GameButton MenuI, PlayerNumber pn) const;

  static InputDevice MultiPlayerToInputDevice(MultiPlayer mp);
  static MultiPlayer InputDeviceToMultiPlayer(InputDevice id);

  void Unmap(InputDevice device);
  void ApplyMapping(
      const std::vector<AutoMappingEntry>& vMmaps, GameController gc,
      InputDevice id);

 protected:
  InputMappings m_mappings;

  void UpdateTempDItoGI();
  const InputScheme* m_pInputScheme;

 private:
  InputMapper(const InputMapper& rhs);
  InputMapper& operator=(const InputMapper& rhs);
};

extern InputMapper*
    INPUTMAPPER;  // global and accessible from anywhere in our program

#endif

/*
 * (c) 2001-2003 Chris Danford
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
