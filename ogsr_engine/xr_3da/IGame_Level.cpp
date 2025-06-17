#include "stdafx.h"
#include "igame_level.h"
#include "igame_persistent.h"

#include "x_ray.h"
#include "std_classes.h"
#include "customHUD.h"
#include "render.h"
#include "gamefont.h"
#include "xrLevel.h"
#include "CameraManager.h"

ENGINE_API IGame_Level* g_pGameLevel = NULL;

IGame_Level::IGame_Level()
{
    m_pCameras = xr_new<CCameraManager>(true);

    bReady = false;
    pCurrentEntity = nullptr;
    pCurrentViewEntity = nullptr;

    g_pGameLevel = this; // that is for pure_relcase
}

IGame_Level::~IGame_Level()
{
    // Render-level unload
    Render->level_Unload();
    xr_delete(m_pCameras);

    // Unregister
    Device.seqRender.Remove(this);
    Device.seqFrame.Remove(this);
    CCameraManager::ResetPP();

    g_pGameLevel = nullptr;

    xr_delete(g_hud);
}

void IGame_Level::net_Stop()
{
    for (int i = 0; i < 6; i++)
        Objects.Update(true);

    // Destroy all objects
    Objects.Unload();
    R_ASSERT(Objects.o_count() == 0, "Objects not empty on level unload!");

    IR_Release();

    bReady = false;
}

//-------------------------------------------------------------------------------------------

BOOL IGame_Level::Load(u32 dwNum)
{
    // Open
    g_pGamePersistent->LoadTitle("st_opening_stream");
    IReader* LL_Stream = FS.r_open(fsgame::level, fsgame::level_files::level);

    // Header
    hdrLEVEL H;
    LL_Stream->r_chunk_safe(fsL_HEADER, &H, sizeof(H));
    R_ASSERT(XRCL_PRODUCTION_VERSION == H.XRLC_version, "Incompatible level version.");

    // CForms
    g_pGamePersistent->LoadTitle("st_loading_cform");
    ObjectSpace.Load();

    if (!g_hud)
        g_hud = (CCustomHUD*)NEW_INSTANCE(CLSID_HUDMANAGER);

    Render->level_Load(LL_Stream);

    // Done
    FS.r_close(LL_Stream);

    // Objects
    g_pGamePersistent->Environment().mods_load();
    R_ASSERT(Load_GameSpecific_Before());
    Objects.Load();

    bReady = true;
    IR_Capture();
    Device.seqRender.Add(this);
    Device.seqFrame.Add(this);

    return TRUE;
}

void IGame_Level::OnRender()
{}

void IGame_Level::OnFrame()
{
    ZoneScoped;

    // Update all objects
    VERIFY(bReady);
    Objects.Update(false);
    g_hud->OnFrame();

    // Ambience
    if (Sounds_Random.size() && (Device.dwTimeGlobal > Sounds_Random_dwNextTime))
    {
        Sounds_Random_dwNextTime = Device.dwTimeGlobal + ::Random.randI(10000, 20000);
        Fvector pos;
        pos.random_dir().normalize().mul(::Random.randF(30, 100)).add(Device.vCameraPosition);
        int id = ::Random.randI(Sounds_Random.size());
        if (Sounds_Random_Enabled)
        {
            Sounds_Random[id].play_at_pos(nullptr, pos, 0);
            Sounds_Random[id].set_volume(1.f);
            Sounds_Random[id].set_range(10, 200);
        }
    }
}

void IGame_Level::LL_CheckTextures()
{
    u32 m_base, c_base, m_lmaps, c_lmaps;
    Device.m_pRender->ResourcesGetMemoryUsage(m_base, c_base, m_lmaps, c_lmaps);

    Msg("* t-report - base: %d, %d K", c_base, m_base / 1024);
    Msg("* t-report - lmap: %d, %d K", c_lmaps, m_lmaps / 1024);
}
