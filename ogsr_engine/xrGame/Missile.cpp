#include "stdafx.h"
#include "missile.h"
#include "PhysicsShell.h"
#include "actor.h"
#include "torch.h"
#include "xrserver_objects_alife.h"
#include "level.h"
#include "xr_level_controller.h"
#include "../Include/xrRender/Kinematics.h"
#include "ai_object_location.h"
#include "ExtendedGeom.h"
#include "characterphysicssupport.h"
#include "inventory.h"
#ifdef DEBUG
#include "phdebug.h"
#endif
#include "../xr_3da/x_ray.h"

#include "ui/UIProgressShape.h"
#include "ui/UIXmlInit.h"

#include "CalculateTriangle.h"
#include "tri-colliderknoopc/dcTriangle.h"

CUIProgressShape* g_MissileForceShape = NULL;

void create_force_progress()
{
    VERIFY(!g_MissileForceShape);
    CUIXml uiXml;
    bool xml_result = uiXml.Init(CONFIG_PATH, UI_PATH, "grenade.xml");
    R_ASSERT3(xml_result, "xml file not found", "grenade.xml");

    CUIXmlInit xml_init;
    g_MissileForceShape = xr_new<CUIProgressShape>();
    xml_init.InitProgressShape(uiXml, "progress", 0, g_MissileForceShape);
}

CMissile::CMissile(void)
{
    m_dwStateTime = 0;
    m_throwMotionMarksAvailable = false;
}

CMissile::~CMissile(void)
{
    HUD_SOUND::DestroySound(sndPlaying);
    HUD_SOUND::DestroySound(sndItemOn);
}

void CMissile::reinit()
{
    inherited::reinit();
    m_throw = false;
    m_constpower = false;
    m_fThrowForce = 0;
    m_dwDestroyTime = 0xffffffff;
    SetPending(FALSE);
    m_fake_missile = nullptr;
    SetState(eHidden);
}

void CMissile::Load(LPCSTR section)
{
    inherited::Load(section);

    m_fMinForce = pSettings->r_float(section, "force_min");
    m_fConstForce = pSettings->r_float(section, "force_const");
    m_fMaxForce = pSettings->r_float(section, "force_max");
    m_fForceGrowSpeed = pSettings->r_float(section, "force_grow_speed");

    m_dwDestroyTimeMax = pSettings->r_u32(section, "destroy_time");

    m_vThrowPoint = pSettings->r_fvector3(section, "throw_point");
    m_vThrowDir = pSettings->r_fvector3(section, "throw_dir");
    m_vHudThrowPoint = pSettings->r_fvector3(*hud_sect, "throw_point");
    m_vHudThrowDir = pSettings->r_fvector3(*hud_sect, "throw_dir");

    if (pSettings->line_exist(section, "snd_playing"))
        HUD_SOUND::LoadSound(section, "snd_playing", sndPlaying);

    if (pSettings->line_exist(section, "snd_item_on"))
        HUD_SOUND::LoadSound(section, "snd_item_on", sndItemOn);

    m_ef_weapon_type = READ_IF_EXISTS(pSettings, r_u32, section, "ef_weapon_type", u32(-1));

    m_safe_dist_to_explode = READ_IF_EXISTS(pSettings, r_float, section, "safe_dist_to_explode", 0);
    m_kick_on_explode = READ_IF_EXISTS(pSettings, r_bool, section, "explosion_on_kick", false);
    m_explode_by_timer_on_safe_dist = READ_IF_EXISTS(pSettings, r_bool, section, "explode_by_timer_on_safe_dist", true);
	
	if (pSettings->line_exist(section, "checkout_bones"))
	{
		m_sCheckoutBones.clear();
		LPCSTR lineStr = pSettings->r_string(section, "checkout_bones");
		for (int j = 0, cnt = _GetItemCount(lineStr); j < cnt; ++j)
		{
			string128 bone_name;
			_GetItem(lineStr, j, bone_name);
			m_sCheckoutBones.push_back(bone_name);
		}
	}
}

BOOL CMissile::net_Spawn(CSE_Abstract* DC)
{
    BOOL l_res = inherited::net_Spawn(DC);

    dwXF_Frame = 0xffffffff;

    m_throw_direction.set(0.0f, 1.0f, 0.0f);
    m_throw_matrix.identity();

    return l_res;
}

void CMissile::net_Destroy()
{
    inherited::net_Destroy();
    m_fake_missile = 0;
}

void CMissile::OnActiveItem()
{
    SwitchState(eShowing);
    inherited::OnActiveItem();
    SetState(eIdle);
    SetNextState(eIdle);
}

void CMissile::OnHiddenItem()
{
    SwitchState(eHiding);
    inherited::OnHiddenItem();
    SetState(eHidden);
    SetNextState(eHidden);
}

void CMissile::spawn_fake_missile()
{
    if (!getDestroy())
    {
        CSE_Abstract* object = Level().spawn_item(*cNameSect(), Position(), ai_location().level_vertex_id(), ID(), true);

        CSE_ALifeObject* alife_object = smart_cast<CSE_ALifeObject*>(object);
        VERIFY(alife_object);
        alife_object->m_flags.set(CSE_ALifeObject::flCanSave, FALSE);

        NET_Packet P;
        object->Spawn_Write(P, TRUE);
        Level().Send(P, net_flags(TRUE));
        F_entity_Destroy(object);
    }
}

void CMissile::OnH_A_Chield()
{
    inherited::OnH_A_Chield();
}

void CMissile::OnH_B_Independent(bool just_before_destroy)
{
    inherited::OnH_B_Independent(just_before_destroy);

    if (!just_before_destroy)
    {
        VERIFY(PPhysicsShell());
        PPhysicsShell()->SetAirResistance(0.f, 0.f);
        PPhysicsShell()->set_DynamicScales(1.f, 1.f);

        if (GetState() == eThrow)
        {
            Msg("Throw on reject");
            Throw();
        }
    }

    if (!m_dwDestroyTime && Local())
    {
        DestroyObject();
        return;
    }
}

extern int g_bHudAdjustMode;

void CMissile::DeviceUpdate()
{
    if (auto pA = smart_cast<CActor*>(H_Parent()))
    {
        if (HeadLampSwitch)
        {
            auto pActorTorch = smart_cast<CTorch*>(pA->inventory().ItemFromSlot(TORCH_SLOT));
            pActorTorch->Switch();
            HeadLampSwitch = false;
        }
        else if (NightVisionSwitch)
        {
            if (auto pActorTorch = smart_cast<CTorch*>(pA->inventory().ItemFromSlot(TORCH_SLOT)))
                pActorTorch->SwitchNightVision();
            NightVisionSwitch = false;
        }
    }
}

void CMissile::UpdateCL()
{
    inherited::UpdateCL();

    if (!Core.Features.test(xrCore::Feature::stop_anim_playing))
    {
        CActor* pActor = smart_cast<CActor*>(H_Parent());
        if (pActor && !(pActor->get_state() & EMoveCommand::mcAnyMove) && this == pActor->inventory().ActiveItem())
        {
            if (g_bHudAdjustMode == 0 && GetState() == eIdle && (Device.dwTimeGlobal - m_dw_curr_substate_time > 20000))
            {
                SwitchState(eBore);
                ResetSubStateTime();
            }
        }
    }

    if (GetState() == eReady)
    {
        if (m_throw)
        {
            SwitchState(eThrow);
        }
        else
        {
            CActor* actor = smart_cast<CActor*>(H_Parent());
            if (actor)
            {
                m_fThrowForce += (m_fForceGrowSpeed * Device.dwTimeDelta) * .001f;
                clamp(m_fThrowForce, m_fMinForce, m_fMaxForce);
            }
        }
    }
}

void CMissile::shedule_Update(u32 dt)
{
    inherited::shedule_Update(dt);
    if (!H_Parent() && getVisible() && m_pPhysicsShell)
    {
        if (m_dwDestroyTime <= Level().timeServer())
        {
            m_dwDestroyTime = 0xffffffff;
            VERIFY(!m_pCurrentInventory);
            Destroy();
            return;
        }
    }
	
	if (!Useful())
	{
		IKinematics* pGrenadeVisual = dynamic_cast<IKinematics*>(Visual());
		u16 bone_id;

		pGrenadeVisual->CalculateBones_Invalidate();
		for (const auto& boneName : m_sCheckoutBones)
		{
			bone_id = pGrenadeVisual->LL_BoneID(boneName);
			if (bone_id != BI_NONE && pGrenadeVisual->LL_GetBoneVisible(bone_id))
				pGrenadeVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
		}
		pGrenadeVisual->CalculateBones_Invalidate();
		pGrenadeVisual->CalculateBones(TRUE);
	}
}

void CMissile::State(u32 state, u32 oldState)
{
    switch (state)
    {
    case eShowing: 
    {
        SetPending(TRUE);
        PlayHUDMotion({"anim_show", "anm_show"}, false, GetState());
    }
    break;
    case eIdle: 
    {
        SetPending(FALSE);
        PlayAnimIdle();
    }
    break;
    case eHiding: 
    {
        if (H_Parent())
        {
            if (oldState != eHiding)
            {
                SetPending(TRUE);
                PlayHUDMotion({"anim_hide", "anm_hide"}, true, GetState());
            }
        }
    }
    break;
    case eHidden:
    {
        StopCurrentAnimWithoutCallback();

        if (H_Parent())
        {
            setVisible(FALSE);
            setEnabled(FALSE);
        }
        SetPending(FALSE);
    }
    break;
    case eThrowStart: 
    {
        SetPending(TRUE);
        m_fThrowForce = m_fMinForce;
        PlayHUDMotion({"anim_throw_begin", "anm_throw_begin"}, true, GetState());
    }
    break;
    case eReady: 
    {
        PlayHUDMotion({"anim_throw_idle", "anm_throw_idle"}, true, GetState());
    }
    break;
    case eThrow: 
    {
        SetPending(TRUE);
        m_throw = false;
        PlayHUDMotion({"anim_throw_act", "anm_throw"}, true, GetState());
        m_throwMotionMarksAvailable = !m_current_motion_def->marks.empty();
    }
    break;
    case eThrowEnd: 
    {
        PlayHUDMotion({"anim_throw_end", "anm_throw_end"}, true, GetState());
        if (m_throwMotionMarksAvailable)
            SwitchState(eShowing);
        else
            SetPending(TRUE);
    }
    break;
    case eBore: 
    {
        PlaySound(sndPlaying, Position());
        PlayHUDMotion({"anim_playing", "anm_bore"}, true, GetState());
    }
    break;
    case eDeviceSwitch: 
    {
        SetPending(TRUE);
        PlayAnimDeviceSwitch();
    }
    break;
    }
}

void CMissile::PlayAnimIdle()
{
    if (TryPlayAnimIdle())
        return;

    PlayHUDMotion({"anim_idle", "anm_idle"}, true, GetState());
}

void CMissile::PlayAnimDeviceSwitch()
{
    PlaySound(sndItemOn, Position());

    if (AnimationExist("anm_headlamp_on"))
        PlayHUDMotion("anm_headlamp_on", true, GetState());
    else
    {
        DeviceUpdate();
        SwitchState(eIdle);
    }
}

void CMissile::OnStateSwitch(u32 S, u32 oldState)
{
    inherited::OnStateSwitch(S, oldState);
    State(S, oldState);
}

void CMissile::OnAnimationEnd(u32 state)
{
    switch (state)
    {
    case eHiding: 
    {
        setVisible(FALSE);
        SwitchState(eHidden);
    }
    break;
    case eShowing: 
    {
        setVisible(TRUE);
        SwitchState(eIdle);
    }
    break;
    case eIdle: 
    {
        SwitchState(eIdle);
    }
    break;
    case eThrowStart: 
    {
        if (!m_fake_missile && !smart_cast<CMissile*>(H_Parent()))
            spawn_fake_missile();

        if (m_throw)
            SwitchState(eThrow);
        else
            SwitchState(eReady);
    }
    break;
    case eThrow: 
    {
        if (!m_throwMotionMarksAvailable)
            Throw();
        SwitchState(eThrowEnd);
    }
    break;
    case eThrowEnd: 
    {
        SwitchState(eShowing);
    }
    break;
    case eBore:
    case eDeviceSwitch: 
    {
        SwitchState(eIdle);
    }
    break;
    default: 
        inherited::OnAnimationEnd(state);
    }
}

void CMissile::OnMotionMark(u32 state, const motion_marks& M)
{
    inherited::OnMotionMark(state, M);
    if (state == eThrow && !m_throw)
    {
        if (H_Parent())
            Throw();
    }
}

void CMissile::UpdatePosition(const Fmatrix& trans) { XFORM().mul(trans, offset()); }

void CMissile::UpdateXForm()
{
    if (Device.dwFrame != dwXF_Frame)
    {
        dwXF_Frame = Device.dwFrame;

        if (0 == H_Parent())
            return;

        // Get access to entity and its visual
        CEntityAlive* E = smart_cast<CEntityAlive*>(H_Parent());

        if (!E)
            return;

        const CInventoryOwner* parent = smart_cast<const CInventoryOwner*>(E);
        if (!parent || parent && parent->use_simplified_visual())
            return;

        if (parent->attached(this))
            return;

        VERIFY(E);
        IKinematics* V = smart_cast<IKinematics*>(E->Visual());
        VERIFY(V);

        // Get matrices
        int boneL, boneR, boneR2;
        E->g_WeaponBones(boneL, boneR, boneR2);

        boneL = boneR2;

        V->CalculateBones();
        Fmatrix& mL = V->LL_GetTransform(u16(boneL));
        Fmatrix& mR = V->LL_GetTransform(u16(boneR));

        // Calculate
        Fmatrix mRes;
        Fvector R, D, N;
        D.sub(mL.c, mR.c);
        D.normalize_safe();
        R.crossproduct(mR.j, D);
        R.normalize_safe();
        N.crossproduct(D, R);
        N.normalize_safe();
        mRes.set(R, N, D, mR.c);
        mRes.mulA_43(E->XFORM());
        UpdatePosition(mRes);
    }
}

void CMissile::Show(bool now)
{
    if (now)
    {
        OnAnimationEnd(eShowing);
        StopHUDSounds();
    }
    else
        SwitchState(eShowing);
}

void CMissile::Hide(bool now)
{
    if (now)
    {
        OnAnimationEnd(eHiding);
        StopHUDSounds();
    }
    else
        SwitchState(eHiding);
}

void CMissile::setup_throw_params()
{
    CEntity* entity = smart_cast<CEntity*>(H_Parent());
    VERIFY(entity);
    CInventoryOwner* inventory_owner = smart_cast<CInventoryOwner*>(H_Parent());
    VERIFY(inventory_owner);
    Fmatrix trans;
    trans.identity();
    Fvector FirePos, FireDir;
    if (this == inventory_owner->inventory().ActiveItem())
    {
#ifdef DEBUG
        CInventoryOwner* io = smart_cast<CInventoryOwner*>(H_Parent());
        if (NULL == io->inventory().ActiveItem())
        {
            Log("current_state", GetState());
            Log("next_state", GetNextState());
            Log("state_time", m_dwStateTime);
            Log("item_sect", cNameSect().c_str());
            Log("H_Parent", H_Parent()->cNameSect().c_str());
        }
#endif

        entity->g_fireParams(this, FirePos, FireDir);
    }
    else
    {
        FirePos = XFORM().c;
        FireDir = XFORM().k;
    }
    trans.k.set(FireDir);
    Fvector::generate_orthonormal_basis(trans.k, trans.j, trans.i);
    trans.c.set(FirePos);
    m_throw_matrix.set(trans);
    m_throw_direction.set(trans.k);
}

void CMissile::Throw()
{
    VERIFY(smart_cast<CEntity*>(H_Parent()));
    setup_throw_params();

    m_fake_missile->m_throw_direction = m_throw_direction;
    m_fake_missile->m_throw_matrix = m_throw_matrix;
    m_fake_missile->m_pOwner = smart_cast<CGameObject*>(H_Parent());

    CInventoryOwner* inventory_owner = smart_cast<CInventoryOwner*>(H_Parent());
    VERIFY(inventory_owner);
    if (inventory_owner->use_default_throw_force())
        m_fake_missile->m_fThrowForce = m_constpower ? m_fConstForce : m_fThrowForce;
    else
        m_fake_missile->m_fThrowForce = inventory_owner->missile_throw_force();

    if (Local() && H_Parent())
    {
        NET_Packet P;
        u_EventGen(P, GE_OWNERSHIP_REJECT, ID());
        P.w_u16(u16(m_fake_missile->ID()));
        u_EventSend(P);
    }
}

void CMissile::OnEvent(NET_Packet& P, u16 type)
{
    inherited::OnEvent(P, type);
    u16 id;
    switch (type)
    {
    case GE_OWNERSHIP_TAKE: 
    {
        P.r_u16(id);
        CMissile* missile = smart_cast<CMissile*>(Level().Objects.net_Find(id));
        m_fake_missile = missile;
        missile->H_SetParent(this);
        missile->Position().set(Position());
        break;
    }
    case GE_OWNERSHIP_REJECT: 
    {
        P.r_u16(id);
        if (m_fake_missile && (id == m_fake_missile->ID()))
        {
            m_fake_missile = NULL;
        }

        CMissile* missile = smart_cast<CMissile*>(Level().Objects.net_Find(id));
        if (!missile)
        {
            break;
        }
        missile->H_SetParent(0, !P.r_eof() && P.r_u8());
        break;
    }
    }
}

void CMissile::Destroy()
{
    if (Local())
        DestroyObject();
}

bool CMissile::Action(s32 cmd, u32 flags)
{
    if (inherited::Action(cmd, flags))
        return true;

    switch (cmd)
    {
    case kWPN_FIRE: 
    {
        m_constpower = true;
        if (flags & CMD_START)
        {
            if (GetState() == eIdle)
            {
                m_throw = true;
                SwitchState(eThrowStart);
            }
        }
        return true;
    }
    break;

    case kWPN_ZOOM: 
    {
        m_constpower = false;
        if (flags & CMD_START)
        {
            m_throw = false;
            if (GetState() == eIdle)
                SwitchState(eThrowStart);
            else if (GetState() == eReady)
            {
                m_throw = true;
            }
        }
        else if (GetState() == eReady || GetState() == eThrowStart || GetState() == eIdle)
        {
            m_throw = true;
            if (GetState() == eReady)
                SwitchState(eThrow);
        }
        return true;
    }
    break;
    case kTORCH: 
    {
        auto pActorTorch = smart_cast<CActor*>(H_Parent())->inventory().ItemFromSlot(TORCH_SLOT);
        if ((flags & CMD_START) && pActorTorch && GetState() == eIdle)
        {
            HeadLampSwitch = true;
            SwitchState(eDeviceSwitch);
        }
        return true;
    }
    break;
    case kNIGHT_VISION: 
    {
        auto pActorNv = smart_cast<CActor*>(H_Parent())->inventory().ItemFromSlot(IS_OGSR_GA ? NIGHT_VISION_SLOT : TORCH_SLOT);
        if ((flags & CMD_START) && pActorNv && GetState() == eIdle)
        {
            NightVisionSwitch = true;
            SwitchState(eDeviceSwitch);
        }
        return true;
    }
    break;
    }
    return false;
}

void CMissile::activate_physic_shell()
{
    if (!smart_cast<CMissile*>(H_Parent()))
    {
        inherited::activate_physic_shell();
        return;
    }

    Fvector l_vel;
    l_vel.set(m_throw_direction);
    l_vel.normalize_safe();
    l_vel.mul(m_fThrowForce);

    Fvector a_vel;
    CInventoryOwner* inventory_owner = smart_cast<CInventoryOwner*>(H_Root());
    if (inventory_owner && inventory_owner->use_throw_randomness())
    {
        float fi, teta, r;
        fi = ::Random.randF(0.f, 2.f * M_PI);
        teta = ::Random.randF(0.f, M_PI);
        r = ::Random.randF(2.f * M_PI, 3.f * M_PI);
        float rxy = r * _sin(teta);
        a_vel.set(rxy * _cos(fi), rxy * _sin(fi), r * _cos(teta));
    }
    else
        a_vel.set(0.f, 0.f, 0.f);

    XFORM().set(m_throw_matrix);

    CEntityAlive* entity_alive = smart_cast<CEntityAlive*>(H_Root());
    if (entity_alive && entity_alive->character_physics_support())
    {
        Fvector parent_vel;
        entity_alive->character_physics_support()->movement()->GetCharacterVelocity(parent_vel);
        l_vel.add(parent_vel);
    }

    VERIFY(!m_pPhysicsShell);
    create_physic_shell();
    m_pPhysicsShell->Activate(m_throw_matrix, l_vel, a_vel);
    m_pPhysicsShell->SetAllGeomTraced();
    m_pPhysicsShell->add_ObjectContactCallback(ExitContactCallback);
    m_pPhysicsShell->set_CallbackData(smart_cast<CPhysicsShellHolder*>(entity_alive));
    m_pPhysicsShell->SetAirResistance(0.f, 0.f);
    m_pPhysicsShell->set_DynamicScales(1.f, 1.f);

    IKinematics* kinematics = smart_cast<IKinematics*>(Visual());
    VERIFY(kinematics);
    kinematics->CalculateBones_Invalidate();
    kinematics->CalculateBones();
}

void CMissile::net_Relcase(CObject* O)
{
    inherited::net_Relcase(O);
    if (PPhysicsShell() && PPhysicsShell()->isActive())
    {
        if (O == smart_cast<CObject*>((CPhysicsShellHolder*)PPhysicsShell()->get_CallbackData()))
        {
            PPhysicsShell()->remove_ObjectContactCallback(ExitContactCallback);
            PPhysicsShell()->set_CallbackData(NULL);
        }
    }
}

void CMissile::create_physic_shell()
{
    CInventoryItemObject::CreatePhysicsShell();
}

void CMissile::setup_physic_shell()
{
    VERIFY(!m_pPhysicsShell);
    create_physic_shell();
    m_pPhysicsShell->Activate(XFORM(), 0, XFORM());
    IKinematics* kinematics = smart_cast<IKinematics*>(Visual());
    VERIFY(kinematics);
    kinematics->CalculateBones_Invalidate();
    kinematics->CalculateBones();
}

u32 CMissile::ef_weapon_type() const
{
    VERIFY(m_ef_weapon_type != u32(-1));
    return (m_ef_weapon_type);
}

void CMissile::OnDrawUI()
{
    if (GetState() == eReady && !m_throw)
    {
        CActor* actor = smart_cast<CActor*>(H_Parent());
        if (actor)
        {
            if (!g_MissileForceShape)
                create_force_progress();
            float k = (m_fThrowForce - m_fMinForce) / (m_fMaxForce - m_fMinForce);
            g_MissileForceShape->SetPos(k);
            g_MissileForceShape->Draw();
        }
    }
}

void CMissile::ExitContactCallback(bool& do_colide, bool bo1, dContact& c, SGameMtl* material_1, SGameMtl* material_2)
{
    dxGeomUserData *gd1 = NULL, *gd2 = NULL;
    if (bo1)
    {
        gd1 = retrieveGeomUserData(c.geom.g1);
        gd2 = retrieveGeomUserData(c.geom.g2);
    }
    else
    {
        gd2 = retrieveGeomUserData(c.geom.g1);
        gd1 = retrieveGeomUserData(c.geom.g2);
    }
    if (gd1 && gd2 && (CPhysicsShellHolder*)gd1->callback_data == gd2->ph_ref_object)
    {
        do_colide = false;
    }

    {
        dxGeomUserData* l_pUD1 = retrieveGeomUserData(c.geom.g1);
        dxGeomUserData* l_pUD2 = retrieveGeomUserData(c.geom.g2);

        SGameMtl* material;
        CMissile* l_this = l_pUD1 ? smart_cast<CMissile*>(l_pUD1->ph_ref_object) : NULL;
        if (!l_this)
        {
            l_this = l_pUD2 ? smart_cast<CMissile*>(l_pUD2->ph_ref_object) : NULL;
            material = material_1;
        }
        else
        {
            material = material_2;
        }

        VERIFY(material);
        if (material->Flags.is(SGameMtl::flPassable))
            return;

        if (!l_this || !l_this->m_kick_on_explode || l_this->has_already_contact)
            return;

        bool safe_to_explode = true;

        Fvector l_pos;
        l_pos.set(l_this->Position());

        if (l_this->m_pOwner)
        {
            if (!l_pUD1 || !l_pUD2)
            {
                dGeomID g = NULL;
                dxGeomUserData*& l_pUD = l_pUD1 ? l_pUD1 : l_pUD2;
                if (l_pUD1)
                    g = c.geom.g1;
                else
                    g = c.geom.g2;

                if (l_pUD->pushing_neg)
                {
                    Fvector velocity;
                    l_this->PHGetLinearVell(velocity);
                    if (velocity.square_magnitude() > EPS)
                    {
                        velocity.normalize();
                        Triangle neg_tri;
                        CalculateTriangle(l_pUD->neg_tri, g, neg_tri);
                        float cosinus = velocity.dotproduct(*((Fvector*)neg_tri.norm));
                        VERIFY(_valid(neg_tri.dist));
                        float dist = neg_tri.dist / cosinus;
                        velocity.mul(dist * 1.1f);
                        l_pos.sub(velocity);
                    }
                }
            }

            float dist = l_this->m_pOwner->Position().distance_to(l_pos);
            if (dist < l_this->m_safe_dist_to_explode)
            {
                safe_to_explode = false;

                if (!l_this->m_explode_by_timer_on_safe_dist)
                {
                    CActor* pActor = smart_cast<CActor*>(l_this->m_pOwner);
                    if (pActor)
                    {
                        u32 lvid = l_this->UsedAI_Locations() ? l_this->ai_location().level_vertex_id() : ai().level_graph().vertex_id(l_pos);
                        CSE_Abstract* object = Level().spawn_item(l_this->cNameSect().c_str(), l_pos, lvid, 0xffff, true);

                        NET_Packet P;
                        object->Spawn_Write(P, TRUE);
                        Level().Send(P, net_flags(TRUE));
                        F_entity_Destroy(object);
                    }

                    l_this->set_destroy_time(500);
                    l_this->DestroyObject();
                }
            }
        }

        if (safe_to_explode)
        {
            l_this->set_destroy_time(5);
        }

        l_this->has_already_contact = true;
    }
}

void CMissile::GetBriefInfo(xr_string& str_name, xr_string& icon_sect_name, xr_string& str_count)
{
    str_name = NameShort();
    str_count = "";
    icon_sect_name = "";
}
