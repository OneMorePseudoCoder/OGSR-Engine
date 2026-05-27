#include "stdafx.h"
#include "actorcondition.h"
#include "actor.h"
#include "actorEffector.h"
#include "inventory.h"
#include "level.h"
#include "sleepeffector.h"
#include "game_base_space.h"
#include "xrserver.h"
#include "ai_space.h"
#include "script_callback_ex.h"
#include "script_game_object.h"
#include "game_object_space.h"
#include "object_broker.h"
#include "weapon.h"
#include "PDA.h"
#include "ai/monsters/BaseMonster/base_monster.h"

#define MAX_SATIETY 1.0f
#define START_SATIETY 0.5f

BOOL GodMode() { return psActorFlags.test(AF_GODMODE); }

CActorCondition::CActorCondition(CActor* object) : inherited(object)
{
    m_fJumpPower = 0.f;
    m_fStandPower = 0.f;
    m_fWalkPower = 0.f;
    m_fJumpWeightPower = 0.f;
    m_fWalkWeightPower = 0.f;
    m_fOverweightWalkK = 0.f;
    m_fOverweightJumpK = 0.f;
    m_fAccelK = 0.f;
    m_fSprintK = 0.f;
    m_fAlcohol = 0.f;
    m_fSatiety = 1.0f;
    m_fThirst = 1.0f;

    m_bJumpRequirePower = false;

    VERIFY(object);
    m_object = object;
    m_condition_flags.zero();

    m_f_time_affected = Device.fTimeGlobal;

    monsters_feel_touch = xr_new<Feel::Touch>();
    monsters_aura_radius = 0.f;

    m_max_power_restore_speed = 0.0f;
    m_max_wound_protection = 0.0f;
    m_max_fire_wound_protection = 0.0f;
}

CActorCondition::~CActorCondition(void) { xr_delete(monsters_feel_touch); }

void CActorCondition::LoadCondition(LPCSTR entity_section)
{
    inherited::LoadCondition(entity_section);

    LPCSTR section = READ_IF_EXISTS(pSettings, r_string, entity_section, "condition_sect", entity_section);

    m_fJumpPower = pSettings->r_float(section, "jump_power");
    m_fStandPower = pSettings->r_float(section, "stand_power");
    m_fWalkPower = pSettings->r_float(section, "walk_power");
    m_fJumpWeightPower = pSettings->r_float(section, "jump_weight_power");
    m_fWalkWeightPower = pSettings->r_float(section, "walk_weight_power");
    m_fOverweightWalkK = pSettings->r_float(section, "overweight_walk_k");
    m_fOverweightJumpK = pSettings->r_float(section, "overweight_jump_k");
    m_fAccelK = pSettings->r_float(section, "accel_k");
    m_fSprintK = pSettings->r_float(section, "sprint_k");

    m_bJumpRequirePower = READ_IF_EXISTS(pSettings, r_bool, section, "jump_require_power", false);

    //порог силы и здоровья меньше которого актер начинает хромать
    m_fLimpingHealthBegin = pSettings->r_float(section, "limping_health_begin");
    m_fLimpingHealthEnd = pSettings->r_float(section, "limping_health_end");
    R_ASSERT(m_fLimpingHealthBegin <= m_fLimpingHealthEnd);

    m_fLimpingPowerBegin = pSettings->r_float(section, "limping_power_begin");
    m_fLimpingPowerEnd = pSettings->r_float(section, "limping_power_end");
    R_ASSERT(m_fLimpingPowerBegin <= m_fLimpingPowerEnd);

    m_fCantWalkPowerBegin = pSettings->r_float(section, "cant_walk_power_begin");
    m_fCantWalkPowerEnd = pSettings->r_float(section, "cant_walk_power_end");
    R_ASSERT(m_fCantWalkPowerBegin <= m_fCantWalkPowerEnd);

    m_fCantSprintPowerBegin = pSettings->r_float(section, "cant_sprint_power_begin");
    m_fCantSprintPowerEnd = pSettings->r_float(section, "cant_sprint_power_end");
    R_ASSERT(m_fCantSprintPowerBegin <= m_fCantSprintPowerEnd);

    m_fPowerLeakSpeed = pSettings->r_float(section, "max_power_leak_speed");

    m_fV_Alcohol = pSettings->r_float(section, "alcohol_v");
    m_fV_Power = READ_IF_EXISTS(pSettings, r_float, section, "power_v", 0.0f);

    m_fV_Satiety = pSettings->r_float(section, "satiety_v");
    m_fV_SatietyPower = pSettings->r_float(section, "satiety_power_v");
    m_fV_SatietyHealth = pSettings->r_float(section, "satiety_health_v");

    m_fSatietyLightLimit = READ_IF_EXISTS(pSettings, r_float, section, "satiety_light_limit", 0.0f);
    clamp(m_fSatietyLightLimit, 0.0f, 1.0f);

    m_fSatietyCriticalLimit = READ_IF_EXISTS(pSettings, r_float, section, "satiety_critical_limit", 0.0f);
    clamp(m_fSatietyCriticalLimit, 0.0f, 1.0f);

    if (m_fSatietyCriticalLimit > m_fSatietyLightLimit)
    {
        m_fSatietyCriticalLimit = m_fSatietyLightLimit;
    }

    if (Core.Features.test(xrCore::Feature::actor_thirst))
    {
        m_fThirstLightLimit = READ_IF_EXISTS(pSettings, r_float, section, "thirst_light_limit", 0.0f);
        clamp(m_fThirstLightLimit, 0.0f, 1.0f);

        m_fThirstCriticalLimit = READ_IF_EXISTS(pSettings, r_float, section, "thirst_critical_limit", 0.0f);
        clamp(m_fThirstCriticalLimit, 0.0f, 1.0f);

        if (m_fThirstCriticalLimit > m_fThirstLightLimit)
        {
            m_fThirstCriticalLimit = m_fThirstLightLimit;
        }

        m_fV_Thirst = pSettings->r_float(section, "thirst_v");
        m_fV_ThirstPower = pSettings->r_float(section, "thirst_power_v");
        m_fV_ThirstHealth = pSettings->r_float(section, "thirst_health_v");
    }

    m_MaxWalkWeight = pSettings->r_float(section, "max_walk_weight");
}

//вычисление параметров с ходом времени
#include "UI.h"
#include "HUDManager.h"

void CActorCondition::UpdateCondition()
{
    if (GodMode())
        return;
    if (!object().g_Alive())
        return;
    if (!object().Local() && m_object != Level().CurrentViewEntity())
        return;

    float weight = object().GetCarryWeight();
    float max_weight;
    if (Core.Features.test(xrCore::Feature::condition_jump_weight_mod))
        max_weight = object().inventory().GetMaxWeight() + object().ArtefactsAddWeight(false);
    else
        max_weight = object().MaxCarryWeight();

    float weight_coef = weight / max_weight;

    if ((object().mstate_real & mcAnyMove))
    {
        ConditionWalk(weight_coef, isActorAccelerated(object().mstate_real, object().IsZoomAimingMode()), (object().mstate_real & mcSprint) != 0);
    }
    else
    {
        ConditionStand(weight_coef);
    }

    if (m_fPowerLeakSpeed > 0.0f)
    {
        float k_max_power = 1.0f;

        if (true)
        {
            float base_w = object().MaxCarryWeight();

            k_max_power = 1.0f + _min(weight, base_w) / base_w + _max(0.0f, (weight - base_w) / 10.0f);
        }
        else
            k_max_power = 1.0f;

        SetMaxPower(GetMaxPower() - m_fPowerLeakSpeed * m_fDeltaTime * k_max_power);
    }    

    m_fAlcohol += m_fV_Alcohol * m_fDeltaTime;
    clamp(m_fAlcohol, 0.0f, 1.0f);

    CEffectorCam* ce = Actor()->Cameras().GetCamEffector((ECamEffectorType)effAlcohol);
    if ((m_fAlcohol > 0.0001f))
    {
        if (!ce)
        {
            AddEffector(m_object, effAlcohol, "effector_alcohol", fastdelegate::MakeDelegate(this, &CActorCondition::GetAlcohol));
        }
    }
    else
    {
        if (ce)
            RemoveEffector(m_object, effAlcohol);
    }

    CEffectorPP* ppe = object().Cameras().GetPPEffector((EEffectorPPType)effPsyHealth);

    string64 pp_sect_name;
    shared_str ln = Level().name();
    strconcat(sizeof(pp_sect_name), pp_sect_name, "effector_psy_health", "_", *ln);
    if (!pSettings->section_exist(pp_sect_name))
        strcpy_s(pp_sect_name, "effector_psy_health");

    if (!fsimilar(GetPsyHealth(), 1.0f, 0.05f))
    {
        if (!ppe)
        {
            AddEffector(m_object, effPsyHealth, pp_sect_name, fastdelegate::MakeDelegate(this, &CActorCondition::GetPsy));
        }
    }
    else
    {
        if (ppe)
            RemoveEffector(m_object, effPsyHealth);
    }

    if (fis_zero(GetPsyHealth()))
        health() = 0.0f;

    UpdateSatiety();
    UpdateBoosters();

    if (Core.Features.test(xrCore::Feature::actor_thirst))
        UpdateThirst();

    inherited::UpdateCondition();

    UpdateTutorialThresholds();

    AffectDamage_InjuriousMaterialAndMonstersInfluence();
}

void CActorCondition::AffectDamage_InjuriousMaterialAndMonstersInfluence()
{
    float one = 0.1f;
    float tg = Device.fTimeGlobal;
    if (m_f_time_affected + one > tg)
    {
        return;
    }

    clamp(m_f_time_affected, tg - (one * 3), tg);

    float psy_influence = 0;
    float fire_influence = 0;
    float radiation_influence = GetInjuriousMaterialDamage(); // Get Radiation from Material

    // Add Radiation and Psy Level from Monsters
    monsters_feel_touch->feel_touch_update(object().Position(), monsters_aura_radius, {}, [&](const auto O) -> bool { const auto monster = smart_cast<CBaseMonster*>(O); return monster ? true : false; });
    for (const auto& it : monsters_feel_touch->feel_touch)
    {
        const auto monster = smart_cast<CBaseMonster*>(it);
        if (!monster)
            continue;
        psy_influence += monster->get_psy_influence();
        radiation_influence += monster->get_radiation_influence();
        fire_influence += monster->get_fire_influence();
    }
    /*
        CPda* const pda						=	m_object->GetPDA();

        if ( pda )
        {
            using monsters = xr_vector<CObject*>;

            for ( monsters::const_iterator	it	=	pda->feel_touch.begin();
                                            it	!=	pda->feel_touch.end();
                                            ++it )
            {
                CBaseMonster* const	monster		=	smart_cast<CBaseMonster*>(*it);
                if ( !monster || !monster->g_Alive() ) continue;

                psy_influence					+=	monster->get_psy_influence();
                radiation_influence				+=	monster->get_radiation_influence();
                fire_influence					+=	monster->get_fire_influence();
            }
        }
    */

    struct
    {
        ALife::EHitType type;
        float value;

    } 
    hits[] = {{ALife::eHitTypeRadiation, radiation_influence * one}, {ALife::eHitTypeTelepatic, psy_influence * one}, {ALife::eHitTypeBurn, fire_influence * one}};

    NET_Packet np;

    while (m_f_time_affected + one < tg)
    {
        m_f_time_affected += one;

        for (auto& hit : hits)
        {
            float damage = hit.value;
            ALife::EHitType type = hit.type;

            if (damage > EPS)
            {
                SHit HDS = SHit(damage, Fvector().set(0, 1, 0), nullptr, BI_NONE, Fvector().set(0, 0, 0), 0.0f, type, 0.0f, false);
                HDS.GenHeader(GE_HIT, m_object->ID());
                HDS.Write_Packet(np);
                CGameObject::u_EventSend(np);
            }
        }
    }
}

#include "characterphysicssupport.h"
float CActorCondition::GetInjuriousMaterialDamage()
{
    /*
        u16 mat_injurios = m_object->character_physics_support()->movement()->injurious_material_idx();

        if(mat_injurios!=GAMEMTL_NONE_IDX)
        {
            const SGameMtl* mtl		= GMLib.GetMaterialByIdx(mat_injurios);
            return					mtl->fInjuriousSpeed;
        }else
    */
    return 0.0f;
}

void CActorCondition::UpdateBoosters()
{
    for (u8 i = 0; i < eBoostMaxCount; i++)
    {
        BOOSTER_MAP::iterator it = m_booster_influences.find((EBoostParams)i);
        if (it != m_booster_influences.end())
        {
            it->second.fBoostTime -= m_fDeltaTime / Level().GetGameTimeFactor();
            if (it->second.fBoostTime <= 0.0f)
            {
                DisableBoostParameters(it->second);
                m_booster_influences.erase(it);
            }
        }
    }
}

void CActorCondition::UpdateSatiety()
{
    if (m_fSatiety > 0)
    {
        m_fSatiety -= m_fV_Satiety * m_fDeltaTime;
        clamp(m_fSatiety, 0.0f, 1.0f);
    }

    float satiety_health_koef = 1;
    float satiety_power_koef = 1;

    if (m_fSatietyLightLimit > 0)
    {
        if (m_fSatiety < m_fSatietyLightLimit)
        {
            satiety_power_koef = m_fSatiety / m_fSatietyLightLimit;

            const float critical_k = m_fSatietyCriticalLimit / m_fSatietyLightLimit;
            satiety_health_koef = (m_fSatiety / m_fSatietyLightLimit - critical_k) / (m_fSatiety >= m_fSatietyCriticalLimit ? 1 - critical_k : critical_k);
        }
    }
    else
    {
        if (fis_zero(m_fSatiety))
        {
            satiety_health_koef = -1;
        }
    }

    if (m_bIsBleeding && satiety_health_koef > 0)
    {
        satiety_health_koef = 0;
    }

    m_fDeltaHealth += m_fV_SatietyHealth * satiety_health_koef * m_fDeltaTime;
    m_fDeltaPower += m_fV_SatietyPower * satiety_power_koef * m_fDeltaTime;
}

void CActorCondition::UpdateThirst()
{
    if (m_fThirst > 0)
    {
        m_fThirst -= m_fV_Thirst * m_fDeltaTime;
        clamp(m_fThirst, 0.0f, 1.0f);
    }

    float thirst_health_koef = 1;
    float thirst_power_koef = 0;

    if (m_fThirstLightLimit > 0)
    {
        if (m_fThirst < m_fThirstLightLimit)
        {
            thirst_power_koef = (1 - m_fThirst / m_fThirstLightLimit) * -1;
            const float critical_k = m_fThirstCriticalLimit / m_fThirstLightLimit;
            thirst_health_koef = (m_fThirst / m_fThirstLightLimit - critical_k) / (m_fThirst >= m_fThirstCriticalLimit ? 1 - critical_k : critical_k);
        }
    }
    else
    {
        if (fis_zero(m_fThirst))
        {
            thirst_health_koef = -1;
        }
    }

    if (m_bIsBleeding && thirst_health_koef > 0)
    {
        thirst_health_koef = 0;
    }

    m_fDeltaHealth += m_fV_ThirstHealth * thirst_health_koef * m_fDeltaTime;
    m_fDeltaPower += m_fV_ThirstPower * thirst_power_koef * m_fDeltaTime;
}

void CActorCondition::UpdatePower()
{
    m_fPower += m_fV_Power * m_fDeltaTime;
    clamp(m_fPower, 0.0f, 1.0f);
}

CWound* CActorCondition::ConditionHit(SHit* pHDS)
{
    if (GodMode())
        return NULL;
    return inherited::ConditionHit(pHDS);
}

void CActorCondition::PowerHit(float power, bool apply_outfit)
{
    m_fPower -= apply_outfit ? HitPowerEffect(power) : power;
    clamp(m_fPower, 0.f, 1.f);
}

// weight - "удельный" вес от 0..1
void CActorCondition::ConditionJump(float weight)
{
    float power = m_fJumpPower;
    power += m_fJumpWeightPower * weight * (weight > 1.f ? m_fOverweightJumpK : 1.f);
    m_fPower -= HitPowerEffect(power);
}

void CActorCondition::ConditionWalk(float weight, bool accel, bool sprint)
{
    float power = m_fWalkPower;
    power += m_fWalkWeightPower * weight * (weight > 1.f ? m_fOverweightWalkK : 1.f);
    power *= m_fDeltaTime * (accel ? (sprint ? m_fSprintK : m_fAccelK) : 1.f);
    m_fPower -= HitPowerEffect(power);
}

void CActorCondition::ConditionStand(float weight)
{
    float power = m_fStandPower;
    power *= m_fDeltaTime;
    m_fPower -= power;
}

bool CActorCondition::IsCantWalk()
{
    if (m_fPower < m_fCantWalkPowerBegin)
        m_condition_flags.set(eCantWalk, TRUE);
    else if (m_fPower > m_fCantWalkPowerEnd)
        m_condition_flags.set(eCantWalk, FALSE);
    return m_condition_flags.test(eCantWalk);
}

#include "CustomOutfit.h"

bool CActorCondition::IsCantWalkWeight()
{
    if (!GodMode())
    {
        float max_w = m_MaxWalkWeight + object().ArtefactsAddWeight();

        CCustomOutfit* outfit = m_object->GetOutfit();
        if (outfit)
            max_w += outfit->m_additional_weight;

        if (object().GetCarryWeight() > max_w)
            return true;
    }
    return false;
}

bool CActorCondition::IsCantSprint()
{
    if (m_fPower < m_fCantSprintPowerBegin)
        m_condition_flags.set(eCantSprint, TRUE);
    else if (m_fPower > m_fCantSprintPowerEnd)
        m_condition_flags.set(eCantSprint, FALSE);
    return m_condition_flags.test(eCantSprint);
}

bool CActorCondition::IsCantJump(float weight)
{
    if (!m_bJumpRequirePower || GodMode())
    {
        return false;
    }

    float power = m_fJumpPower;
    power += m_fJumpWeightPower * weight * (weight > 1.f ? m_fOverweightJumpK : 1.f);
    return m_fPower < HitPowerEffect(power);
}

bool CActorCondition::IsLimping()
{
    if (m_fPower < m_fLimpingPowerBegin || GetHealth() < m_fLimpingHealthBegin)
        m_condition_flags.set(eLimping, TRUE);
    else if (m_fPower > m_fLimpingPowerEnd && GetHealth() > m_fLimpingHealthEnd)
        m_condition_flags.set(eLimping, FALSE);
    return m_condition_flags.test(eLimping);
}

extern bool g_bShowHudInfo;

void CActorCondition::save(NET_Packet& output_packet)
{
    inherited::save(output_packet);
    save_data(m_fAlcohol, output_packet);
    save_data(m_condition_flags, output_packet);
    save_data(m_fSatiety, output_packet);
    save_data(m_fThirst, output_packet);
    save_data(m_curr_medicine_influence.fHealth, output_packet);
    save_data(m_curr_medicine_influence.fPower, output_packet);
    save_data(m_curr_medicine_influence.fSatiety, output_packet);
    save_data(m_curr_medicine_influence.fRadiation, output_packet);
    save_data(m_curr_medicine_influence.fWoundsHeal, output_packet);
    save_data(m_curr_medicine_influence.fMaxPowerUp, output_packet);
    save_data(m_curr_medicine_influence.fAlcohol, output_packet);
    save_data(m_curr_medicine_influence.fTimeTotal, output_packet);
    save_data(m_curr_medicine_influence.fTimeCurrent, output_packet);
    save_data(m_curr_medicine_influence.fThirst, output_packet);

    output_packet.w_u8((u8)m_booster_influences.size());
    BOOSTER_MAP::iterator b = m_booster_influences.begin(), e = m_booster_influences.end();
    for (; b != e; b++)
    {
        output_packet.w_u8((u8)b->second.m_type);
        output_packet.w_float(b->second.fBoostValue);
        output_packet.w_float(b->second.fBoostTime);
    }
}

#include "alife_registry_wrappers.h"
#include "alife_simulator_header.h"

void CActorCondition::load(IReader& input_packet)
{
    inherited::load(input_packet);
    load_data(m_fAlcohol, input_packet);
    load_data(m_condition_flags, input_packet);
    load_data(m_fSatiety, input_packet);
    load_data(m_curr_medicine_influence.fHealth, input_packet);
    load_data(m_curr_medicine_influence.fPower, input_packet);
    load_data(m_curr_medicine_influence.fSatiety, input_packet);
    load_data(m_curr_medicine_influence.fRadiation, input_packet);
    load_data(m_curr_medicine_influence.fWoundsHeal, input_packet);
    load_data(m_curr_medicine_influence.fMaxPowerUp, input_packet);
    load_data(m_curr_medicine_influence.fAlcohol, input_packet);
    load_data(m_curr_medicine_influence.fTimeTotal, input_packet);
    load_data(m_curr_medicine_influence.fTimeCurrent, input_packet);
    load_data(m_curr_medicine_influence.fThirst, input_packet);

    u8 cntr = input_packet.r_u8();
    for (; cntr > 0; cntr--)
    {
        SBooster B;
        B.m_type = (EBoostParams)input_packet.r_u8();
        B.fBoostValue = input_packet.r_float();
        B.fBoostTime = input_packet.r_float();
        m_booster_influences[B.m_type] = B;
        BoostParameters(B, false);
    }

    if (ai().get_alife()->header().version() > 8)
    {
        load_data(m_fThirst, input_packet);
    }
}

void CActorCondition::reinit()
{
    inherited::reinit();
    m_condition_flags.set(eLimping, FALSE);
    m_condition_flags.set(eCantWalk, FALSE);
    m_condition_flags.set(eCantSprint, FALSE);
    m_fSatiety = 1.f;
    m_fAlcohol = 0.f;
    m_fThirst = 1.f;
}

void CActorCondition::ChangeAlcohol(float value) { m_fAlcohol += value; }

void CActorCondition::ChangeSatiety(float value)
{
    m_fSatiety += value;
    clamp(m_fSatiety, 0.0f, 1.0f);
}

void CActorCondition::ChangeThirst(float value)
{
    m_fThirst += value;
    clamp(m_fThirst, 0.0f, 1.0f);
}

bool CActorCondition::ApplyInfluence(const SMedicineInfluenceValues& V, const shared_str& sect)
{
    if (m_curr_medicine_influence.InProcess())
        return false;

    if (m_object->Local() && m_object == Level().CurrentViewEntity())
    {
        if (pSettings->line_exist(sect, "use_sound"))
        {
            if (m_use_sound._feedback())
                m_use_sound.stop();

            shared_str snd_name = pSettings->r_string(sect, "use_sound");
            m_use_sound.create(snd_name.c_str(), st_Effect, sg_SourceType);
            m_use_sound.play(NULL, sm_2D);
        }
    }

    if (V.fTimeTotal < 0.0f)
        return inherited::ApplyInfluence(V, sect);

    m_curr_medicine_influence = V;
    m_curr_medicine_influence.fTimeCurrent = m_curr_medicine_influence.fTimeTotal;
    return true;
}

bool CActorCondition::ApplyBooster(const SBooster& B, const shared_str& sect)
{
    if (!fis_zero(B.fBoostValue))
    {
        if (m_object->Local() && m_object == Level().CurrentViewEntity())
        {
            if (pSettings->line_exist(sect, "use_sound"))
            {
                if (m_use_sound._feedback())
                    m_use_sound.stop();
                shared_str snd_name = pSettings->r_string(sect, "use_sound");
                m_use_sound.create(snd_name.c_str(), st_Effect, sg_SourceType);
                m_use_sound.play(NULL, sm_2D);
            }
        }

        BOOSTER_MAP::iterator it = m_booster_influences.find(B.m_type);
        if (it != m_booster_influences.end())
            DisableBoostParameters((*it).second);
        m_booster_influences[B.m_type] = B;
        BoostParameters(B);
    }
    return true;
}

void CActorCondition::BoostParameters(const SBooster& B, bool need_change_tf)
{
    switch (B.m_type)
    {
        case eBoostHpRestore: BoostHpRestore(B.fBoostValue); break;
        case eBoostPowerRestore: BoostPowerRestore(B.fBoostValue); break;
        case eBoostRadiationRestore: BoostRadiationRestore(B.fBoostValue); break;
        case eBoostBleedingRestore: BoostBleedingRestore(B.fBoostValue); break;
        case eBoostMaxWeight: BoostMaxWeight(B.fBoostValue); break;
        case eBoostBurnImmunity: BoostBurnImmunity(B.fBoostValue); break;
        case eBoostShockImmunity: BoostShockImmunity(B.fBoostValue); break;
        case eBoostRadiationImmunity: BoostRadiationImmunity(B.fBoostValue); break;
        case eBoostTelepaticImmunity: BoostTelepaticImmunity(B.fBoostValue); break;
        case eBoostChemicalBurnImmunity: BoostChemicalBurnImmunity(B.fBoostValue); break;
        case eBoostExplImmunity: BoostExplImmunity(B.fBoostValue); break;
        case eBoostStrikeImmunity: BoostStrikeImmunity(B.fBoostValue); break;
        case eBoostFireWoundImmunity: BoostFireWoundImmunity(B.fBoostValue); break;
        case eBoostWoundImmunity: BoostWoundImmunity(B.fBoostValue); break;
        case eBoostRadiationProtection: BoostRadiationProtection(B.fBoostValue); break;
        case eBoostTelepaticProtection: BoostTelepaticProtection(B.fBoostValue); break;
        case eBoostChemicalBurnProtection: BoostChemicalBurnProtection(B.fBoostValue); break;
        case eBoostSatietyRestore: BoostSatietyRestore(B.fBoostValue); break;
        case eBoostThirstRestore: BoostThirstRestore(B.fBoostValue); break;
        case eBoostPsyHealthRestore: BoostPsyHealthRestore(B.fBoostValue); break;
        case eBoostAlcoholRestore: BoostAlcoholRestore(B.fBoostValue); break;
        case eBoostTimeFactor: need_change_tf ? BoostTimeFactor(B.fBoostValue) : BoostTimeFactor(0.0f); break;
        default: NODEFAULT;	
    }
}

void CActorCondition::DisableBoostParameters(const SBooster& B)
{
    switch (B.m_type)
    {
        case eBoostHpRestore: BoostHpRestore(-B.fBoostValue); break;
        case eBoostPowerRestore: BoostPowerRestore(-B.fBoostValue); break;
        case eBoostRadiationRestore: BoostRadiationRestore(-B.fBoostValue); break;
        case eBoostBleedingRestore: BoostBleedingRestore(-B.fBoostValue); break;
        case eBoostMaxWeight: BoostMaxWeight(-B.fBoostValue); break;
        case eBoostBurnImmunity: BoostBurnImmunity(-B.fBoostValue); break;
        case eBoostShockImmunity: BoostShockImmunity(-B.fBoostValue); break;
        case eBoostRadiationImmunity: BoostRadiationImmunity(-B.fBoostValue); break;
        case eBoostTelepaticImmunity: BoostTelepaticImmunity(-B.fBoostValue); break;
        case eBoostChemicalBurnImmunity: BoostChemicalBurnImmunity(-B.fBoostValue); break;
        case eBoostExplImmunity: BoostExplImmunity(-B.fBoostValue); break;
        case eBoostStrikeImmunity: BoostStrikeImmunity(-B.fBoostValue); break;
        case eBoostFireWoundImmunity: BoostFireWoundImmunity(-B.fBoostValue); break;
        case eBoostWoundImmunity: BoostWoundImmunity(-B.fBoostValue); break;
        case eBoostRadiationProtection: BoostRadiationProtection(-B.fBoostValue); break;
        case eBoostTelepaticProtection: BoostTelepaticProtection(-B.fBoostValue); break;
        case eBoostChemicalBurnProtection: BoostChemicalBurnProtection(-B.fBoostValue); break;
        case eBoostSatietyRestore: BoostSatietyRestore(-B.fBoostValue); break;
        case eBoostThirstRestore: BoostThirstRestore(-B.fBoostValue); break;
        case eBoostPsyHealthRestore: BoostPsyHealthRestore(-B.fBoostValue); break;
        case eBoostAlcoholRestore: BoostAlcoholRestore(-B.fBoostValue); break;
        case eBoostTimeFactor: BoostTimeFactor(-B.fBoostValue); break;
        default: NODEFAULT;	
    }
}

void CActorCondition::WoundForEach(const luabind::functor<bool>& funct)
{
    CEntityCondition::WOUND_VECTOR::const_iterator it = wounds().begin();
    CEntityCondition::WOUND_VECTOR::const_iterator it_e = wounds().end();
    for (; it != it_e; ++it)
    {
        if (funct(it) == true)
            break;
    }
}

void CActorCondition::BoosterForEach(const luabind::functor<bool>& funct)
{
    const auto& cur_booster_influences = GetCurBoosterInfluences();
    CEntityCondition::BOOSTER_MAP::const_iterator it = cur_booster_influences.begin();
    CEntityCondition::BOOSTER_MAP::const_iterator it_e = cur_booster_influences.end();
    for (; it != it_e; ++it)
    {
        if (funct((*it).first, (*it).second.fBoostTime, (*it).second.fBoostValue) == true)
            break;
    }
}

bool CActorCondition::ApplyBooster_script(const SBooster& B, LPCSTR sect)
{
    return ApplyBooster(B, sect);
}

void CActorCondition::ClearAllBoosters()
{
    const auto& cur_booster_influences = GetCurBoosterInfluences();
    CEntityCondition::BOOSTER_MAP::const_iterator it = cur_booster_influences.begin();
    CEntityCondition::BOOSTER_MAP::const_iterator it_e = cur_booster_influences.end();
    for (; it != it_e; ++it)
    {
        DisableBoostParameters((*it).second);
    }
}

void CActorCondition::BoostHpRestore(const float value)
{
    m_change_v.m_fV_HealthRestore += value;
}

void CActorCondition::BoostPowerRestore(const float value)
{
    m_fV_SatietyPower += value;
}

void CActorCondition::BoostRadiationRestore(const float value)
{
    m_change_v.m_fV_Radiation += value;
}

void CActorCondition::BoostBleedingRestore(const float value)
{
    m_change_v.m_fV_WoundIncarnation += value;
}

void CActorCondition::BoostMaxWeight(const float value)
{
    m_object->inventory().SetMaxWeight(object().inventory().GetMaxWeight()+value);
    m_MaxWalkWeight += value;
}

void CActorCondition::BoostBurnImmunity(const float value)
{
    m_fBoostBurnImmunity += value;
}

void CActorCondition::BoostShockImmunity(const float value)
{
    m_fBoostShockImmunity += value;
}

void CActorCondition::BoostRadiationImmunity(const float value)
{
    m_fBoostRadiationImmunity += value;
}

void CActorCondition::BoostTelepaticImmunity(const float value)
{
    m_fBoostTelepaticImmunity += value;
}

void CActorCondition::BoostChemicalBurnImmunity(const float value)
{
    m_fBoostChemicalBurnImmunity += value;
}

void CActorCondition::BoostExplImmunity(const float value)
{
    m_fBoostExplImmunity += value;
}

void CActorCondition::BoostStrikeImmunity(const float value)
{
    m_fBoostStrikeImmunity += value;
}

void CActorCondition::BoostFireWoundImmunity(const float value)
{
    m_fBoostFireWoundImmunity += value;
}

void CActorCondition::BoostWoundImmunity(const float value)
{
    m_fBoostWoundImmunity += value;
}

void CActorCondition::BoostRadiationProtection(const float value)
{
    m_fBoostRadiationProtection += value;
}

void CActorCondition::BoostTelepaticProtection(const float value)
{
    m_fBoostTelepaticProtection += value;
}

void CActorCondition::BoostChemicalBurnProtection(const float value)
{
    m_fBoostChemicalBurnProtection += value;
}

void CActorCondition::BoostTimeFactor(const float value)
{
    m_fBoostTimeFactor = Device.time_factor();

    m_fBoostTimeFactor += value;
    clamp(m_fBoostTimeFactor, 0.25f, 1.5f);

    Device.time_factor(m_fBoostTimeFactor);
}

void CActorCondition::BoostSatietyRestore(const float value)
{
    m_fV_Satiety += value;
}

void CActorCondition::BoostThirstRestore(const float value)
{
    m_fV_Thirst += value;
}

void CActorCondition::BoostPsyHealthRestore(const float value)
{
    m_change_v.m_fV_PsyHealth += value;
}

void CActorCondition::BoostAlcoholRestore(const float value)
{
    m_fV_Alcohol += value;
}

void CActorCondition::UpdateTutorialThresholds()
{
    string256 cb_name;
    static float _cPowerThr = pSettings->r_float("tutorial_conditions_thresholds", "power");
    static float _cPowerMaxThr = pSettings->r_float("tutorial_conditions_thresholds", "max_power");
    static float _cBleeding = pSettings->r_float("tutorial_conditions_thresholds", "bleeding");
    static float _cSatiety = pSettings->r_float("tutorial_conditions_thresholds", "satiety");
    static float _cRadiation = pSettings->r_float("tutorial_conditions_thresholds", "radiation");
    static float _cWpnCondition = pSettings->r_float("tutorial_conditions_thresholds", "weapon_jammed");
    static float _cPsyHealthThr = pSettings->r_float("tutorial_conditions_thresholds", "psy_health");
    static float _cThirst = 0.0f;

    if (Core.Features.test(xrCore::Feature::actor_thirst))
    {
        _cThirst = pSettings->r_float("tutorial_conditions_thresholds", "thirst");
    }

    bool b = true;
    if (b && !m_condition_flags.test(eCriticalPowerReached) && GetPower() < _cPowerThr)
    {
        m_condition_flags.set(eCriticalPowerReached, TRUE);
        b = false;
        strcpy_s(cb_name, "_G.on_actor_critical_power");
    }

    if (b && !m_condition_flags.test(eCriticalMaxPowerReached) && GetMaxPower() < _cPowerMaxThr)
    {
        m_condition_flags.set(eCriticalMaxPowerReached, TRUE);
        b = false;
        strcpy_s(cb_name, "_G.on_actor_critical_max_power");
    }

    if (b && !m_condition_flags.test(eCriticalBleedingSpeed) && BleedingSpeed() > _cBleeding)
    {
        m_condition_flags.set(eCriticalBleedingSpeed, TRUE);
        b = false;
        strcpy_s(cb_name, "_G.on_actor_bleeding");
    }

    if (b && !m_condition_flags.test(eCriticalSatietyReached) && GetSatiety() < _cSatiety)
    {
        m_condition_flags.set(eCriticalSatietyReached, TRUE);
        b = false;
        strcpy_s(cb_name, "_G.on_actor_satiety");
    }

    if (Core.Features.test(xrCore::Feature::actor_thirst))
    {
        if (b && !m_condition_flags.test(eCriticalThirstReached) && GetThirst() < _cThirst)
        {
            m_condition_flags.set(eCriticalThirstReached, true);
            b = false;
            xr_strcpy(cb_name, "_G.on_actor_thirst");
        }
    }

    if (b && !m_condition_flags.test(eCriticalRadiationReached) && GetRadiation() > _cRadiation)
    {
        m_condition_flags.set(eCriticalRadiationReached, TRUE);
        b = false;
        strcpy_s(cb_name, "_G.on_actor_radiation");
    }

    if (b && !m_condition_flags.test(ePhyHealthMinReached) && GetPsyHealth() < _cPsyHealthThr)
    {
        m_condition_flags.set(ePhyHealthMinReached, TRUE);
        b = false;
        strcpy_s(cb_name, "_G.on_actor_psy");
    }

    if (b && !m_condition_flags.test(eCantWalkWeight) && IsCantWalkWeight())
    {
        m_condition_flags.set(eCantWalkWeight, TRUE);
        b = false;
        strcpy_s(cb_name, "_G.on_actor_cant_walk_weight");
    }

    if (b && !m_condition_flags.test(eWeaponJammedReached) && m_object->inventory().GetActiveSlot() != NO_ACTIVE_SLOT)
    {
        PIItem item = m_object->inventory().ItemFromSlot(m_object->inventory().GetActiveSlot());
        CWeapon* pWeapon = smart_cast<CWeapon*>(item);
        if (pWeapon && pWeapon->GetCondition() < _cWpnCondition)
        {
            m_condition_flags.set(eWeaponJammedReached, TRUE);
            b = false;
            strcpy_s(cb_name, "_G.on_actor_weapon_jammed");
        }
    }

    if (!b)
    {
        luabind::functor<LPCSTR> fl;
        R_ASSERT(ai().script_engine().functor<LPCSTR>(cb_name, fl));
        fl();
    }
}

bool CActorCondition::DisableSprint(SHit* pHDS)
{
    return (pHDS->hit_type != ALife::eHitTypeTelepatic) && (pHDS->hit_type != ALife::eHitTypeChemicalBurn) && (pHDS->hit_type != ALife::eHitTypeBurn) && (pHDS->hit_type != ALife::eHitTypeRadiation);
}

float CActorCondition::HitSlowmo(SHit* pHDS)
{
    float ret;
    if (pHDS->hit_type == ALife::eHitTypeWound || pHDS->hit_type == ALife::eHitTypeStrike)
    {
        ret = pHDS->damage();
        clamp(ret, 0.0f, 1.f);
    }
    else
        ret = 0.0f;

    return ret;
}

void CActorCondition::net_Relcase(CObject* O)
{
    if (Level().is_removing_objects())
        return;

    monsters_feel_touch->feel_touch_relcase(O);
}

using namespace luabind;

void CActorCondition::script_register(lua_State* L)
{
    module(L)
    [(
        class_<SBooster>("SBooster")
            .def(constructor<>())
            .def_readwrite("fBoostTime", &SBooster::fBoostTime)
            .def_readwrite("fBoostValue", &SBooster::fBoostValue)
            .def_readwrite("m_type", &SBooster::m_type),

        class_<CActorCondition, CEntityCondition>("CActorCondition")
            .def("ClearAllBoosters", &CActorCondition::ClearAllBoosters)
            .def("ApplyBooster", &CActorCondition::ApplyBooster_script)
            .def("BoosterForEach", &CActorCondition::BoosterForEach)
            .def("WoundForEach", &CActorCondition::WoundForEach)
            .def("GetSatiety", &CActorCondition::GetSatiety)
            .def("BoostMaxWeight", &CActorCondition::BoostMaxWeight)
            .def("BoostHpRestore", &CActorCondition::BoostHpRestore)
            .def("BoostPowerRestore", &CActorCondition::BoostPowerRestore)
            .def("BoostRadiationRestore", &CActorCondition::BoostRadiationRestore)
            .def("BoostBleedingRestore", &CActorCondition::BoostBleedingRestore)
            .def("BoostBurnImmunity", &CActorCondition::BoostBurnImmunity)
            .def("BoostShockImmunity", &CActorCondition::BoostShockImmunity)
            .def("BoostRadiationImmunity", &CActorCondition::BoostRadiationImmunity)
            .def("BoostTelepaticImmunity", &CActorCondition::BoostTelepaticImmunity)
            .def("BoostChemicalBurnImmunity", &CActorCondition::BoostChemicalBurnImmunity)
            .def("BoostExplImmunity", &CActorCondition::BoostExplImmunity)
            .def("BoostStrikeImmunity", &CActorCondition::BoostStrikeImmunity)
            .def("BoostFireWoundImmunity", &CActorCondition::BoostFireWoundImmunity)
            .def("BoostWoundImmunity", &CActorCondition::BoostWoundImmunity)
            .def("BoostRadiationProtection", &CActorCondition::BoostRadiationProtection)
            .def("BoostTelepaticProtection", &CActorCondition::BoostTelepaticProtection)
            .def("BoostChemicalBurnProtection", &CActorCondition::BoostChemicalBurnProtection)
            .def("BoostSatietyRestore", &CActorCondition::BoostSatietyRestore)
            .def("BoostThirstRestore", &CActorCondition::BoostThirstRestore)
            .def("BoostPsyHealthRestore", &CActorCondition::BoostPsyHealthRestore)
            .def("BoostAlcoholRestore", &CActorCondition::BoostAlcoholRestore)
            .def("BoostTimeFactor", &CActorCondition::BoostTimeFactor)
            .def("IsLimping", &CActorCondition::IsLimping)
            .def("IsCantWalk", &CActorCondition::IsCantWalk)
            .def("IsCantWalkWeight", &CActorCondition::IsCantWalkWeight)
            .def("IsCantSprint", &CActorCondition::IsCantSprint)
            .def_readwrite("m_MaxWalkWeight", &CActorCondition::m_MaxWalkWeight)
    )];
};