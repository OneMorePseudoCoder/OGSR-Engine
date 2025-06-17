//---------------------------------------------------------------------------
#include "stdafx.h"

#include "GameMtlLib.h"

CGameMtlLibrary GMLib;

CGameMtlLibrary::CGameMtlLibrary()
{
    material_index = 0;
    material_pair_index = 0;
    material_count = 0;

    PGMLib = &GMLib;
}

void SGameMtl::Load(IReader& fs)
{
    R_ASSERT(fs.find_chunk(GAMEMTL_CHUNK_MAIN));
    ID = fs.r_u32();
    fs.r_stringZ(m_Name);

    if (fs.find_chunk(GAMEMTL_CHUNK_DESC))
    {
        fs.r_stringZ(m_Desc);
    }

    R_ASSERT(fs.find_chunk(GAMEMTL_CHUNK_FLAGS));
    Flags.assign(fs.r_u32());

    R_ASSERT(fs.find_chunk(GAMEMTL_CHUNK_PHYSICS));
    fPHFriction = fs.r_float();
    fPHDamping = fs.r_float();
    fPHSpring = fs.r_float();
    fPHBounceStartVelocity = fs.r_float();
    fPHBouncing = fs.r_float();

    R_ASSERT(fs.find_chunk(GAMEMTL_CHUNK_FACTORS));
    fShootFactor = fs.r_float();
    fBounceDamageFactor = fs.r_float();
    fVisTransparencyFactor = fs.r_float();
    fSndOcclusionFactor = fs.r_float();

    if (fs.find_chunk(GAMEMTL_CHUNK_FACTORS_MP))
        fs.r_float();

    if (fs.find_chunk(GAMEMTL_CHUNK_FLOTATION))
        fFlotationFactor = fs.r_float();

    if (fs.find_chunk(GAMEMTL_CHUNK_INJURIOUS))
        fInjuriousSpeed = fs.r_float();

    if (fs.find_chunk(GAMEMTL_CHUNK_DENSITY))
        fDensityFactor = fs.r_float();
}

void CGameMtlLibrary::Load()
{
    string_path name;
    if (!FS.exist(name, fsgame::game_data, GAMEMTL_FILENAME))
    {
        Msg("! Can't find game material file: [%s]", name);
        return;
    }

    R_ASSERT(material_pairs.empty());
    R_ASSERT(materials.empty());

    IReader* F = FS.r_open(name);

    R_ASSERT(F->find_chunk(GAMEMTLS_CHUNK_VERSION));
    u16 version = F->r_u16();
    if (GAMEMTL_CURRENT_VERSION != version)
    {
        Log("CGameMtlLibrary: invalid version. Library can't load.");
        FS.r_close(F);
        return;
    }

    R_ASSERT(F->find_chunk(GAMEMTLS_CHUNK_AUTOINC));
    material_index = F->r_u32();
    material_pair_index = F->r_u32();

    materials.clear();
    material_pairs.clear();

    
    CTimer timer;
    timer.Start();

    IReader* OBJ = F->open_chunk(GAMEMTLS_CHUNK_MTLS);
    if (OBJ)
    {
        u32 count;
        for (IReader* O = OBJ->open_chunk_iterator(count); O; O = OBJ->open_chunk_iterator(count, O))
        {
            SGameMtl* M = xr_new<SGameMtl>();
            M->Load(*O);
            materials.push_back(M);
        }
        OBJ->close();
    }

    OBJ = F->open_chunk(GAMEMTLS_CHUNK_MTLS_PAIR);
    if (OBJ)
    {
        u32 count;
        for (IReader* O = OBJ->open_chunk_iterator(count); O; O = OBJ->open_chunk_iterator(count, O))
        {
            SGameMtlPair* M = xr_new<SGameMtlPair>(this);
            M->Load(*O);
            material_pairs.push_back(M);
        }
        OBJ->close();
        loadSounds();
    }

    Msg("* [%s]: mtl pairs loading time (%u): [%.3f s.]", __FUNCTION__, material_pairs.size(), timer.GetElapsed_sec());

    material_count = (u32)materials.size();
    material_pairs_rt.resize(material_count * material_count, nullptr);

    for (const auto& S : material_pairs)
    {
        u16 idx_1 = GetMaterialIdx(S->mtl0);
        u16 idx_2 = GetMaterialIdx(S->mtl1);
        if (idx_1 >= materials.size() || idx_2 >= materials.size())
        {
            Msg("~ Wrong material pars: mtl0=[%d] mtl1=[%d]", S->mtl0, S->mtl1);
            continue;
        }
        int idx0 = idx_1 * material_count + idx_2;
        int idx1 = idx_2 * material_count + idx_1;
        material_pairs_rt[idx0] = S;
        material_pairs_rt[idx1] = S;
    }

    FS.r_close(F);
}

void CGameMtlLibrary::loadSounds()
{
    CTimer timer;
    timer.Start();

#pragma todo("SIMP: новый код падает, вероятно не хватает мьютексов где-то")
    for (auto* it : material_pairs)
    {
        it->CreateAllSounds();
    }

    Msg("* [%s]: sounds creating time: [%.3f s.]", __FUNCTION__, timer.GetElapsed_sec());
}

void CGameMtlLibrary::loadParticles()
{
    CTimer timer;
    timer.Start();

    for (auto& it : material_pairs)
    {
        TTAPI->submit_detach([](SGameMtlPair* pair) { pair->CreateAllParticles(); }, it);
    }

    TTAPI->wait_for_tasks();

    Msg("* [%s]: particles creating time: [%.3f s.]", __FUNCTION__, timer.GetElapsed_sec());
}

#ifdef DEBUG
LPCSTR SGameMtlPair::dbg_Name()
{
    static string256 nm;
    SGameMtl* M0 = GMLib.GetMaterialByID(GetMtl0());
    SGameMtl* M1 = GMLib.GetMaterialByID(GetMtl1());
    xr_sprintf(nm, sizeof(nm), "Pair: %s - %s", *M0->m_Name, *M1->m_Name);
    return nm;
}
#endif
