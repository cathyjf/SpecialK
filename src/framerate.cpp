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

#include <SpecialK/stdafx.h>

#include <SpecialK/commands/limit_reset.inl>

#ifndef _d3d9TYPES_H_
#undef  _D3D9_H_
#include <d3d9.h>
#endif

#include <d3d11.h>

SK::Framerate::Stats* frame_history  = nullptr;
SK::Framerate::Stats* frame_history2 = nullptr;

using NTSTATUS = _Return_type_success_(return >= 0) LONG;

using NtQueryTimerResolution_pfn = NTSTATUS (NTAPI *)
(
  OUT PULONG              MinimumResolution,
  OUT PULONG              MaximumResolution,
  OUT PULONG              CurrentResolution
);

using NtSetTimerResolution_pfn = NTSTATUS (NTAPI *)
(
  IN  ULONG               DesiredResolution,
  IN  BOOLEAN             SetResolution,
  OUT PULONG              CurrentResolution
);

HMODULE                    NtDll                  = nullptr;
NtQueryTimerResolution_pfn NtQueryTimerResolution = nullptr;
NtSetTimerResolution_pfn   NtSetTimerResolution   = nullptr;

typedef NTSTATUS (WINAPI *NtDelayExecution_pfn)(
  IN  BOOLEAN        Alertable,
  IN  PLARGE_INTEGER DelayInterval
);

NtDelayExecution_pfn NtDelayExecution = nullptr;


// Dispatch through the trampoline, rather than hook
//
using WaitForVBlank_pfn = HRESULT (STDMETHODCALLTYPE *)(
  IDXGIOutput *This
);
extern WaitForVBlank_pfn WaitForVBlank_Original;


SK::Framerate::EventCounter* SK::Framerate::events = nullptr;

LPVOID pfnQueryPerformanceCounter = nullptr;
LPVOID pfnSleep                   = nullptr;

Sleep_pfn                   Sleep_Original                     = nullptr;
SleepEx_pfn                 SleepEx_Original                   = nullptr;
QueryPerformanceCounter_pfn QueryPerformanceCounter_Original   = nullptr;
QueryPerformanceCounter_pfn ZwQueryPerformanceCounter          = nullptr;
QueryPerformanceCounter_pfn RtlQueryPerformanceCounter         = nullptr;
QueryPerformanceCounter_pfn QueryPerformanceFrequency_Original = nullptr;
QueryPerformanceCounter_pfn RtlQueryPerformanceFrequency       = nullptr;

LARGE_INTEGER
SK_QueryPerf ()
{
  return
    SK_CurrentPerf ();
}

HANDLE hModSteamAPI = nullptr;

LARGE_INTEGER SK::Framerate::Stats::freq;

#include <SpecialK/utility.h>
#include <SpecialK/steam_api.h>


auto
  long_double_cast =
  [](auto val) ->
    long double
    {
      return
        static_cast <long double> (val);
    };

void
SK_Thread_WaitWhilePumpingMessages (DWORD dwMilliseconds)
{
  ////if (! SK_Win32_IsGUIThread ())
  ////{
  ////  while (GetQueueStatus (QS_ALLEVENTS) == 0)
  ////  {
  ////    SK_Sleep (dwMilliseconds /= 2);
  ////
  ////    if (dwMilliseconds == 1)
  ////    {
  ////      SK_SleepEx (dwMilliseconds, TRUE);
  ////      return;
  ////    }
  ////  }
  ////}

  HWND hWndThis = GetActiveWindow ();
  bool bUnicode =
    IsWindowUnicode (hWndThis);

  auto PeekAndDispatch =
  [&]
  {
    MSG msg     = {      };
    msg.message = WM_NULL ;

    // Avoid having Windows marshal Unicode messages like a dumb ass
    if (bUnicode)
    {
      if ( PeekMessageW ( &msg, 0, 0, 0,
                                            PM_REMOVE | QS_ALLINPUT)
               &&          msg.message != WM_NULL
         )
      {
        SK_LOG0 ( ( L"Dispatched Message: %x to Unicode HWND: %x while framerate limiting!", msg.message, msg.hwnd ),
                    L"Win32-Pump" );

        DispatchMessageW (&msg);
      }
    }

    else
    {
      if ( PeekMessageA ( &msg, 0, 0, 0,
                                            PM_REMOVE | QS_ALLINPUT)
               &&          msg.message != WM_NULL
         )
      {
        SK_LOG0 ( ( L"Dispatched Message: %x to ANSI HWND: %x while framerate limiting!", msg.message, msg.hwnd ),
                    L"Win32-Pump" );
        DispatchMessageA (&msg);
      }
    }
  };


  if (dwMilliseconds == 0)
  {
    return;
  }


  LARGE_INTEGER liStart      = SK_CurrentPerf ();
  long long     liTicksPerMS = SK_GetPerfFreq ().QuadPart / 1000LL;
  long long     liEnd        = liStart.QuadPart + ( liTicksPerMS * dwMilliseconds );

  LARGE_INTEGER liNow = liStart;

  while ((liNow = SK_CurrentPerf ()).QuadPart < liEnd)
  {
    DWORD dwMaxWait =
      narrow_cast <DWORD> (std::max (0LL, (liEnd - liNow.QuadPart) / liTicksPerMS));

    if (dwMaxWait < INT_MAX && dwMaxWait > 1)
    {
      ////dwMaxWait = std::min (150UL, dwMaxWait);

      DWORD dwWait =
        MsgWaitForMultipleObjectsEx (
          1, &__SK_DLL_TeardownEvent, dwMaxWait,
            QS_ALLINPUT, MWMO_ALERTABLE    |
                         MWMO_INPUTAVAILABLE
                                    );

      if ( dwWait == WAIT_OBJECT_0 )
        break;

      else
      {
        PeekAndDispatch ();
      }
    }

    else
      break;
  }
}

bool fix_sleep_0 = false;


float
SK_Sched_ThreadContext::most_recent_wait_s::getRate (void)
{
  if (sequence > 0)
  {
    double ms =
      SK_DeltaPerfMS (
        SK_CurrentPerf ().QuadPart - (last_wait.QuadPart - start.QuadPart), 1
      );

    return
      static_cast <float> ( ms / long_double_cast (sequence) );
  }

  // Sequence just started
  return -1.0f;
}

typedef
NTSTATUS (NTAPI *NtWaitForSingleObject_pfn)(
  IN HANDLE         Handle,
  IN BOOLEAN        Alertable,
  IN PLARGE_INTEGER Timeout    // Microseconds
);

NtWaitForSingleObject_pfn
NtWaitForSingleObject_Original = nullptr;

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L) // ntsubauth
#define STATUS_ALERTED                   ((NTSTATUS)0x00000101L)

DWORD
WINAPI
SK_WaitForSingleObject_Micro ( _In_  HANDLE          hHandle,
                               _In_ PLARGE_INTEGER pliMicroseconds )
{
  //if (NtWaitForSingleObject_Original != nullptr)
  //{
  //  NTSTATUS NtStatus =
  //    NtWaitForSingleObject_Original (
  //      hHandle,
  //        FALSE,
  //          pliMicroseconds
  //    );
  //
  //  switch (NtStatus)
  //  {
  //    case STATUS_SUCCESS:
  //      return WAIT_OBJECT_0;
  //    case STATUS_TIMEOUT:
  //      return WAIT_TIMEOUT;
  //    case STATUS_ALERTED:
  //      return WAIT_IO_COMPLETION;
  //    case STATUS_USER_APC:
  //      return WAIT_IO_COMPLETION;
  //  }
  //
  //  // ???
  //  return
  //    WAIT_FAILED;
  //}

  DWORD dwMilliseconds =
    ( pliMicroseconds == nullptr ?
              INFINITE : static_cast <DWORD> (
                           pliMicroseconds->QuadPart / 1000ULL
                         )
    );

  return
    WaitForSingleObject ( hHandle,
                            dwMilliseconds );
}

DWORD
WINAPI
SK_WaitForSingleObject (_In_ HANDLE hHandle,
                        _In_ DWORD  dwMilliseconds )
{
  //return
  //  WaitForSingleObject (hHandle, dwMilliseconds);

  if (dwMilliseconds == INFINITE)
  {
    return
      SK_WaitForSingleObject_Micro ( hHandle, nullptr );
  }

  LARGE_INTEGER usecs;
                usecs.QuadPart =
                  static_cast <LONGLONG> (
                    dwMilliseconds
                  ) * 1000ULL;

  return
    SK_WaitForSingleObject_Micro ( hHandle, &usecs );
}

typedef enum _OBJECT_WAIT_TYPE {
  WaitAllObject,
  WaitAnyObject
} OBJECT_WAIT_TYPE, *POBJECT_WAIT_TYPE;

typedef
NTSTATUS (NTAPI *NtWaitForMultipleObjects_pfn)(
  IN ULONG                ObjectCount,
  IN PHANDLE              ObjectsArray,
  IN OBJECT_WAIT_TYPE     WaitType,
  IN BOOLEAN              Alertable,
  IN PLARGE_INTEGER       TimeOut OPTIONAL );

NtWaitForMultipleObjects_pfn
NtWaitForMultipleObjects_Original = nullptr;

NTSTATUS
NTAPI
NtWaitForMultipleObjects_Detour (
  IN ULONG                ObjectCount,
  IN PHANDLE              ObjectsArray,
  IN OBJECT_WAIT_TYPE     WaitType,
  IN BOOLEAN              Alertable,
  IN PLARGE_INTEGER       TimeOut OPTIONAL )
{
  // Unity spins a loop to signal its job semaphore
  if (TimeOut != nullptr && TimeOut->QuadPart == 0)
  {
    return
      NtWaitForMultipleObjects_Original (
           ObjectCount, ObjectsArray,
             WaitType, Alertable,
               TimeOut                  );
  }

  if (! ( SK_GetFramesDrawn () && SK_MMCS_GetTaskCount () > 1 ) )
  {
    return
      NtWaitForMultipleObjects_Original (
           ObjectCount, ObjectsArray,
             WaitType, Alertable,
               TimeOut                  );
  }

  SK_TLS *pTLS =
    SK_TLS_Bottom ();

  if ( pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *)1 )
  {
    //if (pTLS->scheduler->mmcs_task->hTask > 0)
    //    pTLS->scheduler->mmcs_task->disassociateWithTask ();
  }

  DWORD dwRet =
    NtWaitForMultipleObjects_Original (
         ObjectCount, ObjectsArray,
           WaitType, Alertable,
             TimeOut                  );

  if ( pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *)1 )
  {
    auto task =
      pTLS->scheduler->mmcs_task;

    //if (pTLS->scheduler->mmcs_task->hTask > 0)
    //  task->reassociateWithTask ();

    if (InterlockedCompareExchange (&task->change.pending, 0, 1))
    {
      task->setPriority            ( task->change.priority );

      ///if (_stricmp (task->change.task0, task->task0) ||
      ///    _stricmp (task->change.task1, task->task1))
      ///{
      ///  task->disassociateWithTask  ();
      ///  task->setMaxCharacteristics (task->change.task0, task->change.task1);
      ///}
    }
  }

  return dwRet;
}

// -------------------
// This code is largely obsolete, but will rate-limit
//   Unity's Asynchronous Procedure Calls if they are
//     ever shown to devestate performance like they
//       were in PoE2.
// -------------------
//

extern volatile LONG SK_POE2_Horses_Held;
extern volatile LONG SK_POE2_SMT_Assists;
extern volatile LONG SK_POE2_ThreadBoostsKilled;
extern          bool SK_POE2_FixUnityEmployment;
extern          bool SK_POE2_Stage2UnityFix;
extern          bool SK_POE2_Stage3UnityFix;

NTSTATUS
WINAPI
NtWaitForSingleObject_Detour (
  IN HANDLE         Handle,
  IN BOOLEAN        Alertable,
  IN PLARGE_INTEGER Timeout  )
{
  if (Timeout != nullptr && Timeout->QuadPart == 0)
  {
    return
      NtWaitForSingleObject_Original (
        Handle, Alertable, Timeout
      );
  }

#ifndef _UNITY_HACK
  if (! ( SK_GetFramesDrawn () && SK_MMCS_GetTaskCount () > 1 ) )
#else
  if (  ! SK_GetFramesDrawn () )
#endif
  {
    return
      NtWaitForSingleObject_Original (
        Handle, Alertable, Timeout
      );
  }

#ifdef _UNITY_HACK
  if (bAlertable)
    InterlockedIncrement (&pTLS->scheduler->alert_waits);

  // Consider double-buffering this since the information
  //   is used almost exclusively by OHTER threads, and
  //     we have to do a synchronous copy the get at this
  //       thing without thread A murdering thread B.
  SK_Sched_ThreadContext::wait_record_s& scheduled_wait =
    (*pTLS->scheduler->objects_waited) [hHandle];

  scheduled_wait.calls++;

  if (dwMilliseconds == INFINITE)
    scheduled_wait.time = 0;//+= dwMilliseconds;

  LARGE_INTEGER liStart =
    SK_QueryPerf ();

  bool same_as_last_time =
    ( pTLS->scheduler->mru_wait.handle == hHandle );

      pTLS->scheduler->mru_wait.handle = hHandle;

  auto ret =
    NtWaitForSingleObject_Original (
      Handle, Alertable, Timeout
    );

  InterlockedAdd ( &scheduled_wait.time_blocked,
                       static_cast <uint64_t> (
                         SK_DeltaPerfMS (liStart.QuadPart, 1)
                       )
                   );

  // We're waiting on the same event as last time on this thread
  if ( same_as_last_time )
  {
    pTLS->scheduler->mru_wait.last_wait = liStart;
    pTLS->scheduler->mru_wait.sequence++;
  }

  // This thread found actual work and has stopped abusing the kernel
  //   waiting on the same always-signaled event; it can have its
  //     normal preemption behavior back  (briefly anyway).
  else
  {
    pTLS->scheduler->mru_wait.start     = liStart;
    pTLS->scheduler->mru_wait.last_wait = liStart;
    pTLS->scheduler->mru_wait.sequence  = 0;
  }

  if ( ret            == WAIT_OBJECT_0 &&   SK_POE2_FixUnityEmployment &&
       dwMilliseconds == INFINITE      && ( bAlertable != FALSE ) )
  {
    // Not to be confused with the other thing
    bool hardly_working =
      (! StrCmpW (pTLS->debug.name, L"Worker Thread"));

    if ( SK_POE2_Stage3UnityFix || hardly_working )
    {
      if (pTLS->scheduler->mru_wait.getRate () >= 0.00666f)
      {
        // This turns preemption of threads in the same priority level off.
        //
        //    * Yes, TRUE means OFF.  Use this wrong and you will hurt
        //                              performance; just sayin'
        //
        if (pTLS->scheduler->mru_wait.preemptive == -1)
        {
          GetThreadPriorityBoost ( GetCurrentThread (),
                                   &pTLS->scheduler->mru_wait.preemptive );
        }

        if (pTLS->scheduler->mru_wait.preemptive == FALSE)
        {
          SetThreadPriorityBoost ( GetCurrentThread (), TRUE );
          InterlockedIncrement   (&SK_POE2_ThreadBoostsKilled);
        }

        //
        // (Everything below applies to the Unity unemployment office only)
        //
        if (hardly_working)
        {
          // Unity Worker Threads have special additional considerations to
          //   make them less of a pain in the ass for the kernel.
          //
          LARGE_INTEGER core_sleep_begin =
            SK_QueryPerf ();

          if (SK_DeltaPerfMS (liStart.QuadPart, 1) < 0.05)
          {
            InterlockedIncrement (&SK_POE2_Horses_Held);
            SwitchToThread       ();

            if (SK_POE2_Stage2UnityFix)
            {
              // Micro-sleep the core this thread is running on to try
              //   and salvage its logical (HyperThreaded) partner's
              //     ability to do work.
              //
              while (SK_DeltaPerfMS (core_sleep_begin.QuadPart, 1) < 0.0000005)
              {
                InterlockedIncrement (&SK_POE2_SMT_Assists);

                // Very brief pause that is good for next to nothing
                //   aside from voluntarily giving up execution resources
                //     on this core's superscalar pipe and hoping the
                //       related Logical Processor can work more
                //         productively if we get out of the way.
                //
                YieldProcessor       (                    );
                //
                // ^^^ Literally does nothing, but an even less useful
                //       nothing if the processor does not support SMT.
                //
              }
            }
          };
        }
      }

      else
      {
        if (pTLS->scheduler->mru_wait.preemptive == -1)
        {
          GetThreadPriorityBoost ( GetCurrentThread (),
                                  &pTLS->scheduler->mru_wait.preemptive );
        }

        if (pTLS->scheduler->mru_wait.preemptive != FALSE)
        {
          SetThreadPriorityBoost (GetCurrentThread (), FALSE);
          InterlockedIncrement   (&SK_POE2_ThreadBoostsKilled);
        }
      }
    }
  }

  // They took our jobs!
  else if (pTLS->scheduler->mru_wait.preemptive != -1)
  {
    SetThreadPriorityBoost (
      GetCurrentThread (),
        pTLS->scheduler->mru_wait.preemptive );

    // Status Quo restored: Jobs nobody wants are back and have
    //   zero future relevance and should be ignored if possible.
    pTLS->scheduler->mru_wait.preemptive = -1;
  }
#endif

  SK_TLS *pTLS =
    SK_TLS_Bottom ();

  if (! pTLS)
  {
    return
      NtWaitForSingleObject_Original (
        Handle, Alertable, Timeout
      );
  }

  //if (config.system.log_level > 0)
  //{
  //  dll_log.Log ( L"tid=%lu (\"%s\") WaitForSingleObject [Alertable: %lu] Timeout: %lli",
  //                  pTLS->debug.tid,
  //                    pTLS->debug.mapped ? pTLS->debug.name : L"Unnamed",
  //                      Alertable,
  //                        Timeout != nullptr ? Timeout->QuadPart : -1
  //              );
  //}
  //
  //if (Timeout != nullptr)
  //  Timeout = nullptr;

  if (pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *) 1)
  {
    //if (pTLS->scheduler->mmcs_task->hTask > 0)
    //    pTLS->scheduler->mmcs_task->disassociateWithTask ();
  }

  auto ret =
    Handle == nullptr ? STATUS_SUCCESS :
    NtWaitForSingleObject_Original (
      Handle, Alertable, Timeout
    );

  if (pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *) 1)
  {
    auto task =
      pTLS->scheduler->mmcs_task;

    //if (pTLS->scheduler->mmcs_task->hTask > 0)
    //                         task->reassociateWithTask ();

    if (InterlockedCompareExchange (&task->change.pending, 0, 1))
    {
      task->setPriority             ( task->change.priority );

      ///if (_stricmp (task->change.task0, task->task0) ||
      ///    _stricmp (task->change.task1, task->task1))
      ///{
      ///  task->disassociateWithTask  ();
      ///  task->setMaxCharacteristics (task->change.task0, task->change.task1);
      ///}
    }
  }

  return ret;
}


typedef BOOL (WINAPI *SwitchToThread_pfn)(void);
                      SwitchToThread_pfn
                      SwitchToThread_Original = nullptr;

#include <avrt.h>

BOOL
WINAPI
SwitchToThread_Detour (void)
{
#ifdef _M_AMD64
  static bool is_mhw =
    ( SK_GetCurrentGameID () == SK_GAME_ID::MonsterHunterWorld     );
  static bool is_aco =
    ( SK_GetCurrentGameID () == SK_GAME_ID::AssassinsCreed_Odyssey );

  if (! (is_mhw || is_aco))
  {
#endif
    return
      SwitchToThread_Original ();
#ifdef _M_AMD64
  }


  static volatile DWORD dwAntiDebugTid = 0;

  extern bool __SK_MHW_KillAntiDebug;

  if (is_mhw && __SK_MHW_KillAntiDebug)
  {
    DWORD dwTid =
      ReadULongAcquire (&dwAntiDebugTid);

    if ( dwTid == 0 ||
         dwTid == SK_Thread_GetCurrentId () )
    {
      SK_TLS *pTLS =
        SK_TLS_Bottom ();

      if ( pTLS->win32->getThreadPriority () !=
             THREAD_PRIORITY_HIGHEST )
      {
        return
          SwitchToThread_Original ();
      }

      else if (dwTid == 0)
      {
        dwTid =
          SK_Thread_GetCurrentId ();

        InterlockedExchange (&dwAntiDebugTid, dwTid);
      }

      if (pTLS->debug.tid == dwTid)
      {
        ULONG ulFrames =
          SK_GetFramesDrawn ();

        if (pTLS->scheduler->last_frame   < (ulFrames - 3) ||
            pTLS->scheduler->switch_count > 3)
        {
          pTLS->scheduler->switch_count = 0;
        }

        if (WAIT_TIMEOUT != MsgWaitForMultipleObjectsEx (0, nullptr, pTLS->scheduler->switch_count++, QS_ALLEVENTS & ~QS_INPUT, 0x0))
        {
          pTLS->scheduler->last_frame =
            ulFrames;

          return FALSE;
        }

      //SK_Sleep (pTLS->scheduler->switch_count++);

        pTLS->scheduler->last_frame =
          ulFrames;

        return
          TRUE;
      }
    }

    return
      SwitchToThread_Original ();
  }


  //SK_LOG0 ( ( L"Thread: %x (%s) is SwitchingThreads", SK_GetCurrentThreadId (), SK_Thread_GetName (GetCurrentThread ()).c_str () ),
  //         L"AntiTamper");


  BOOL bRet = FALSE;

  if (__SK_MHW_KillAntiDebug && is_aco)
  {
    config.render.framerate.enable_mmcss = true;

    SK_TLS *pTLS =
      SK_TLS_Bottom ();

    if (pTLS->scheduler->last_frame != SK_GetFramesDrawn ())
        pTLS->scheduler->switch_count = 0;

    bRet = TRUE;

    if (pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *) 1)
    {
      //if (pTLS->scheduler->mmcs_task->hTask > 0)
      //    pTLS->scheduler->mmcs_task->disassociateWithTask ();
    }

    if (pTLS->scheduler->switch_count++ < 20)
    {
      if (pTLS->scheduler->switch_count < 15)
        SK_Sleep (0);
      else
        bRet = SwitchToThread_Original ();
    }
    else
      SK_Sleep (1);

    if (pTLS->scheduler->mmcs_task > (SK_MMCS_TaskEntry *)1)
    {
      auto task =
        pTLS->scheduler->mmcs_task;

      //if (pTLS->scheduler->mmcs_task->hTask > 0)
      //                         task->reassociateWithTask ();

      if (InterlockedCompareExchange (&task->change.pending, 0, 1))
      {
        task->setPriority            ( task->change.priority );

        ///if (_stricmp (task->change.task0, task->task0) ||
        ///    _stricmp (task->change.task1, task->task1))
        ///{
        ///  task->disassociateWithTask  ();
        ///  task->setMaxCharacteristics (task->change.task0, task->change.task1);
        ///}
      }
    }

    pTLS->scheduler->last_frame =
      SK_GetFramesDrawn ();
  }

  else
  {
    bRet =
      SwitchToThread_Original ();
  }

  return
    bRet;
#endif
}


volatile LONG __sleep_init = 0;

DWORD
WINAPI
SK_SleepEx (DWORD dwMilliseconds, BOOL bAlertable)
{
  if (! ReadAcquire (&__sleep_init))
    return SleepEx (dwMilliseconds, bAlertable);

  return
    SleepEx_Original != nullptr       ?
      SleepEx_Original (dwMilliseconds, bAlertable) :
      SleepEx          (dwMilliseconds, bAlertable);
}

void
WINAPI
SK_Sleep (DWORD dwMilliseconds)
{
  SK_SleepEx (dwMilliseconds, FALSE);
}


DWORD
WINAPI
SleepEx_Detour (DWORD dwMilliseconds, BOOL bAlertable)
{
  if (   ReadAcquire (&__SK_DLL_Ending  ) ||
      (! ReadAcquire (&__SK_DLL_Attached) ) )
  {
    return 0;
  }

  static auto& rb =
    SK_GetCurrentRenderBackend ();

  static auto game_id =
    SK_GetCurrentGameID ();

  if ( game_id        == SK_GAME_ID::FinalFantasyXV &&
       dwMilliseconds == 0                          && fix_sleep_0 )
  {
    SwitchToThread ();
    return 0;
  }

  const bool sleepless_render = config.render.framerate.sleepless_render;
  const bool sleepless_window = config.render.framerate.sleepless_window;

  bool bWantThreadClassification =
    ( sleepless_render ||
      sleepless_window );

  bWantThreadClassification |=
    ( game_id == SK_GAME_ID::Tales_of_Vesperia &&
            1 == dwMilliseconds );

  DWORD dwTid =
    bWantThreadClassification ?
        SK_Thread_GetCurrentId () : 0;

  SK_TLS *pTLS =
    nullptr;

  BOOL bGUIThread    = sleepless_window ?     SK_Win32_IsGUIThread (dwTid, &pTLS)         :
                                                                                   false;
  BOOL bRenderThread = sleepless_render ? ((DWORD)ReadULongAcquire (&rb.thread) == dwTid) :
                                                                                   false;

  if (bRenderThread)
  {
#ifdef _M_AMD64
    if (game_id == SK_GAME_ID::Tales_of_Vesperia)
    {
      extern bool SK_TVFix_NoRenderSleep (void);

      if (SK_TVFix_NoRenderSleep ())
      {
        YieldProcessor ( );
        return 0;
      }
    }
#endif

    if (sleepless_render && dwMilliseconds != INFINITE)
    {
      static bool reported = false;
            if (! reported)
            {
              dll_log->Log (L"[FrameLimit] Sleep called from render thread: %lu ms!", dwMilliseconds);
              reported = true;
            }

      SK::Framerate::events->getRenderThreadStats ().wake (dwMilliseconds);

      //if (bGUIThread)
      //  SK::Framerate::events->getMessagePumpStats ().wake (dwMilliseconds);

      if (dwMilliseconds <= 1)
      {
        return
          SleepEx_Original (0, bAlertable);
      }

      return 0;
    }

    SK::Framerate::events->getRenderThreadStats ().sleep  (dwMilliseconds);
  }

  if (bGUIThread)
  {
    if (sleepless_window && dwMilliseconds != INFINITE)
    {
      static bool reported = false;
            if (! reported)
            {
              dll_log->Log (L"[FrameLimit] Sleep called from GUI thread: %lu ms!", dwMilliseconds);
              reported = true;
            }

      SK::Framerate::events->getMessagePumpStats ().wake   (dwMilliseconds);

      if (bRenderThread)
        SK::Framerate::events->getMessagePumpStats ().wake (dwMilliseconds);

      SK_Thread_WaitWhilePumpingMessages (dwMilliseconds);

      return 0;
    }

    SK::Framerate::events->getMessagePumpStats ().sleep (dwMilliseconds);
  }

  DWORD max_delta_time =
    narrow_cast <DWORD> (config.render.framerate.max_delta_time);

#ifdef _M_AMD64
  extern bool SK_TVFix_ActiveAntiStutter (void);

  if ( game_id == SK_GAME_ID::Tales_of_Vesperia &&
                   SK_TVFix_ActiveAntiStutter () )
  {
    if (dwTid == 0)
        dwTid = SK_Thread_GetCurrentId ();

    static DWORD   dwTidBusy = 0;
    static DWORD   dwTidWork = 0;
    static SK_TLS *pTLSBusy  = nullptr;
    static SK_TLS *pTLSWork  = nullptr;

    if (dwTid != 0 && ( dwTidBusy == 0 || dwTidWork == 0 ))
    {
      if (pTLS == nullptr)
          pTLS = SK_TLS_Bottom ();

           if (! _wcsicmp (pTLS->debug.name, L"BusyThread")) { dwTidBusy = dwTid; pTLSBusy = pTLS; }
      else if (! _wcsicmp (pTLS->debug.name, L"WorkThread")) { dwTidWork = dwTid; pTLSWork = pTLS; }
    }

    if ( dwTidBusy == dwTid || dwTidWork == dwTid )
    {
      pTLS = ( dwTidBusy == dwTid ? pTLSBusy :
                                    pTLSWork );

      ULONG ulFrames =
        SK_GetFramesDrawn ();

      if (pTLS->scheduler->last_frame < ulFrames)
      {
        pTLS->scheduler->switch_count = 0;
      }

      pTLS->scheduler->last_frame =
        ulFrames;

      YieldProcessor ();

      return 0;
    }

    else if ( dwMilliseconds == 0 ||
              dwMilliseconds == 1 )
    {
      max_delta_time = std::max (2UL, max_delta_time);
      //dll_log.Log (L"Sleep %lu - %s - %x", dwMilliseconds, thread_name.c_str (), SK_Thread_GetCurrentId ());
    }
  }
#endif

  // TODO: Stop this nonsense and make an actual parameter for this...
  //         (min sleep?)
  if ( max_delta_time <= dwMilliseconds )
  {
    //dll_log.Log (L"SleepEx (%lu, %s) -- %s", dwMilliseconds, bAlertable ? L"Alertable" : L"Non-Alertable", SK_SummarizeCaller ().c_str ());
    return
      SK_SleepEx (dwMilliseconds, bAlertable);
  }

  else
  {
    static volatile LONG __init = 0;

    long double dMinRes = 0.498800;

    if (! InterlockedCompareExchange (&__init, 1, 0))
    {
      NtDll =
        LoadLibrary (L"ntdll.dll");

      NtQueryTimerResolution =
        reinterpret_cast <NtQueryTimerResolution_pfn> (
          GetProcAddress (NtDll, "NtQueryTimerResolution")
        );

      NtSetTimerResolution =
        reinterpret_cast <NtSetTimerResolution_pfn> (
          GetProcAddress (NtDll, "NtSetTimerResolution")
        );

      if (NtQueryTimerResolution != nullptr &&
          NtSetTimerResolution   != nullptr)
      {
        ULONG min, max, cur;
        NtQueryTimerResolution (&min, &max, &cur);
        dll_log->Log ( L"[  Timing  ] Kernel resolution.: %f ms",
                            long_double_cast (cur * 100)/1000000.0F );
        NtSetTimerResolution   (max, TRUE,  &cur);
        dll_log->Log ( L"[  Timing  ] New resolution....: %f ms",
                            long_double_cast (cur * 100)/1000000.0L );

        dMinRes =
                            long_double_cast (cur * 100)/1000000.0L;
      }
    }
  }

  return 0;
}

void
WINAPI
Sleep_Detour (DWORD dwMilliseconds)
{
  SleepEx_Detour (dwMilliseconds, FALSE);
}

#ifdef _M_AMD64
float __SK_SHENMUE_ClockFuzz = 20.0f;
extern volatile LONG SK_BypassResult;
#endif

BOOL
WINAPI
QueryPerformanceFrequency_Detour (_Out_ LARGE_INTEGER *lpPerfFreq)
{
  if (lpPerfFreq)
  {
    *lpPerfFreq =
      SK_GetPerfFreq ();

    return TRUE;
  }

  return
    FALSE;
}

BOOL
WINAPI
SK_QueryPerformanceCounter (_Out_ LARGE_INTEGER *lpPerformanceCount)
{
  if (RtlQueryPerformanceCounter != nullptr)
    return RtlQueryPerformanceCounter (lpPerformanceCount);

  else if (QueryPerformanceCounter_Original != nullptr)
    return QueryPerformanceCounter_Original (lpPerformanceCount);

  else
    return QueryPerformanceCounter (lpPerformanceCount);
}

#ifdef _M_AMD64
extern bool SK_Shenmue_IsLimiterBypassed   (void              );
extern bool SK_Shenmue_InitLimiterOverride (LPVOID pQPCRetAddr);
extern bool SK_Shenmue_UseNtDllQPC;
#endif

BOOL
WINAPI
QueryPerformanceCounter_Detour (_Out_ LARGE_INTEGER *lpPerformanceCount)
{
#ifdef _M_AMD64
  struct SK_ShenmueLimitBreaker
  {
    bool detected = false;
    bool pacified = false;
  } static
      shenmue_clock {
        SK_GetCurrentGameID () == SK_GAME_ID::Shenmue,
      //SK_GetCurrentGameID () == SK_GAME_ID::DragonQuestXI,
        false
      };


  if ( lpPerformanceCount != nullptr   &&
       shenmue_clock.detected          &&
       shenmue_clock.pacified == false &&
    (! SK_Shenmue_IsLimiterBypassed () ) )
  {
    extern volatile LONG
      __SK_SHENMUE_FinishedButNotPresented;

    if (ReadAcquire (&__SK_SHENMUE_FinishedButNotPresented))
    {
      if ( SK_Thread_GetCurrentId () == SK_GetRenderThreadID () )
      {
        if ( SK_GetCallingDLL () == SK_Modules->HostApp () )
        {
          static std::unordered_set <LPCVOID> ret_addrs;

          if (ret_addrs.emplace (_ReturnAddress ()).second)
          {
            SK_LOG1 ( ( L"New QueryPerformanceCounter Return Addr: %ph -- %s",
                        _ReturnAddress (), SK_SummarizeCaller ().c_str () ),
                        L"ShenmueDbg" );

            // The instructions we're looking for are jl ...
            //
            //   Look-ahead up to about 12-bytes and if not found,
            //     this isn't the primary limiter code.
            //
            if ( reinterpret_cast <uintptr_t> (
                   SK_ScanAlignedEx (
                     "\x7C\xEE",          2,
                     nullptr,     (void *)_ReturnAddress ()
                   )
                 ) < (uintptr_t)_ReturnAddress () + 12 )
            {
              shenmue_clock.pacified =
                SK_Shenmue_InitLimiterOverride (_ReturnAddress ());

              SK_LOG0 ( (L"Shenmue Framerate Limiter Located and Pacified"),
                         L"ShenmueDbg" );
            }
          }

          InterlockedExchange (&__SK_SHENMUE_FinishedButNotPresented, 0);

          BOOL bRet =
            RtlQueryPerformanceCounter ?
            RtlQueryPerformanceCounter          (lpPerformanceCount) :
               QueryPerformanceCounter_Original ?
               QueryPerformanceCounter_Original (lpPerformanceCount) :
               QueryPerformanceCounter          (lpPerformanceCount);

          static LARGE_INTEGER last_poll {
            lpPerformanceCount->u.LowPart,
            lpPerformanceCount->u.HighPart
          };

          LARGE_INTEGER pre_fuzz {
            lpPerformanceCount->u.LowPart,
            lpPerformanceCount->u.HighPart
          };

          lpPerformanceCount->QuadPart +=
            static_cast <LONGLONG> (
                                     ( long_double_cast (lpPerformanceCount->QuadPart) -
                                       long_double_cast (last_poll.QuadPart)           *
                                       long_double_cast (__SK_SHENMUE_ClockFuzz)       )
                                   );

          last_poll.QuadPart =
           pre_fuzz.QuadPart;

          return bRet;
        }
      }
    }
  }
#endif

  return
    RtlQueryPerformanceCounter          ?
    RtlQueryPerformanceCounter          (lpPerformanceCount) :
       QueryPerformanceCounter_Original ?
       QueryPerformanceCounter_Original (lpPerformanceCount) :
       QueryPerformanceCounter          (lpPerformanceCount);
}

float __target_fps    = 0.0;
float __target_fps_bg = 0.0;
float fHPETWeight  = 0.875f;

void
SK::Framerate::Init (void)
{
  static SK::Framerate::Stats        _frame_history;
  static SK::Framerate::Stats        _frame_history2;
  static SK::Framerate::EventCounter _events;

  static bool basic_init = false;

  if (! basic_init)
  {
    basic_init = true;

    frame_history         = &_frame_history;
    frame_history2        = &_frame_history2;
    SK::Framerate::events = &_events;

    SK_ICommandProcessor* pCommandProc =
      SK_GetCommandProcessor ();


    if (! pCommandProc) // TODO: Error message
      return;


    // TEMP HACK BECAUSE THIS ISN'T STORED in D3D9.INI
    if (GetModuleHandle (L"AgDrag.dll"))
      config.render.framerate.max_delta_time = 5;

    if (GetModuleHandle (L"tsfix.dll"))
      config.render.framerate.max_delta_time = 0;

    pCommandProc->AddVariable ( "WaitForVBLANK",
            new SK_IVarStub <bool> (&config.render.framerate.wait_for_vblank));
    pCommandProc->AddVariable ( "MaxDeltaTime",
            new SK_IVarStub <int> (&config.render.framerate.max_delta_time));

    pCommandProc->AddVariable ( "LimiterTolerance",
            new SK_IVarStub <float> (&config.render.framerate.limiter_tolerance));
    pCommandProc->AddVariable ( "TargetFPS",
            new SK_IVarStub <float> (&__target_fps));
    pCommandProc->AddVariable ( "BackgroundFPS",
            new SK_IVarStub <float> (&__target_fps_bg));

    pCommandProc->AddVariable ( "MaxRenderAhead",
            new SK_IVarStub <int> (&config.render.framerate.max_render_ahead));

    pCommandProc->AddVariable ( "FPS.HPETWeight",
            new SK_IVarStub <float> (&fHPETWeight));

    RtlQueryPerformanceFrequency =
      (QueryPerformanceCounter_pfn)
      SK_GetProcAddress ( L"NtDll",
                           "RtlQueryPerformanceFrequency" );

    RtlQueryPerformanceCounter =
    (QueryPerformanceCounter_pfn)
      SK_GetProcAddress ( L"NtDll",
                           "RtlQueryPerformanceCounter" );

    ZwQueryPerformanceCounter =
      (QueryPerformanceCounter_pfn)
      SK_GetProcAddress ( L"NtDll",
                           "ZwQueryPerformanceCounter" );

    NtDelayExecution =
      (NtDelayExecution_pfn)
      SK_GetProcAddress ( L"NtDll",
                           "NtDelayExecution" );

//#define NO_HOOK_QPC
#ifndef NO_HOOK_QPC
  SK_GetPerfFreq ();
  SK_CreateDLLHook2 (      L"kernel32",
                            "QueryPerformanceFrequency",
                             QueryPerformanceFrequency_Detour,
    static_cast_p2p <void> (&QueryPerformanceFrequency_Original) );

  SK_CreateDLLHook2 (      L"kernel32",
                            "QueryPerformanceCounter",
                             QueryPerformanceCounter_Detour,
    static_cast_p2p <void> (&QueryPerformanceCounter_Original),
    static_cast_p2p <void> (&pfnQueryPerformanceCounter) );
#endif

    SK_CreateDLLHook2 (      L"kernel32",
                              "Sleep",
                               Sleep_Detour,
      static_cast_p2p <void> (&Sleep_Original),
      static_cast_p2p <void> (&pfnSleep) );

    SK_CreateDLLHook2 (      L"KernelBase.dll",
                              "SleepEx",
                               SleepEx_Detour,
      static_cast_p2p <void> (&SleepEx_Original) );

        SK_CreateDLLHook2 (  L"KernelBase.dll",
                              "SwitchToThread",
                               SwitchToThread_Detour,
      static_cast_p2p <void> (&SwitchToThread_Original) );

    SK_CreateDLLHook2 (      L"NtDll",
                              "NtWaitForSingleObject",
                               NtWaitForSingleObject_Detour,
      static_cast_p2p <void> (&NtWaitForSingleObject_Original) );

    SK_CreateDLLHook2 (      L"NtDll",
                              "NtWaitForMultipleObjects",
                               NtWaitForMultipleObjects_Detour,
      static_cast_p2p <void> (&NtWaitForMultipleObjects_Original) );

  //SK_ApplyQueuedHooks ();
    InterlockedExchange (&__sleep_init, 1);

#ifdef NO_HOOK_QPC
      QueryPerformanceCounter_Original =
        reinterpret_cast <QueryPerformanceCounter_pfn> (
          GetProcAddress ( SK_GetModuleHandle (L"kernel32"),
                             "QueryPerformanceCounter" )
        );
#endif

    if (! config.render.framerate.enable_mmcss)
    {
      if (NtDll == nullptr)
      {
        NtDll =
          LoadLibrary (L"ntdll.dll");

        NtQueryTimerResolution =
          reinterpret_cast <NtQueryTimerResolution_pfn> (
            GetProcAddress (NtDll, "NtQueryTimerResolution")
          );

        NtSetTimerResolution =
          reinterpret_cast <NtSetTimerResolution_pfn> (
            GetProcAddress (NtDll, "NtSetTimerResolution")
          );

        if (NtQueryTimerResolution != nullptr &&
            NtSetTimerResolution   != nullptr)
        {
          ULONG min, max, cur;
          NtQueryTimerResolution (&min, &max, &cur);
          dll_log->Log ( L"[  Timing  ] Kernel resolution.: %f ms",
                           static_cast <float> (cur * 100)/1000000.0f );
          NtSetTimerResolution   (max, TRUE,  &cur);
          dll_log->Log ( L"[  Timing  ] New resolution....: %f ms",
                           static_cast <float> (cur * 100)/1000000.0f );
        }
      }
    }
  }
}

void
SK::Framerate::Shutdown (void)
{
  if (NtDll != nullptr)
    FreeLibrary (NtDll);

  SK_DisableHook (pfnSleep);
  //SK_DisableHook (pfnQueryPerformanceCounter);
}

SK::Framerate::Limiter::Limiter (long double target)
{
  effective_ms = 0.0;

  init (target);
}


IDirect3DDevice9Ex*
SK_D3D9_GetTimingDevice (void)
{
  static auto* pTimingDevice =
    reinterpret_cast <IDirect3DDevice9Ex *> (-1);

  if (pTimingDevice == reinterpret_cast <IDirect3DDevice9Ex *> (-1))
  {
    CComPtr <IDirect3D9Ex> pD3D9Ex = nullptr;

    using Direct3DCreate9ExPROC = HRESULT (STDMETHODCALLTYPE *)(UINT           SDKVersion,
                                                                IDirect3D9Ex** d3d9ex);

    extern Direct3DCreate9ExPROC Direct3DCreate9Ex_Import;

    // For OpenGL, bootstrap D3D9
    SK_BootD3D9 ();

    HRESULT hr = (config.apis.d3d9ex.hook) ?
      Direct3DCreate9Ex_Import (D3D_SDK_VERSION, &pD3D9Ex)
                                    :
                               E_NOINTERFACE;

    HWND hwnd = nullptr;

    IDirect3DDevice9Ex* pDev9Ex = nullptr;

    if (SUCCEEDED (hr))
    {
      hwnd =
        SK_Win32_CreateDummyWindow ();

      D3DPRESENT_PARAMETERS pparams = { };

      pparams.SwapEffect       = D3DSWAPEFFECT_FLIPEX;
      pparams.BackBufferFormat = D3DFMT_UNKNOWN;
      pparams.hDeviceWindow    = hwnd;
      pparams.Windowed         = TRUE;
      pparams.BackBufferCount  = 2;
      pparams.BackBufferHeight = 2;
      pparams.BackBufferWidth  = 2;

      if ( FAILED ( pD3D9Ex->CreateDeviceEx (
                      D3DADAPTER_DEFAULT,
                        D3DDEVTYPE_HAL,
                          hwnd,
                            D3DCREATE_HARDWARE_VERTEXPROCESSING,
                              &pparams,
                                nullptr,
                                  &pDev9Ex )
                  )
          )
      {
        pTimingDevice = nullptr;
      } else {
        pDev9Ex->AddRef ();
        pTimingDevice = pDev9Ex;
      }
    }
    else {
      pTimingDevice = nullptr;
    }
  }

  return pTimingDevice;
}


void
SK::Framerate::Limiter::init (long double target)
{
  QueryPerformanceFrequency (&freq);

  ms  = 1000.0L / long_double_cast (target);
  fps =           long_double_cast (target);

  ticks_per_frame =
    static_cast <ULONGLONG> (
      ( ms / 1000.0L ) * long_double_cast ( freq.QuadPart )
    );

  frames = 0;

  CComPtr <IDirect3DDevice9Ex> d3d9ex      = nullptr;
  CComPtr <IDXGISwapChain>     dxgi_swap   = nullptr;
  CComPtr <IDXGIOutput>        dxgi_output = nullptr;

  static auto& rb =
   SK_GetCurrentRenderBackend ();

  SK_RenderAPI api = rb.api;

  if (                    api ==                    SK_RenderAPI::D3D10  ||
       static_cast <int> (api) & static_cast <int> (SK_RenderAPI::D3D11) ||
       static_cast <int> (api) & static_cast <int> (SK_RenderAPI::D3D12) )
  {
    if (rb.swapchain != nullptr)
    {
      HRESULT hr =
        rb.swapchain->QueryInterface <IDXGISwapChain> (&dxgi_swap);

      if (SUCCEEDED (hr))
      {
        if (SUCCEEDED (dxgi_swap->GetContainingOutput (&dxgi_output)))
        {
          //WaitForVBlank_Original (dxgi_output);
          dxgi_output->WaitForVBlank ();
        }
      }
    }
  }

  else if (static_cast <int> (api) & static_cast <int> (SK_RenderAPI::D3D9))
  {
    if (rb.device != nullptr)
    {
      rb.device->QueryInterface ( IID_PPV_ARGS (&d3d9ex) );

      // Align the start to VBlank for minimum input latency
      if (d3d9ex != nullptr || (d3d9ex = SK_D3D9_GetTimingDevice ()))
      {
        UINT orig_latency = 3;
        d3d9ex->GetMaximumFrameLatency (&orig_latency);

        d3d9ex->SetMaximumFrameLatency (1);
        d3d9ex->WaitForVBlank          (0);
        d3d9ex->SetMaximumFrameLatency (
          config.render.framerate.pre_render_limit == -1 ?
               orig_latency : config.render.framerate.pre_render_limit );
      }
    }
  }

  SK_QueryPerformanceCounter (&start);

  time.QuadPart = 0ULL;
  last.QuadPart = static_cast <LONGLONG> (start.QuadPart - (ms / 1000.0L) * freq.QuadPart);
  next.QuadPart = static_cast <LONGLONG> (start.QuadPart + (ms / 1000.0L) * freq.QuadPart);
}

#include <SpecialK/window.h>

bool
SK::Framerate::Limiter::try_wait (void)
{
  if (limit_behavior != LIMIT_APPLY)
    return false;

  if (game_window.active || __target_fps_bg == 0.0f)
  {
    if (__target_fps <= 0.0f)
      return false;
  }

  LARGE_INTEGER next_;
  next_.QuadPart =
    start.QuadPart + frames * ticks_per_frame;

  SK_QueryPerformanceCounter (&time);

  return
    (time.QuadPart < next_.QuadPart);
}

void
SK::Framerate::Limiter::wait (void)
{
  //SK_Win32_AssistStalledMessagePump (100);

  if (limit_behavior != LIMIT_APPLY)
    return;

  if (background == game_window.active)
  {
    background = (! background);
  }

  // Don't limit under certain circumstances or exiting / alt+tabbing takes
  //   longer than it should.
  if (ReadAcquire (&__SK_DLL_Ending))
    return;

  if (! background)
  {
    if (fps != __target_fps)
         init (__target_fps);
  }

  else
  {
    if (__target_fps_bg > 0.0f)
    {
      if (fps != __target_fps_bg)
           init (__target_fps_bg);
    }

    else
    {
      if (fps != __target_fps)
           init (__target_fps);
    }
  }

  if (__target_fps <= 0.0f)
    return;

  frames++;

  SK_QueryPerformanceCounter (&time);

  // Actual frametime before we forced a delay
  effective_ms =
    1000.0L * ( long_double_cast (time.QuadPart - last.QuadPart) /
                long_double_cast (freq.QuadPart)                 );

  next.QuadPart =
    start.QuadPart + frames * ticks_per_frame;

  long double missed_frames,
              missing_time =
    long_double_cast ( time.QuadPart - next.QuadPart ) /
    long_double_cast ( ticks_per_frame ),
              edge_distance =
      modfl ( missing_time, &missed_frames );

  static DWORD dwLastFullReset        = timeGetTime ();
   const DWORD dwMinTimeBetweenResets = 333L;

  if (missing_time > config.render.framerate.limiter_tolerance)
  {
    if (edge_distance > 0.333L && edge_distance < 0.666L)
    {
      if (timeGetTime () - dwMinTimeBetweenResets > dwLastFullReset)
      {
        SK_LOG1 ( ( L"Framerate limiter is running too far behind... (%f frames late)",
                      missed_frames ),
                    L"Frame Rate" );

        if (missing_time > 3 * config.render.framerate.limiter_tolerance)
          full_restart = true;

        restart         = true;
        dwLastFullReset = timeGetTime ();
      }
    }
  }

  if (restart || full_restart)
  {
    if (full_restart)
    {
      init (__target_fps);
      full_restart = false;
    }

    restart        = false;
    frames         = 0;
    start.QuadPart = SK_QueryPerf ().QuadPart - ticks_per_frame;
     time.QuadPart = start.QuadPart           + ticks_per_frame;
     next.QuadPart =  time.QuadPart;
  }



  auto
    SK_RecalcTimeToNextFrame =
    [&](void)->
      long double
      {
        long double ldRet =
          ( long_double_cast ( next.QuadPart -
                               SK_QueryPerf ().QuadPart ) /
            long_double_cast ( freq.QuadPart            ) );

        if (ldRet < 0.0L)
          ldRet = 0.0L;

        return ldRet;
      };

  if (next.QuadPart > 0ULL)
  {
    long double
      to_next_in_secs =
        SK_RecalcTimeToNextFrame ();

    LARGE_INTEGER liDelay;
                  liDelay.QuadPart =
                    static_cast <LONGLONG> (
                      to_next_in_secs * 1000.0L * (long double)fHPETWeight
                    );

    //dll_log.Log (L"Wait MS: %f", to_next_in_secs * 1000.0 );

    // Create an unnamed waitable timer.
    static HANDLE hLimitTimer =
      CreateWaitableTimer (nullptr, FALSE, nullptr);

    if ( hLimitTimer != 0 && liDelay.QuadPart > 0)
    {
      liDelay.QuadPart = -(liDelay.QuadPart * 10000);

      if ( SetWaitableTimer ( hLimitTimer, &liDelay,
                                0, nullptr, nullptr, TRUE ) )
      {
        DWORD dwWait = 1337;

        while (dwWait != WAIT_OBJECT_0)
        {
          to_next_in_secs =
            SK_RecalcTimeToNextFrame ();

          if (to_next_in_secs <= 0.0L)
          {
            break;
          }

          LARGE_INTEGER uSecs;
                        uSecs.QuadPart =
            static_cast <LONGLONG> (to_next_in_secs * 1000.0L * 1000.0L);

          dwWait =
            SK_WaitForSingleObject ( hLimitTimer,
                  static_cast <DWORD> (to_next_in_secs * 1000.0) );
                   YieldProcessor (  );
        }
      }
    }

    static SK_RenderBackend& rb =
      SK_GetCurrentRenderBackend ();

    // If available (Windows 8+), wait on the swapchain
    SK_ComPtr <IDirect3DDevice9Ex>  d3d9ex = nullptr;

    // D3D10/11/12
    SK_ComPtr <IDXGISwapChain> dxgi_swap   = nullptr;
    SK_ComPtr <IDXGIOutput>    dxgi_output = nullptr;

    if (config.render.framerate.wait_for_vblank)
    {
      SK_RenderAPI api = rb.api;

      if (                    api ==                    SK_RenderAPI::D3D10  ||
           static_cast <int> (api) & static_cast <int> (SK_RenderAPI::D3D11) ||
           static_cast <int> (api) & static_cast <int> (SK_RenderAPI::D3D12) )
      {
        if (rb.swapchain != nullptr)
        {
          HRESULT hr =
            rb.swapchain->QueryInterface <IDXGISwapChain> (&dxgi_swap);

          if (SUCCEEDED (hr))
          {
            dxgi_swap->GetContainingOutput (&dxgi_output);
          }
        }
      }

      else if ( static_cast <int> (api)       &
                static_cast <int> (SK_RenderAPI::D3D9) )
      {
        if (rb.device != nullptr)
        {
          if (FAILED (rb.device->QueryInterface <IDirect3DDevice9Ex> (&d3d9ex)))
          {
            d3d9ex =
              SK_D3D9_GetTimingDevice ();
          }
        }
      }
    }

    while (time.QuadPart <= next.QuadPart)
    {
      DWORD dwWaitMS =
        static_cast <DWORD> (
          std::max (0.0L, SK_RecalcTimeToNextFrame () * 1000.0L - 1.0L)
        );

      // Attempt to use a deeper sleep when possible instead of hammering the
      //   CPU into submission ;)
      if (dwWaitMS > 2)
      {
        YieldProcessor ();

        if (! config.render.framerate.enable_mmcss)
        {
          if (dwWaitMS > 4)
            SK_SleepEx (1, FALSE);
        }
      }

      if ( config.render.framerate.wait_for_vblank )
      {
        if (d3d9ex != nullptr)
          d3d9ex->WaitForVBlank (0);

        else if (dxgi_output != nullptr)
          dxgi_output->WaitForVBlank ();
      }

      SK_QueryPerformanceCounter (&time);
    }
  }

  else
  {
    dll_log->Log (L"[FrameLimit] Framerate limiter lost time?! (non-monotonic clock)");
    start.QuadPart += -next.QuadPart;
  }

  last.QuadPart = time.QuadPart;
}

void
SK::Framerate::Limiter::set_limit (long double target)
{
  init (target);
}

long double
SK::Framerate::Limiter::effective_frametime (void)
{
  return effective_ms;
}


SK::Framerate::Limiter*
SK::Framerate::GetLimiter (void)
{
  static          std::unique_ptr <Limiter> limiter = nullptr;
  static volatile LONG                      init    = 0;

  if (! InterlockedCompareExchangeAcquire (&init, 1, 0))
  {
    SK_GetCommandProcessor ()->AddCommand (
      "SK::Framerate::ResetLimit", new skLimitResetCmd ());

    limiter =
      std::make_unique <Limiter> (config.render.framerate.target_fps);

    SK_ReleaseAssert (limiter != nullptr)

    if (limiter != nullptr)
      InterlockedIncrementRelease (&init);
    else
      InterlockedDecrementRelease (&init);
  }

  else
    SK_Thread_SpinUntilAtomicMin (&init, 2);

  return
    limiter.get ();
}

void
SK::Framerate::Tick (long double& dt, LARGE_INTEGER& now)
{
  if ( frame_history  == nullptr ||
       frame_history2 == nullptr )
  {
    // Late initialization
    Init ();
  }

  static LARGE_INTEGER last_frame = { };

  now = SK_CurrentPerf ();

  dt =
    long_double_cast (now.QuadPart -  last_frame.QuadPart) /
    long_double_cast (SK::Framerate::Stats::freq.QuadPart);


  // What the bloody hell?! How do we ever get a dt value near 0?
  if (dt > 0.000001L)
    frame_history->addSample (1000.0L * dt, now);
  else // Less than single-precision FP epsilon, toss this frame out
    frame_history->addSample (INFINITY, now);



  frame_history2->addSample (
    SK::Framerate::GetLimiter ()->effective_frametime (),
      now
  );


  last_frame = now;
};


long double
SK::Framerate::Stats::calcMean (long double seconds)
{
  return
    calcMean (SK_DeltaPerf (seconds, freq.QuadPart));
}

long double
SK::Framerate::Stats::calcSqStdDev (long double mean, long double seconds)
{
  return
    calcSqStdDev (mean, SK_DeltaPerf (seconds, freq.QuadPart));
}

long double
SK::Framerate::Stats::calcMin (long double seconds)
{
  return
    calcMin (SK_DeltaPerf (seconds, freq.QuadPart));
}

long double
SK::Framerate::Stats::calcMax (long double seconds)
{
  return
    calcMax (SK_DeltaPerf (seconds, freq.QuadPart));
}

int
SK::Framerate::Stats::calcHitches (long double tolerance, long double mean, long double seconds)
{
  return
    calcHitches (tolerance, mean, SK_DeltaPerf (seconds, freq.QuadPart));
}

int
SK::Framerate::Stats::calcNumSamples (long double seconds)
{
  return
    calcNumSamples (SK_DeltaPerf (seconds, freq.QuadPart));
}


LARGE_INTEGER
SK_GetPerfFreq (void)
{
  static LARGE_INTEGER freq = { 0UL };
  static volatile LONG init = FALSE;

  if (ReadAcquire (&init) < 2)
  {
      RtlQueryPerformanceFrequency =
        (QueryPerformanceCounter_pfn)
    SK_GetProcAddress ( L"NtDll",
                         "RtlQueryPerformanceFrequency" );

    if (! InterlockedCompareExchange (&init, 1, 0))
    {
      if (RtlQueryPerformanceFrequency != nullptr)
          RtlQueryPerformanceFrequency            (&freq);
      else if (QueryPerformanceFrequency_Original != nullptr)
               QueryPerformanceFrequency_Original (&freq);
      else
        QueryPerformanceFrequency                 (&freq);

      InterlockedIncrement (&init);

      return freq;
    }

    if (ReadAcquire (&init) < 2)
    {
      LARGE_INTEGER freq2 = { };

      if (RtlQueryPerformanceFrequency != nullptr)
          RtlQueryPerformanceFrequency            (&freq2);
      else if (QueryPerformanceFrequency_Original != nullptr)
               QueryPerformanceFrequency_Original (&freq2);
      else
        QueryPerformanceFrequency                 (&freq2);

      return
        freq2;
    }
  }

  return freq;
}