/**
 * This file is part of Special K.
 *
 * Special K is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Special K is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Special K.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/

#pragma once

#ifndef __SK__ACHIEVEMENTS_H__
#define __SK__ACHIEVEMENTS_H__

#include <EOS/eos_achievements_types.h>

//
// Internal data stored in the Achievement Manager, this is
//   the publicly visible data...
//
//   I do not want to expose the poorly designed interface
//     of the full achievement manager outside the DLL,
//       so there exist a few flattened API functions
//         that can communicate with it and these are
//           the data they provide.
//
struct SK_Achievement
{
  std::string name_;          // UTF-8 (I think?)

  struct text_s
  {
    struct
    {
      std::wstring human_name;  // UTF-16
      std::wstring desc;        // UTF-16
    } unlocked,
      locked;
  } text_;

  // Raw pixel data (RGB8) for achievement icons
  struct
  {
    uint8_t*  achieved        = nullptr;
    uint8_t*  unachieved      = nullptr;
  } icons_;

  // If we were to call ISteamStats::GetAchievementName (...),
  //   this is the index we could use.
  int         idx_            = -1;

  float       global_percent_ = 0.0f;
  __time64_t  time_           =    0;

  struct
  {
    int       unlocked        = 0; // Number of friends who have unlocked
    int       possible        = 0; // Number of friends who may be able to unlock
  } friends_;

  struct
  {
    int       current         = 0;
    int       max             = 0;
    double    precalculated   = 0.0;

    __forceinline float getPercent (void) noexcept
    {
      if (precalculated != 0.0)
        return sk::narrow_cast <float> (precalculated);

      return 100.0F * sk::narrow_cast <float> (current) /
                      sk::narrow_cast <float> (max);
    }
  } progress_;

  bool        unlocked_       = false;
  bool        hidden_         = false;
};

namespace CEGUI
{
  class Window;
};

class SK_AchievementManager
{
public:
  class Achievement : public SK_Achievement
  {
  public:
    Achievement (int idx, const char* szName, ISteamUserStats*               stats);
    Achievement (int idx,                     EOS_Achievements_DefinitionV2* def);

     Achievement (const Achievement& copy) = delete;

    ~Achievement (void)
    {
      if (icons_.unachieved != nullptr)
      {
        free (std::exchange (
          icons_.unachieved, nullptr)
        );
      }

      if (icons_.achieved != nullptr)
      {
        free (
          std::exchange (icons_.achieved, nullptr)
        );
      }
    }

    void update (ISteamUserStats* stats)
    {
      if (stats == nullptr)
        return;

      stats->GetAchievementAndUnlockTime ( name_.c_str (),
                                          &unlocked_,
                              (uint32_t *)&time_ );
    }

    void update_global (ISteamUserStats* stats)
    {
      if (stats == nullptr)
        return;

      // Reset to 0.0 on read failure
      if (! stats->GetAchievementAchievedPercent (
              name_.c_str (),
                &global_percent_                 )
         )
      {
        steam_log->Log (
          L" Global Achievement Read Failure For '%hs'", name_.c_str ()
        );
        global_percent_ = 0.0f;
      }
    }
  };

  float getPercentOfAchievementsUnlocked (void) const;

  void             loadSound       (const wchar_t* wszUnlockSound);

  void             addAchievement  (Achievement* achievement);
  Achievement*     getAchievement  (const char* szName                  ) const;
  SK_Achievement** getAchievements (    size_t* pnAchievements = nullptr);

  void             clearPopups     (void);
  int              drawPopups      (void);

  // Make protected once Epic enumerates achievements
  float percent_unlocked = 0.0f;

protected:
  struct SK_AchievementPopup
  {
    CEGUI::Window* window;
    DWORD          time;
    bool           final_pos; // When the animation is finished, this will be set.
    Achievement*   achievement;
  };

  CEGUI::Window* createPopupWindow (SK_AchievementPopup* popup);

  struct SK_AchievementStorage
  {
    std::vector        <             Achievement*> list;
    std::unordered_map <std::string, Achievement*> string_map;
  } achievements; // SELF

  std::vector <SK_AchievementPopup> popups;
  int                               lifetime_popups;

  bool                  default_loaded = false;
  std::vector <uint8_t> unlock_sound; // A .WAV (PCM) file
};

#endif /* __SK__ACHIEVEMENTS_H__ */