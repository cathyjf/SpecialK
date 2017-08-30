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

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#include <SpecialK/d3d9_backend.h>
#include <SpecialK/D3D9/texmgr.h>
#include <SpecialK/render_backend.h>
#include <SpecialK/dxgi_backend.h>
#include <SpecialK/import.h>

MIDL_INTERFACE("D0223B96-BF7A-43fd-92BD-A43B0D82B9EB") IDirect3DDevice9;
MIDL_INTERFACE("B18B10CE-2649-405a-870F-95F777D4313A") IDirect3DDevice9Ex;

#include <SpecialK/core.h>
#include <SpecialK/log.h>

#include <SpecialK/stdafx.h>
#include <SpecialK/nvapi.h>
#include <SpecialK/config.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <atlbase.h>
#include <comdef.h>

#include <SpecialK/log.h>
#include <SpecialK/utility.h>
#include <SpecialK/command.h>
#include <SpecialK/hooks.h>
#include <SpecialK/window.h>
#include <SpecialK/steam_api.h>

#include <imgui/backends/imgui_d3d9.h>

#include <SpecialK/framerate.h>
#include <SpecialK/diagnostics/compatibility.h>

volatile LONG                        __d3d9_ready = FALSE;
SK::D3D9::PipelineStatsD3D9 SK::D3D9::pipeline_stats_d3d9;

unsigned int
__stdcall
HookD3D9 (LPVOID user);

unsigned int
__stdcall
HookD3D9Ex (LPVOID user);

extern volatile ULONG ImGui_Init = FALSE;

void
WINAPI
WaitForInit_D3D9 (void)
{
  while (! InterlockedCompareExchange (&__d3d9_ready, FALSE, FALSE))
    SleepEx (config.system.init_delay, TRUE);
}

extern "C++" void
__stdcall
SK_D3D9_UpdateRenderStats (IDirect3DSwapChain9* pSwapChain, IDirect3DDevice9* pDevice = nullptr);



using Direct3DCreate9PROC   =
  IDirect3D9* (STDMETHODCALLTYPE *)(UINT           SDKVersion);
using Direct3DCreate9ExPROC =
  HRESULT     (STDMETHODCALLTYPE *)(UINT           SDKVersion,
                                    IDirect3D9Ex** d3d9ex);

__declspec (noinline)
D3DPRESENT_PARAMETERS*
WINAPI
SK_SetPresentParamsD3D9 (IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pparams);


D3D9PresentDevice_pfn         D3D9Present_Original_Pre               = nullptr;
D3D9PresentDevice_pfn         D3D9Present_Original                   = nullptr;

D3D9PresentDeviceEx_pfn       D3D9PresentEx_Original_Pre             = nullptr;
D3D9PresentDeviceEx_pfn       D3D9PresentEx_Original                 = nullptr;

D3D9PresentSwapChain_pfn      D3D9PresentSwap_Original_Pre           = nullptr;
D3D9PresentSwapChain_pfn      D3D9PresentSwap_Original               = nullptr;

D3D9CreateDevice_pfn          D3D9CreateDevice_Original              = nullptr;
D3D9CreateDeviceEx_pfn        D3D9CreateDeviceEx_Original            = nullptr;
D3D9Reset_pfn                 D3D9Reset_Original                     = nullptr;
D3D9ResetEx_pfn               D3D9ResetEx_Original                   = nullptr;

SetGammaRamp_pfn              D3D9SetGammaRamp_Original              = nullptr;

Direct3DCreate9PROC           Direct3DCreate9_Import                 = nullptr;
Direct3DCreate9ExPROC         Direct3DCreate9Ex_Import               = nullptr;

CreateAdditionalSwapChain_pfn D3D9CreateAdditionalSwapChain_Original = nullptr;

TestCooperativeLevel_pfn      D3D9TestCooperativeLevel_Original      = nullptr;
BeginScene_pfn                D3D9BeginScene_Original                = nullptr;
EndScene_pfn                  D3D9EndScene_Original                  = nullptr;
DrawPrimitive_pfn             D3D9DrawPrimitive_Original             = nullptr;
DrawIndexedPrimitive_pfn      D3D9DrawIndexedPrimitive_Original      = nullptr;
DrawPrimitiveUP_pfn           D3D9DrawPrimitiveUP_Original           = nullptr;
DrawIndexedPrimitiveUP_pfn    D3D9DrawIndexedPrimitiveUP_Original    = nullptr;
SetTexture_pfn                D3D9SetTexture_Original                = nullptr;
SetSamplerState_pfn           D3D9SetSamplerState_Original           = nullptr;
SetViewport_pfn               D3D9SetViewport_Original               = nullptr;
SetRenderState_pfn            D3D9SetRenderState_Original            = nullptr;
SetVertexShaderConstantF_pfn  D3D9SetVertexShaderConstantF_Original  = nullptr;
SetPixelShaderConstantF_pfn   D3D9SetPixelShaderConstantF_Original   = nullptr;
SetPixelShader_pfn            D3D9SetPixelShader_Original            = nullptr;
SetVertexShader_pfn           D3D9SetVertexShader_Original           = nullptr;
SetScissorRect_pfn            D3D9SetScissorRect_Original            = nullptr;
CreateTexture_pfn             D3D9CreateTexture_Original             = nullptr;
CreateVertexBuffer_pfn        D3D9CreateVertexBuffer_Original        = nullptr;
SetStreamSource_pfn           D3D9SetStreamSource_Original           = nullptr;
SetStreamSourceFreq_pfn       D3D9SetStreamSourceFreq_Original       = nullptr;
SetFVF_pfn                    D3D9SetFVF_Original                    = nullptr;
SetVertexDeclaration_pfn      D3D9SetVertexDeclaration_Original      = nullptr;
CreateVertexDeclaration_pfn   D3D9CreateVertexDeclaration_Original   = nullptr;
CreateRenderTarget_pfn        D3D9CreateRenderTarget_Original        = nullptr;
CreateDepthStencilSurface_pfn D3D9CreateDepthStencilSurface_Original = nullptr;
SetRenderTarget_pfn           D3D9SetRenderTarget_Original           = nullptr;
SetDepthStencilSurface_pfn    D3D9SetDepthStencilSurface_Original    = nullptr;
UpdateTexture_pfn             D3D9UpdateTexture_Original             = nullptr;
StretchRect_pfn               D3D9StretchRect_Original               = nullptr;
SetCursorPosition_pfn         D3D9SetCursorPosition_Original         = nullptr;


D3DPRESENT_PARAMETERS    g_D3D9PresentParams          = {  };

__declspec (noinline)
HRESULT
WINAPI D3D9PresentCallback_Pre ( IDirect3DDevice9 *This,
                      _In_ const RECT             *pSourceRect,
                      _In_ const RECT             *pDestRect,
                      _In_       HWND              hDestWindowOverride,
                      _In_ const RGNDATA          *pDirtyRegion );

#define D3D9_VIRTUAL_HOOK(_Base,_Index,_Name,_Override,_Original,_Type) {    \
  void** _vftable = *(void***)*(_Base);                                      \
                                                                             \
  if ((_Original) == nullptr) {                                              \
    SK_CreateVFTableHook2 ( L##_Name,                                        \
                              _vftable,                                      \
                                (_Index),                                    \
                                  (_Override),                               \
                                    (LPVOID *)&(_Original));                 \
  }                                                                          \
}

#define D3D9_VIRTUAL_HOOK_EX(_Base,_Index,_Name,_Override,_Original,_Type) {  \
  void** _vftable = *(void***)*(_Base);                                       \
                                                                              \
  if ((_Original) == nullptr) {                                               \
    SK_CreateVFTableHookEx( L##_Name,                                         \
                              _vftable,                                       \
                                (_Index),                                     \
                                  (_Override),                                \
                                    (LPVOID *)&(_Original));                  \
  }                                                                           \
}

#define D3D9_INTERCEPT_EX(_Base,_Index,_Name,_Override,_Original,_Type)\
    D3D9_VIRTUAL_HOOK_EX (   _Base,   _Index, _Name, _Override,        \
                        _Original, _Type );                            \

class SK_D3D9RenderBackend : public SK_IVariableListener
{
  virtual bool OnVarChange (SK_IVariable* var, void* val) override
  {
    if (val != nullptr)
    {
      if (var == osd_vidcap_)
      {
        if (*static_cast <bool *> (val) == true)
        {
          static volatile ULONG __installed_second_hook = FALSE;

          if (! InterlockedCompareExchange (&__installed_second_hook, TRUE, FALSE))
          {
                  D3D9_INTERCEPT_EX ( &SK_GetCurrentRenderBackend ().device, 17,
                                        "IDirect3DDevice9::Present",
                                        D3D9PresentCallback_Pre,
                                        D3D9Present_Original_Pre,
                                        D3D9PresentDevice_pfn );

                    InterlockedExchange (
                      &SK_D3D9RenderBackend::getInstance ()->__D3D9Present_PreHooked,
                        TRUE
                    );
          }

          dll_log.Log ( L"[  D3D9  ]  First Present Hook: %p, "
                                   L"Second Present Hook: %p",
                        D3D9Present_Original,
                        D3D9Present_Original_Pre );

          if (D3D9Present_Original_Pre != nullptr)
          {
            config.render.d3d9.osd_in_vidcap = true;
          }
        }

        else
        {
          config.render.d3d9.osd_in_vidcap = false;
        }

        return true;
      }
    }
    
    return false;
  }

public:
  volatile ULONG __D3D9Present_PreHooked      = FALSE;
  volatile ULONG __D3D9PresentEx_PreHooked    = FALSE;
  volatile ULONG __D3D9PresentChain_PreHooked = FALSE;

  static SK_D3D9RenderBackend* getInstance (void)
  {
    if (pBackend == nullptr)
      pBackend = new SK_D3D9RenderBackend ();

    return pBackend;
  }

protected:
  SK_IVariable* osd_vidcap_;

private:
  SK_D3D9RenderBackend (void)
  {
    config.render.d3d9.osd_in_vidcap = true;

    SK_ICommandProcessor* cmd =
        SK_GetCommandProcessor ();

    osd_vidcap_ =
      SK_CreateVar (
        SK_IVariable::Boolean,
          static_cast <bool *> (&config.render.d3d9.osd_in_vidcap),
            this
      );

    // Tales of Berseria "Fix" uses this
    static bool broken_lod_bias = false;

    cmd->AddVariable ("d3d9.OSDInVidcap",            osd_vidcap_);
    cmd->AddVariable ("d3d9.HasBrokenMipmapLODBias", SK_CreateVar (SK_IVariable::Boolean, &broken_lod_bias));
  }

  static SK_D3D9RenderBackend* pBackend;
};

SK_D3D9RenderBackend* SK_D3D9RenderBackend::pBackend = nullptr;

#include <CEGUI/CEGUI.h>
#include <CEGUI/System.h>
#include <CEGUI/DefaultResourceProvider.h>
#include <CEGUI/ImageManager.h>
#include <CEGUI/Image.h>
#include <CEGUI/Font.h>
#include <CEGUI/Scheme.h>
#include <CEGUI/WindowManager.h>
#include <CEGUI/falagard/WidgetLookManager.h>
#include <CEGUI/ScriptModule.h>
#include <CEGUI/XMLParser.h>
#include <CEGUI/GeometryBuffer.h>
#include <CEGUI/GUIContext.h>
#include <CEGUI/RenderTarget.h>
#include <CEGUI/AnimationManager.h>
#include <CEGUI/FontManager.h>
#include <CEGUI/RendererModules/Direct3D9/Renderer.h>

#include <SpecialK/osd/text.h>
#include <SpecialK/osd/popup.h>

#ifndef SK_BUILD__INSTALLER
#pragma comment (lib, "d3dx9.lib")

#ifdef _WIN64
#pragma comment (lib, "CEGUI/x64/CEGUIDirect3D9Renderer-0.lib")
#else
#pragma comment (lib, "CEGUI/Win32/CEGUIDirect3D9Renderer-0.lib")
#endif
#endif

CEGUI::Direct3D9Renderer* cegD3D9    = nullptr;
IDirect3DStateBlock9*     cegD3D9_SB = nullptr;

static volatile ULONG __gui_reset          = TRUE;
static volatile ULONG __cegui_frames_drawn = 0;

void ResetCEGUI_D3D9 (IDirect3DDevice9* pDev);

#include <CEGUI/CEGUI.h>
#include <CEGUI/Rect.h>
#include <CEGUI/RendererModules/Direct3D9/Renderer.h>

void
SK_CEGUI_DrawD3D9 (IDirect3DDevice9* pDev, IDirect3DSwapChain9* pSwapChain)
{
  if (! config.cegui.enable)
    return;

  InterlockedIncrement (&__cegui_frames_drawn);

  if (InterlockedCompareExchange (&__gui_reset, FALSE, TRUE))
  {
    if (cegD3D9 != nullptr)
    {
      SK_TextOverlayManager::getInstance ()->destroyAllOverlays ();
      SK_PopupManager::getInstance       ()->destroyAllPopups   ();

      CEGUI::WindowManager::getDllSingleton ().cleanDeadPool    ();
    }

    if (cegD3D9_SB != nullptr) cegD3D9_SB->Release ();
        cegD3D9_SB  = nullptr;

    if (cegD3D9 != nullptr) cegD3D9->destroySystem ();
        cegD3D9  = nullptr;
  }

  else if (cegD3D9 == nullptr)
  {
    extern void
    SK_InstallWindowHook (HWND hWnd);

    D3DPRESENT_PARAMETERS pparams;
    pSwapChain->GetPresentParameters (&pparams);

    SK_InstallWindowHook (pparams.hDeviceWindow);

    ResetCEGUI_D3D9 (pDev);
  }

  else if (pDev != nullptr)
  {
    CComPtr <IDirect3DStateBlock9> pStateBlock = nullptr;
    pDev->CreateStateBlock (D3DSBT_ALL, &pStateBlock);

    if (! pStateBlock)
      return;

    pStateBlock->Capture ();

    bool new_sb = (! cegD3D9_SB);

    if (new_sb)
    {
      pDev->CreateStateBlock (D3DSBT_ALL, &cegD3D9_SB);

      pDev->SetRenderState (D3DRS_CULLMODE,                 D3DCULL_NONE);

      pDev->SetRenderState (D3DRS_LIGHTING,                 FALSE);
      pDev->SetRenderState (D3DRS_SPECULARENABLE,           FALSE);
      pDev->SetRenderState (D3DRS_FOGENABLE,                FALSE);
      pDev->SetRenderState (D3DRS_AMBIENT,                  0);

      pDev->SetRenderState (D3DRS_ALPHABLENDENABLE,         TRUE);
      pDev->SetRenderState (D3DRS_ALPHATESTENABLE,          FALSE);

      pDev->SetRenderState (D3DRS_SRCBLEND,                 D3DBLEND_INVSRCALPHA);
      pDev->SetRenderState (D3DRS_DESTBLEND,                D3DBLEND_SRCALPHA);
      pDev->SetRenderState (D3DRS_BLENDOP,                  D3DBLENDOP_ADD);
      pDev->SetRenderState (D3DRS_BLENDOPALPHA,             D3DBLENDOP_ADD);

      pDev->SetRenderState (D3DRS_STENCILENABLE,            FALSE);
      pDev->SetRenderState (D3DRS_SCISSORTESTENABLE,        FALSE);

      pDev->SetRenderState (D3DRS_ZENABLE,                  FALSE);
      pDev->SetRenderState (D3DRS_ZWRITEENABLE,             FALSE);
      pDev->SetRenderState (D3DRS_SRGBWRITEENABLE,          FALSE);
      pDev->SetRenderState (D3DRS_COLORWRITEENABLE,         D3DCOLORWRITEENABLE_RED   | 
                                                            D3DCOLORWRITEENABLE_GREEN | 
                                                            D3DCOLORWRITEENABLE_BLUE  |
                                                            D3DCOLORWRITEENABLE_ALPHA );

      pDev->SetSamplerState        (0, D3DSAMP_SRGBTEXTURE, FALSE);

      pDev->SetTextureStageState   (0, D3DTSS_COLOROP,      D3DTOP_MODULATE);
      pDev->SetTextureStageState   (0, D3DTSS_COLORARG0,    D3DTA_CURRENT);
      pDev->SetTextureStageState   (0, D3DTSS_COLORARG1,    D3DTA_TEXTURE);
      pDev->SetTextureStageState   (0, D3DTSS_COLORARG2,    D3DTA_DIFFUSE);

      pDev->SetTextureStageState   (0, D3DTSS_ALPHAOP,      D3DTOP_MODULATE);
      pDev->SetTextureStageState   (0, D3DTSS_ALPHAARG0,    D3DTA_CURRENT);
      pDev->SetTextureStageState   (0, D3DTSS_ALPHAARG1,    D3DTA_TEXTURE);
      pDev->SetTextureStageState   (0, D3DTSS_ALPHAARG2,    D3DTA_DIFFUSE);

      pDev->SetTextureStageState   (0, D3DTSS_RESULTARG,    D3DTA_CURRENT);

      pDev->SetSamplerState        (0, D3DSAMP_MINFILTER,   D3DTEXF_POINT);
      pDev->SetSamplerState        (0, D3DSAMP_MAGFILTER,   D3DTEXF_POINT);

      cegD3D9_SB->Capture ();
    } else {
      cegD3D9_SB->Apply ();
    }

    D3DVIEWPORT9        vp_orig;
    pDev->GetViewport (&vp_orig);

    D3DCAPS9 caps;
    pDev->GetDeviceCaps (&caps);

    CComPtr <IDirect3DSurface9> pBackBuffer = nullptr;
    CComPtr <IDirect3DSurface9> rts [32];
    CComPtr <IDirect3DSurface9> ds          = nullptr;

    for (UINT target = 0; target < caps.NumSimultaneousRTs; target++) {
      pDev->GetRenderTarget (target, &rts [target]);
    }

    pDev->GetDepthStencilSurface (&ds);

    D3DSURFACE_DESC surf_desc = { };

    if (SUCCEEDED (pDev->GetBackBuffer (0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
    {
      pBackBuffer->GetDesc (&surf_desc);

      pDev->SetRenderTarget (0, pBackBuffer);

      for (UINT target = 1; target < caps.NumSimultaneousRTs; target++)
        pDev->SetRenderTarget (target, nullptr);
    }

    D3DPRESENT_PARAMETERS pp;

    if (SUCCEEDED (pSwapChain->GetPresentParameters (&pp)) && pp.hDeviceWindow != nullptr)
    {
      extern HWND hWndRender;
      if (pp.hDeviceWindow != hWndRender)
        hWndRender = pp.hDeviceWindow;
    }

#if 0    
    surf_desc.Width  = ImGui::GetIO ().DisplaySize.x;
    surf_desc.Height = ImGui::GetIO ().DisplaySize.y;

    D3DVIEWPORT9 vp_new;

    vp_new.X      = 0;
    vp_new.Y      = 0;
    vp_new.Width  = surf_desc.Width;
    vp_new.Height = surf_desc.Height;

    vp_new.MinZ =  0.0f;
    vp_new.MaxZ =  1.0f;

    pDev->SetViewport (&vp_new);
#else
    // Keep within the confines of the game's viewport, in case it is managing
    //   aspect ratio itself
    surf_desc.Width  = vp_orig.Width;
    surf_desc.Height = vp_orig.Height;

    CEGUI::System::getDllSingleton ().getRenderer ()->setDisplaySize (
        CEGUI::Sizef (
          static_cast <float> (surf_desc.Width),
          static_cast <float> (surf_desc.Height)
        )
    );
#endif

    cegD3D9->beginRendering ();
    {
      SK_TextOverlayManager::getInstance ()->drawAllOverlays (0.0f, 0.0f);

      SK_Steam_DrawOSD ();

      pDev->SetRenderState (D3DRS_SRGBWRITEENABLE,          FALSE);
      pDev->SetRenderState (D3DRS_ALPHABLENDENABLE,         TRUE);
      pDev->SetRenderState (D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
      pDev->SetRenderState (D3DRS_SRCBLEND,                 D3DBLEND_INVSRCALPHA);
      pDev->SetRenderState (D3DRS_DESTBLEND,                D3DBLEND_SRCALPHA);

      pDev->SetRenderState (D3DRS_ALPHATESTENABLE,          FALSE);

      CEGUI::System::getDllSingleton ().renderAllGUIContexts ();

      if ( static_cast <int> (SK_GetCurrentRenderBackend ().api) &
           static_cast <int> (SK_RenderAPI::D3D9)                  )
      {
        extern DWORD SK_ImGui_DrawFrame (DWORD dwFlags, void* user);
                     SK_ImGui_DrawFrame (       0x00,     nullptr );
      }
    }
    cegD3D9->endRendering ();

    pDev->SetViewport (&vp_orig);

    for (UINT target = 0; target < caps.NumSimultaneousRTs; target++)
      pDev->SetRenderTarget (target, rts [target]);

    pDev->SetDepthStencilSurface (ds);
    pStateBlock->Apply ();
  }
}

void
SK_CEGUI_QueueResetD3D9 (void)
{
  InterlockedExchange (&__gui_reset, TRUE);
}

void
ResetCEGUI_D3D9 (IDirect3DDevice9* pDev)
{
  if (! config.cegui.enable)
    return;

  if (InterlockedCompareExchange (&__cegui_frames_drawn, 0, 0) < 5)
    return;

  if (cegD3D9 != nullptr || (pDev == nullptr))
  {
    SK_Steam_ClearPopups ();

    if (cegD3D9 != nullptr)
    {
      SK_TextOverlayManager::getInstance ()->destroyAllOverlays ();
      SK_PopupManager::getInstance ()->destroyAllPopups         ();

      CEGUI::WindowManager::getDllSingleton ().cleanDeadPool    ();
    }

    if (cegD3D9_SB != nullptr) cegD3D9_SB->Release ();
        cegD3D9_SB  = nullptr;

    if (cegD3D9 != nullptr) cegD3D9->destroySystem ();
        cegD3D9  = nullptr;
  }

  else if (cegD3D9 == nullptr)
  {
    if (cegD3D9_SB != nullptr) cegD3D9_SB->Release ();
        cegD3D9_SB  = nullptr;

    try
    {
      cegD3D9 = (CEGUI::Direct3D9Renderer *)
        &CEGUI::Direct3D9Renderer::bootstrapSystem (pDev);

      extern void
      SK_CEGUI_RelocateLog (void);

      SK_CEGUI_RelocateLog ();
    } catch (...) {
      cegD3D9       = nullptr;
      return;
    }

    extern void
    SK_CEGUI_InitBase (void);

    SK_CEGUI_InitBase ();

    SK_PopupManager::getInstance ()->destroyAllPopups       ();
    SK_TextOverlayManager::getInstance ()->resetAllOverlays (cegD3D9);

    SK_Steam_ClearPopups ();

    //
    // Initialize ImGui for D3D9 games
    //
    D3DDEVICE_CREATION_PARAMETERS params;
    pDev->GetCreationParameters (&params);
    
    if ( ImGui_ImplDX9_Init ( (void *)params.hFocusWindow,
                                    pDev,
                                     nullptr )
       )
    {
      InterlockedExchange ( &ImGui_Init, TRUE );
    }
  }
}



using finish_pfn =
  void (WINAPI *)(void);

void
WINAPI
SK_HookD3D9 (void)
{
  static volatile ULONG hooked = FALSE;

  if (InterlockedCompareExchange (&hooked, TRUE, FALSE))
  {
    return;
  }

  HMODULE hBackend = 
    (SK_GetDLLRole () & DLL_ROLE::D3D9) ? backend_dll :
                                   GetModuleHandle (L"d3d9.dll");

  dll_log.Log (L"[   D3D9   ] Importing Direct3DCreate9{Ex}...");
  dll_log.Log (L"[   D3D9   ] ================================");

  if (! _wcsicmp (SK_GetModuleName (SK_GetDLL ()).c_str (), L"d3d9.dll"))
  {
    dll_log.Log (L"[   D3D9   ]   Direct3DCreate9:   %ph",
      (Direct3DCreate9_Import) =  \
        (Direct3DCreate9PROC)GetProcAddress (hBackend, "Direct3DCreate9"));

    if (config.apis.d3d9ex.hook)
    {
      dll_log.Log (L"[   D3D9   ]   Direct3DCreate9Ex: %ph",
        (Direct3DCreate9Ex_Import) =  \
          (Direct3DCreate9ExPROC)GetProcAddress (hBackend, "Direct3DCreate9Ex"));
    }
  }

  else
  {
    if ( MH_OK ==
           SK_CreateDLLHook2 ( L"d3d9.dll",
                                "Direct3DCreate9",
                                 Direct3DCreate9,
        static_cast_p2p <void> (&Direct3DCreate9_Import) )
       )
    {
      dll_log.Log (L"[   D3D9   ]   Direct3DCreate9:   %p  { Hooked }",
        (Direct3DCreate9_Import) );
      
      
      if ( config.apis.d3d9ex.hook &&
             MH_OK ==
               SK_CreateDLLHook2 ( L"d3d9.dll",
                                    "Direct3DCreate9Ex",
                                     Direct3DCreate9Ex,
            static_cast_p2p <void> (&Direct3DCreate9Ex_Import) )
         )
      {
        dll_log.Log (L"[   D3D9   ]   Direct3DCreate9Ex: %p  { Hooked }",
          (Direct3DCreate9Ex_Import) );
      }
    }
  }

  SK_ApplyQueuedHooks ();

// Load user-defined DLLs (Plug-In)
#ifdef _WIN64
  SK_LoadPlugIns64 ();
#else
  SK_LoadPlugIns32 ();
#endif

  HookD3D9 (nullptr);
}

static std::queue <DWORD> old_threads;

void
WINAPI
d3d9_init_callback (finish_pfn finish)
{
  if (! SK_IsHostAppSKIM ())
  {
    SK_BootD3D9 ();

    while (! InterlockedCompareExchange (&__d3d9_ready, FALSE, FALSE))
      SleepEx (100UL, TRUE);
  }

  finish ();
}


bool
SK::D3D9::Startup (void)
{
  bool ret = SK_StartupCore (L"d3d9", d3d9_init_callback);

  return ret;
}

bool
SK::D3D9::Shutdown (void)
{
#if 0
  // The texture manager built-in to SK is derived from these ...
  //   until those projects are modified to use THIS texture manager,
  //     they need special treatment.
  if ( GetModuleHandle (L"tzfix.dll") == NULL &&
       GetModuleHandle (L"tsfix.dll") == NULL )
  {
    sk::d3d9::tex_mgr.Shutdown ();
  }
#endif

  return SK_ShutdownCore (L"d3d9");
}


extern "C" const wchar_t* SK_DescribeVirtualProtectFlags (DWORD dwProtect);

#define __PTR_SIZE   sizeof LPCVOID
#define __PAGE_PRIVS PAGE_EXECUTE_READWRITE

#define D3D9_VIRTUAL_OVERRIDE(_Base,_Index,_Name,_Override,_Original,_Type) { \
  void** _vftable = *(void***)*_Base;                                         \
                                                                              \
  if (_vftable [_Index] != _Override) {                                       \
    DWORD dwProtect;                                                          \
                                                                              \
    VirtualProtect (&_vftable [_Index], __PTR_SIZE, __PAGE_PRIVS, &dwProtect);\
                                                                              \
    /*dll_log.Log (L" Old VFTable entry for %s: %ph  (Memory Policy: %s)",*/  \
                 /*L##_Name, vftable [_Index],                              */\
                 /*SK_DescribeVirtualProtectFlags (dwProtect));             */\
                                                                              \
    if (_Original == NULL)                                                    \
      _Original = (##_Type)_vftable [_Index];                                 \
                                                                              \
    /*dll_log.Log (L"  + %s: %ph", L#_Original, _Original);*/                 \
                                                                              \
    _vftable [_Index] = _Override;                                            \
                                                                              \
    VirtualProtect (&_vftable [_Index], __PTR_SIZE, dwProtect, &dwProtect);   \
                                                                              \
    /*dll_log.Log(L" New VFTable entry for %s: %ph  (Memory Policy: %s)\n"*/  \
                  /*,L##_Name, vftable [_Index],                            */\
                  /*SK_DescribeVirtualProtectFlags (dwProtect));            */\
  }                                                                           \
}

#define D3D9_INTERCEPT(_Base,_Index,_Name,_Override,_Original,_Type) { \
  if (config.render.d3d9.hook_type == 0) {                             \
    D3D9_VIRTUAL_HOOK (   _Base,   _Index, _Name, _Override,           \
                        _Original, _Type );                            \
  } else {                                                             \
    D3D9_VIRTUAL_OVERRIDE(_Base,   _Index, _Name, _Override,           \
                        _Original, _Type );                            \
  }                                                                    \
}

#define D3D9_INTERCEPT_EX(_Base,_Index,_Name,_Override,_Original,_Type)\
    D3D9_VIRTUAL_HOOK_EX (   _Base,   _Index, _Name, _Override,        \
                        _Original, _Type );                            \

#define D3D9_CALL(_Ret, _Call) {                                      \
  (_Ret) = (_Call);                                                   \
  dll_log.Log ( L"[   D3D9   ] [@]  Return: %s  -  < " L#_Call L" >", \
                  SK_DescribeHRESULT (_Ret) );                        \
}

void
WINAPI
SK_D3D9_FixUpBehaviorFlags (DWORD& BehaviorFlags)
{
  if (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING)
  {
    dll_log.Log (L"[CompatHack] D3D9 Fixup: "
                 L"Software Vertex Processing Replaced with Mixed-Mode.");
    BehaviorFlags &= ~D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    BehaviorFlags |=  D3DCREATE_MIXED_VERTEXPROCESSING;
  }

  if (config.render.d3d9.force_impure)
    BehaviorFlags &= ~D3DCREATE_PUREDEVICE;

  if ( GetModuleHandle (L"tbfix.dll") ||
       GetModuleHandle (L"tzfix.dll") ||
       GetModuleHandle (L"tsfix.dll") )
  {
    BehaviorFlags |= D3DCREATE_MULTITHREADED;
  }

  //BehaviorFlags = 0x144;
}

void
WINAPI
SK_D3D9_SetFPSTarget ( D3DPRESENT_PARAMETERS* pPresentationParameters,
                       D3DDISPLAYMODEEX*      pFullscreenMode = nullptr )
{
  auto TargetFPS =
    static_cast <int> (config.render.framerate.target_fps);

  // First consider D3D9Ex FullScreen Mode
  int Refresh   = pFullscreenMode != nullptr ?
                    pFullscreenMode->RefreshRate :
                    0;

  // Then, use the presentation parameters
  if (Refresh == 0)
  {
    if ( pPresentationParameters           != nullptr &&
         pPresentationParameters->Windowed == FALSE )
      Refresh = pPresentationParameters->FullScreen_RefreshRateInHz;
    else
      Refresh = 0;
  }

  if (config.render.framerate.refresh_rate != -1)
  {
    if ( pPresentationParameters           != nullptr &&
         pPresentationParameters->Windowed == FALSE) {
      dll_log.Log ( L"[  D3D9  ]  >> Refresh Rate Override: %li",
                      config.render.framerate.refresh_rate );

      Refresh = config.render.framerate.refresh_rate;

      if ( pFullscreenMode != nullptr )
        pFullscreenMode->RefreshRate = Refresh;

      pPresentationParameters->FullScreen_RefreshRateInHz = Refresh;
    }
  }

  if ( TargetFPS != 0 && Refresh != 0       &&
       pPresentationParameters   != nullptr &&
       pPresentationParameters->Windowed == FALSE)
  {
    if (Refresh >= TargetFPS) {
      if (! (Refresh % TargetFPS)) {
        dll_log.Log ( L"[   D3D9   ]  >> Targeting %li FPS - using 1:%li VSYNC;"
                      L" (refresh = %li Hz)",
                        TargetFPS,
                          Refresh / TargetFPS,
                            Refresh );

        pPresentationParameters->SwapEffect           = D3DSWAPEFFECT_DISCARD;
        pPresentationParameters->PresentationInterval = Refresh / TargetFPS;

      } else {
        dll_log.Log ( L"[   D3D9   ]  >> Cannot target %li FPS using VSYNC - no such factor exists;"
                      L" (refresh = %li Hz)",
                        TargetFPS,
                          Refresh );
      }
    } else {
      dll_log.Log ( L"[   D3D9   ]  >> Cannot target %li FPS using VSYNC - higher than refresh rate;"
                    L" (refresh = %li Hz)",
                      TargetFPS,
                        Refresh );
    }
  }

  if (pPresentationParameters != nullptr)
  {
    if (       config.render.framerate.buffer_count != -1 &&
         (UINT)config.render.framerate.buffer_count != 
           pPresentationParameters->BackBufferCount ) {
      dll_log.Log ( L"[   D3D9   ]  >> Backbuffer Override: (Requested=%lu, Override=%li)",
                      pPresentationParameters->BackBufferCount,
                        config.render.framerate.buffer_count );
      pPresentationParameters->BackBufferCount =
        config.render.framerate.buffer_count;
    }

    if (       config.render.framerate.present_interval != -1 &&
         (UINT)config.render.framerate.present_interval !=
            pPresentationParameters->PresentationInterval ) {
      dll_log.Log ( L"[   D3D9   ]  >> VSYNC Override: (Requested=1:%lu, Override=1:%li)",
                      pPresentationParameters->PresentationInterval,
                        config.render.framerate.present_interval );
      pPresentationParameters->PresentationInterval =
        config.render.framerate.present_interval;
    }
  }
}

extern bool WINAPI SK_CheckForGeDoSaTo (void);

__declspec (noinline)
HRESULT
WINAPI
D3D9PresentCallbackEx ( IDirect3DDevice9Ex *This,
             _In_ const RECT               *pSourceRect,
             _In_ const RECT               *pDestRect,
             _In_       HWND                hDestWindowOverride,
             _In_ const RGNDATA            *pDirtyRegion,
             _In_       DWORD               dwFlags )
{
  if (This == nullptr)
    return E_NOINTERFACE;

  //SK_D3D9_UpdateRenderStats (nullptr, This);

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  rb.api = SK_RenderAPI::D3D9Ex;
  SK_BeginBufferSwap ();

  HRESULT hr = E_FAIL;

  CComPtr <IDirect3DSwapChain9> pSwapChain = nullptr;

  if (SUCCEEDED (This->GetSwapChain (0, &pSwapChain)))
  {
    SK_CEGUI_DrawD3D9 (This, pSwapChain);

    CComPtr <IDirect3DSurface9> pSurf = nullptr;

    if (SUCCEEDED (pSwapChain->GetBackBuffer (0,  D3DBACKBUFFER_TYPE_MONO, (IDirect3DSurface9 **)&pSurf)))
    {
      rb.device    = This;
      rb.swapchain = pSwapChain;


      //
      // Update G-Sync; doing this here prevents trying to do this on frames where
      //   the swapchain was resized, which would deadlock the software.
      //
      if (sk::NVAPI::nv_hardware && config.apis.NvAPI.gsync_status)
      {
        NvAPI_D3D9_GetSurfaceHandle (pSurf, &rb.surface);
        rb.gsync_state.update ();
      }
    }
  }

  // Hack for GeDoSaTo
  if (! SK_CheckForGeDoSaTo ())
    hr = D3D9PresentEx_Original ( This,
                                    pSourceRect,
                                      pDestRect,
                                        hDestWindowOverride,
                                           pDirtyRegion,
                                            dwFlags );
  else
    hr = D3D9Present_Original ( This,
                                  pSourceRect,
                                    pDestRect,
                                      hDestWindowOverride,
                                        pDirtyRegion );

  if (! config.osd.pump)
  {
    if (trigger_reset == reset_stage_e::Clear)
      hr = SK_EndBufferSwap (hr, This);

    else
      hr = D3DERR_DEVICELOST;
  }

  return hr;
}

__declspec (noinline)
HRESULT
WINAPI
D3D9PresentCallback_Pre ( IDirect3DDevice9 *This,
               _In_ const RECT             *pSourceRect,
               _In_ const RECT             *pDestRect,
               _In_       HWND              hDestWindowOverride,
               _In_ const RGNDATA          *pDirtyRegion )
{
  if ( config.render.d3d9.osd_in_vidcap && InterlockedExchangeAdd (
         &SK_D3D9RenderBackend::getInstance ()->__D3D9Present_PreHooked,
           0 )
     )
  {
    CComPtr <IDirect3DSwapChain9> pSwapChain = nullptr;

    if (SUCCEEDED (This->GetSwapChain (0, &pSwapChain))) {
      SK_CEGUI_DrawD3D9 (This, pSwapChain);
    }
  }

  return
    D3D9Present_Original_Pre ( This,
                                 pSourceRect, 
                                   pDestRect,
                                     hDestWindowOverride,
                                       pDirtyRegion );
}

__declspec (noinline)
HRESULT
WINAPI
D3D9PresentCallback ( IDirect3DDevice9 *This,
           _In_ const RECT             *pSourceRect,
           _In_ const RECT             *pDestRect,
           _In_       HWND              hDestWindowOverride,
           _In_ const RGNDATA          *pDirtyRegion )
{
#if 0
  SetThreadIdealProcessor (GetCurrentThread (),       6);
  SetThreadAffinityMask   (GetCurrentThread (), (1 << 7) | (1 << 6));//config.render.framerate.pin_render_thread);
#endif

  if (This == nullptr)
    return E_NOINTERFACE;

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if (g_D3D9PresentParams.SwapEffect == D3DSWAPEFFECT_FLIPEX)
  {
    HRESULT hr =
      D3D9PresentCallbackEx ( static_cast <IDirect3DDevice9Ex *> (This),
                                pSourceRect,
                                  pDestRect,
                                    hDestWindowOverride,
                                      pDirtyRegion,
                                        D3DPRESENT_FORCEIMMEDIATE |
                                        D3DPRESENT_DONOTWAIT );

    return hr;
  }

  //SK_D3D9_UpdateRenderStats (nullptr, This);

  if (! ( config.render.d3d9.osd_in_vidcap && InterlockedExchangeAdd (
            &SK_D3D9RenderBackend::getInstance ()->__D3D9Present_PreHooked,
              0 )
        )
     )
  {
    CComPtr <IDirect3DSwapChain9> pSwapChain = nullptr;

    if (SUCCEEDED (This->GetSwapChain (0, &pSwapChain)))
    {
      SK_CEGUI_DrawD3D9 (This, pSwapChain);

      CComPtr <IDirect3DSurface9> pSurf = nullptr;

      if (SUCCEEDED (pSwapChain->GetBackBuffer (0,  D3DBACKBUFFER_TYPE_MONO, (IDirect3DSurface9 **)&pSurf)))
      {
        rb.device    = This;
        rb.swapchain = pSwapChain;

        //
        // Update G-Sync; doing this here prevents trying to do this on frames where
        //   the swapchain was resized, which would deadlock the software.
        //
        if (sk::NVAPI::nv_hardware && config.apis.NvAPI.gsync_status)
        {
          NvAPI_D3D9_GetSurfaceHandle (pSurf, &rb.surface);
          rb.gsync_state.update ();
        }
      }
    }
  }

  CComPtr <IDirect3DDevice9Ex> pDev9Ex = nullptr;

  if (SUCCEEDED (rb.device->QueryInterface <IDirect3DDevice9Ex> (&pDev9Ex)))
  {
    reinterpret_cast <int &> (rb.api) = ( static_cast <int> (SK_RenderAPI::D3D9  ) |
                                          static_cast <int> (SK_RenderAPI::D3D9Ex)   );
  }

  else
  {
    rb.api = SK_RenderAPI::D3D9;
  }

  SK_BeginBufferSwap ();

  HRESULT hr = D3D9Present_Original (This,
                                     pSourceRect,
                                     pDestRect,
                                     hDestWindowOverride,
                                     pDirtyRegion);

  if (! config.osd.pump)
  {
    if (trigger_reset == reset_stage_e::Clear)
    {
      hr = SK_EndBufferSwap (hr, This);
    }
    else
      hr = D3DERR_DEVICELOST;
  }

  return hr;
}


#define D3D9_STUB_HRESULT(_Return, _Name, _Proto, _Args)                  \
  __declspec (noinline) _Return STDMETHODCALLTYPE                         \
  _Name _Proto {                                                          \
    WaitForInit ();                                                       \
                                                                          \
    typedef _Return (STDMETHODCALLTYPE *passthrough_pfn) _Proto;          \
    static passthrough_pfn _default_impl = nullptr;                       \
                                                                          \
    if (_default_impl == nullptr) {                                       \
      static const char* szName = #_Name;                                 \
      _default_impl = (passthrough_pfn)                                   \
        GetProcAddress (backend_dll, szName);                             \
                                                                          \
      if (_default_impl == nullptr) {                                     \
        dll_log.Log (                                                     \
          L"[   D3D9   ] Unable to locate symbol '%s' in d3d9.dll",       \
          L#_Name);                                                       \
        return E_NOTIMPL;                                                 \
      }                                                                   \
    }                                                                     \
                                                                          \
    /*dll_log.Log (L"[!] %s %s - "                                      */\
             /*L"[Calling Thread: 0x%04x]",                             */\
      /*L#_Name, L#_Proto, GetCurrentThreadId ());                      */\
                                                                          \
    return _default_impl _Args;                                           \
}

#define D3D9_STUB_VOIDP(_Return, _Name, _Proto, _Args)                    \
  __declspec (noinline) _Return STDMETHODCALLTYPE                         \
  _Name _Proto {                                                          \
    WaitForInit ();                                                       \
                                                                          \
    typedef _Return (STDMETHODCALLTYPE *passthrough_pfn) _Proto;          \
    static passthrough_pfn _default_impl = nullptr;                       \
                                                                          \
    if (_default_impl == nullptr) {                                       \
      static const char* szName = #_Name;                                 \
      _default_impl = (passthrough_pfn)                                   \
        GetProcAddress (backend_dll, szName);                             \
                                                                          \
      if (_default_impl == nullptr) {                                     \
        dll_log.Log (                                                     \
          L"[   D3D9   ] Unable to locate symbol '%s' in d3d9.dll",       \
          L#_Name);                                                       \
        return nullptr;                                                   \
      }                                                                   \
    }                                                                     \
                                                                          \
    /*dll_log.Log (L"[!] %s %s - "                                      */\
           /*L"[Calling Thread: 0x%04x]",                               */\
      /*L#_Name, L#_Proto, GetCurrentThreadId ());                      */\
                                                                          \
    return _default_impl _Args;                                           \
}

#define D3D9_STUB_VOID(_Return, _Name, _Proto, _Args)                     \
  __declspec (noinline) _Return STDMETHODCALLTYPE                         \
  _Name _Proto {                                                          \
    WaitForInit ();                                                       \
                                                                          \
    typedef _Return (STDMETHODCALLTYPE *passthrough_pfn) _Proto;          \
    static passthrough_pfn _default_impl = nullptr;                       \
                                                                          \
    if (_default_impl == nullptr) {                                       \
      static const char* szName = #_Name;                                 \
      _default_impl = (passthrough_pfn)                                   \
        GetProcAddress (backend_dll, szName);                             \
                                                                          \
      if (_default_impl == nullptr) {                                     \
        dll_log.Log (                                                     \
          L"[   D3D9   ] Unable to locate symbol '%s' in d3d9.dll",       \
          L#_Name);                                                       \
        return;                                                           \
      }                                                                   \
    }                                                                     \
                                                                          \
    /*dll_log.Log (L"[!] %s %s - "                                      */\
             /*L"[Calling Thread: 0x%04x]",                             */\
      /*L#_Name, L#_Proto, GetCurrentThreadId ());                      */\
                                                                          \
    _default_impl _Args;                                                  \
}

#define D3D9_STUB_INT(_Return, _Name, _Proto, _Args)                      \
  __declspec (noinline) _Return STDMETHODCALLTYPE                         \
  _Name _Proto {                                                          \
    WaitForInit ();                                                       \
                                                                          \
    typedef _Return (STDMETHODCALLTYPE *passthrough_pfn) _Proto;          \
    static passthrough_pfn _default_impl = nullptr;                       \
                                                                          \
    if (_default_impl == nullptr) {                                       \
      static const char* szName = #_Name;                                 \
      _default_impl = (passthrough_pfn)                                   \
        GetProcAddress (backend_dll, szName);                             \
                                                                          \
      if (_default_impl == nullptr) {                                     \
        dll_log.Log (                                                     \
          L"[   D3D9   ] Unable to locate symbol '%s' in d3d9.dll",       \
          L#_Name);                                                       \
        return 0;                                                         \
      }                                                                   \
    }                                                                     \
                                                                          \
    /*dll_log.Log (L"[!] %s %s - "                                      */\
             /*L"[Calling Thread: 0x%04x]",                             */\
      /*L#_Name, L#_Proto, GetCurrentThreadId ());                      */\
                                                                          \
    return _default_impl _Args;                                           \
}

D3D9_STUB_VOIDP   (void*, Direct3DShaderValidatorCreate, (void),
                                                         (    ))

D3D9_STUB_INT     (int,   D3DPERF_BeginEvent, (D3DCOLOR color, LPCWSTR name),
                                                       (color,         name))
D3D9_STUB_INT     (int,   D3DPERF_EndEvent,   (void),          ( ))

D3D9_STUB_INT     (DWORD, D3DPERF_GetStatus,  (void),          ( ))
D3D9_STUB_VOID    (void,  D3DPERF_SetOptions, (DWORD options), (options))

D3D9_STUB_INT     (BOOL,  D3DPERF_QueryRepeatFrame, (void),    ( ))
D3D9_STUB_VOID    (void,  D3DPERF_SetMarker, (D3DCOLOR color, LPCWSTR name),
                                                      (color,         name))
D3D9_STUB_VOID    (void,  D3DPERF_SetRegion, (D3DCOLOR color, LPCWSTR name),
                                                      (color,         name))

  __declspec (noinline)
  HRESULT
  WINAPI
  D3D9PresentSwapCallback ( IDirect3DSwapChain9 *This,
                 _In_ const RECT                *pSourceRect,
                 _In_ const RECT                *pDestRect,
                 _In_       HWND                 hDestWindowOverride,
                 _In_ const RGNDATA             *pDirtyRegion,
                 _In_       DWORD                dwFlags )
  {
    if (dwFlags & D3DPRESENT_DONOTWAIT)
    {
      return
        D3D9PresentSwap_Original ( This,
                                     pSourceRect,
                                       pDestRect,
                                         hDestWindowOverride,
                                           pDirtyRegion,
                                             dwFlags );
    }

    if (This == nullptr)
      return E_NOINTERFACE;

    SK_RenderBackend& rb =
      SK_GetCurrentRenderBackend ();

    //SK_D3D9_UpdateRenderStats (This);

    CComPtr <IDirect3DDevice9>   pDev    = nullptr;
    CComPtr <IDirect3DDevice9Ex> pDev9Ex = nullptr;

    if (SUCCEEDED (This->GetDevice (&pDev)))
    {
      SK_CEGUI_DrawD3D9 (pDev, This);

      CComPtr <IDirect3DSurface9> pSurf = nullptr;

      if (SUCCEEDED (This->GetBackBuffer (0,  D3DBACKBUFFER_TYPE_MONO, &pSurf)))
      {
        rb.device    = pDev;
        rb.swapchain = This;

        //
        // Update G-Sync; doing this here prevents trying to do this on frames where
        //   the swapchain was resized, which would deadlock the software.
        //
        if (sk::NVAPI::nv_hardware && config.apis.NvAPI.gsync_status)
        {
          NvAPI_D3D9_GetSurfaceHandle (pSurf, &rb.surface);
          rb.gsync_state.update ();
        }
      }
    }

    if (SUCCEEDED (rb.device->QueryInterface <IDirect3DDevice9Ex> (&pDev9Ex)))
    {
      reinterpret_cast <int &> (rb.api)  = ( static_cast <int> (SK_RenderAPI::D3D9  ) |
                                             static_cast <int> (SK_RenderAPI::D3D9Ex)   );
    }

    else
    {
      rb.api = SK_RenderAPI::D3D9;
    }

    SK_BeginBufferSwap ();

    HRESULT hr =
      D3D9PresentSwap_Original ( This,
                                   pSourceRect,
                                     pDestRect,
                                       hDestWindowOverride,
                                         pDirtyRegion,
                                           dwFlags );

    // We are manually pumping OSD updates, do not do them on buffer swaps.
    if (config.osd.pump)
    {
      return hr;
    }

    hr =
      D3DERR_DEVICELOST;

    if (pDev != nullptr)
    {
      if (trigger_reset == reset_stage_e::Clear)
      {
        hr =
          SK_EndBufferSwap (hr, pDev);
      }

      return hr;
    }

    // pDev should be nullptr ... I'm not exactly sure what I was trying to accomplish? :P
    if (trigger_reset == reset_stage_e::Clear)
    {
      hr =
        SK_EndBufferSwap (hr, pDev);
    }

    return hr;
  }

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateAdditionalSwapChain_Override ( IDirect3DDevice9       *This,
                                         D3DPRESENT_PARAMETERS  *pPresentationParameters,
                                         IDirect3DSwapChain9   **pSwapChain )
{
  dll_log.Log (L"[   D3D9   ] [!] %s (%ph, %ph, %ph) - "
    L"%s",
    L"IDirect3DDevice9::CreateAdditionalSwapChain", This,
      pPresentationParameters, pSwapChain,
        SK_SummarizeCaller ().c_str ()
  );

  HRESULT    hr = E_FAIL;
  D3D9_CALL (hr,D3D9CreateAdditionalSwapChain_Original ( This,
                                                           pPresentationParameters,
                                                             pSwapChain ) );

  if (SUCCEEDED (hr))
  {
    D3D9_INTERCEPT ( pSwapChain, 3,
                     "IDirect3DSwapChain9::Present",
                      D3D9PresentSwapCallback,
                      D3D9PresentSwap_Original,
                      D3D9PresentSwapChain_pfn );
  }

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9TestCooperativeLevel_Override (IDirect3DDevice9 *This)
{
  if (trigger_reset == reset_stage_e::Initiate)
  {
    trigger_reset = reset_stage_e::Respond;

    return D3DERR_DEVICELOST;
  }


  else if (trigger_reset == reset_stage_e::Respond)
  {
    return D3DERR_DEVICENOTRESET;
  }


  return D3D9TestCooperativeLevel_Original (This);
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9BeginScene_Override (IDirect3DDevice9 *This)
{
  HRESULT hr =
    D3D9BeginScene_Original (This);

  //D3D9_CALL (hr, D3D9Begincene_Original (This));

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9EndScene_Override (IDirect3DDevice9 *This)
{
  //dll_log.Log (L"[   D3D9   ] [!] %s (%ph) - "
    //L"[Calling Thread: 0x%04x]",
    //L"IDirect3DDevice9::EndScene", This,
    //GetCurrentThreadId ()
  //);

  return D3D9EndScene_Original (This);
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9Reset_Override ( IDirect3DDevice9      *This,
                     D3DPRESENT_PARAMETERS *pPresentationParameters );

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9ResetEx ( IDirect3DDevice9Ex    *This,
              D3DPRESENT_PARAMETERS *pPresentationParameters,
              D3DDISPLAYMODEEX      *pFullscreenDisplayMode );

void
SK_D3D9_HookReset (IDirect3DDevice9 *pDev)
{
  static LPVOID vftable_16  = nullptr;
  static LPVOID vftable_132 = nullptr;

  void** vftable =
    *(reinterpret_cast <void ***> (*&pDev));

  if (D3D9Reset_Original != nullptr)
  {
    if ( config.render.d3d9.hook_type == 0              &&
         (! config.compatibility.d3d9.hook_reset_vftbl) &&
            config.compatibility.d3d9.rehook_reset )
    {
      //dll_log.Log (L"Rehooking IDirect3DDevice9::Present (...)");

      if (MH_OK == SK_RemoveHook (vftable [16]))
      {
        D3D9Reset_Original = nullptr;
      }

      else
      {
        dll_log.Log ( L"[   D3D9   ] Altered vftable detected, re-hooking "
                      L"IDirect3DDevice9::Reset (...)!" );
        if (MH_OK == SK_RemoveHook (vftable_16))
          D3D9Reset_Original = nullptr;
      }
    }
  }

  int hook_type =
    config.render.d3d9.hook_type;

  if (config.compatibility.d3d9.hook_reset_vftbl)
      config.render.d3d9.hook_type = 1;

  if ( D3D9Reset_Original == nullptr ||
       config.compatibility.d3d9.rehook_reset )
  {
    D3D9_INTERCEPT ( &pDev, 16,
                     "IDirect3DDevice9::Reset",
                      D3D9Reset_Override,
                      D3D9Reset_Original,
                      D3D9Reset_pfn );
    vftable_16 = vftable [16];
  }

  config.render.d3d9.hook_type = hook_type;


  if (! config.apis.d3d9ex.hook)
    return;

  //
  // D3D9Ex Specific Stuff
  //

  CComPtr <IDirect3DDevice9Ex> pDevEx = nullptr;

  if (SUCCEEDED ((static_cast <IUnknown *> (pDev))->QueryInterface <IDirect3DDevice9Ex> (&pDevEx)))
  {
    vftable =
      *( reinterpret_cast <void ***> (*&pDevEx) );

    if (D3D9ResetEx_Original != nullptr)
    {
      if ( config.render.d3d9.hook_type == 0              &&
           (! config.compatibility.d3d9.hook_reset_vftbl) &&
              config.compatibility.d3d9.rehook_reset )
      {
        //dll_log.Log (L"Rehooking IDirect3DDevice9Ex::ResetEx (...)");

        if (MH_OK == SK_RemoveHook (vftable [132]))
        {
          D3D9ResetEx_Original = nullptr;
        }

        else
        {
          dll_log.Log ( L"[   D3D9   ] Altered vftable detected, re-hooking "
                        L"IDirect3DDevice9Ex::ResetEx (...)!" );
          if (MH_OK == SK_RemoveHook (vftable_132))
            D3D9ResetEx_Original = nullptr;
        }
      }
    }

    if (config.compatibility.d3d9.hook_reset_vftbl)
        config.render.d3d9.hook_type = 1;

    if ( D3D9ResetEx_Original == nullptr ||
         config.compatibility.d3d9.rehook_reset )
    {
      D3D9_INTERCEPT ( &pDevEx, 132,
                      "IDirect3DDevice9Ex::ResetEx",
                      D3D9ResetEx,
                      D3D9ResetEx_Original,
                      D3D9ResetEx_pfn );

      vftable_132 = vftable [132];
    }

    config.render.d3d9.hook_type = hook_type;
  }
}

void
WINAPI
SK_D3D9_HookPresent (IDirect3DDevice9 *pDev)
{
  static LPVOID vftable_17  = nullptr;
  static LPVOID vftable_121 = nullptr;

  void** vftable =
    *( reinterpret_cast <void ***> (*&pDev) );

  if (D3D9Present_Original != nullptr)
  {
    if ( config.render.d3d9.hook_type == 0                &&
         (! config.compatibility.d3d9.hook_present_vftbl) &&
            config.compatibility.d3d9.rehook_present )
    {
      //dll_log.Log (L"Rehooking IDirect3DDevice9::Present (...)");

      if (MH_OK == SK_RemoveHook (vftable [17]))
      {
        D3D9Present_Original = nullptr;
      }

      else
      {
        dll_log.Log ( L"[   D3D9   ] Altered vftable detected, re-hooking "
                      L"IDirect3DDevice9::Present (...)!" );
        if (MH_OK == SK_RemoveHook (vftable_17))
          D3D9Present_Original = nullptr;
      }
    }
  }

  int hook_type =
    config.render.d3d9.hook_type;

  if (config.compatibility.d3d9.hook_present_vftbl)
      config.render.d3d9.hook_type = 1;

  if ( D3D9Present_Original == nullptr ||
       config.compatibility.d3d9.rehook_present )
  {
    D3D9_INTERCEPT ( &pDev, 17,
                     "IDirect3DDevice9::Present",
                      D3D9PresentCallback,
                      D3D9Present_Original,
                      D3D9PresentDevice_pfn );

    vftable_17 = vftable [17];
  }

  config.render.d3d9.hook_type = hook_type;


  if (! config.apis.d3d9ex.hook)
    return;

  CComPtr <IDirect3DDevice9Ex> pDevEx = nullptr;

  if (SUCCEEDED (static_cast <IUnknown *> (pDev)->QueryInterface <IDirect3DDevice9Ex> (&pDevEx)))
  {
    vftable =
      *( reinterpret_cast <void ***> (*&pDevEx) );

    if (D3D9PresentEx_Original != nullptr)
    {
      if (  config.render.d3d9.hook_type == 0             &&
         (! config.compatibility.d3d9.hook_present_vftbl) &&
            config.compatibility.d3d9.rehook_present )
      {
        //dll_log.Log (L"Rehooking IDirect3DDevice9Ex::PresentEx (...)");

        if (MH_OK == SK_RemoveHook (vftable [121]))
        {
          D3D9PresentEx_Original = nullptr;
        }

        else
        {
          dll_log.Log ( L"[   D3D9   ] Altered vftable detected, re-hooking "
                        L"IDirect3DDevice9Ex::PresentEx (...)!" );
          if (MH_OK == SK_RemoveHook (vftable_121))
            D3D9PresentEx_Original = nullptr;
        }
      }
    }

    if (config.compatibility.d3d9.hook_present_vftbl)
      config.render.d3d9.hook_type = 1;

    if (D3D9PresentEx_Original == nullptr || config.compatibility.d3d9.rehook_present)
    {
      //
      // D3D9Ex Specific Stuff
      //
      D3D9_INTERCEPT ( &pDevEx, 121,
                         "IDirect3DDevice9Ex::PresentEx",
                          D3D9PresentCallbackEx,
                          D3D9PresentEx_Original,
                          D3D9PresentDeviceEx_pfn );

      vftable_121 = vftable [121];
    }

    config.render.d3d9.hook_type = hook_type;
  }
}

__declspec (noinline)
void
STDMETHODCALLTYPE
D3D9Reset_Pre ( IDirect3DDevice9      *This,
                D3DPRESENT_PARAMETERS *pPresentationParameters,
                D3DDISPLAYMODEEX      *pFullscreenDisplayMode )
{
  UNREFERENCED_PARAMETER (This);
  UNREFERENCED_PARAMETER (pFullscreenDisplayMode);

  if (InterlockedCompareExchange (&ImGui_Init, 0, 0))
  {
    ResetCEGUI_D3D9                       (nullptr);
    ImGui_ImplDX9_InvalidateDeviceObjects (pPresentationParameters);
  }
}

__declspec (noinline)
void
STDMETHODCALLTYPE
D3D9Reset_Post ( IDirect3DDevice9      *This,
                 D3DPRESENT_PARAMETERS *pPresentationParameters,
                 D3DDISPLAYMODEEX      *pFullscreenDisplayMode )
{
  UNREFERENCED_PARAMETER (pFullscreenDisplayMode);

  if (ImGui_ImplDX9_Init ( (void *)pPresentationParameters->hDeviceWindow,
                             This,
                               pPresentationParameters) )
  {
    InterlockedExchange ( &ImGui_Init, TRUE );
  }

  trigger_reset       = reset_stage_e::Clear;
  request_mode_change = mode_change_request_e::None;
}


__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9Reset_Override ( IDirect3DDevice9      *This,
                     D3DPRESENT_PARAMETERS *pPresentationParameters )
{
  if (This == nullptr || pPresentationParameters == nullptr)
    return E_NOINTERFACE;

  dll_log.Log ( L"[   D3D9   ] [!] %s (%ph, %ph) - "
                L"%s",
                L"IDirect3DDevice9::Reset", This, pPresentationParameters,
                  SK_SummarizeCaller ().c_str ()
  );

  if (InterlockedExchangeAdd (&__d3d9_ready, 0))
  {
    SK_InitWindow ( pPresentationParameters->hDeviceWindow, 
                 (! pPresentationParameters->Windowed) );

    SK_D3D9_SetFPSTarget    (      pPresentationParameters);
    SK_SetPresentParamsD3D9 (This, pPresentationParameters);
  }

#if 0
  // The texture manager built-in to SK is derived from these ...
  //   until those projects are modified to use THIS texture manager,
  //     they need special treatment.
  if ( GetModuleHandle (L"tzfix.dll") == NULL &&
       GetModuleHandle (L"tsfix.dll") == NULL ) {
    sk::d3d9::tex_mgr.reset ();
  }
#endif

  HRESULT hr = E_FAIL;

  if (InterlockedExchangeAdd (&__d3d9_ready, 0))
  {
    D3D9Reset_Pre ( This, pPresentationParameters, nullptr );
  }

  D3D9_CALL (hr, D3D9Reset_Original (This,
                                      pPresentationParameters));

  if (SUCCEEDED (hr))
  {
    if (config.compatibility.d3d9.rehook_reset)
      SK_D3D9_HookReset   (This);

    if ( config.compatibility.d3d9.rehook_present ||
         D3D9Present_Original == nullptr )
      SK_D3D9_HookPresent (This);

    if (InterlockedExchangeAdd (&__d3d9_ready, 0))
    {
      SK_SetPresentParamsD3D9 (This, pPresentationParameters);
      D3D9Reset_Post          (This, pPresentationParameters, nullptr);
    }
  }

  else
  {
    if (InterlockedExchangeAdd (&__d3d9_ready, 0))
    {
      D3D9Reset_Pre ( This, pPresentationParameters, nullptr );
    }
  }

  return hr;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9ResetEx ( IDirect3DDevice9Ex    *This,
              D3DPRESENT_PARAMETERS *pPresentationParameters,
              D3DDISPLAYMODEEX      *pFullscreenDisplayMode )
{
  if (This == nullptr || pPresentationParameters == nullptr)
    return E_NOINTERFACE;

  dll_log.Log ( L"[   D3D9   ] [!] %s (%ph, %ph, %ph) - "
                L"%s",
                  L"IDirect3DDevice9Ex::ResetEx",
                    This, pPresentationParameters, pFullscreenDisplayMode,
                      SK_SummarizeCaller ().c_str () );

  if (InterlockedExchangeAdd (&__d3d9_ready, 0))
  {
    SK_InitWindow ( pPresentationParameters->hDeviceWindow, 
                 (! pPresentationParameters->Windowed) );

    SK_D3D9_SetFPSTarget    (      pPresentationParameters, pFullscreenDisplayMode);
    SK_SetPresentParamsD3D9 (This, pPresentationParameters);
  }

  CComPtr <IDirect3DDevice9> pDev = nullptr;

  if (SUCCEEDED (static_cast <IUnknown *> (This)->QueryInterface <IDirect3DDevice9> (&pDev)))
  {
    if (config.compatibility.d3d9.rehook_reset)
      SK_D3D9_HookReset   (pDev);

    if (config.compatibility.d3d9.rehook_present)
      SK_D3D9_HookPresent (pDev);

    pDev.Release ();
  }

  if (InterlockedExchangeAdd (&__d3d9_ready, 0))
  {
    D3D9Reset_Pre ( This, pPresentationParameters, pFullscreenDisplayMode );
  }

  HRESULT    hr = E_FAIL;
  D3D9_CALL (hr, D3D9ResetEx_Original ( This,
                                          pPresentationParameters,
                                            pFullscreenDisplayMode ));

  if (SUCCEEDED (hr))
  {
    if (InterlockedExchangeAdd (&__d3d9_ready, 0))
    {
      SK_SetPresentParamsD3D9 (This, pPresentationParameters);
      D3D9Reset_Post          (This, pPresentationParameters, pFullscreenDisplayMode);
    }
  }

  else
  {
    if (InterlockedExchangeAdd (&__d3d9_ready, 0))
    {
      D3D9Reset_Pre ( This, pPresentationParameters, pFullscreenDisplayMode );
    }
  }


  return hr;
}

__declspec (noinline)
void
STDMETHODCALLTYPE
D3D9SetGammaRamp_Override ( IDirect3DDevice9 *This,
                 _In_       UINT              iSwapChain,
                 _In_       DWORD             Flags,
                 _In_ const D3DGAMMARAMP     *pRamp )
{
  dll_log.Log (L"[   D3D9   ] SetGammaRamp (...) ");

  return
    D3D9SetGammaRamp_Original ( This,
                                  iSwapChain,
                                    Flags,
                                      pRamp );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9DrawPrimitive_Override ( IDirect3DDevice9 *This,
                             D3DPRIMITIVETYPE  PrimitiveType,
                             UINT              StartVertex,
                             UINT              PrimitiveCount )
{
  return
    D3D9DrawPrimitive_Original ( This,
                                   PrimitiveType,
                                     StartVertex,
                                       PrimitiveCount );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9DrawIndexedPrimitive_Override ( IDirect3DDevice9 *This,
                                    D3DPRIMITIVETYPE  Type,
                                    INT               BaseVertexIndex,
                                    UINT              MinVertexIndex,
                                    UINT              NumVertices,
                                    UINT              startIndex,
                                    UINT              primCount )
{
  return
    D3D9DrawIndexedPrimitive_Original ( This,
                                          Type,
                                            BaseVertexIndex,
                                              MinVertexIndex,
                                                NumVertices,
                                                  startIndex,
                                                    primCount );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9DrawPrimitiveUP_Override (       IDirect3DDevice9 *This,
                                     D3DPRIMITIVETYPE  PrimitiveType,
                                     UINT              PrimitiveCount,
                               const void             *pVertexStreamZeroData,
                                     UINT              VertexStreamZeroStride )
{
  return
    D3D9DrawPrimitiveUP_Original ( This,
                                     PrimitiveType,
                                       PrimitiveCount,
                                         pVertexStreamZeroData,
                                           VertexStreamZeroStride );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9DrawIndexedPrimitiveUP_Override (       IDirect3DDevice9 *This,
                                            D3DPRIMITIVETYPE  PrimitiveType,
                                            UINT              MinVertexIndex,
                                            UINT              NumVertices,
                                            UINT              PrimitiveCount,
                                      const void             *pIndexData,
                                            D3DFORMAT         IndexDataFormat,
                                      const void             *pVertexStreamZeroData,
                                            UINT              VertexStreamZeroStride )
{
  return
    D3D9DrawIndexedPrimitiveUP_Original (
      This,
        PrimitiveType,
          MinVertexIndex,
            NumVertices,
              PrimitiveCount,
                pIndexData,
                  IndexDataFormat,
                    pVertexStreamZeroData,
                      VertexStreamZeroStride );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetTexture_Override ( IDirect3DDevice9      *This,
                    _In_  DWORD                  Sampler,
                    _In_  IDirect3DBaseTexture9 *pTexture )
{
  return
    D3D9SetTexture_Original ( This,
                                Sampler,
                                  pTexture );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetSamplerState_Override ( IDirect3DDevice9    *This,
                               DWORD                Sampler,
                               D3DSAMPLERSTATETYPE  Type,
                               DWORD                Value )
{
  return
    D3D9SetSamplerState_Original ( This,
                                     Sampler,
                                       Type,
                                         Value );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetViewport_Override (       IDirect3DDevice9 *This,
                           CONST D3DVIEWPORT9     *pViewport )
{
  return
    D3D9SetViewport_Original ( This,
                                 pViewport );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetRenderState_Override ( IDirect3DDevice9   *This,
                              D3DRENDERSTATETYPE  State,
                              DWORD               Value )
{
  return
    D3D9SetRenderState_Original ( This,
                                    State,
                                      Value );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetVertexShaderConstantF_Override (       IDirect3DDevice9 *This,
                                              UINT              StartRegister,
                                        CONST float            *pConstantData,
                                              UINT              Vector4fCount )
{
  return
    D3D9SetVertexShaderConstantF_Original ( This,
                                              StartRegister,
                                                pConstantData,
                                                  Vector4fCount );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetPixelShaderConstantF_Override (       IDirect3DDevice9 *This,
                                             UINT              StartRegister,
                                       CONST float            *pConstantData,
                                             UINT              Vector4fCount )
{
  return
    D3D9SetPixelShaderConstantF_Original ( This,
                                             StartRegister,
                                               pConstantData,
                                                 Vector4fCount );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetPixelShader_Override ( IDirect3DDevice9      *This,
                              IDirect3DPixelShader9 *pShader )
{
  return 
    D3D9SetPixelShader_Original ( This,
                                    pShader );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetVertexShader_Override ( IDirect3DDevice9       *This,
                               IDirect3DVertexShader9 *pShader )
{
  return
    D3D9SetVertexShader_Original ( This,
                                     pShader );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetScissorRect_Override ( IDirect3DDevice9 *This,
                        const RECT             *pRect)
{
  return
    D3D9SetScissorRect_Original ( This,
                                    pRect );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateTexture_Override ( IDirect3DDevice9   *This,
                             UINT                Width,
                             UINT                Height,
                             UINT                Levels,
                             DWORD               Usage,
                             D3DFORMAT           Format,
                             D3DPOOL             Pool,
                             IDirect3DTexture9 **ppTexture,
                             HANDLE             *pSharedHandle )
{
  return
    D3D9CreateTexture_Original ( This,
                                   Width, Height, Levels,
                                     Usage, Format, Pool,
                                       ppTexture, pSharedHandle );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateVertexBuffer_Override
(
  _In_  IDirect3DDevice9        *This,
  _In_  UINT                     Length,
  _In_  DWORD                    Usage,
  _In_  DWORD                    FVF,
  _In_  D3DPOOL                  Pool,
  _Out_ IDirect3DVertexBuffer9 **ppVertexBuffer,
  _In_  HANDLE                  *pSharedHandle )
{
  return
    D3D9CreateVertexBuffer_Original ( This,
                                        Length, Usage,
                                        FVF,    Pool,
                                          ppVertexBuffer,
                                           pSharedHandle );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetStreamSource_Override
(
  IDirect3DDevice9       *This,
  UINT                    StreamNumber,
  IDirect3DVertexBuffer9 *pStreamData,
  UINT                    OffsetInBytes,
  UINT                    Stride )
{
  return
    D3D9SetStreamSource_Original ( This,
                                     StreamNumber,
                                       pStreamData,
                                         OffsetInBytes,
                                           Stride );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetStreamSourceFreq_Override
(
  IDirect3DDevice9 *This,
  UINT              StreamNumber,
  UINT              FrequencyParameter )
{
  return 
    D3D9SetStreamSourceFreq_Original ( This,
                                         StreamNumber,
                                           FrequencyParameter );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetFVF_Override ( IDirect3DDevice9 *This,
                      DWORD             FVF )
{
  return
    D3D9SetFVF_Original ( This,
                            FVF );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetVertexDeclaration_Override ( IDirect3DDevice9            *This,
                                    IDirect3DVertexDeclaration9 *pDecl )
{
  return
    D3D9SetVertexDeclaration_Original ( This,
                                          pDecl );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateVertexDeclaration_Override
(
        IDirect3DDevice9             *This,
  CONST D3DVERTEXELEMENT9            *pVertexElements,
        IDirect3DVertexDeclaration9 **ppDecl )
{
  return
    D3D9CreateVertexDeclaration_Original ( This,
                                             pVertexElements,
                                               ppDecl );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateRenderTarget_Override (IDirect3DDevice9     *This,
                                 UINT                  Width,
                                 UINT                  Height,
                                 D3DFORMAT             Format,
                                 D3DMULTISAMPLE_TYPE   MultiSample,
                                 DWORD                 MultisampleQuality,
                                 BOOL                  Lockable,
                                 IDirect3DSurface9   **ppSurface,
                                 HANDLE               *pSharedHandle)
{
  return
    D3D9CreateRenderTarget_Original ( This,
                                        Width, Height,
                                          Format,
                                            MultiSample, MultisampleQuality,
                                              Lockable,
                                                ppSurface, pSharedHandle );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateDepthStencilSurface_Override ( IDirect3DDevice9     *This,
                                         UINT                  Width,
                                         UINT                  Height,
                                         D3DFORMAT             Format,
                                         D3DMULTISAMPLE_TYPE   MultiSample,
                                         DWORD                 MultisampleQuality,
                                         BOOL                  Discard,
                                         IDirect3DSurface9   **ppSurface,
                                         HANDLE               *pSharedHandle )
{
  return
    D3D9CreateDepthStencilSurface_Original ( This,
                                               Width, Height,
                                                 Format,
                                                   MultiSample, MultisampleQuality,
                                                     Discard,
                                                       ppSurface, pSharedHandle );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetRenderTarget_Override ( IDirect3DDevice9  *This,
                               DWORD              RenderTargetIndex,
                               IDirect3DSurface9 *pRenderTarget )
{
  return
    D3D9SetRenderTarget_Original ( This,
                                     RenderTargetIndex,
                                       pRenderTarget );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetDepthStencilSurface_Override ( IDirect3DDevice9  *This,
                                      IDirect3DSurface9 *pNewZStencil )
{
  return
    D3D9SetDepthStencilSurface_Original ( This,
                                            pNewZStencil );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9UpdateTexture_Override ( IDirect3DDevice9      *This,
                             IDirect3DBaseTexture9 *pSourceTexture,
                             IDirect3DBaseTexture9 *pDestinationTexture )
{
  return
    D3D9UpdateTexture_Original ( This,
                                   pSourceTexture,
                                     pDestinationTexture );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9StretchRect_Override (      IDirect3DDevice9    *This,
                                IDirect3DSurface9   *pSourceSurface,
                          const RECT                *pSourceRect,
                                IDirect3DSurface9   *pDestSurface,
                          const RECT                *pDestRect,
                                D3DTEXTUREFILTERTYPE Filter )
{
  return
    D3D9StretchRect_Original ( This,
                                 pSourceSurface,
                                 pSourceRect,
                                   pDestSurface,
                                   pDestRect,
                                     Filter );
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9SetCursorPosition_Override (      IDirect3DDevice9 *This,
                                 _In_ INT               X,
                                 _In_ INT               Y,
                                 _In_ DWORD             Flags )
{
  return
    D3D9SetCursorPosition_Original ( This,
                                       X,
                                         Y,
                                           Flags );
}

//
// Returns true if x1 != x2, but x1 is within (tolerance)-many units of x2
//
bool
SK_DiscontEpsilon (int x1, int x2, int tolerance)
{
  if (x1 == x2)
    return false;

  if ( x1 <= (x2 + tolerance) &&
       x1 >= (x2 - tolerance) )
    return true;

  return false;
}

__declspec (noinline)
D3DPRESENT_PARAMETERS*
WINAPI
SK_SetPresentParamsD3D9 (IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pparams)
{
  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if (InterlockedExchangeAdd (&__d3d9_ready, 0))
  {
    if (pparams != nullptr)
    {
      extern HWND hWndRender;

      //
      // Initialize ImGui for D3D9 games
      //
      if ( ImGui_ImplDX9_Init ( static_cast <void *> (pparams->hDeviceWindow),
                                  pDevice,
                                    pparams )
         )
      {
        InterlockedExchange ( &ImGui_Init, TRUE );
      }


      if (hWndRender == nullptr || (! IsWindow (hWndRender)))
      {
        hWndRender  =  pparams->hDeviceWindow != nullptr ?
                            pparams->hDeviceWindow :
                                     hWndRender;
      }

      bool switch_to_fullscreen = config.display.force_fullscreen  ||
                                 ( (! rb.fullscreen_exclusive)  && 
                                      request_mode_change       ==   mode_change_request_e::Fullscreen );

      bool switch_to_windowed   = config.display.force_windowed    ||
                                 (    rb.fullscreen_exclusive   && 
                                      request_mode_change       ==   mode_change_request_e::Windowed   );

      if (switch_to_fullscreen)
      {
        HMONITOR hMonitor =
          MonitorFromWindow ( game_window.hWnd,
                                MONITOR_DEFAULTTONEAREST );

        MONITORINFO mi  = { };
        mi.cbSize       = sizeof (mi);
        GetMonitorInfo (hMonitor, &mi);

        pparams->Windowed                   = FALSE;
        pparams->BackBufferCount            = 1;
        pparams->EnableAutoDepthStencil     = true;
        pparams->FullScreen_RefreshRateInHz = 60;

        if (pparams->BackBufferWidth  < 512)
            pparams->BackBufferWidth  =      ( mi.rcMonitor.right  - mi.rcMonitor.left );
        if (pparams->BackBufferHeight < 256)
            pparams->BackBufferHeight =      ( mi.rcMonitor.bottom - mi.rcMonitor.top  );
      }

      else if (switch_to_windowed)
      {
        if (pparams->hDeviceWindow)
        {
          pparams->Windowed                   = TRUE;
          pparams->FullScreen_RefreshRateInHz = 0;
        }

        else
        {
          dll_log.Log (L"[  D3D9  ] Could not force windowed mode, game has no device window?!");
        }
      }

      //
      //  <-(+] Forced Borderless Window [+)->
      //     ------------------------------
      //        -> ( NOT FULLSCREEN ) <-
      //
      if (pparams->Windowed && config.window.borderless && (! config.window.fullscreen))
      {
        //  <- @  [No Resolution Override]  @ ->
        //
        if (config.window.res.override.isZero ())
        {
          RECT wnd_rect =
            *SK_GetGameRect ();

          int x_dlg = SK_GetSystemMetrics (SM_CXDLGFRAME);
          int y_dlg = SK_GetSystemMetrics (SM_CYDLGFRAME);
          int title = SK_GetSystemMetrics (SM_CYCAPTION );

          if ( SK_DiscontEpsilon ( pparams->BackBufferWidth,
                                     ( wnd_rect.right  - wnd_rect.left ),
                                     (       2 * x_dlg + 1             ) )

                                 ||

               SK_DiscontEpsilon ( pparams->BackBufferHeight,
                                     ( wnd_rect.bottom - wnd_rect.top  ),
                                     (       2 * y_dlg + title + 1     ) )
             )
          {
            pparams->BackBufferWidth  = ( wnd_rect.right  - wnd_rect.left );
            pparams->BackBufferHeight = ( wnd_rect.bottom - wnd_rect.top  );

            dll_log.Log ( L"[Window Mgr] Border Compensated Resolution ==> (%lu x %lu)",
                            pparams->BackBufferWidth,
                              pparams->BackBufferHeight );
          }
        }

        //  <- @  [OVERRIDE Resolution]  @ ->
        //
        else
        {
          pparams->BackBufferWidth  = config.window.res.override.x;
          pparams->BackBufferHeight = config.window.res.override.y;
        }
      }


      extern GetClientRect_pfn    GetClientRect_Original;

             RECT                 client = {        };
      GetClientRect_Original ( pparams->hDeviceWindow,
                                 &client );


      //  Non-Zero Values for Backbuffer Width / Height
      //  =============================================
      //
      //   An override may be necessary; at the very least we will make
      //     note of the explicit values provided by the game.
      //
      //
      //  NOTE:    If (Zero, Zero) is supplied for (Width, Height) -- it is a
      // ~~~~~~~     special-case indicating to Windows that the client rect.
      //               determines SwapChain resolution
      //
      //     -=> * The client rectangle can be spoofed by Special K even +
      //         +   to the D3D9 Runtime itself !                        * <=-
      //
      //
      //    -------------------------------------------------------------------
      //    (  The 0x0 case is one we have to consider carefully in order     )
      //    (    to figure out how the game is designed to behave vis-a-vis.  )
      //    (      resolution scaling if the window is resized.               )
      //
      if (pparams->BackBufferWidth != 0 && pparams->BackBufferHeight != 0)
      {
        /* User wants an override, so let's get down to brass tacks... */
        if (! config.window.res.override.isZero ())
        {
          pparams->BackBufferWidth  = config.window.res.override.x;
          pparams->BackBufferHeight = config.window.res.override.y;
        }

        /* If this is Zero, we need to actually create the render device / swapchain and
             then get the value Windows assigned us... */
        SK_SetWindowResX (pparams->BackBufferWidth);
        SK_SetWindowResY (pparams->BackBufferHeight);
      }

      // Implicit Resolution
      //
      //  
      else
      {
        // If this is zero, we need to actually create the render device / swapchain and
        //   then get the value Windows assigned us...
        SK_SetWindowResX (client.right  - client.left);
        SK_SetWindowResY (client.bottom - client.top);;
      }

      // Range Restrict to prevent D3DERR_INVALID_CALL
      if (pparams->PresentationInterval > 16)
          pparams->PresentationInterval = 0;

      if (pparams->Windowed)
      {
        //SetWindowPos_Original ( hWndRender,
                                  //HWND_TOP,
                                    //0, 0,
                                      //pparams->BackBufferWidth, pparams->BackBufferHeight,
                                        //SWP_NOZORDER | SWP_NOSENDCHANGING );
      }

      else if (switch_to_fullscreen && (! pparams->Windowed))
      {
        if ( SetWindowLongPtrW_Original == nullptr ||
             GetWindowLongPtrW_Original == nullptr )
        {
          SetWindowLongPtrW (pparams->hDeviceWindow, GWL_EXSTYLE, (GetWindowLongPtrW (pparams->hDeviceWindow, GWL_EXSTYLE) & ~(WS_EX_TOPMOST)) | (WS_EX_APPWINDOW));
          SetWindowPos      (pparams->hDeviceWindow, HWND_TOP, 0, 0, 0, 0, SWP_NOSENDCHANGING | SWP_NOMOVE             | SWP_NOSIZE     | SWP_DEFERERASE |
                                                                           SWP_NOCOPYBITS     | SWP_ASYNCWINDOWPOS     | SWP_SHOWWINDOW | SWP_NOREPOSITION );
          SK_InstallWindowHook (pparams->hDeviceWindow);
        }

        else
        {
          SetWindowLongPtrW_Original (game_window.hWnd, GWL_EXSTYLE, (GetWindowLongPtrW_Original (game_window.hWnd, GWL_EXSTYLE) & ~(WS_EX_TOPMOST)) | (WS_EX_APPWINDOW));
          SetWindowPos_Original      (game_window.hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSENDCHANGING | SWP_NOMOVE         | SWP_NOSIZE     | SWP_DEFERERASE |
                                                                              SWP_NOCOPYBITS     | SWP_ASYNCWINDOWPOS | SWP_SHOWWINDOW | SWP_NOREPOSITION );
        }
      }

      memcpy ( &g_D3D9PresentParams,
                pparams,
                  sizeof D3DPRESENT_PARAMETERS );

      rb.fullscreen_exclusive = (! pparams->Windowed);
    }
  }

  return pparams;
}

__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9CreateDeviceEx_Override ( IDirect3D9Ex           *This,
                              UINT                    Adapter,
                              D3DDEVTYPE              DeviceType,
                              HWND                    hFocusWindow,
                              DWORD                   BehaviorFlags,
                              D3DPRESENT_PARAMETERS  *pPresentationParameters,
                              D3DDISPLAYMODEEX       *pFullscreenDisplayMode,
                              IDirect3DDevice9Ex    **ppReturnedDeviceInterface )
{
  dll_log.Log ( L"[   D3D9   ] [!] %s (%ph, %lu, %lu, %ph, 0x%04X, %ph, %ph, %ph) - "
                L"%s",
                L"IDirect3D9Ex::CreateDeviceEx",
                  This, Adapter, (DWORD)DeviceType,
                    hFocusWindow, BehaviorFlags, pPresentationParameters,
                      pFullscreenDisplayMode, ppReturnedDeviceInterface,
                        SK_SummarizeCaller ().c_str () );

  HRESULT ret = E_FAIL;

  // Don't do this for the dummy context we create during init.
  if (InterlockedExchangeAdd (&__d3d9_ready, 0))
  {
    SK_D3D9_SetFPSTarget       (pPresentationParameters, pFullscreenDisplayMode);
    SK_D3D9_FixUpBehaviorFlags (BehaviorFlags);
    SK_SetPresentParamsD3D9    ( nullptr,
                                   pPresentationParameters );
  }

  if (config.display.force_windowed)
    hFocusWindow = pPresentationParameters->hDeviceWindow;

  D3D9_CALL ( ret, D3D9CreateDeviceEx_Original ( This,
                                                   Adapter,
                                                     DeviceType,
                                                       hFocusWindow,
                                                         BehaviorFlags,
                                                           pPresentationParameters,
                                                             pFullscreenDisplayMode,
                                                               ppReturnedDeviceInterface ) );

  if (! SUCCEEDED (ret))
    return ret;

  if (hFocusWindow != nullptr)
    SK_InstallWindowHook (hFocusWindow);

  CComPtr <IDirect3DSwapChain9> pSwapChain = nullptr;

  // For games that present frames using the actual Swapchain...
  //
  if (SUCCEEDED ((*ppReturnedDeviceInterface)->GetSwapChain (0, &pSwapChain)))
  {
    D3D9_INTERCEPT ( &pSwapChain, 3,
                     "IDirect3DSwapChain9::Present",
                     D3D9PresentSwapCallback, D3D9PresentSwap_Original,
                     D3D9PresentSwapChain_pfn );
  }

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 3,
                      "IDirect3DDevice9::TestCooperativeLevel",
                      D3D9TestCooperativeLevel_Override,
                      D3D9TestCooperativeLevel_Original,
                      TestCooperativeLevel_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 11,
                      "IDirect3DDevice9::SetCursorPosition",
                      D3D9SetCursorPosition_Override,
                      D3D9SetCursorPosition_Original,
                      SetCursorPosition_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 13,
                      "IDirect3DDevice9::CreateAdditionalSwapChain",
                      D3D9CreateAdditionalSwapChain_Override,
                      D3D9CreateAdditionalSwapChain_Original,
                      CreateAdditionalSwapChain_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 21,
                      "IDirect3DDevice9::SetGammaRamp",
                      D3D9SetGammaRamp_Override,
                      D3D9SetGammaRamp_Original,
                      SetGammaRamp_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 23,
                      "IDirect3DDevice9::CreateTexture",
                      D3D9CreateTexture_Override,
                      D3D9CreateTexture_Original,
                      CreateTexture_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 26,
                      "IDirect3DDevice9::CreateVertexBuffer",
                      D3D9CreateVertexBuffer_Override,
                      D3D9CreateVertexBuffer_Original,
                      CreateVertexBuffer_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 28,
                      "IDirect3DDevice9::CreateRenderTarget",
                      D3D9CreateRenderTarget_Override,
                      D3D9CreateRenderTarget_Original,
                      CreateRenderTarget_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 29,
                      "IDirect3DDevice9::CreateDepthStencilSurface",
                      D3D9CreateDepthStencilSurface_Override,
                      D3D9CreateDepthStencilSurface_Original,
                      CreateDepthStencilSurface_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 31,
                      "IDirect3DDevice9::UpdateTexture",
                      D3D9UpdateTexture_Override,
                      D3D9UpdateTexture_Original,
                      UpdateTexture_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 34,
                      "IDirect3DDevice9::StretchRect",
                      D3D9StretchRect_Override,
                      D3D9StretchRect_Original,
                      StretchRect_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 37,
                      "IDirect3DDevice9::SetRenderTarget",
                      D3D9SetRenderTarget_Override,
                      D3D9SetRenderTarget_Original,
                      SetRenderTarget_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 39,
                   "IDirect3DDevice9::SetDepthStencilSurface",
                    D3D9SetDepthStencilSurface_Override,
                    D3D9SetDepthStencilSurface_Original,
                    SetDepthStencilSurface_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface,
                   41,
                   "IDirect3DDevice9::BeginScene",
                    D3D9BeginScene_Override,
                    D3D9BeginScene_Original,
                    BeginScene_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface,
                   42,
                   "IDirect3DDevice9::EndScene",
                    D3D9EndScene_Override,
                    D3D9EndScene_Original,
                    EndScene_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 47,
                   "IDirect3DDevice9::SetViewport",
                    D3D9SetViewport_Override,
                    D3D9SetViewport_Original,
                    SetViewport_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 57,
                   "IDirect3DDevice9::SetRenderState",
                    D3D9SetRenderState_Override,
                    D3D9SetRenderState_Original,
                    SetRenderState_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 65,
                   "IDirect3DDevice9::SetTexture",
                    D3D9SetTexture_Override,
                    D3D9SetTexture_Original,
                    SetTexture_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 69,
                   "IDirect3DDevice9::SetSamplerState",
                    D3D9SetSamplerState_Override,
                    D3D9SetSamplerState_Original,
                    SetSamplerState_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 75,
                   "IDirect3DDevice9::SetScissorRect",
                    D3D9SetScissorRect_Override,
                    D3D9SetScissorRect_Original,
                    SetScissorRect_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 81,
                   "IDirect3DDevice9::DrawPrimitive",
                    D3D9DrawPrimitive_Override,
                    D3D9DrawPrimitive_Original,
                    DrawPrimitive_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 82,
                   "IDirect3DDevice9::DrawIndexedPrimitive",
                    D3D9DrawIndexedPrimitive_Override,
                    D3D9DrawIndexedPrimitive_Original,
                    DrawIndexedPrimitive_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 83,
                   "IDirect3DDevice9::DrawPrimitiveUP",
                    D3D9DrawPrimitiveUP_Override,
                    D3D9DrawPrimitiveUP_Original,
                    DrawPrimitiveUP_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 84,
                   "IDirect3DDevice9::DrawIndexedPrimitiveUP",
                    D3D9DrawIndexedPrimitiveUP_Override,
                    D3D9DrawIndexedPrimitiveUP_Original,
                    DrawIndexedPrimitiveUP_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 86,
                   "IDirect3DDevice9::CreateVertexDeclaration",
                    D3D9CreateVertexDeclaration_Override,
                    D3D9CreateVertexDeclaration_Original,
                    CreateVertexDeclaration_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 87,
                   "IDirect3DDevice9::SetVertexDeclaration",
                    D3D9SetVertexDeclaration_Override,
                    D3D9SetVertexDeclaration_Original,
                    SetVertexDeclaration_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 89,
                   "IDirect3DDevice9::SetFVF",
                    D3D9SetFVF_Override,
                    D3D9SetFVF_Original,
                    SetFVF_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 92,
                   "IDirect3DDevice9::SetVertexShader",
                    D3D9SetVertexShader_Override,
                    D3D9SetVertexShader_Original,
                    SetVertexShader_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 94,
                   "IDirect3DDevice9::SetSetVertexShaderConstantF",
                    D3D9SetVertexShaderConstantF_Override,
                    D3D9SetVertexShaderConstantF_Original,
                    SetVertexShaderConstantF_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 100,
                   "IDirect3DDevice9::SetStreamSource",
                    D3D9SetStreamSource_Override,
                    D3D9SetStreamSource_Original,
                    SetStreamSource_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 102,
                   "IDirect3DDevice9::SetStreamSourceFreq",
                    D3D9SetStreamSourceFreq_Override,
                    D3D9SetStreamSourceFreq_Original,
                    SetStreamSourceFreq_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 107,
                   "IDirect3DDevice9::SetPixelShader",
                    D3D9SetPixelShader_Override,
                    D3D9SetPixelShader_Original,
                    SetPixelShader_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 109,
                   "IDirect3DDevice9::SetPixelShaderConstantF",
                    D3D9SetPixelShaderConstantF_Override,
                    D3D9SetPixelShaderConstantF_Original,
                    SetPixelShaderConstantF_pfn );

  SK_SetPresentParamsD3D9 (*ppReturnedDeviceInterface, pPresentationParameters);
  SK_D3D9_HookPresent     (*ppReturnedDeviceInterface);

  //if (InterlockedExchangeAdd (&__d3d9_ready, 0))
    //(*ppReturnedDeviceInterface)->ResetEx (pPresentationParameters, nullptr);

  if (InterlockedExchangeAdd (&__d3d9_ready, 0))
  {
    ResetCEGUI_D3D9 (nullptr);

    //
    // Initialize ImGui for D3D9 games
    //
    if ( ImGui_ImplDX9_Init ( (void *)pPresentationParameters->hDeviceWindow,
                                    *ppReturnedDeviceInterface,
                                     pPresentationParameters )
       )
    {
      InterlockedExchange ( &ImGui_Init, TRUE );
    }
    //ResetCEGUI_D3D9 (*ppReturnedDeviceInterface);
  }

  return ret;
}

HRESULT
__declspec (noinline)
WINAPI
D3D9CreateDevice_Override ( IDirect3D9*            This,
                            UINT                   Adapter,
                            D3DDEVTYPE             DeviceType,
                            HWND                   hFocusWindow,
                            DWORD                  BehaviorFlags,
                            D3DPRESENT_PARAMETERS* pPresentationParameters,
                            IDirect3DDevice9**     ppReturnedDeviceInterface )
{
  dll_log.Log ( L"[   D3D9   ] [!] %s (%ph, %lu, %lu, %ph, 0x%04X, %ph, %ph) - "
                L"%s",
                  L"IDirect3D9::CreateDevice", This, Adapter, (DWORD)DeviceType,
                    hFocusWindow, BehaviorFlags, pPresentationParameters,
                      ppReturnedDeviceInterface,
                        SK_SummarizeCaller ().c_str () );

  HRESULT ret = E_FAIL;

  if (InterlockedExchangeAdd (&__d3d9_ready, 0))
  {
    SK_D3D9_SetFPSTarget       (pPresentationParameters);
    SK_D3D9_FixUpBehaviorFlags (BehaviorFlags);

    SK_SetPresentParamsD3D9    ( nullptr,
                                   pPresentationParameters );
  }

  if (config.display.force_windowed)
    hFocusWindow = pPresentationParameters->hDeviceWindow;

  D3D9_CALL ( ret, D3D9CreateDevice_Original ( This, Adapter,
                                                 DeviceType,
                                                   hFocusWindow,
                                                     BehaviorFlags,
                                                       pPresentationParameters,
                                                         ppReturnedDeviceInterface ) );

  // Do not attempt to do vtable override stuff if this failed,
  //   that will cause an immediate crash! Instead log some information that
  //     might help diagnose the problem.
  if (! SUCCEEDED (ret))
  {
    if (pPresentationParameters != nullptr)
    {
      dll_log.LogEx (true,
                L"[   D3D9   ]  SwapChain Settings:   Res=(%ux%u), Format=%04i, "
                                        L"Count=%lu - "
                                        L"SwapEffect: 0x%02X, Flags: 0x%04X,"
                                        L"AutoDepthStencil: %s "
                                        L"PresentationInterval: %u\n",
                   pPresentationParameters->BackBufferWidth,
                   pPresentationParameters->BackBufferHeight,
                   pPresentationParameters->BackBufferFormat,
                   pPresentationParameters->BackBufferCount,
                   pPresentationParameters->SwapEffect,
                   pPresentationParameters->Flags,
                   pPresentationParameters->EnableAutoDepthStencil ? L"true" :
                                                                     L"false",
                   pPresentationParameters->PresentationInterval);

      if (! pPresentationParameters->Windowed)
      {
        dll_log.LogEx (true,
                L"[   D3D9   ]  Fullscreen Settings:  Refresh Rate: %u\n",
                   pPresentationParameters->FullScreen_RefreshRateInHz);
        dll_log.LogEx (true,
                L"[   D3D9   ]  Multisample Settings: Type: %X, Quality: %u\n",
                   pPresentationParameters->MultiSampleType,
                   pPresentationParameters->MultiSampleQuality);
      }
    }

    return ret;
  }

  if (hFocusWindow != nullptr)
    SK_InstallWindowHook (hFocusWindow);

  CComPtr <IDirect3DSwapChain9> pSwapChain = nullptr;

  if (SUCCEEDED ((*ppReturnedDeviceInterface)->GetSwapChain (0, &pSwapChain)))
  {
    D3D9_INTERCEPT ( &pSwapChain, 3,
                     "IDirect3DSwapChain9::Present",
                     D3D9PresentSwapCallback, D3D9PresentSwap_Original,
                     D3D9PresentSwapChain_pfn );
  }

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 3,
                      "IDirect3DDevice9::TestCooperativeLevel",
                      D3D9TestCooperativeLevel_Override,
                      D3D9TestCooperativeLevel_Original,
                      TestCooperativeLevel_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 11,
                      "IDirect3DDevice9::SetCursorPosition",
                      D3D9SetCursorPosition_Override,
                      D3D9SetCursorPosition_Original,
                      SetCursorPosition_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 13,
                      "IDirect3DDevice9::CreateAdditionalSwapChain",
                      D3D9CreateAdditionalSwapChain_Override,
                      D3D9CreateAdditionalSwapChain_Original,
                      CreateAdditionalSwapChain_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 21,
                      "IDirect3DDevice9::SetGammaRamp",
                      D3D9SetGammaRamp_Override,
                      D3D9SetGammaRamp_Original,
                      SetGammaRamp_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 23,
                      "IDirect3DDevice9::CreateTexture",
                      D3D9CreateTexture_Override,
                      D3D9CreateTexture_Original,
                      CreateTexture_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 26,
                      "IDirect3DDevice9::CreateVertexBuffer",
                      D3D9CreateVertexBuffer_Override,
                      D3D9CreateVertexBuffer_Original,
                      CreateVertexBuffer_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 28,
                      "IDirect3DDevice9::CreateRenderTarget",
                      D3D9CreateRenderTarget_Override,
                      D3D9CreateRenderTarget_Original,
                      CreateRenderTarget_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 29,
                      "IDirect3DDevice9::CreateDepthStencilSurface",
                      D3D9CreateDepthStencilSurface_Override,
                      D3D9CreateDepthStencilSurface_Original,
                      CreateDepthStencilSurface_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 31,
                      "IDirect3DDevice9::UpdateTexture",
                      D3D9UpdateTexture_Override,
                      D3D9UpdateTexture_Original,
                      UpdateTexture_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 34,
                      "IDirect3DDevice9::StretchRect",
                      D3D9StretchRect_Override,
                      D3D9StretchRect_Original,
                      StretchRect_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 37,
                      "IDirect3DDevice9::SetRenderTarget",
                      D3D9SetRenderTarget_Override,
                      D3D9SetRenderTarget_Original,
                      SetRenderTarget_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 39,
                   "IDirect3DDevice9::SetDepthStencilSurface",
                    D3D9SetDepthStencilSurface_Override,
                    D3D9SetDepthStencilSurface_Original,
                    SetDepthStencilSurface_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface,
                   41,
                   "IDirect3DDevice9::BeginScene",
                    D3D9BeginScene_Override,
                    D3D9BeginScene_Original,
                    BeginScene_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface,
                   42,
                   "IDirect3DDevice9::EndScene",
                    D3D9EndScene_Override,
                    D3D9EndScene_Original,
                    EndScene_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 47,
                   "IDirect3DDevice9::SetViewport",
                    D3D9SetViewport_Override,
                    D3D9SetViewport_Original,
                    SetViewport_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 57,
                   "IDirect3DDevice9::SetRenderState",
                    D3D9SetRenderState_Override,
                    D3D9SetRenderState_Original,
                    SetRenderState_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 65,
                   "IDirect3DDevice9::SetTexture",
                    D3D9SetTexture_Override,
                    D3D9SetTexture_Original,
                    SetTexture_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 69,
                   "IDirect3DDevice9::SetSamplerState",
                    D3D9SetSamplerState_Override,
                    D3D9SetSamplerState_Original,
                    SetSamplerState_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 75,
                   "IDirect3DDevice9::SetScissorRect",
                    D3D9SetScissorRect_Override,
                    D3D9SetScissorRect_Original,
                    SetScissorRect_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 81,
                   "IDirect3DDevice9::DrawPrimitive",
                    D3D9DrawPrimitive_Override,
                    D3D9DrawPrimitive_Original,
                    DrawPrimitive_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 82,
                   "IDirect3DDevice9::DrawIndexedPrimitive",
                    D3D9DrawIndexedPrimitive_Override,
                    D3D9DrawIndexedPrimitive_Original,
                    DrawIndexedPrimitive_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 83,
                   "IDirect3DDevice9::DrawPrimitiveUP",
                    D3D9DrawPrimitiveUP_Override,
                    D3D9DrawPrimitiveUP_Original,
                    DrawPrimitiveUP_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 84,
                   "IDirect3DDevice9::DrawIndexedPrimitiveUP",
                    D3D9DrawIndexedPrimitiveUP_Override,
                    D3D9DrawIndexedPrimitiveUP_Original,
                    DrawIndexedPrimitiveUP_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 86,
                   "IDirect3DDevice9::CreateVertexDeclaration",
                    D3D9CreateVertexDeclaration_Override,
                    D3D9CreateVertexDeclaration_Original,
                    CreateVertexDeclaration_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 87,
                   "IDirect3DDevice9::SetVertexDeclaration",
                    D3D9SetVertexDeclaration_Override,
                    D3D9SetVertexDeclaration_Original,
                    SetVertexDeclaration_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 89,
                   "IDirect3DDevice9::SetFVF",
                    D3D9SetFVF_Override,
                    D3D9SetFVF_Original,
                    SetFVF_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 92,
                   "IDirect3DDevice9::SetVertexShader",
                    D3D9SetVertexShader_Override,
                    D3D9SetVertexShader_Original,
                    SetVertexShader_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 94,
                   "IDirect3DDevice9::SetSetVertexShaderConstantF",
                    D3D9SetVertexShaderConstantF_Override,
                    D3D9SetVertexShaderConstantF_Original,
                    SetVertexShaderConstantF_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 100,
                   "IDirect3DDevice9::SetStreamSource",
                    D3D9SetStreamSource_Override,
                    D3D9SetStreamSource_Original,
                    SetStreamSource_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 102,
                   "IDirect3DDevice9::SetStreamSourceFreq",
                    D3D9SetStreamSourceFreq_Override,
                    D3D9SetStreamSourceFreq_Original,
                    SetStreamSourceFreq_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 107,
                   "IDirect3DDevice9::SetPixelShader",
                    D3D9SetPixelShader_Override,
                    D3D9SetPixelShader_Original,
                    SetPixelShader_pfn );

  D3D9_INTERCEPT ( ppReturnedDeviceInterface, 109,
                   "IDirect3DDevice9::SetPixelShaderConstantF",
                    D3D9SetPixelShaderConstantF_Override,
                    D3D9SetPixelShaderConstantF_Original,
                    SetPixelShaderConstantF_pfn );

  SK_SetPresentParamsD3D9 (*ppReturnedDeviceInterface, pPresentationParameters);
  SK_D3D9_HookPresent     (*ppReturnedDeviceInterface);

  //if (InterlockedExchangeAdd (&__d3d9_ready, 0))
    //(*ppReturnedDeviceInterface)->Reset (pPresentationParameters);

  if (InterlockedExchangeAdd (&__d3d9_ready, 0))
  {
    ResetCEGUI_D3D9 (nullptr);
    //ResetCEGUI_D3D9 (*ppReturnedDeviceInterface);

    //
    // Initialize ImGui for D3D9 games
    //
    if ( ImGui_ImplDX9_Init ( reinterpret_cast <void *> (pPresentationParameters->hDeviceWindow),
                                                        *ppReturnedDeviceInterface,
                                                         pPresentationParameters )
       )
    {
      InterlockedExchange ( &ImGui_Init, TRUE );
    }
  }

  return ret;
}


bool tex_init = false;

__declspec (noinline)
IDirect3D9*
STDMETHODCALLTYPE
Direct3DCreate9 (UINT SDKVersion)
{
  WaitForInit_D3D9 ();
  WaitForInit      ();

  dll_log.Log ( L"[   D3D9   ] [!] %s (%lu) - "
                L"%s",
                  L"Direct3DCreate9",
                    SDKVersion,
                      SK_SummarizeCaller ().c_str () );

  bool force_d3d9ex = config.render.d3d9.force_d3d9ex;

  // Disable D3D9EX whenever GeDoSaTo is present
  if (force_d3d9ex)
  {
    if (GetModuleHandle (L"GeDoSaTo.dll"))
    {
      dll_log.Log ( L"[CompatHack] <!> Disabling D3D9Ex optimizations "
                                       "because GeDoSaTo.dll is present!" );
      force_d3d9ex = false;
    }
  }

  IDirect3D9*   d3d9    = nullptr;
  IDirect3D9Ex* pD3D9Ex = nullptr;

  if ((! force_d3d9ex) || FAILED (Direct3DCreate9Ex (SDKVersion, &pD3D9Ex)))
  {
    if (Direct3DCreate9_Import)
    {
      if (force_d3d9ex)
      {
        dll_log.Log ( L"[   D3D9   ] [!] %s (%lu) - "
          L"%s",
          L"Direct3DCreate9", SDKVersion,
            SK_SummarizeCaller ().c_str () );
      }

      d3d9 = Direct3DCreate9_Import (SDKVersion);
    }
  }

  else if (force_d3d9ex)
  {
  }

  if (d3d9 != nullptr)
  {
    // ...
  }

  return d3d9;
}

HRESULT
__declspec (noinline)
STDMETHODCALLTYPE
Direct3DCreate9Ex (_In_ UINT SDKVersion, _Out_ IDirect3D9Ex **ppD3D)
{
  WaitForInit_D3D9 ();
  WaitForInit      ();

  dll_log.Log ( L"[   D3D9   ] [!] %s (%lu, %ph) - "
                L"%s",
                  L"Direct3DCreate9Ex",
                    SDKVersion,
                      ppD3D,
                        SK_SummarizeCaller ().c_str () );

  HRESULT       hr     = E_FAIL;
  IDirect3D9Ex* d3d9ex = nullptr;

  if (Direct3DCreate9Ex_Import)
  {
    D3D9_CALL (hr, Direct3DCreate9Ex_Import (SDKVersion, &d3d9ex));

    if (SUCCEEDED (hr) && d3d9ex != nullptr)
    {
      *ppD3D = d3d9ex;
    }
  }

  return hr;
}

void
__stdcall
SK_D3D9_UpdateRenderStats (IDirect3DSwapChain9* pSwapChain, IDirect3DDevice9* pDevice)
{
  if (! ((pDevice || pSwapChain) && config.render.show))
    return;

  SK::D3D9::PipelineStatsD3D9& pipeline_stats =
    SK::D3D9::pipeline_stats_d3d9;

  CComPtr <IDirect3DDevice9> dev = pDevice;

  if (pDevice != nullptr || SUCCEEDED (pSwapChain->GetDevice (&dev)))
  {
    if (pipeline_stats.query.object != nullptr)
    {
      if (pipeline_stats.query.active)
      {
        pipeline_stats.query.object->Issue (D3DISSUE_END);
        pipeline_stats.query.active = false;
      }

      else
      {
        HRESULT hr =
          pipeline_stats.query.object->GetData (
                              &pipeline_stats.last_results,
                                sizeof D3DDEVINFO_D3D9PIPELINETIMINGS,
                                  D3DGETDATA_FLUSH );
        if (hr == S_OK)
        {
          pipeline_stats.query.object->Release ();
          pipeline_stats.query.object = nullptr;
        }
      }
    }

    else
    {
      if (SUCCEEDED (dev->CreateQuery (D3DQUERYTYPE_PIPELINETIMINGS, &pipeline_stats.query.object)))
      {
        pipeline_stats.query.object->Issue (D3DISSUE_BEGIN);
        pipeline_stats.query.active = true;
      }
    }
  }
}

std::wstring
WINAPI
SK::D3D9::getPipelineStatsDesc (void)
{
  wchar_t wszDesc [1024] = { };

  D3DDEVINFO_D3D9PIPELINETIMINGS& stats =
    pipeline_stats_d3d9.last_results;

  //
  // VERTEX SHADING
  //
  if (stats.VertexProcessingTimePercent > 0.0f)
  {
    _swprintf ( wszDesc,
                  L"  VERTEX : %5.2f%%\n",
                    stats.VertexProcessingTimePercent );
  }

  else
  {
    _swprintf ( wszDesc,
                  L"  VERTEX : <Unused>\n" );
  }

  //
  // PIXEL SHADING
  //
  if (stats.PixelProcessingTimePercent > 0.0f)
  {
    _swprintf ( wszDesc,
                  L"%s  PIXEL  : %5.2f%%\n",
                    wszDesc,
                      stats.PixelProcessingTimePercent );
  }

  else
  {
    _swprintf ( wszDesc,
                  L"%s  PIXEL  : <Unused>\n",
                    wszDesc );
  }

  //
  // OTHER
  //
  if (stats.OtherGPUProcessingTimePercent > 0.0f)
  {
    _swprintf ( wszDesc,
                  L"%s  OTHER  : %5.2f%%\n",
                    wszDesc, stats.OtherGPUProcessingTimePercent);
  }

  //
  // IDLE
  //
  if (stats.GPUIdleTimePercent > 0.0f)
  {
    _swprintf ( wszDesc,
                  L"%s  IDLE   : %5.2f%%\n",
                    wszDesc,
                      stats.GPUIdleTimePercent );
  }

  return wszDesc;
}

unsigned int
__stdcall
HookD3D9 (LPVOID user)
{
  UNREFERENCED_PARAMETER (user);

  if (! config.apis.d3d9.hook)
  {
    return 0;
  }

  const bool success = SUCCEEDED (
    CoInitializeEx (nullptr, COINIT_MULTITHREADED)
  );
  {
    CComPtr <IDirect3D9> pD3D9 = nullptr;

    pD3D9 =
      Direct3DCreate9_Import (D3D_SDK_VERSION);

    HWND hwnd = nullptr;

    if (pD3D9 != nullptr)
    {      
      hwnd =
        SK_Win32_CreateDummyWindow ();

      D3DPRESENT_PARAMETERS pparams = { };

      pparams.SwapEffect            = D3DSWAPEFFECT_COPY;
      pparams.BackBufferFormat      = D3DFMT_UNKNOWN;
      pparams.Windowed              = TRUE;
      pparams.BackBufferCount       = 1;
      pparams.hDeviceWindow         = hwnd;

      CComPtr <IDirect3DDevice9> pD3D9Dev = nullptr;

      dll_log.Log (L"[   D3D9   ]  Hooking D3D9...");

      HRESULT hr = pD3D9->CreateDevice (
                     D3DADAPTER_DEFAULT,
                       D3DDEVTYPE_NULLREF,
                         hwnd,
                           D3DCREATE_HARDWARE_VERTEXPROCESSING,
                             &pparams,
                               &pD3D9Dev );

      if (SUCCEEDED (hr))
      {
        D3D9_INTERCEPT ( &pD3D9, 16,
                        "IDirect3D9::CreateDevice",
                          D3D9CreateDevice_Override,
                          D3D9CreateDevice_Original,
                          D3D9CreateDevice_pfn );

        //vtbl (This)
        //-----------
        // 3   TestCooperativeLevel
        // 4   GetAvailableTextureMem
        // 5   EvictManagedResources
        // 6   GetDirect3D
        // 7   GetDeviceCaps
        // 8   GetDisplayMode
        // 9   GetCreationParameters
        // 10  SetCursorProperties
        // 11  SetCursorPosition
        // 12  ShowCursor
        // 13  CreateAdditionalSwapChain
        // 14  GetSwapChain
        // 15  GetNumberOfSwapChains
        // 16  Reset
        // 17  Present
        // 18  GetBackBuffer
        // 19  GetRasterStatus
        // 20  SetDialogBoxMode
        // 21  SetGammaRamp
        // 22  GetGammaRamp
        // 23  CreateTexture
        // 24  CreateVolumeTexture
        // 25  CreateCubeTexture
        // 26  CreateVertexBuffer
        // 27  CreateIndexBuffer
        // 28  CreateRenderTarget
        // 29  CreateDepthStencilSurface
        // 30  UpdateSurface
        // 31  UpdateTexture
        // 32  GetRenderTargetData
        // 33  GetFrontBufferData
        // 34  StretchRect
        // 35  ColorFill
        // 36  CreateOffscreenPlainSurface
        // 37  SetRenderTarget
        // 38  GetRenderTarget
        // 39  SetDepthStencilSurface
        // 40  GetDepthStencilSurface
        // 41  BeginScene
        // 42  EndScene
        // 43  Clear
        // 44  SetTransform
        // 45  GetTransform
        // 46  MultiplyTransform
        // 47  SetViewport
        // 48  GetViewport
        // 49  SetMaterial
        // 50  GetMaterial
        // 51  SetLight
        // 52  GetLight
        // 53  LightEnable
        // 54  GetLightEnable
        // 55  SetClipPlane
        // 56  GetClipPlane
        // 57  SetRenderState
        // 58  GetRenderState
        // 59  CreateStateBlock
        // 60  BeginStateBlock
        // 61  EndStateBlock
        // 62  SetClipStatus
        // 63  GetClipStatus
        // 64  GetTexture
        // 65  SetTexture
        // 66  GetTextureStageState
        // 67  SetTextureStageState
        // 68  GetSamplerState
        // 69  SetSamplerState
        // 70  ValidateDevice
        // 71  SetPaletteEntries
        // 72  GetPaletteEntries
        // 73  SetCurrentTexturePalette
        // 74  GetCurrentTexturePalette
        // 75  SetScissorRect
        // 76  GetScissorRect
        // 77  SetSoftwareVertexProcessing
        // 78  GetSoftwareVertexProcessing
        // 79  SetNPatchMode
        // 80  GetNPatchMode
        // 81  DrawPrimitive
        // 82  DrawIndexedPrimitive
        // 83  DrawPrimitiveUP
        // 84  DrawIndexedPrimitiveUP
        // 85  ProcessVertices
        // 86  CreateVertexDeclaration
        // 87  SetVertexDeclaration
        // 88  GetVertexDeclaration
        // 89  SetFVF
        // 90  GetFVF
        // 91  CreateVertexShader
        // 92  SetVertexShader
        // 93  GetVertexShader
        // 94  SetVertexShaderConstantF
        // 95  GetVertexShaderConstantF
        // 96  SetVertexShaderConstantI
        // 97  GetVertexShaderConstantI
        // 98  SetVertexShaderConstantB
        // 99  GetVertexShaderConstantB
        // 100 SetStreamSource
        // 101 GetStreamSource
        // 102 SetStreamSourceFreq
        // 103 GetStreamSourceFreq
        // 104 SetIndices
        // 105 GetIndices
        // 106 CreatePixelShader
        // 107 SetPixelShader
        // 108 GetPixelShader
        // 109 SetPixelShaderConstantF
        // 110 GetPixelShaderConstantF
        // 111 SetPixelShaderConstantI
        // 112 GetPixelShaderConstantI
        // 113 SetPixelShaderConstantB
        // 114 GetPixelShaderConstantB
        // 115 DrawRectPatch
        // 116 DrawTriPatch
        // 117 DeletePatch
        // 118 CreateQuery

        dll_log.Log (L"[   D3D9   ]   * Success");
      }

      else
      {
        _com_error err (hr);
        dll_log.Log ( L"[   D3D9   ]   * Failure  (%s)",
                      err.ErrorMessage () );
      }

      DestroyWindow               (hwnd);
      SK_Win32_CleanupDummyWindow (    );

      CComPtr <IDirect3D9Ex> pD3D9Ex = nullptr;

      hr = (config.apis.d3d9ex.hook) ?
        Direct3DCreate9Ex_Import (D3D_SDK_VERSION, &pD3D9Ex)
                         :
                    E_NOINTERFACE;

      hwnd = nullptr;

      if (SUCCEEDED (hr))
      {
        dll_log.Log (L"[   D3D9   ]  Hooking D3D9Ex...");
        
        hwnd    =
          SK_Win32_CreateDummyWindow ();
        pparams = { };

        pparams.SwapEffect            = D3DSWAPEFFECT_FLIPEX;
        pparams.BackBufferFormat      = D3DFMT_UNKNOWN;
        pparams.Windowed              = TRUE;
        pparams.BackBufferCount       = 1;
        pparams.hDeviceWindow         = hwnd;

        CComPtr <IDirect3DDevice9Ex> pD3D9DevEx = nullptr;

        if ( SUCCEEDED ( pD3D9Ex->CreateDeviceEx (
                          D3DADAPTER_DEFAULT,
                            D3DDEVTYPE_NULLREF,
                               hwnd,
                                D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                  &pparams,
                                    nullptr,
                                      &pD3D9DevEx )
                      )
          )
        {
          D3D9_INTERCEPT ( &pD3D9Ex, 20,
                           "IDirect3D9Ex::CreateDeviceEx",
                            D3D9CreateDeviceEx_Override,
                            D3D9CreateDeviceEx_Original,
                            D3D9CreateDeviceEx_pfn );

          //IDirect3DDevice9Ex **ppReturnedDeviceInterface = &pD3D9DevEx;

          SK_D3D9_HookReset   (pD3D9DevEx);
          SK_D3D9_HookPresent (pD3D9DevEx);

          // Initialize stuff...
          SK_D3D9RenderBackend::getInstance ();

          dll_log.Log (L"[   D3D9   ]   * Success");
        }

        else
        {
          dll_log.Log (L"[   D3D9   ]   * Failure");
        }

        DestroyWindow               (hwnd);
        SK_Win32_CleanupDummyWindow (    );
      }

      else
      {
        SK_D3D9_HookReset   (pD3D9Dev);
        SK_D3D9_HookPresent (pD3D9Dev);

        // Initialize stuff...
        SK_D3D9RenderBackend::getInstance ();
      }

      SK_ApplyQueuedHooks (                   );
      InterlockedExchange (&__d3d9_ready, TRUE);

      if (! (SK_GetDLLRole () & DLL_ROLE::DXGI))
        SK::DXGI::StartBudgetThread_NoAdapter ();
    }
  }
  if (success)
    CoUninitialize ();

  return 0;
}

unsigned int
__stdcall
HookD3D9Ex (LPVOID user)
{
  UNREFERENCED_PARAMETER (user);

  return 0;
}

void
SK_D3D9_TriggerReset (bool temporary)
{
  trigger_reset =
    reset_stage_e::Initiate;

  if (temporary)
  {
    request_mode_change =
      SK_GetCurrentRenderBackend ().fullscreen_exclusive ?
         mode_change_request_e::Windowed :
         mode_change_request_e::Fullscreen;
  }
}






#if 0
#define _CRT_SECURE_NO_WARNINGS

#define NOMINMAX

#include "DLL_VERSION.H"
#include "imgui\imgui.h"

#include <string>
#include <vector>
#include <algorithm>

#include "config.h"
#include "command.h"


#include <atlbase.h>

extern std::wstring
SK_D3D9_FormatToStr (D3DFORMAT Format, bool include_ordinal = true);

#if 0
void
SK_D3D9_DrawFileList (bool& can_scroll)
{
  const float font_size = ImGui::GetFont ()->FontSize * ImGui::GetIO ().FontGlobalScale;

  ImGui::PushItemWidth (500.0f);

  struct enumerated_source_s
  {
    std::string            name       = "invalid";
    std::vector <uint32_t> checksums;

    struct {
      std::vector < std::pair < uint32_t, sk_d3d9_tex_record_s > >
                 records;
      uint64_t   size                 = 0ULL;
    } streaming, blocking;

    uint64_t     totalSize (void) { return streaming.size + blocking.size; };
  };

  static std::vector <enumerated_source_s>                        sources;
  static std::vector < std::pair < uint32_t, tbf_tex_record_s > > injectable;
  static std::vector < std::wstring >                             archives;
  static bool                                                     list_dirty = true;
  static int                                                      sel        = 0;

  auto EnumerateSource =
    [](int archive_no) ->
      enumerated_source_s
      {
        enumerated_source_s source;

        char szFileName [MAX_PATH] = { '\0' };

        if (archive_no != std::numeric_limits <unsigned int>::max ()) {
          sprintf (szFileName, "%ws", archives [archive_no].c_str ()); 
        }

        else strncpy (szFileName, "Regular Filesystem", MAX_PATH);

        source.name = szFileName;

        for ( auto it : injectable )
        {
          if (it.second.archive == archive_no)
          {
            switch (it.second.method)
            {
              case DontCare:
              case Streaming:
                source.streaming.records.push_back (std::make_pair (it.first, it.second));
                source.streaming.size += it.second.size;
                break;

              case Blocking:
                source.blocking.records.push_back (std::make_pair (it.first, it.second));
                source.blocking.size += it.second.size;
                break;
            }

            source.checksums.push_back (it.first);
          }
        }

        return source;
      };

  if (list_dirty)
  {
    injectable = TBF_GetInjectableTextures ();
    archives   = TBF_GetTextureArchives    ();

    sources.clear  ();

        sel = 0;
    int idx = 0;

    // First the .7z Data Sources
    for ( auto it : archives )
    {
      sources.push_back (EnumerateSource (idx++));
    }

    // Then the Straight Filesystem
    sources.push_back (EnumerateSource (std::numeric_limits <unsigned int>::max ()));

    list_dirty = false;
  }

  ImGui::PushStyleVar   (ImGuiStyleVar_ChildWindowRounding, 0.0f);
  ImGui::PushStyleColor (ImGuiCol_Border,                   ImVec4 (0.4f, 0.6f, 0.9f, 1.0f));

#define FILE_LIST_WIDTH  font_size * 20
#define FILE_LIST_HEIGHT font_size * (sources.size () + 3)

  ImGui::BeginChild ( ImGui::GetID ("Source List"),
                        ImVec2 ( FILE_LIST_WIDTH, FILE_LIST_HEIGHT ),
                          true,
                            ImGuiWindowFlags_AlwaysAutoResize );

  if (ImGui::IsWindowHovered ())
    can_scroll = false;

  auto DataSourceTooltip =
    [](int sel) ->
      void
      {
        ImGui::BeginTooltip ();
        ImGui::TextColored  (ImVec4 (0.9f, 0.7f, 0.3f, 1.f), "Data Source  (%s)", sources [sel].name.c_str ());
        ImGui::Separator    ();

        ImGui::Text ( "Total Size:                 %#5.2f MiB",
                        (double)sources [sel].totalSize () / (1024.0 * 1024.0) );
        ImGui::Text ( "Blocking Data:  %4lu File%c (%#5.2f MiB)",
                        sources [sel].blocking.records.size (),
                        sources [sel].blocking.records.size () != 1 ? 's' : ' ',
                        (double)sources [sel].blocking.size / (1024.0 * 1024.0) );
        ImGui::Text ( "Streaming Data: %4lu File%c (%#5.2f MiB)",
                        sources [sel].streaming.records.size (),
                        sources [sel].streaming.records.size () != 1 ? 's' : ' ',
                        (double)sources [sel].streaming.size / (1024.0 * 1024.0) );

        ImGui::EndTooltip    ();
      };

  if (sources.size ())
  {
    static      int last_sel = 0;
    static bool sel_changed  = false;
  
    if (sel != last_sel)
      sel_changed = true;
  
    last_sel = sel;
  
    for ( int line = 0; line < sources.size (); line++)
    {
      if (line == sel)
      {
        bool selected = true;
        ImGui::Selectable (sources [line].name.c_str (), &selected);
   
        if (sel_changed)
        {
          ImGui::SetScrollHere (0.5f); // 0.0f:top, 0.5f:center, 1.0f:bottom
          sel_changed = false;
        }
      }
   
      else
      {
        bool selected = false;

        if (ImGui::Selectable (sources [line].name.c_str (), &selected))
        {
          sel_changed = true;
          //tex_dbg_idx                 =  line;
          sel                         =  line;
          //debug_tex_id                =  textures_used_last_dump [line];
        }
      }

      if (ImGui::IsItemHovered ())
        DataSourceTooltip (line);
    }

    ImGui::EndChild      ();

    ImVec2 list_size = ImGui::GetItemRectSize ();

    ImGui::PopStyleColor ();
    ImGui::PopStyleVar   ();

    ImGui::SameLine      ();

    ImGui::BeginGroup    ();

    ImGui::PushStyleColor  (ImGuiCol_Border, ImVec4 (0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::BeginChild ( ImGui::GetID ("Texture Selection"),
                           ImVec2 (font_size * 30, list_size.y - font_size * 2),
                             true );

    if (ImGui::IsWindowHovered ())
      can_scroll = false;

    for ( auto it : sources [sel].checksums )
    {
      tbf_tex_record_s* injectable =
        TBF_GetInjectableTexture (it);

      if (injectable != nullptr) {
                    
        ImGui::TextColored ( ImVec4 (0.9f, 0.6f, 0.3f, 1.0f), " %08x    ", it );
        ImGui::SameLine    (                                                  );

        bool streaming = 
          injectable->method == Streaming;

        ImGui::TextColored ( streaming ?
                               ImVec4 ( 0.2f,  0.90f, 0.3f, 1.0f ) :
                               ImVec4 ( 0.90f, 0.3f,  0.2f, 1.0f ),
                                 "  %s    ",
                                   streaming ? "Streaming" : 
                                               " Blocking" );
        ImGui::SameLine    (                               );

        ImGui::TextColored ( ImVec4 (1.f, 1.f, 1.f, 1.f), "%#5.2f MiB  ",
                            (double)injectable->size / (1024.0 * 1024.0) );
      }
    }

    ImGui::EndChild      (   );
    ImGui::PopStyleColor ( 1 );

    if (ImGui::Button ("  Refresh Data Sources  "))
    {
      TBF_RefreshDataSources ();
      list_dirty = true;
    }

    ImGui::SameLine ();

    if (ImGui::Button ("  Reload All Textures  "))
    {
      tbf::RenderFix::TriggerReset ();
    }

    ImGui::EndGroup ();
  }

  ImGui::PopItemWidth ();
}
#endif


    struct sk_d3d9_draw_states_s
    {
      bool            has_aniso       = false; // Has he game even once set anisotropy?!
      int             max_aniso       = 4;

      D3DVIEWPORT9    vp              = { 0 };
      bool            postprocessing  = false;
      bool            fullscreen      = false;

      // Most of these states are not tracked
      DWORD           srcblend        = 0;
      DWORD           dstblend        = 0;
      DWORD           srcalpha        = 0;     // Separate Alpha Blend Eq: Src
      DWORD           dstalpha        = 0;     // Separate Alpha Blend Eq: Dst
      bool            alpha_test      = false; // Test Alpha?
      DWORD           alpha_ref       = 0;     // Value to test.
      bool            zwrite          = false; // Depth Mask

      // This one is
      bool            scissor_test    = false;

      int             last_vs_vec4    = 0; // Number of vectors in the last call to
                                          //   set vertex shader constant...

      int             draws           = 0; // Number of draw calls
      int             frames          = 0;

      bool            cegui_active    = false;
      
      bool            fullscreen_blit = false;
      bool            fix_scissor     = true;
      int             instances       = 0;

      uint32_t        current_tex  [256];
      float           viewport_off [4]  = { 0.0f }; // Most vertex shaders use this and we can
                                                    //   test the set values to determine if a draw
                                                    //     is HUD or world-space.
    } extern draw_state;                            

    struct frame_state_s
    {
      void clear (void) { pixel_shaders.clear (); vertex_shaders.clear (); vertex_buffers.dynamic.clear (); vertex_buffers.immutable.clear (); }
    
      std::unordered_set <uint32_t>                 pixel_shaders;
      std::unordered_set <uint32_t>                 vertex_shaders;

      struct {
        std::unordered_set <IDirect3DVertexBuffer9 *> dynamic;
        std::unordered_set <IDirect3DVertexBuffer9 *> immutable;
      } vertex_buffers;
    } extern last_frame;

    struct known_objects_s
    {
      void clear (void) { static_vbs.clear (); dynamic_vbs.clear (); };

      std::unordered_set <IDirect3DVertexBuffer9 *> static_vbs;
      std::unordered_set <IDirect3DVertexBuffer9 *> dynamic_vbs;
    } extern known_objs;

    struct render_target_tracking_s
    {
      void clear (void) { pixel_shaders.clear (); vertex_shaders.clear (); active = false; }

      IDirect3DBaseTexture9*        tracking_tex  = nullptr;

      std::unordered_set <uint32_t> pixel_shaders;
      std::unordered_set <uint32_t> vertex_shaders;

      bool                          active        = false;
    } extern tracked_rt;

    struct shader_tracking_s
    {
      void clear (void) {
        active    = false;
        num_draws = 0;
        used_textures.clear ();

        for (int i = 0; i < 16; i++)
          current_textures [i] = 0x00;
      }

      void use (IUnknown* pShader);

      uint32_t                      crc32        =  0x00;
      bool                          cancel_draws = false;
      bool                          clamp_coords = false;
      bool                          active       = false;
      int                           num_draws    =     0;
      std::unordered_set <uint32_t>    used_textures;
                          uint32_t  current_textures [16];

      //std::vector <IDirect3DBaseTexture9 *> samplers;

      IUnknown*                     shader_obj  = nullptr;
      ID3DXConstantTable*           ctable      = nullptr;

      struct shader_constant_s
      {
        char                Name [128];
        D3DXREGISTER_SET    RegisterSet;
        UINT                RegisterIndex;
        UINT                RegisterCount;
        D3DXPARAMETER_CLASS Class;
        D3DXPARAMETER_TYPE  Type;
        UINT                Rows;
        UINT                Columns;
        UINT                Elements;
        std::vector <shader_constant_s>
                            struct_members;
        bool                Override;
        float               Data [4]; // TEMP HACK
      };

      std::vector <shader_constant_s> constants;
    } extern tracked_vs, tracked_ps;

    struct vertex_buffer_tracking_s
    {
      void clear (void) {
        active = false; num_draws = 0; instances = 0;

        vertex_shaders.clear ();
        pixel_shaders.clear  ();
        textures.clear       ();

        for (auto it : vertex_decls) it->Release ();

        vertex_decls.clear ();
      }

      void use (void)
      {
        extern uint32_t vs_checksum, ps_checksum;

        vertex_shaders.emplace (vs_checksum);
        pixel_shaders.emplace  (ps_checksum);

        IDirect3DVertexDeclaration9* decl = nullptr;

        if (SUCCEEDED (tbf::RenderFix::pDevice->GetVertexDeclaration (&decl)))
        {
          static D3DVERTEXELEMENT9 elem_decl [MAXD3DDECLLENGTH];
          static UINT              num_elems;

          // Run through the vertex decl and figure out which samplers have texcoords,
          //   that is a pretty good indicator as to which textures are actually used...
          if (SUCCEEDED (decl->GetDeclaration (elem_decl, &num_elems)))
          {
            for ( UINT i = 0; i < num_elems; i++ )
            {
              if (elem_decl [i].Usage == D3DDECLUSAGE_TEXCOORD)
                textures.emplace (draw_state.current_tex [elem_decl [i].UsageIndex]);
            }
          }

          if (! vertex_decls.count (decl))
            vertex_decls.emplace (decl);
          else
            decl->Release ();
        }
      }

      IDirect3DVertexBuffer9*       vertex_buffer = nullptr;

      std::unordered_set <
        IDirect3DVertexDeclaration9*
      >                             vertex_decls;

      //uint32_t                      crc32        =  0x00;
      bool                          cancel_draws  = false;
      bool                          wireframe     = false;
      bool                          active        = false;
      int                           num_draws     =     0;
      int                           instanced     =     0;
      int                           instances     =     1;

      std::unordered_set <uint32_t> vertex_shaders;
      std::unordered_set <uint32_t> pixel_shaders;
      std::unordered_set <uint32_t> textures;

      std::unordered_set <IDirect3DVertexBuffer9 *>
                                    wireframes;
    } extern tracked_vb;

    struct shader_disasm_s {
      std::string header;
      std::string code;
      std::string footer;
    };

    extern std::unordered_set <IDirect3DBaseTexture9 *> ui_map_rts;
    extern bool                                         ui_map_active;
  }
}

extern std::unordered_map <uint32_t, sk_d3d9_shader_disasm_s> ps_disassembly;
extern std::unordered_map <uint32_t, sk_d3d9_shader_disasm_s> vs_disassembly;

enum class sk_d3d9_shader_class {
  Unknown = 0x00,
  Pixel   = 0x01,
  Vertex  = 0x02
};

void
SK_D3D9_LiveShaderClassView (sk_d3d9_shader_class shader_type, bool& can_scroll)
{
  ImGui::BeginGroup ();

  static float last_width = 256.0f;
  const  float font_size  = ImGui::GetFont ()->FontSize * ImGui::GetIO ().FontGlobalScale;

  struct shader_class_imp_s
  {
    std::vector <std::string> contents;
    bool                      dirty      = true;
    uint32_t                  last_sel   =    0;
    int                            sel   =   -1;
    float                     last_ht    = 256.0f;
    ImVec2                    last_min   = ImVec2 (0.0f, 0.0f);
    ImVec2                    last_max   = ImVec2 (0.0f, 0.0f);
  };

  struct {
    shader_class_imp_s vs;
    shader_class_imp_s ps;
  } static list_base;

  shader_class_imp_s*
    list    = ( shader_type == sk_d3d9_shader_class::Pixel ? &list_base.ps :
                                                         &list_base.vs );

  tbf::RenderFix::shader_tracking_s*
    tracker = ( shader_type == sk_d3d9_shader_class::Pixel ? &tracked_ps :
                                                             &tracked_vs );

  std::vector <uint32_t>
    shaders   ( shader_type == sk_d3d9_shader_class::Pixel ? last_frame.pixel_shaders.begin  () :
                                                             last_frame.vertex_shaders.begin (),
                shader_type == sk_d3d9_shader_class::Pixel ? last_frame.pixel_shaders.end    () :
                                                             last_frame.vertex_shaders.end   () );

  std::unordered_map <uint32_t, shader_disasm_s>&
    disassembly = ( shader_type == sk_d3d9_shader_class::Pixel ? ps_disassembly :
                                                                 vs_disassembly );

  const char*
    szShaderWord =  shader_type == tbf_shader_class::Pixel ? "Pixel" :
                                                             "Vertex";

  if (list->dirty)
  {
        list->sel = -1;
    int idx    =  0;
        list->contents.clear ();

    // The underlying list is unsorted for speed, but that's not at all
    //   intuitive to humans, so sort the thing when we have the RT view open.
    std::sort ( shaders.begin (),
                shaders.end   () );



    for ( auto it : shaders )
    {
      char szDesc [16] = { };

      sprintf (szDesc, "%08llx", (uintptr_t)it);

      list->contents.emplace_back (szDesc);

      if ((uint32_t)it == list->last_sel)
      {
        list->sel = idx;
        //tbf::RenderFix::tracked_rt.tracking_tex = render_textures [sel];
      }

      ++idx;
    }
  }

  if (ImGui::IsMouseHoveringRect (list->last_min, list->last_max))
  {
         if (ImGui::GetIO ().KeysDown [VK_OEM_4] && ImGui::GetIO ().KeysDownDuration [VK_OEM_4] == 0.0f) { list->sel--;  ImGui::GetIO ().WantCaptureKeyboard = true; }
    else if (ImGui::GetIO ().KeysDown [VK_OEM_6] && ImGui::GetIO ().KeysDownDuration [VK_OEM_6] == 0.0f) { list->sel++;  ImGui::GetIO ().WantCaptureKeyboard = true; }
  }

  ImGui::PushStyleVar   (ImGuiStyleVar_ChildWindowRounding, 0.0f);
  ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.9f, 0.7f, 0.5f, 1.0f));

  ImGui::BeginChild ( ImGui::GetID (szShaderWord),
                      ImVec2 ( font_size * 7.0f, std::max (font_size * 15.0f, list->last_ht)),
                        true, ImGuiWindowFlags_AlwaysAutoResize );

  if (ImGui::IsWindowHovered ())
  {
    can_scroll = false;

    ImGui::BeginTooltip ();
    ImGui::TextColored  (ImVec4 (0.9f, 0.6f, 0.2f, 1.0f), "You can cancel all render passes using the selected %s shader to disable an effect", szShaderWord);
    ImGui::Separator    ();
    ImGui::BulletText   ("Press [ while the mouse is hovering this list to select the previous shader");
    ImGui::BulletText   ("Press ] while the mouse is hovering this list to select the next shader");
    ImGui::EndTooltip   ();

         if (ImGui::GetIO ().KeysDown [VK_OEM_4] && ImGui::GetIO ().KeysDownDuration [VK_OEM_4] == 0.0f) { list->sel--;  ImGui::GetIO ().WantCaptureKeyboard = true; }
    else if (ImGui::GetIO ().KeysDown [VK_OEM_6] && ImGui::GetIO ().KeysDownDuration [VK_OEM_6] == 0.0f) { list->sel++;  ImGui::GetIO ().WantCaptureKeyboard = true; }
  }

  if (shaders.size ())
  {
    struct {
      int  last_sel    = 0;
      bool sel_changed = false;
    } static shader_state [3];

    int&  last_sel    = shader_state [(int)shader_type].last_sel;
    bool& sel_changed = shader_state [(int)shader_type].sel_changed;

    if (list->sel != last_sel)
      sel_changed = true;

    last_sel = list->sel;

    for ( int line = 0; line < shaders.size (); line++ )
    {
      if (line == list->sel)
      {
        bool selected    = true;

        ImGui::Selectable (list->contents [line].c_str (), &selected);

        if (sel_changed)
        {
          ImGui::SetScrollHere (0.5f);

          sel_changed    = false;
          list->last_sel = (uint32_t)shaders [list->sel];
          tracker->crc32 = (uint32_t)shaders [list->sel];
        }
      }

      else
      {
        bool selected    = false;

        if (ImGui::Selectable (list->contents [line].c_str (), &selected))
        {
          sel_changed    = true;
          list->sel      =  line;
          list->last_sel = (uint32_t)shaders [list->sel];
          tracker->crc32 = (uint32_t)shaders [list->sel];
        }
      }
    }
  }

  ImGui::EndChild      ();
  ImGui::PopStyleColor ();

  ImGui::SameLine      ();
  ImGui::BeginGroup    ();

  if (ImGui::IsItemHoveredRect ()) {
         if (ImGui::GetIO ().KeysDownDuration [VK_OEM_4] == 0.0f) list->sel--;
    else if (ImGui::GetIO ().KeysDownDuration [VK_OEM_6] == 0.0f) list->sel++;
  }

  if (tracker->crc32 != 0x00)
  {
    ImGui::BeginGroup ();
    ImGui::Checkbox ( shader_type == sk_d3d9_shader_class::Pixel ? "Cancel Draws Using Selected Pixel Shader" :
                                                                   "Cancel Draws Using Selected Vertex Shader", 
                        &tracker->cancel_draws );  ImGui::SameLine ();

    if (tracker->cancel_draws)
      ImGui::TextDisabled ("%lu Skipped Draw%c Last Frame (%lu textures)", tracker->num_draws, tracker->num_draws != 1 ? 's' : ' ', tracker->used_textures.size () );
    else
      ImGui::TextDisabled ("%lu Draw%c Last Frame         (%lu textures)", tracker->num_draws, tracker->num_draws != 1 ? 's' : ' ', tracker->used_textures.size () );

    ImGui::Checkbox ( shader_type == sk_d3d9_shader_class::Pixel ? "Clamp Texture Coordinates For Selected Pixel Shader" :
                                                                   "Clamp Texture Coordinates For Selected Vertex Shader",
                        &tracker->clamp_coords );

    ImGui::Separator      ();
    ImGui::EndGroup       ();

    if (ImGui::IsItemHoveredRect () && tracker->used_textures.size ())
    {
      ImGui::BeginTooltip ();

      D3DFORMAT fmt = D3DFMT_UNKNOWN;

      for ( auto it : tracker->used_textures )
      {
        ISKTextureD3D9* pTex = tbf::RenderFix::tex_mgr.getTexture (it)->d3d9_tex;
        
        if (pTex && pTex->pTex)
        {
          D3DSURFACE_DESC desc;
          if (SUCCEEDED (pTex->pTex->GetLevelDesc (0, &desc)))
          {
            fmt = desc.Format;
            ImGui::Image ( pTex->pTex, ImVec2  ( std::max (64.0f, (float)desc.Width / 16.0f),
      ((float)desc.Height / (float)desc.Width) * std::max (64.0f, (float)desc.Width / 16.0f) ),
                                       ImVec2  (0,0),             ImVec2  (1,1),
                                       ImColor (255,255,255,255), ImColor (242,242,13,255) );
          }

          ImGui::SameLine ();

          ImGui::BeginGroup ();
          ImGui::Text       ("Texture: %08lx", it);
          ImGui::Text       ("Format:  %ws",   SK_D3D9_FormatToStr (fmt).c_str ());
          ImGui::EndGroup   ();
        }
      }

      ImGui::EndTooltip ();
    }

    ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.80f, 0.80f, 1.0f, 1.0f));
    ImGui::TextWrapped    (disassembly [tracker->crc32].header.c_str ());

    ImGui::SameLine       ();
    ImGui::BeginGroup     ();
    ImGui::TreePush       ("");
    ImGui::Spacing        (); ImGui::Spacing ();
    ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.666f, 0.666f, 0.666f, 1.0f));

    char szName    [192] = { '\0' };
    char szOrdinal [64 ] = { '\0' };
    char szOrdEl   [ 96] = { '\0' };

    int  el = 0;

    ImGui::PushItemWidth (font_size * 25);

    for ( auto&& it : tracker->constants )
    {
      if (it.struct_members.size ())
      {
        ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.9f, 0.1f, 0.7f, 1.0f));
        ImGui::Text           (it.Name);
        ImGui::PopStyleColor  ();

        for ( auto&& it2 : it.struct_members )
        {
          snprintf ( szOrdinal, 64, " (%c%-3lu) ",
                        it2.RegisterSet != D3DXRS_SAMPLER ? 'c' : 's',
                          it2.RegisterIndex );
          snprintf ( szOrdEl,  96,  "%s::%lu %c", // Uniquely identify parameters that share registers
                       szOrdinal, el++, shader_type == sk_d3d9_shader_class::Pixel ? 'p' : 'v' );
          snprintf ( szName, 192, "[%s] %-24s :%s",
                       shader_type == sk_d3d9_shader_class::Pixel ? "ps" :
                                                                    "vs",
                         it2.Name, szOrdinal );

          if (it2.Type == D3DXPT_FLOAT && it2.Class == D3DXPC_VECTOR)
          {
            ImGui::Checkbox    (szName,  &it2.Override); ImGui::SameLine ();
            ImGui::InputFloat4 (szOrdEl,  it2.Data);
          }
          else {
            ImGui::TreePush (""); ImGui::TextColored (ImVec4 (0.45f, 0.75f, 0.45f, 1.0f), szName); ImGui::TreePop ();
          }
        }

        ImGui::Separator ();
      }

      else
      {
        snprintf ( szOrdinal, 64, " (%c%-3lu) ",
                     it.RegisterSet != D3DXRS_SAMPLER ? 'c' : 's',
                        it.RegisterIndex );
        snprintf ( szOrdEl,  96,  "%s::%lu %c", // Uniquely identify parameters that share registers
                       szOrdinal, el++, shader_type == tbf_shader_class::Pixel ? 'p' : 'v' );
        snprintf ( szName, 192, "[%s] %-24s :%s",
                     shader_type == sk_d3d9_shader_class::Pixel ? "ps" :
                                                                  "vs",
                         it.Name, szOrdinal );

        if (it.Type == D3DXPT_FLOAT && it.Class == D3DXPC_VECTOR)
        {
          ImGui::Checkbox    (szName,  &it.Override); ImGui::SameLine ();
          ImGui::InputFloat4 (szOrdEl,  it.Data);
        } else {
          ImGui::TreePush (""); ImGui::TextColored (ImVec4 (0.45f, 0.75f, 0.45f, 1.0f), szName); ImGui::TreePop ();
        }
      }
    }
    ImGui::PopItemWidth ();
    ImGui::TreePop      ();
    ImGui::EndGroup     ();

    ImGui::Separator      ();

    ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.99f, 0.99f, 0.01f, 1.0f));
    ImGui::TextWrapped    (disassembly [tracker->crc32].code.c_str ());

    ImGui::Separator      ();

    ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.5f, 0.95f, 0.5f, 1.0f));
    ImGui::TextWrapped    (disassembly [tracker->crc32].footer.c_str ());

    ImGui::PopStyleColor (4);
  }
  else
    tracker->cancel_draws = false;

  ImGui::EndGroup      ();

  list->last_ht    = ImGui::GetItemRectSize ().y;

  list->last_min   = ImGui::GetItemRectMin ();
  list->last_max   = ImGui::GetItemRectMax ();

  ImGui::PopStyleVar   ();
  ImGui::EndGroup      ();
}

void
TBF_LiveVertexStreamView (bool& can_scroll)
{
  static int filter_type = 0; // ALL

  ImGui::BeginGroup ();

  static float last_width = 256.0f;
  const  float font_size  = ImGui::GetFont ()->FontSize * ImGui::GetIO ().FontGlobalScale;

  struct vertex_stream_s
  {
    std::vector <std::string> contents;
    bool                      dirty      = true;
    uintptr_t                 last_sel   =    0;
    int                            sel   =   -1;
    float                     last_ht    = 256.0f;
    ImVec2                    last_min   = ImVec2 (0.0f, 0.0f);
    ImVec2                    last_max   = ImVec2 (0.0f, 0.0f);
  };

  struct {
    vertex_stream_s stream0;
  } static list_base;

  vertex_stream_s*
    list    = &list_base.stream0;

  tbf::RenderFix::vertex_buffer_tracking_s*
    tracker = &tbf::RenderFix::tracked_vb;

  std::vector <IDirect3DVertexBuffer9 *> buffers;

  switch (filter_type)
  {
    case 2:
      for (auto it : last_frame.vertex_buffers.immutable) if (it != nullptr) buffers.emplace_back (it);
      break;
    case 1:
      for (auto it : last_frame.vertex_buffers.dynamic)   if (it != nullptr) buffers.emplace_back (it);
      break;
    case 0:
      for (auto it : last_frame.vertex_buffers.immutable) if (it != nullptr) buffers.emplace_back (it);
      for (auto it : last_frame.vertex_buffers.dynamic)   if (it != nullptr) buffers.emplace_back (it);
      break;
  };

  if (list->dirty)
  {
        list->sel = -1;
    int idx    =  0;
        list->contents.clear ();

    // The underlying list is unsorted for speed, but that's not at all
    //   intuitive to humans, so sort the thing when we have the RT view open.
    std::sort ( buffers.begin (),
                buffers.end   () );



    for ( auto it : buffers )
    {
      char szDesc [16] = { };

      sprintf (szDesc, "%08llx", (uintptr_t)it);

      list->contents.emplace_back (szDesc);

      if ((uintptr_t)it == list->last_sel)
      {
        list->sel = idx;
        //tbf::RenderFix::tracked_rt.tracking_tex = render_textures [sel];
      }

      ++idx;
    }
  }

  if (ImGui::IsMouseHoveringRect (list->last_min, list->last_max))
  {
         if (ImGui::GetIO ().KeysDown [VK_OEM_4] && ImGui::GetIO ().KeysDownDuration [VK_OEM_4] == 0.0f) { list->sel--;  ImGui::GetIO ().WantCaptureKeyboard = true; }
    else if (ImGui::GetIO ().KeysDown [VK_OEM_6] && ImGui::GetIO ().KeysDownDuration [VK_OEM_6] == 0.0f) { list->sel++;  ImGui::GetIO ().WantCaptureKeyboard = true; }
  }

  ImGui::PushStyleVar   (ImGuiStyleVar_ChildWindowRounding, 0.0f);
  ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.9f, 0.7f, 0.5f, 1.0f));

  ImGui::BeginChild ( ImGui::GetID ("Stream 0"),
                      ImVec2 ( font_size * 7.0f, std::max (font_size * 15.0f, list->last_ht)),
                        true, ImGuiWindowFlags_AlwaysAutoResize );

  if (ImGui::IsWindowHovered ())
  {
    can_scroll = false;

    ImGui::BeginTooltip ();
    ImGui::TextColored  (ImVec4 (0.9f, 0.6f, 0.2f, 1.0f), "You can cancel all render passes using the selected vertex buffer to debug a model");
    ImGui::Separator    ();
    ImGui::BulletText   ("Press [ while the mouse is hovering this list to select the previous shader");
    ImGui::BulletText   ("Press ] while the mouse is hovering this list to select the next shader");
    ImGui::EndTooltip   ();

         if (ImGui::GetIO ().KeysDown [VK_OEM_4] && ImGui::GetIO ().KeysDownDuration [VK_OEM_4] == 0.0f) { list->sel--;  ImGui::GetIO ().WantCaptureKeyboard = true; }
    else if (ImGui::GetIO ().KeysDown [VK_OEM_6] && ImGui::GetIO ().KeysDownDuration [VK_OEM_6] == 0.0f) { list->sel++;  ImGui::GetIO ().WantCaptureKeyboard = true; }
  }

  if (buffers.size ())
  {
    struct {
      int  last_sel    = 0;
      bool sel_changed = false;
    } static stream [3];

    int&  last_sel    = stream [0].last_sel;
    bool& sel_changed = stream [0].sel_changed;

    if (list->sel != last_sel)
      sel_changed = true;

    last_sel = list->sel;

    for ( int line = 0; line < buffers.size (); line++ )
    {
      if (line == list->sel)
      {
        bool selected    = true;

        ImGui::Selectable (list->contents [line].c_str (), &selected);

        if (sel_changed)
        {
          ImGui::SetScrollHere (0.5f);

          sel_changed            = false;
          list->last_sel         = (uintptr_t)buffers [list->sel];
          tracker->vertex_buffer =            buffers [list->sel];
        }
      }

      else
      {
        bool selected    = false;

        if (ImGui::Selectable (list->contents [line].c_str (), &selected))
        {
          sel_changed            = true;
          list->sel              =  line;
          list->last_sel         = (uintptr_t)buffers [list->sel];
          tracker->vertex_buffer =            buffers [list->sel];
        }
      }
    }
  }

  else {
    tracker->vertex_buffer = nullptr;
  }

  ImGui::EndChild      ();
  ImGui::PopStyleColor ();

  ImGui::SameLine      ();
  ImGui::BeginGroup    ();

  if (ImGui::IsItemHoveredRect ())
  {
         if (ImGui::GetIO ().KeysDown [VK_OEM_4] && ImGui::GetIO ().KeysDownDuration [VK_OEM_4] == 0.0f) { list->sel--;  ImGui::GetIO ().WantCaptureKeyboard = true; }
    else if (ImGui::GetIO ().KeysDown [VK_OEM_6] && ImGui::GetIO ().KeysDownDuration [VK_OEM_6] == 0.0f) { list->sel++;  ImGui::GetIO ().WantCaptureKeyboard = true; }

    if ( ImGui::GetIO ().KeysDownDuration ['W'] == 0.0f &&
         ImGui::GetIO ().KeysDown [VK_CONTROL]          &&
         ImGui::GetIO ().KeysDown [VK_SHIFT]               )
    {
      ImGui::GetIO ().WantCaptureKeyboard = true;

      if (tracker->vertex_buffer != nullptr) {
        bool wireframe = tracker->wireframes.count (tracker->vertex_buffer);
        
        if (wireframe && tracker->wireframes.count (tracker->vertex_buffer))
          tracker->wireframes.erase   (tracker->vertex_buffer);
        else
          tracker->wireframes.emplace (tracker->vertex_buffer);
      }
    }
  }

  ImGui::Combo ("Vertex Buffer Filter", &filter_type, "  All   Geometry (Huge List)\0"
                                                      "Dynamic Geometry (Models)\0"
                                                      " Static Geometry (World)\0\0", 3 );

  ImGui::Checkbox ("Cancel Draws Using Selected Vertex Buffer",  &tracker->cancel_draws);  ImGui::SameLine ();

  if (tracker->cancel_draws)
    ImGui::TextDisabled ("%lu Skipped Draw%c Last Frame [%lu Instanced]", tracker->num_draws, tracker->num_draws != 1 ? 's' : ' ', tracker->instanced);
  else
    ImGui::TextDisabled ("%lu Draw%c Last Frame [%lu Instanced]        ", tracker->num_draws, tracker->num_draws != 1 ? 's' : ' ', tracker->instanced);

  ImGui::Checkbox ("Highlight Selected Vertex Buffer (Wireframe)", &tracker->wireframe);

  ImGui::Separator ();


  if ( tracker->vertex_buffer != nullptr &&
         ( tbf::RenderFix::last_frame.vertex_buffers.dynamic.count   (tracker->vertex_buffer) ||
           tbf::RenderFix::last_frame.vertex_buffers.immutable.count (tracker->vertex_buffer) ) )
  {
    bool wireframe = tracker->wireframes.count (tracker->vertex_buffer);

    extern std::wstring
    SK_D3D9_UsageToStr (DWORD dwUsage);

    D3DVERTEXBUFFER_DESC desc;
    if (SUCCEEDED (tracker->vertex_buffer->GetDesc (&desc)))
    {
      ImGui::BeginGroup   ();
      ImGui::PushStyleVar (ImGuiStyleVar_ChildWindowRounding, 20.0f);

      ImVec4 border_color = wireframe ? ImVec4 (1.0f, 0.5f, 0.5f, 1.0f) :
                              tracker->wireframe ? 
                                ImVec4 (0.5f, 0.5f, 1.0f, 1.0f) :
                                ImVec4 (0.6f, 0.6f, 0.6f, 1.0f);

      ImGui::PushStyleColor (ImGuiCol_Border, border_color);

      static int last_count = 1;

      ImGui::BeginChild     ( ImGui::GetID ("Buffer Overview"),
                              ImVec2 ( font_size * 44.0f,
                                       font_size * 8.0f +
                                       font_size * (float)last_count ),
                                true,
                                  ImGuiWindowFlags_AlwaysAutoResize );

      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), "Format:  "); ImGui::SameLine ();
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 0.4f, 1.0f), "%ws",  SK_D3D9_FormatToStr (desc.Format).c_str ());
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), "Type:    "); ImGui::SameLine ();
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 0.4f, 1.0f), "%s",  desc.Type == D3DRTYPE_VERTEXBUFFER ? "Vertex Buffer" :
                                                                  desc.Type == D3DRTYPE_INDEXBUFFER  ? "Index Buffer"  :
                                                                                                       "Unknown?!" );
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), "Usage:   "); ImGui::SameLine ();
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 0.4f, 1.0f), "%ws",  SK_D3D9_UsageToStr (desc.Usage).c_str ());
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), "Size:    "); ImGui::SameLine ();
      ImGui::TextColored (ImVec4 (1.0f, 1.0f, 0.4f, 1.0f), "%llu", desc.Size);

      last_count = 0;

      for (auto vtx_decl : tracker->vertex_decls)
      {
        ++last_count;

        static D3DVERTEXELEMENT9 elem_decl [MAXD3DDECLLENGTH];
        static UINT              num_elems;

        auto SK_D3D9_DeclTypeToStr = [](D3DDECLTYPE type) ->
          const char* 
          {
            switch (type)
            {
              case D3DDECLTYPE_FLOAT1:     return "float";    
              case D3DDECLTYPE_FLOAT2:     return "float2";
              case D3DDECLTYPE_FLOAT3:     return "float3";
              case D3DDECLTYPE_FLOAT4:     return "float4";
              case D3DDECLTYPE_D3DCOLOR:   return "D3DCOLOR";
              case D3DDECLTYPE_UBYTE4:     return "ubyte4";
              case D3DDECLTYPE_SHORT2:     return "short2";
              case D3DDECLTYPE_SHORT4:     return "short4";
              case D3DDECLTYPE_UBYTE4N:    return "ubyte4 (UNORM)";
              case D3DDECLTYPE_SHORT2N:    return "short2 (SNORM)";
              case D3DDECLTYPE_SHORT4N:    return "short2 (SNORM)";
              case D3DDECLTYPE_USHORT2N:   return "short2 (UNORM)";
              case D3DDECLTYPE_USHORT4N:   return "short4 (UNORM)";
              case D3DDECLTYPE_UDEC3:      return "udec3";
              case D3DDECLTYPE_DEC3N:      return "dec3 (NORM)";
              case D3DDECLTYPE_FLOAT16_2:  return "half2";
              case D3DDECLTYPE_FLOAT16_4:  return "half4";
              case D3DDECLTYPE_UNUSED:     return "UNUSED";
            }

            return "UNKNOWN";
          };

        auto SK_D3D9_DeclUsageToStr = [](D3DDECLUSAGE usage, int idx) ->
          const char*
          {
            static char szOut [64] = { '\0' };

            switch (usage)
            {
              case D3DDECLUSAGE_POSITION:     snprintf (szOut, 64, "POSITION     [%lu]", idx); break;
              case D3DDECLUSAGE_BLENDWEIGHT:  snprintf (szOut, 64, "BLENDWEIGHT  [%lu]", idx); break;
              case D3DDECLUSAGE_BLENDINDICES: snprintf (szOut, 64, "BLENDINDICES [%lu]", idx); break;
              case D3DDECLUSAGE_NORMAL:       snprintf (szOut, 64, "NORMAL       [%lu]", idx); break;
              case D3DDECLUSAGE_PSIZE:        snprintf (szOut, 64, "PSIZE"                  ); break;
              case D3DDECLUSAGE_TEXCOORD:     snprintf (szOut, 64, "TEXCOORD     [%lu]", idx); break;
              case D3DDECLUSAGE_TANGENT:      snprintf (szOut, 64, "TANGENT      [%lu]", idx); break;
              case D3DDECLUSAGE_BINORMAL:     snprintf (szOut, 64, "BINORMAL     [%lu]", idx); break;
              case D3DDECLUSAGE_TESSFACTOR:   snprintf (szOut, 64, "TESSFACTOR   [%lu]", idx); break;
              case D3DDECLUSAGE_POSITIONT:    snprintf (szOut, 64, "POSITIONT"              ); break;
              case D3DDECLUSAGE_COLOR:        snprintf (szOut, 64, "COLOR        [%lu]", idx); break;
              case D3DDECLUSAGE_FOG:          snprintf (szOut, 64, "FOG"                    ); break;
              case D3DDECLUSAGE_DEPTH:        snprintf (szOut, 64, "DEPTH        [%lu]", idx); break;
              case D3DDECLUSAGE_SAMPLE:       snprintf (szOut, 64, "SV_SampleIndex"         ); break;
            };

            return szOut;
          };

        if (SUCCEEDED (vtx_decl->GetDeclaration (elem_decl, &num_elems)))
        {
          ImGui::Separator ();

          for (UINT i = 0; i < num_elems; i++)
          {
            if (elem_decl [i].Type != D3DDECLTYPE_UNUSED)
            {
              ++last_count;

              ImGui::TextColored (ImVec4 (0.9f, 0.9f, 0.9f, 1.0f),    "Stream %3lu ", elem_decl [i].Stream);
              ImGui::SameLine    ();
              ImGui::TextColored (ImVec4 (0.66f, 0.66f, 0.66f, 1.0f), "(+%02lu): ",
                              elem_decl [i].Offset);
              ImGui::SameLine    ();
              ImGui::TextColored (ImVec4 (0.35f, 0.85f, 0.35f, 1.0f), "%16s ",
                                SK_D3D9_DeclTypeToStr  ((D3DDECLTYPE)elem_decl [i].Type) );
              ImGui::SameLine    ();
              ImGui::TextColored (ImVec4 (0.6f, 0.6f, 1.0f, 1.0f), "\"Attrib%02lu\"", i);
              ImGui::SameLine    ();
              ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), " : %s",
                                SK_D3D9_DeclUsageToStr ((D3DDECLUSAGE)elem_decl [i].Usage, elem_decl [i].UsageIndex) );
            }
          }

          --last_count;
        }
      }
      ImGui::EndChild ();

      ImGui::PopStyleColor ();
      ImGui::PopStyleVar   ();
      ImGui::EndGroup      ();

      if (ImGui::IsItemHoveredRect () && tracker->textures.size ())
      {
        ImGui::BeginTooltip ();

        D3DFORMAT fmt = D3DFMT_UNKNOWN;

        for ( auto it : tracker->textures )
        {
          ISKTextureD3D9* pTex = tbf::RenderFix::tex_mgr.getTexture (it)->d3d9_tex;
          
          if (pTex && pTex->pTex)
          {
            D3DSURFACE_DESC desc;
            if (SUCCEEDED (pTex->pTex->GetLevelDesc (0, &desc)))
            {
              fmt = desc.Format;
              ImGui::Image ( pTex->pTex, ImVec2  ( std::max (64.0f, (float)desc.Width / 16.0f),
        ((float)desc.Height / (float)desc.Width) * std::max (64.0f, (float)desc.Width / 16.0f) ),
                                         ImVec2  (0,0),             ImVec2  (1,1),
                                         ImColor (255,255,255,255), ImColor (242,242,13,255) );
            }

            ImGui::SameLine ();

            ImGui::BeginGroup ();
            ImGui::Text       ("Texture: %08lx", it);
            ImGui::Text       ("Format:  %ws",   SK_D3D9_FormatToStr (fmt).c_str ());
            ImGui::EndGroup   ();
          }
        }

        ImGui::EndTooltip ();
      }

      ImGui::SameLine ();
      ImGui::Checkbox ("Always Draw This Buffer In Wireframe", &wireframe);
      
      if (wireframe)
        tracker->wireframes.emplace (tracker->vertex_buffer);
      else if (tracker->wireframes.count (tracker->vertex_buffer))
        tracker->wireframes.erase   (tracker->vertex_buffer);
    }

    if (tracker->vertex_shaders.size () > 0 || tracker->pixel_shaders.size () > 0)
    {
      ImGui::Separator ();

      ImGui::Columns (2);

      for ( auto it : tracker->vertex_shaders )
        ImGui::Text ("Vertex Shader: %08x", it);

      ImGui::NextColumn ();

      for ( auto it : tracker->pixel_shaders )
        ImGui::Text ("Pixel Shader: %08x", it);

      ImGui::Columns (1);
    }
  }
  else
    tracker->cancel_draws = false;

  ImGui::EndGroup      ();

  list->last_ht    = ImGui::GetItemRectSize ().y;

  list->last_min   = ImGui::GetItemRectMin ();
  list->last_max   = ImGui::GetItemRectMax ();

  ImGui::PopStyleVar   ();
  ImGui::EndGroup      ();
}


bool
TBFix_TextureModDlg (void)
{
  const float font_size = ImGui::GetFont ()->FontSize * ImGui::GetIO ().FontGlobalScale;

  bool show_dlg = true;

  ImGui::SetNextWindowSizeConstraints ( ImVec2 (256.0f, 384.0f), ImVec2 ( ImGui::GetIO ().DisplaySize.x * 0.75f, ImGui::GetIO ().DisplaySize.y * 0.75f ) );

  ImGui::Begin ( "D3D9 Texture Mod Toolkit (v " SK_VERSION_STR_A ")",
                   &show_dlg,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_ShowBorders );

  bool can_scroll = ImGui::IsWindowFocused () && ImGui::IsMouseHoveringRect ( ImVec2 (ImGui::GetWindowPos ().x,                             ImGui::GetWindowPos ().y),
                                                                              ImVec2 (ImGui::GetWindowPos ().x + ImGui::GetWindowSize ().x, ImGui::GetWindowPos ().y + ImGui::GetWindowSize ().y) );

  ImGui::PushItemWidth (ImGui::GetWindowWidth () * 0.666f);

  if (ImGui::CollapsingHeader ("Preliminary Documentation"))
  {
    ImGui::BeginChild (ImGui::GetID ("ModDescription"), ImVec2 (font_size * 66.0f, font_size * 25.0f), true);
      ImGui::TextColored    (ImVec4 (0.9f, 0.7f, 0.5f, 1.0f), "Texture Modding Overview"); ImGui::SameLine ();
      ImGui::Text           ("    (Full Writeup Pending)");

      ImGui::Separator      ();

      ImGui::TextWrapped    ("\nReplacement textures go in (TBFix_Res\\inject\\textures\\{blocking|streaming}\\<checksum>.dds)\n\n");

      ImGui::TreePush ("");
        ImGui::BulletText ("Blocking textures have a high performance penalty, but zero chance of visible pop-in.");
        ImGui::BulletText ("Streaming textures will replace the game's original texture whenever Disk/CPU loads finish.");
        ImGui::TreePush   ("");
          ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.6f, 0.9f, 0.2f, 1.0f));
          ImGui::BulletText     ("Use streaming whenever possible or performance will bite you in the ass.");
          ImGui::PopStyleColor  ();
        ImGui::TreePop    (  );
      ImGui::TreePop  ();

      ImGui::TextWrapped    ("\n\nLoading modified textures from separate files is inefficient; entire groups of textures may also be packaged into \".7z\" files (See TBFix_Res\\inject\\00_License.7z as an example, and use low/no compression ratio or you will kill the game's performance).\n");

      ImGui::Separator      ();

      ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (0.9f, 0.6f, 0.3f, 1.0f));
      ImGui::TextWrapped    ( "\n\nA more detailed synopsis will follow in future versions, for now please refer to the GitHub release notes for Tales of Symphonia "
                              "\"Fix\" v 0.9.0 for a thorough description on authoring texture mods.\n\n" );
      ImGui::PopStyleColor  ();

      ImGui::Separator      ();

      ImGui::Bullet         (); ImGui::SameLine ();
      ImGui::TextWrapped    ( "If texture mods are enabled, you can click on the Injected and Base buttons on the texture cache "
                                "summary pannel to compare modified and unmodified." );
    ImGui::EndChild         ();
  }

  if (ImGui::CollapsingHeader("Injectable Data Sources", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen))
  {
    TBF_DrawFileList  (can_scroll);
  }

  if (ImGui::CollapsingHeader ("Live Texture View", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen))
  {
    static float last_ht    = 256.0f;
    static float last_width = 256.0f;

    static std::vector <std::string> list_contents;
    static bool                      list_dirty     = false;
    static int                       sel            =     0;

    extern std::vector <uint32_t> textures_used_last_dump;
    extern              uint32_t  tex_dbg_idx;
    extern              uint32_t  debug_tex_id;

    ImGui::BeginChild (ImGui::GetID ("ToolHeadings"), ImVec2 (font_size * 66.0f, font_size * 2.5f), false, ImGuiWindowFlags_AlwaysUseWindowPadding);

    if (ImGui::Button ("  Refresh Textures  "))
    {
      SK_ICommandProcessor& command =
        *SK_GetCommandProcessor ();

      command.ProcessCommandLine ("Textures.Trace true");

      tbf::RenderFix::tex_mgr.updateOSD ();

      list_dirty = true;
    }

    if (ImGui::IsItemHovered ()) ImGui::SetTooltip ("Refreshes the set of texture checksums used in the last frame drawn.");

    ImGui::SameLine ();

    if (ImGui::Button (" Clear Debug "))
    {
      sel                         = -1;
      debug_tex_id                =  0;
      textures_used_last_dump.clear ();
      last_ht                     =  0;
      last_width                  =  0;
    }

    if (ImGui::IsItemHovered ()) ImGui::SetTooltip ("Exits texture debug mode.");

    ImGui::SameLine ();

    if (ImGui::Checkbox ("Enable On-Demand Texture Dumping",    &config.textures.on_demand_dump)) tbf::RenderFix::need_reset.graphics = true;
    
    if (ImGui::IsItemHovered ())
    {
      ImGui::BeginTooltip ();
      ImGui::TextColored  (ImVec4 (0.9f, 0.7f, 0.3f, 1.f), "Enable dumping DXT compressed textures from VRAM.");
      ImGui::Separator    ();
      ImGui::Text         ("Drivers may not be able to manage texture memory as efficiently, and you should turn this option off when not modifying textures.\n\n");
      ImGui::BulletText   ("If this is your first time enabling this feature, the dump button will not work until you reload all textures in-game.");
      ImGui::EndTooltip   ();
    }

    ImGui::SameLine ();

    ImGui::Checkbox ("Highlight Selected Texture in Game",    &config.textures.highlight_debug_tex);

    ImGui::Separator ();
    ImGui::EndChild  ();

    if (list_dirty)
    {
           list_contents.clear ();
                sel = tex_dbg_idx;

      if (debug_tex_id == 0)
        last_ht = 0;


      // The underlying list is unsorted for speed, but that's not at all
      //   intuitive to humans, so sort the thing when we have the RT view open.
      std::sort ( textures_used_last_dump.begin (),
                  textures_used_last_dump.end   () );


      for ( auto it : textures_used_last_dump )
      {
        char szDesc [16] = { };

        sprintf (szDesc, "%08x", it);

        list_contents.push_back (szDesc);
      }
    }

    ImGui::BeginGroup ();

    ImGui::PushStyleVar   (ImGuiStyleVar_ChildWindowRounding, 0.0f);
    ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.9f, 0.7f, 0.5f, 1.0f));

    ImGui::BeginChild ( ImGui::GetID ("Item List"),
                        ImVec2 ( font_size * 6.0f, std::max (font_size * 15.0f, last_ht)),
                          true, ImGuiWindowFlags_AlwaysAutoResize );

    if (ImGui::IsWindowHovered ())
      can_scroll = false;

   if (textures_used_last_dump.size ())
   {
     static      int last_sel = 0;
     static bool sel_changed  = false;

     if (sel != last_sel)
       sel_changed = true;

     last_sel = sel;

     for ( int line = 0; line < textures_used_last_dump.size (); line++)
     {
       if (line == sel)
       {
         bool selected = true;
         ImGui::Selectable (list_contents [line].c_str (), &selected);

         if (sel_changed)
         {
           ImGui::SetScrollHere (0.5f); // 0.0f:top, 0.5f:center, 1.0f:bottom
           sel_changed = false;
         }
       }

       else
       {
         bool selected = false;

         if (ImGui::Selectable (list_contents[line].c_str (), &selected))
         {
           sel_changed = true;
           tex_dbg_idx                 =  line;
           sel                         =  line;
           debug_tex_id                =  textures_used_last_dump [line];
         }
       }
     }
   }

   ImGui::EndChild ();

   if (ImGui::IsItemHovered ())
   {
     ImGui::BeginTooltip ();
     ImGui::TextColored  (ImVec4 (0.9f, 0.6f, 0.2f, 1.0f), "The \"debug\" texture will appear black to make identifying textures to modify easier.");
     ImGui::Separator    ();
     ImGui::BulletText   ("Press Ctrl + Shift + [ to select the previous texture from this list");
     ImGui::BulletText   ("Press Ctrl + Shift + ] to select the next texture from this list");
     ImGui::EndTooltip   ();
   }

   ImGui::SameLine     ();
   ImGui::PushStyleVar (ImGuiStyleVar_ChildWindowRounding, 20.0f);

   last_ht    = std::max (last_ht,    16.0f);
   last_width = std::max (last_width, 16.0f);

   if (debug_tex_id != 0x00)
   {
     tbf::RenderFix::Texture* pTex =
       tbf::RenderFix::tex_mgr.getTexture (debug_tex_id);

     extern bool __remap_textures;
            bool has_alternate = (pTex != nullptr && pTex->d3d9_tex->pTexOverride != nullptr);

     if (pTex != nullptr)
     {
        D3DSURFACE_DESC desc;

        if (SUCCEEDED (pTex->d3d9_tex->pTex->GetLevelDesc (0, &desc)))
        {
          ImVec4 border_color = config.textures.highlight_debug_tex ? 
                                  ImVec4 (0.3f, 0.3f, 0.3f, 1.0f) :
                                    (__remap_textures && has_alternate) ? ImVec4 (0.5f,  0.5f,  0.5f, 1.0f) :
                                                                          ImVec4 (0.3f,  1.0f,  0.3f, 1.0f);

          ImGui::PushStyleColor (ImGuiCol_Border, border_color);

          ImGui::BeginGroup     ();
          ImGui::BeginChild     ( ImGui::GetID ("Item Selection"),
                                  ImVec2 ( std::max (font_size * 19.0f, (float)desc.Width + 24.0f),
                                (float)desc.Height + font_size * 10.0f),
                                    true,
                                      ImGuiWindowFlags_AlwaysAutoResize );

          if ((! config.textures.highlight_debug_tex) && has_alternate)
          {
            if (ImGui::IsItemHovered ())
              ImGui::SetTooltip ("Click me to make this texture the visible version.");
            
            // Allow the user to toggle texture override by clicking the frame
            if (ImGui::IsItemClicked ())
              __remap_textures = false;
          }

          last_width  = (float)desc.Width;
          last_ht     = (float)desc.Height + font_size * 10.0f;


          int num_lods = pTex->d3d9_tex->pTex->GetLevelCount ();

          ImGui::Text ( "Dimensions:   %lux%lu (%lu %s)",
                          desc.Width, desc.Height,
                            num_lods, num_lods > 1 ? "LODs" : "LOD" );
          ImGui::Text ( "Format:       %ws",
                          SK_D3D9_FormatToStr (desc.Format).c_str () );
          ImGui::Text ( "Data Size:    %.2f MiB",
                          (double)pTex->d3d9_tex->tex_size / (1024.0f * 1024.0f) );
          ImGui::Text ( "Load Time:    %.6f Seconds",
                          pTex->load_time / 1000.0f );

          ImGui::Separator     ();

          if (! TBF_IsTextureDumped (debug_tex_id))
          {
            if ( ImGui::Button ("  Dump Texture to Disk  ") )
            {
              if (! config.textures.quick_load)
                TBF_DumpTexture (desc.Format, debug_tex_id, pTex->d3d9_tex->pTex);
            }

            if (config.textures.quick_load && ImGui::IsItemHovered ())
              ImGui::SetTooltip ("Turn off Texture QuickLoad to use this feature.");
          }

          else
          {
            if ( ImGui::Button ("  Delete Dumped Texture from Disk  ") )
            {
              TBF_DeleteDumpedTexture (desc.Format, debug_tex_id);
            }
          }

          ImGui::PushStyleColor  (ImGuiCol_Border, ImVec4 (0.95f, 0.95f, 0.05f, 1.0f));
          ImGui::BeginChildFrame (ImGui::GetID ("ChildFrame_XXX"), ImVec2 ((float)desc.Width + 8, (float)desc.Height + 8), ImGuiWindowFlags_ShowBorders);
          ImGui::Image           ( pTex->d3d9_tex->pTex,
                                     ImVec2 ((float)desc.Width, (float)desc.Height),
                                       ImVec2  (0,0),             ImVec2  (1,1),
                                       ImColor (255,255,255,255), ImColor (255,255,255,128)
                                 );
          ImGui::EndChildFrame   ();
          ImGui::EndChild        ();
          ImGui::EndGroup        ();
          ImGui::PopStyleColor   (2);
        }
     }

     if (has_alternate)
     {
       ImGui::SameLine ();

        D3DSURFACE_DESC desc;

        if (SUCCEEDED (pTex->d3d9_tex->pTexOverride->GetLevelDesc (0, &desc)))
        {
          ImVec4 border_color = config.textures.highlight_debug_tex ? 
                                  ImVec4 (0.3f, 0.3f, 0.3f, 1.0f) :
                                    (__remap_textures) ? ImVec4 (0.3f,  1.0f,  0.3f, 1.0f) :
                                                         ImVec4 (0.5f,  0.5f,  0.5f, 1.0f);

          ImGui::PushStyleColor  (ImGuiCol_Border, border_color);

          ImGui::BeginGroup ();
          ImGui::BeginChild ( ImGui::GetID ("Item Selection2"),
                              ImVec2 ( std::max (font_size * 19.0f, (float)desc.Width  + 24.0f),
                                                                    (float)desc.Height + font_size * 10.0f),
                                true,
                                  ImGuiWindowFlags_AlwaysAutoResize );

          if (! config.textures.highlight_debug_tex)
          {
            if (ImGui::IsItemHovered ())
              ImGui::SetTooltip ("Click me to make this texture the visible version.");

            // Allow the user to toggle texture override by clicking the frame
            if (ImGui::IsItemClicked ())
              __remap_textures = true;
          }


          last_width  = std::max (last_width, (float)desc.Width);
          last_ht     = std::max (last_ht,    (float)desc.Height + font_size * 10.0f);


          extern std::wstring
          SK_D3D9_FormatToStr (D3DFORMAT Format, bool include_ordinal = true);


          bool injected  =
            (TBF_GetInjectableTexture (debug_tex_id) != nullptr),
               reloading = false;;

          int num_lods = pTex->d3d9_tex->pTexOverride->GetLevelCount ();

          ImGui::Text ( "Dimensions:   %lux%lu  (%lu %s)",
                          desc.Width, desc.Height,
                             num_lods, num_lods > 1 ? "LODs" : "LOD" );
          ImGui::Text ( "Format:       %ws",
                          SK_D3D9_FormatToStr (desc.Format).c_str () );
          ImGui::Text ( "Data Size:    %.2f MiB",
                          (double)pTex->d3d9_tex->override_size / (1024.0f * 1024.0f) );
          ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), injected ? "Injected Texture" : "Resampled Texture" );

          ImGui::Separator     ();


          if (injected)
          {
            if ( ImGui::Button ("  Reload This Texture  ") && tbf::RenderFix::tex_mgr.reloadTexture (debug_tex_id) )
            {
              reloading    = true;

              tbf::RenderFix::tex_mgr.updateOSD ();
            }
          }

          else {
            ImGui::Button ("  Resample This Texture  "); // NO-OP, but preserves alignment :P
          }

          if (! reloading)
          {
            ImGui::PushStyleColor  (ImGuiCol_Border, ImVec4 (0.95f, 0.95f, 0.05f, 1.0f));
            ImGui::BeginChildFrame (ImGui::GetID ("ChildFrame_YYY"), ImVec2 ((float)desc.Width + 8, (float)desc.Height + 8), ImGuiWindowFlags_ShowBorders);
            ImGui::Image           ( pTex->d3d9_tex->pTexOverride,
                                       ImVec2 ((float)desc.Width, (float)desc.Height),
                                         ImVec2  (0,0),             ImVec2  (1,1),
                                         ImColor (255,255,255,255), ImColor (255,255,255,128)
                                   );
            ImGui::EndChildFrame   ();
            ImGui::PopStyleColor   (1);
          }

          ImGui::EndChild        ();
          ImGui::EndGroup        ();
          ImGui::PopStyleColor   (1);
        }
      }
    }
    ImGui::EndGroup      ();
    ImGui::PopStyleColor (1);
    ImGui::PopStyleVar   (2);
  }

  if (ImGui::CollapsingHeader ("Live Render Target View"))
  {
    static float last_ht    = 256.0f;
    static float last_width = 256.0f;

    static std::vector <std::string> list_contents;
    static bool                      list_dirty     = true;
    static uintptr_t                 last_sel_ptr   =    0;
    static int                            sel       =   -1;

    std::vector <IDirect3DBaseTexture9*> render_textures =
      tbf::RenderFix::tex_mgr.getUsedRenderTargets ();

    tbf::RenderFix::tracked_rt.tracking_tex = 0;

    if (list_dirty)
    {
          sel = -1;
      int idx =  0;
          list_contents.clear ();

      // The underlying list is unsorted for speed, but that's not at all
      //   intuitive to humans, so sort the thing when we have the RT view open.
      std::sort ( render_textures.begin (),
                  render_textures.end   (),
        []( IDirect3DBaseTexture9 *a,
            IDirect3DBaseTexture9 *b )
        {
          return (uintptr_t)a < (uintptr_t)b;
        }
      );


      for ( auto it : render_textures )
      {
        char szDesc [16] = { };

        sprintf (szDesc, "%llx", (uintptr_t)it);

        list_contents.push_back (szDesc);

        if ((uintptr_t)it == last_sel_ptr) {
          sel = idx;
          tbf::RenderFix::tracked_rt.tracking_tex = render_textures [sel];
        }

        ++idx;
      }
    }

    ImGui::PushStyleVar   (ImGuiStyleVar_ChildWindowRounding, 0.0f);
    ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.9f, 0.7f, 0.5f, 1.0f));

    ImGui::BeginChild ( ImGui::GetID ("Item List2"),
                        ImVec2 ( font_size * 6.0f, std::max (font_size * 15.0f, last_ht)),
                          true, ImGuiWindowFlags_AlwaysAutoResize );

    if (ImGui::IsWindowHovered ())
      can_scroll = false;

   if (render_textures.size ())
   {
     static      int last_sel = 0;
     static bool sel_changed  = false;

     if (sel != last_sel)
       sel_changed = true;

     last_sel = sel;

     for ( int line = 0; line < render_textures.size (); line++ )
     {
       D3DSURFACE_DESC desc;

       CComPtr <IDirect3DTexture9> pTex = nullptr;

       if (SUCCEEDED (render_textures [line]->QueryInterface (IID_PPV_ARGS (&pTex))))
       {
         if (SUCCEEDED (pTex->GetLevelDesc (0, &desc)))
         {
           if (line == sel)
           {
             bool selected = true;
             ImGui::Selectable (list_contents [line].c_str (), &selected);

             if (sel_changed)
             {
               ImGui::SetScrollHere (0.5f); // 0.0f:top, 0.5f:center, 1.0f:bottom
               sel_changed = false;
             }
           }

           else
           {
             bool selected = false;

             if (ImGui::Selectable (list_contents [line].c_str (), &selected))
             {
               sel_changed  = true;
               sel          =  line;
               last_sel_ptr = (uintptr_t)render_textures [sel];
               tbf::RenderFix::tracked_rt.tracking_tex = render_textures [sel];
             }
           }
         }
       }
     }
   }

   ImGui::EndChild ();

   ImGui::BeginGroup ();

   ImGui::PopStyleColor ();
   ImGui::PopStyleVar   ();

   CComPtr <IDirect3DTexture9> pTex = nullptr;

   if (render_textures.size () && sel >= 0)
     render_textures [sel]->QueryInterface (IID_PPV_ARGS (&pTex));

   if (pTex != nullptr)
   {
      D3DSURFACE_DESC desc;

      if (SUCCEEDED (pTex->GetLevelDesc (0, &desc)))
      {
        size_t shaders = std::max ( tbf::RenderFix::tracked_rt.pixel_shaders.size  (),
                                    tbf::RenderFix::tracked_rt.vertex_shaders.size () );

        // Some Render Targets are MASSIVE, let's try to keep the damn things on the screen ;)
        float effective_width  = std::min (0.75f * ImGui::GetIO ().DisplaySize.x, (float)desc.Width  / 2.0f);
        float effective_height = std::min (0.75f * ImGui::GetIO ().DisplaySize.y, (float)desc.Height / 2.0f);

        ImGui::SameLine ();

        ImGui::PushStyleColor  (ImGuiCol_Border, ImVec4 (0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::BeginChild ( ImGui::GetID ("Item Selection3"),
                            ImVec2 ( std::max (font_size * 30.0f, effective_width  + 24.0f),
                                     std::max (256.0f,            effective_height + font_size * 4.0f + (float)shaders * font_size) ),
                              true,
                                ImGuiWindowFlags_AlwaysAutoResize );

        last_width  = effective_width;
        last_ht     = effective_height + font_size * 4.0f + (float)shaders * font_size;

        extern std::wstring
        SK_D3D9_FormatToStr (D3DFORMAT Format, bool include_ordinal = true);


        ImGui::Text ( "Dimensions:   %lux%lu",
                        desc.Width, desc.Height/*,
                          pTex->d3d9_tex->GetLevelCount ()*/ );
        ImGui::Text ( "Format:       %ws",
                        SK_D3D9_FormatToStr (desc.Format).c_str () );

        ImGui::Separator     ();

        ImGui::PushStyleColor  (ImGuiCol_Border, ImVec4 (0.95f, 0.95f, 0.05f, 1.0f));
        ImGui::BeginChildFrame (ImGui::GetID ("ChildFrame_ZZZ"), ImVec2 (effective_width + 8.0f, effective_height + 8.0f), ImGuiWindowFlags_ShowBorders);
        ImGui::Image           ( pTex,
                                   ImVec2 (effective_width, effective_height),
                                     ImVec2  (0,0),             ImVec2  (1,1),
                                     ImColor (255,255,255,255), ImColor (255,255,255,128)
                               );
        ImGui::EndChildFrame   ();

        if (shaders > 0)
        {
          ImGui::Columns (2);

          for ( auto it : tbf::RenderFix::tracked_rt.vertex_shaders )
            ImGui::Text ("Vertex Shader: %08x", it);

          ImGui::NextColumn ();

          for ( auto it : tbf::RenderFix::tracked_rt.pixel_shaders )
            ImGui::Text ("Pixel Shader: %08x", it);

          ImGui::Columns (1);
        }

        ImGui::EndChild        ();
        ImGui::PopStyleColor   (2);
      }
    }

    ImGui::EndGroup ();
  }

  if (ImGui::CollapsingHeader ("Live Shader View"))
  {
    ImGui::TreePush ("");

    if (ImGui::CollapsingHeader ("Pixel Shaders"))
      TBF_LiveShaderClassView (tbf_shader_class::Pixel, can_scroll);

    if (ImGui::CollapsingHeader ("Vertex Shaders"))
      TBF_LiveShaderClassView (tbf_shader_class::Vertex, can_scroll);

    ImGui::TreePop ();
  }

  if (ImGui::CollapsingHeader ("Live Vertex Buffer View"))
  {
    ImGui::TreePush ("");

    if (ImGui::CollapsingHeader ("Stream 0"))
      TBF_LiveVertexStreamView (can_scroll);

    ImGui::TreePop ();
  }

  if (ImGui::CollapsingHeader ("Misc. Settings"))
  {
    ImGui::TreePush ("");
    if (ImGui::Checkbox ("Dump ALL Shaders   (TBFix_Res\\dump\\shaders\\<ps|vs>_<checksum>.html)", &config.render.dump_shaders)) tbf::RenderFix::need_reset.graphics = true;
    if (ImGui::Checkbox ("Dump ALL Textures  (TBFix_Res\\dump\\textures\\<format>\\*.dds)",        &config.textures.dump))       tbf::RenderFix::need_reset.graphics = true;

    if (ImGui::IsItemHovered ())
    {
      ImGui::BeginTooltip ();
      ImGui::Text         ("Enabling this will cause the game to run slower and waste disk space, only enable if you know what you are doing.");
      ImGui::EndTooltip   ();
    }

    ImGui::TreePop ();
  }

  ImGui::PopItemWidth ();

  if (can_scroll)
    ImGui::SetScrollY (ImGui::GetScrollY () + 5.0f * ImGui::GetFont ()->FontSize * -ImGui::GetIO ().MouseWheel);

  ImGui::End          ();

  return show_dlg;
}
#endif