#pragma once

#include <Windows.h>

#include <SpecialK/log.h>
#include <SpecialK/config.h>
#include <SpecialK/framerate.h>

extern iSK_Logger tex_log;

//#include "render.h"
#include <d3d9.h>

#include <set>

#include <map>
#include <unordered_map>

interface ISKTextureD3D9;

#include <d3d9.h>
#include <d3dx9tex.h>

#include <set>
#include <map>
#include <queue>
#include <limits>
#include <cstdint>
#include <algorithm>

interface ISKTextureD3D9;

uint32_t
safe_crc32c (uint32_t seed, const void* pData, size_t size);

namespace SK   {
namespace D3D9 {

class Texture {
public:
  Texture (void)
  {
    crc32c    = 0;
    size      = 0;
    refs      = 0;
    load_time = 0.0f;
    d3d9_tex  = nullptr;
  }

  uint32_t        crc32c;
  size_t          size;
  int             refs;
  float           load_time;
  ISKTextureD3D9* d3d9_tex;
};

struct TexThreadStats {
  ULONGLONG bytes_loaded;
  LONG      jobs_retired;

  struct {
    FILETIME start, end;
    FILETIME user,  kernel;
    FILETIME idle; // Computed: (now - start) - (user + kernel)
  } runtime;
};

#if 0
  class Sampler {
    int                id;
    IDirect3DTexture9* pTex;
  };
#endif

  extern std::set <UINT> active_samplers;


  enum TexLoadMethod {
    Streaming,
    Blocking,
    DontCare
  };
  
  struct TexRecord {
    unsigned int           archive = std::numeric_limits <unsigned int>::max ();
             int           fileno  = 0UL;
    enum     TexLoadMethod method  = DontCare;
             size_t        size    = 1UL;
  };

  typedef std::vector < std::pair < uint32_t, TexRecord > > TexList;


  struct TexLoadRequest
  {
    enum {
      Stream,    // This load will be streamed
      Immediate, // This load must finish immediately   (pSrc is unused)
      Resample   // Change image properties             (pData is supplied)
    } type;

    LPDIRECT3DDEVICE9   pDevice;

    // Resample only
    LPVOID              pSrcData;
    UINT                SrcDataSize;

    uint32_t            checksum;
    uint32_t            size;

    // Stream / Immediate
    wchar_t             wszFilename [MAX_PATH];

    LPDIRECT3DTEXTURE9  pDest = nullptr;
    LPDIRECT3DTEXTURE9  pSrc  = nullptr;

    LARGE_INTEGER       start = { 0LL };
    LARGE_INTEGER       end   = { 0LL };
  };
  
  class TexLoadRef
  {
  public:
     TexLoadRef (TexLoadRequest* ref) { ref_ = ref;}
    ~TexLoadRef (void) { }
  
    operator TexLoadRequest* (void) {
      return ref_;
    }
  
  protected:
    TexLoadRequest* ref_;
  };


  class TextureManager
  {
  public:
    void Init     (void);
    void Hook     (void);
    void Shutdown (void);

    void                       removeTexture       (ISKTextureD3D9* pTexD3D9);

    Texture*                   getTexture          (uint32_t crc32c);
    void                       addTexture          (uint32_t crc32c, Texture* pTex, size_t size);

    bool                       reloadTexture       (uint32_t crc32c);

    // Record a cached reference
    void                       refTexture          (Texture* pTex);

    // Similar, just call this to indicate a cache miss
    void                       missTexture         (void) {
      InterlockedIncrement (&misses);
    }

    void                       reset               (void);
    void                       purge               (void); // WIP

    size_t                     numTextures         (void) const {
      return textures.size ();
    }
    int                        numInjectedTextures (void) const;

    int64_t                    cacheSizeTotal      (void) const;
    int64_t                    cacheSizeBasic      (void) const;
    int64_t                    cacheSizeInjected   (void) const;

    int                        numMSAASurfs        (void);

    void                       addInjected         (size_t size) {
      InterlockedIncrement (&injected_count);
      InterlockedAdd64     (&injected_size, size);
    }

    std::string                osdStats            (void) { return osd_stats; }
    void                       updateOSD           (void);


    float                      getTimeSaved        (void) const { return time_saved;                               }
    LONG64                     getByteSaved        (void)       { return InterlockedAdd64       (&bytes_saved, 0); }
    ULONG                      getHitCount         (void)       { return InterlockedExchangeAdd (&hits,   0UL);    }
    ULONG                      getMissCount        (void)       { return InterlockedExchangeAdd (&misses, 0UL);    }


    void                       resetUsedTextures (void);
    void                       applyTexture      (IDirect3DBaseTexture9* tex);

    const std::vector <IDirect3DBaseTexture9 *>
                               getUsedRenderTargets (void) const;

    uint32_t                   getRenderTargetCreationTime 
                                                  (IDirect3DBaseTexture9* rt);
    void                       trackRenderTarget  (IDirect3DBaseTexture9* rt);
    bool                       isRenderTarget     (IDirect3DBaseTexture9* rt) const;
    bool                       isUsedRenderTarget (IDirect3DBaseTexture9* rt) const;

    void                       logUsedTextures    (void);

    void                       queueScreenshot    (wchar_t* wszFileName, bool hudless = true);
    bool                       wantsScreenshot    (void);
    HRESULT                    takeScreenshot     (IDirect3DSurface9* pSurf);

    std::vector <TexThreadStats>
                               getThreadStats     (void);



    HRESULT                    dumpTexture         (D3DFORMAT fmt, uint32_t checksum, IDirect3DTexture9* pTex);
    bool                       deleteDumpedTexture (D3DFORMAT fmt, uint32_t checksum);
    bool                       isTextureDumped     (uint32_t  checksum) const;

    std::vector <std::wstring> getTextureArchives  (void);
    void                       refreshDataSources  (void);

    HRESULT                    injectTexture           (TexLoadRequest* load);
    bool                       isTextureInjectable     (uint32_t checksum) const;
    bool                       removeInjectableTexture (uint32_t checksum);
    TexList                    getInjectableTextures   (void)              const;
    TexRecord&                 getInjectableTexture    (uint32_t checksum);

    bool                       isTextureBlacklisted    (uint32_t checksum) const;



    void                       updateQueueOSD        (void);
    int                        loadQueuedTextures    (void);


    BOOL                       isTexturePowerOfTwo (UINT sampler)
    {
      return sampler_flags [sampler < 255 ? sampler : 255].power_of_two;
    }

    // Sttate of the active texture for each sampler,
    //   needed to correct some texture coordinate address
    //     problems in the base game.
    struct
    {
      BOOL power_of_two;
    } sampler_flags [256] = { 0 };


    // The set of textures used during the last frame
    std::vector        <uint32_t>                            textures_last_frame;
    std::unordered_set <uint32_t>                            textures_used;
//  std::unordered_set <uint32_t>                            non_power_of_two_textures;

    // Textures that we will not allow injection for
    //   (primarily to speed things up, but also for EULA-related reasons).
    std::unordered_set <uint32_t>                            inject_blacklist;


    //
    // Streaming System Internals
    //
    class Injector
    {
      friend class TextureManager;

    public:
      bool            hasPendingLoads    (void)              const;

      void            startLoad          (void);
      void            endLoad            (void);

      bool            hasPendingStreams  (void)              const;
      bool            isStreaming        (uint32_t checksum) const;

      TexLoadRequest* getTextureInFlight (uint32_t        checksum);
      void            addTextureInFlight (TexLoadRequest* load_op);
      void            finishedStreaming  (uint32_t        checksum);

      bool isInjectionThread (DWORD dwThreadId = GetCurrentThreadId ()) const
      {
        bool inject_thread = false;

        lockInjection ();

        if (inject_tids.count (dwThreadId))
        {
          inject_thread = true;
        }

        unlockInjection ();

        return inject_thread;
      }

      void lockStreaming    (void) const { EnterCriticalSection (&cs_tex_stream);    };
      void lockResampling   (void) const { EnterCriticalSection (&cs_tex_resample);  };
      void lockInjection    (void) const { EnterCriticalSection (&cs_tex_inject);    };
      void lockBlacklist    (void) const { EnterCriticalSection (&cs_tex_blacklist); };

      void unlockStreaming  (void) const { LeaveCriticalSection (&cs_tex_stream);    };
      void unlockResampling (void) const { LeaveCriticalSection (&cs_tex_resample);  };
      void unlockInjection  (void) const { LeaveCriticalSection (&cs_tex_inject);    };
      void unlockBlacklist  (void) const { LeaveCriticalSection (&cs_tex_blacklist); };

    //protected:
      static std::unordered_map   <uint32_t, TexLoadRequest *>
                                                               textures_in_flight;
      static std::queue <TexLoadRef>                           textures_to_stream;
      static std::queue <TexLoadRef>                           finished_loads;

      static CRITICAL_SECTION                                  cs_tex_stream;
      static CRITICAL_SECTION                                  cs_tex_resample;
      static CRITICAL_SECTION                                  cs_tex_inject;
      static CRITICAL_SECTION                                  cs_tex_blacklist;

      static std::set <DWORD>                                  inject_tids;

      static volatile  LONG                                    streaming;//       = 0L;
      static volatile ULONG                                    streaming_bytes;// = 0UL;

      static volatile  LONG                                    resampling;//      = 0L;
    } injector;

  private:
    struct {
      // In lieu of actually wrapping render targets with a COM interface, just add the creation time
      //   as the mapped parameter
      std::unordered_map <IDirect3DBaseTexture9 *, uint32_t> render_targets;
    } known;
    
    struct {
      std::unordered_set <IDirect3DBaseTexture9 *>           render_targets;
    } used;

    std::unordered_map <uint32_t, Texture*>                  textures;
    float                                                    time_saved      = 0.0f;
    LONG64                                                   bytes_saved     = 0LL;

    ULONG                                                    hits            = 0UL;
    ULONG                                                    misses          = 0UL;

    LONG64                                                   basic_size      = 0LL;
    LONG64                                                   injected_size   = 0LL;
    ULONG                                                    injected_count  = 0UL;

    std::string                                              osd_stats       = "";
    bool                                                     want_screenshot = false;

    CRITICAL_SECTION                                         cs_cache;

    std::unordered_map <uint32_t, TexRecord>                 injectable_textures;
    std::vector        <std::wstring>                        archives;
    std::unordered_set <uint32_t>                            dumped_textures;

    bool                                                     init            = false;
  } extern tex_mgr;


  class TextureWorkerThread
  {
  friend class TextureThreadPool;
  
  public:
     TextureWorkerThread (TextureThreadPool* pool);
    ~TextureWorkerThread (void);
  
    void startJob  (TexLoadRequest* job) {
      job_ = job;
      SetEvent (control_.start);
    }
  
    void trim (void) {
      SetEvent (control_.trim);
    }
  
    void finishJob (void);
  
    bool isBusy   (void) {
      return (job_ != nullptr);
    }
  
    void shutdown (void) {
      SetEvent (control_.shutdown);
    }
  
    size_t bytesLoaded (void) {
      return InterlockedExchangeAdd (&bytes_loaded_, 0ULL);
    }
  
    int    jobsRetired  (void) {
      return InterlockedExchangeAdd (&jobs_retired_, 0L);
    }
  
    FILETIME idleTime   (void) {
      GetThreadTimes ( thread_,
                         &runtime_.start, &runtime_.end,
                           &runtime_.kernel, &runtime_.user );
  
      FILETIME now;
      GetSystemTimeAsFileTime (&now);
  
      ULONGLONG elapsed =
        ULARGE_INTEGER { now.dwLowDateTime,            now.dwHighDateTime            }.QuadPart -
        ULARGE_INTEGER { runtime_.start.dwLowDateTime, runtime_.start.dwHighDateTime }.QuadPart;
  
      ULONGLONG busy =
        ULARGE_INTEGER { runtime_.kernel.dwLowDateTime, runtime_.kernel.dwHighDateTime }.QuadPart +
        ULARGE_INTEGER { runtime_.user.dwLowDateTime,   runtime_.user.dwHighDateTime   }.QuadPart;
  
      ULARGE_INTEGER idle;
      idle.QuadPart = elapsed - busy;
  
      return FILETIME { idle.LowPart,
                        idle.HighPart };
    }
    FILETIME userTime   (void) { return runtime_.user;   };
    FILETIME kernelTime (void) { return runtime_.kernel; };
  
  protected:
    static CRITICAL_SECTION cs_worker_init;
    static ULONG            num_threads_init;
  
    static unsigned int __stdcall ThreadProc (LPVOID user);
  
    TextureThreadPool*    pool_;
  
    unsigned int          thread_id_;
    HANDLE                thread_;
  
    TexLoadRequest*       job_;
  
    volatile ULONGLONG    bytes_loaded_ = 0ULL;
    volatile LONG         jobs_retired_ = 0L;
  
    struct {
      FILETIME start, end;
      FILETIME user,  kernel;
      FILETIME idle; // Computed: (now - start) - (user + kernel)
    } runtime_ { 0, 0, 0, 0, 0 };
  
    struct {
      union {
        struct {
          HANDLE start;
          HANDLE trim;
          HANDLE shutdown;
        };
        HANDLE   ops [3];
      };
    } control_;
  };
  
  class TextureThreadPool
  {
  friend class TextureWorkerThread;

  public:
    TextureThreadPool (void)
    {
      events_.jobs_added =
        CreateEvent (nullptr, FALSE, FALSE, nullptr);
  
      events_.results_waiting =
        CreateEvent (nullptr, FALSE, FALSE, nullptr);
  
      events_.shutdown =
        CreateEvent (nullptr, FALSE, FALSE, nullptr);
  
      InitializeCriticalSectionAndSpinCount (&cs_jobs,      10UL);
      InitializeCriticalSectionAndSpinCount (&cs_results, 1000UL);
  
      const int   MAX_THREADS      = 2;//config.textures.worker_threads;  
      static bool init_worker_sync = false;

      if (! init_worker_sync)
      {
        // We will add a sync. barrier that waits for all of the threads in this pool, plus all of the threads
        //   in the other pool to initialize. This design is flawed, but safe.
        InitializeCriticalSectionAndSpinCount (&TextureWorkerThread::cs_worker_init, 10000UL);
        init_worker_sync = true;
      }
  
      for (int i = 0; i < MAX_THREADS; i++)
      {
        TextureWorkerThread* pWorker =
          new TextureWorkerThread (this);
  
        workers_.push_back (pWorker);
      }
  
      // This will be deferred until it is first needed...
      spool_thread_ = nullptr;
    }
  
    ~TextureThreadPool (void)
    {
      if (spool_thread_ != nullptr)
      {
        shutdown ();
  
        WaitForSingleObject (spool_thread_, INFINITE);
        CloseHandle         (spool_thread_);
      }
  
      DeleteCriticalSection (&cs_results);
      DeleteCriticalSection (&cs_jobs);
  
      CloseHandle (events_.results_waiting);
      CloseHandle (events_.jobs_added);
      CloseHandle (events_.shutdown);
    }
  
    void postJob (TexLoadRequest* job);
  
    std::vector <TexLoadRequest *> getFinished (void)
    {
      std::vector <TexLoadRequest *> results;
  
      DWORD dwResults =
        WaitForSingleObject (events_.results_waiting, 0);
  
      // Nothing waiting
      if (dwResults != WAIT_OBJECT_0)
        return results;
  
      EnterCriticalSection (&cs_results);
      {
        while (! results_.empty ())
        {
          results.push_back (results_.front ());
                             results_.pop   ();
        }
      }
      LeaveCriticalSection (&cs_results);
  
      return results;
    }
  
    bool   working     (void) { return (! results_.empty ()); }
    void   shutdown    (void) { SetEvent (events_.shutdown);  }
  
    size_t queueLength (void)
    {
      size_t num = 0;
  
      EnterCriticalSection (&cs_jobs);
      {
        num = jobs_.size ();
      }
      LeaveCriticalSection (&cs_jobs);
  
      return num;
    }
  
  
    std::vector <SK::D3D9::TexThreadStats>
    getWorkerStats (void)
    {
      std::vector <SK::D3D9::TexThreadStats> stats;
  
      for ( auto it : workers_ )
      {
        SK::D3D9::TexThreadStats stat;
  
        stat.bytes_loaded   = it->bytesLoaded ();
        stat.jobs_retired   = it->jobsRetired ();
        stat.runtime.idle   = it->idleTime    ();
        stat.runtime.kernel = it->kernelTime  ();
        stat.runtime.user   = it->userTime    ();
  
        stats.push_back (stat);
      }
  
      return stats;
    }
  
  
  protected:
    static unsigned int __stdcall Spooler (LPVOID user);
  
    TexLoadRequest* getNextJob   (void)
    {
      TexLoadRequest* job       = nullptr;
//      DWORD           dwResults = 0;
  
      //while (dwResults != WAIT_OBJECT_0) {
        //dwResults = WaitForSingleObject (events_.jobs_added, INFINITE);
      //}
  
      if (jobs_.empty ())
        return nullptr;
  
      EnterCriticalSection (&cs_jobs);
      {
        job = jobs_.front ();
              jobs_.pop   ();
      }
      LeaveCriticalSection (&cs_jobs);
  
      return job;
    }
  
    void            postFinished (TexLoadRequest* finished)
    {
      EnterCriticalSection (&cs_results);
      {
        // Remove the temporary reference we added earlier
        finished->pDest->Release ();
  
        results_.push (finished);
        SetEvent      (events_.results_waiting);
      }
      LeaveCriticalSection (&cs_results);
    }
  
  private:
    std::queue <TexLoadRef>             jobs_;
    std::queue <TexLoadRef>             results_;
  
    std::vector <TextureWorkerThread *> workers_;
  
    struct {
      HANDLE jobs_added;
      HANDLE results_waiting;
      HANDLE shutdown;
    } events_;
  
    CRITICAL_SECTION cs_jobs;
    CRITICAL_SECTION cs_results;
  
    HANDLE spool_thread_;
  } extern *resample_pool;
  
  //
  // Split stream jobs into small and large in order to prevent
  //   starvation from wreaking havoc on load times.
  //
  //   This is a simple, but remarkably effective approach and
  //     further optimization work probably will not be done.
  //
  struct StreamSplitter
  {
    bool working (void)
    {
      if (lrg_tex && lrg_tex->working ())
        return true;
  
      if (sm_tex  && sm_tex->working  ())
        return true;
  
      return false;
    }
  
    size_t queueLength (void)
    {
      size_t len = 0;
  
      if (lrg_tex) len += lrg_tex->queueLength ();
      if (sm_tex)  len += sm_tex->queueLength  ();
  
      return len;
    }
  
    std::vector <TexLoadRequest *> getFinished (void)
    {
      std::vector <TexLoadRequest *> results;
  
      std::vector <TexLoadRequest *> lrg_loads;
      std::vector <TexLoadRequest *> sm_loads;
  
      if (lrg_tex) lrg_loads = lrg_tex->getFinished ();
      if (sm_tex)  sm_loads  = sm_tex->getFinished  ();
  
      results.insert (results.begin (), lrg_loads.begin (), lrg_loads.end ());
      results.insert (results.begin (), sm_loads.begin  (), sm_loads.end  ());
  
      return results;
    }
  
    void postJob (TexLoadRequest* job)
    {
      // A "Large" load is one >= 128 KiB
      if (job->SrcDataSize > (128 * 1024))
        lrg_tex->postJob (job);
      else
        sm_tex->postJob (job);
    }
  
    TextureThreadPool* lrg_tex = nullptr;
    TextureThreadPool* sm_tex  = nullptr;
  } extern stream_pool;

}
}

#include <SpecialK/utility.h>

#pragma comment (lib, "dxguid.lib")

const GUID IID_SKTextureD3D9 =
  { 0xace1f81b, 0x5f3f, 0x45f4, 0xbf, 0x9f, 0x1b, 0xaf, 0xdf, 0xba, 0x11, 0x9b };

interface ISKTextureD3D9 : public IDirect3DTexture9
{
public:
  ISKTextureD3D9 (IDirect3DTexture9 **ppTex, SIZE_T size, uint32_t crc32c)
  {
      pTexOverride  = nullptr;
      can_free      = true;
      override_size = 0;
      last_used.QuadPart
                    = 0ULL;
      pTex          = *ppTex;
    *ppTex          =  this;
      tex_size      = size;
      tex_crc32c    = crc32c;
      must_block    = false;
      refs          =  1;
  };

  /*** IUnknown methods ***/
  STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) {
    if (IsEqualGUID (riid, IID_SKTextureD3D9)) {
      return S_OK;
    }
  
#if 1
  if ( IsEqualGUID (riid, IID_IUnknown)              ||
       IsEqualGUID (riid, IID_IDirect3DResource9)    ||
       IsEqualGUID (riid, IID_IDirect3DTexture9)     ||
       IsEqualGUID (riid, IID_IDirect3DBaseTexture9)    )
  {
    AddRef ();
    *ppvObj = this;
    return S_OK;
  }
  
  return E_NOINTERFACE;
#else
  return pTex->QueryInterface (riid, ppvObj);
#endif
  }
  STDMETHOD_(ULONG,AddRef)(THIS) {
    ULONG ret = InterlockedIncrement (&refs);
  
    can_free = false;
  
    return ret;
  }
  STDMETHOD_(ULONG,Release)(THIS) {
    ULONG ret = InterlockedDecrement (&refs);
  
    if (ret == 1) {
      can_free = true;
    }
  
    if (ret == 0) {
      // Does not delete this immediately; defers the
      //   process until the next cached texture load.
      SK::D3D9::tex_mgr.removeTexture (this);
    }
  
    return ret;
  }
  
  /*** IDirect3DBaseTexture9 methods ***/
  STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice) {
    tex_log.Log (L"[ Tex. Mgr ] ISKTextureD3D9::GetDevice (%ph)", ppDevice);
    return pTex->GetDevice (ppDevice);
  }
  STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags)
  {
    if (config.system.log_level > 1)
    {
      tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::SetPrivateData (%x, %ph, %lu, %x)",
                      refguid,
                        pData,
                          SizeOfData,
                            Flags );
    }

    return pTex->SetPrivateData (refguid, pData, SizeOfData, Flags);
  }
  STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData)
  {
    if (config.system.log_level > 1)
    {
      tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::GetPrivateData (%x, %ph, %lu)",
                      refguid,
                        pData,
                          *pSizeOfData );
    }

    return pTex->GetPrivateData (refguid, pData, pSizeOfData);
  }
  STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid) {
    tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::FreePrivateData (%x)",
                    refguid );
  
    return pTex->FreePrivateData (refguid);
  }
  STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew) {
    tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::SetPriority (%lu)",
                    PriorityNew );
  
    return pTex->SetPriority (PriorityNew);
  }
  STDMETHOD_(DWORD, GetPriority)(THIS) {
    tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::GetPriority ()" );
  
    return pTex->GetPriority ();
  }
  STDMETHOD_(void, PreLoad)(THIS) {
    tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::PreLoad ()" );
  
    pTex->PreLoad ();
  }
  STDMETHOD_(D3DRESOURCETYPE, GetType)(THIS) {
    tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::GetType ()" );
  
    return pTex->GetType ();
  }
  STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew) {
    tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::SetLOD (%lu)",
                     LODNew );
  
    return pTex->SetLOD (LODNew);
  }
  STDMETHOD_(DWORD, GetLOD)(THIS) {
    tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::GetLOD ()" );
  
    return pTex->GetLOD ();
  }
  STDMETHOD_(DWORD, GetLevelCount)(THIS)
  {
    if (config.system.log_level > 1)
    {
      tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::GetLevelCount ()" );
    }
  
    return pTex->GetLevelCount ();
  }
  STDMETHOD(SetAutoGenFilterType)(THIS_ D3DTEXTUREFILTERTYPE FilterType) {
    tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::SetAutoGenFilterType (%x)",
                    FilterType );
  
    return pTex->SetAutoGenFilterType (FilterType);
  }
  STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS) {
    tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::GetAutoGenFilterType ()" );
  
    return pTex->GetAutoGenFilterType ();
  }
  STDMETHOD_(void, GenerateMipSubLevels)(THIS) {
    tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::GenerateMipSubLevels ()" );
  
    pTex->GenerateMipSubLevels ();
  }
  STDMETHOD(GetLevelDesc)(THIS_ UINT Level,D3DSURFACE_DESC *pDesc)
  {
    if (config.system.log_level > 1)
    {
      tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::GetLevelDesc (%lu, %ph)",
                     Level,
                       pDesc );
    }

    return pTex->GetLevelDesc (Level, pDesc);
  }
  STDMETHOD(GetSurfaceLevel)(THIS_ UINT Level,IDirect3DSurface9** ppSurfaceLevel)
  {
    if (config.system.log_level > 1)
    {
      tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::GetSurfaceLevel (%lu, %ph)",
                      Level,
                        ppSurfaceLevel );
    }

    return pTex->GetSurfaceLevel (Level, ppSurfaceLevel);
  }
  STDMETHOD(LockRect)(THIS_ UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
  {
    if (config.system.log_level > 1)
    {
      tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::LockRect (%lu, %ph, %ph, %x)",
                      Level,
                        pLockedRect,
                          pRect,
                            Flags );
    }

    HRESULT hr =
      pTex->LockRect (Level, pLockedRect, pRect, Flags);

    if (SUCCEEDED (hr))
    {
      dirty = true;

      locks [Level] = *pLockedRect;

      if (Level == 0)
        QueryPerformanceCounter_Original (&begin_map);
    }

    return hr;
  }
  STDMETHOD(UnlockRect)(THIS_ UINT Level)
  {
    if (config.system.log_level > 1)
    {
      tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::UnlockRect (%lu)", Level );
    }

    if (tex_crc32c == 0x00 && dirty && locks.count (Level) && Level == 0)
    {
      auto sptr =
        static_cast <const uint8_t *> (
          locks [Level].pBits
        );

      uint32_t crc32c_ = 0x00;

      D3DSURFACE_DESC desc = { };
      pTex->GetLevelDesc (Level, &desc);

      for (size_t h = 0; h < desc.Height; ++h)
      {
        uint32_t row_checksum = 
          safe_crc32c (crc32c_, sptr, locks [Level].Pitch);

        if (row_checksum != 0x00)
          crc32c_ = row_checksum;

        sptr += locks [Level].Pitch;
      }

      this->tex_crc32c = crc32c_;
      this->tex_size   = locks [Level].Pitch * desc.Height;

      ////tex_log.Log ("Texture CRC3C2: %x", crc32c_);

      //if (desc.Pool != D3DPOOL_SYSTEMMEM)
      {
         SK::D3D9::Texture* pCacheTex =
           new SK::D3D9::Texture ();
         
         pCacheTex->crc32c = this->tex_crc32c;
         
         pCacheTex->d3d9_tex = this;
         pCacheTex->d3d9_tex->AddRef ();
         pCacheTex->refs++;
         
         QueryPerformanceCounter_Original (&end_map);

         pCacheTex->load_time = (float)( 1000.0 *
                                  (double)( end_map.QuadPart - begin_map.QuadPart ) /
                                   (double)SK_GetPerfFreq ().QuadPart );

         SK::D3D9::tex_mgr.addTexture (crc32c_, pCacheTex, tex_size);
      }

      locks.erase (Level);

      //if (height > 1) height >>= 1;
    }
    dirty = false;

    HRESULT hr =
      pTex->UnlockRect (Level);

    return hr;
  }
  STDMETHOD(AddDirtyRect)(THIS_ CONST RECT* pDirtyRect)
  {
    if (config.system.log_level > 1)
    {
      tex_log.Log ( L"[ Tex. Mgr ] ISKTextureD3D9::AddDirtyRect (...)" );
    }

    HRESULT hr = 
      pTex->AddDirtyRect (pDirtyRect);

    if (SUCCEEDED (hr))
    {
      dirty = true;
    }

    return hr;
  }
  
  bool               can_free;      // Whether or not we can free this texture
  bool               must_block;    // Whether or not to draw using this texture before its
                                    //  override finishes streaming

  bool               dirty  = false;//   If the game has changed this texture
  
  IDirect3DTexture9* pTex;          // The original texture data
  SSIZE_T            tex_size;      //   Original data size
  uint32_t           tex_crc32c;    //   Original data checksum
  
  IDirect3DTexture9* pTexOverride;  // The overridden texture data (nullptr if unchanged)
  SSIZE_T            override_size; //   Override data size
  
  ULONG              refs;
  LARGE_INTEGER      last_used;     // The last time this texture was used (for rendering)
                                    //   different from the last time referenced, this is
                                    //     set when SetTexture (...) is called.
                                    //     
                                    //     

  std::map    <UINT, D3DLOCKED_RECT> locks;
  LARGE_INTEGER      begin_map;
  LARGE_INTEGER      end_map;
};

typedef HRESULT (STDMETHODCALLTYPE *D3DXCreateTextureFromFileInMemoryEx_pfn)
(
  _In_        LPDIRECT3DDEVICE9  pDevice,
  _In_        LPCVOID            pSrcData,
  _In_        UINT               SrcDataSize,
  _In_        UINT               Width,
  _In_        UINT               Height,
  _In_        UINT               MipLevels,
  _In_        DWORD              Usage,
  _In_        D3DFORMAT          Format,
  _In_        D3DPOOL            Pool,
  _In_        DWORD              Filter,
  _In_        DWORD              MipFilter,
  _In_        D3DCOLOR           ColorKey,
  _Inout_opt_ D3DXIMAGE_INFO     *pSrcInfo,
  _Out_opt_   PALETTEENTRY       *pPalette,
  _Out_       LPDIRECT3DTEXTURE9 *ppTexture
);

typedef HRESULT (STDMETHODCALLTYPE *D3DXSaveTextureToFile_pfn)(
  _In_           LPCWSTR                 pDestFile,
  _In_           D3DXIMAGE_FILEFORMAT    DestFormat,
  _In_           LPDIRECT3DBASETEXTURE9  pSrcTexture,
  _In_opt_ const PALETTEENTRY           *pSrcPalette
);

typedef HRESULT (WINAPI *D3DXSaveSurfaceToFile_pfn)
(
  _In_           LPCWSTR               pDestFile,
  _In_           D3DXIMAGE_FILEFORMAT  DestFormat,
  _In_           LPDIRECT3DSURFACE9    pSrcSurface,
  _In_opt_ const PALETTEENTRY         *pSrcPalette,
  _In_opt_ const RECT                 *pSrcRect
);

typedef HRESULT (STDMETHODCALLTYPE *CreateTexture_pfn)
(
  IDirect3DDevice9   *This,
  UINT                Width,
  UINT                Height,
  UINT                Levels,
  DWORD               Usage,
  D3DFORMAT           Format,
  D3DPOOL             Pool,
  IDirect3DTexture9 **ppTexture,
  HANDLE             *pSharedHandle
);

typedef HRESULT (STDMETHODCALLTYPE *CreateRenderTarget_pfn)
(
  IDirect3DDevice9     *This,
  UINT                  Width,
  UINT                  Height,
  D3DFORMAT             Format,
  D3DMULTISAMPLE_TYPE   MultiSample,
  DWORD                 MultisampleQuality,
  BOOL                  Lockable,
  IDirect3DSurface9   **ppSurface,
  HANDLE               *pSharedHandle
);

typedef HRESULT (STDMETHODCALLTYPE *CreateDepthStencilSurface_pfn)
(
  IDirect3DDevice9     *This,
  UINT                  Width,
  UINT                  Height,
  D3DFORMAT             Format,
  D3DMULTISAMPLE_TYPE   MultiSample,
  DWORD                 MultisampleQuality,
  BOOL                  Discard,
  IDirect3DSurface9   **ppSurface,
  HANDLE               *pSharedHandle
);

typedef HRESULT (STDMETHODCALLTYPE *SetTexture_pfn)(
  _In_ IDirect3DDevice9      *This,
  _In_ DWORD                  Sampler,
  _In_ IDirect3DBaseTexture9 *pTexture
);

typedef HRESULT (STDMETHODCALLTYPE *SetRenderTarget_pfn)(
  _In_ IDirect3DDevice9      *This,
  _In_ DWORD                  RenderTargetIndex,
  _In_ IDirect3DSurface9     *pRenderTarget
);

typedef HRESULT (STDMETHODCALLTYPE *SetDepthStencilSurface_pfn)(
  _In_ IDirect3DDevice9      *This,
  _In_ IDirect3DSurface9     *pNewZStencil
);