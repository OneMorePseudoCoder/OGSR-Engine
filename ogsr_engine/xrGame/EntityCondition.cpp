#include "stdafx.h"
#include "entitycondition.h"
#include "inventoryowner.h"
#include "customoutfit.h"
#include "inventory.h"
#include "wound.h"
#include "level.h"
#include "game_cl_base.h"
#include "entity_alive.h"
#include "../Include/xrRender/Kinematics.h"
#include "object_broker.h"

#define MAX_HEALTH 1.0f
#define MIN_HEALTH -0.01f

#define MAX_POWER 1.0f
#define MAX_RADIATION 1.0f
#define MAX_PSY_HEALTH 1.0f

CEntityConditionSimple::CEntityConditionSimple()
{
    max_health() = MAX_HEALTH;
    health() = MAX_HEALTH;
}

CEntityConditionSimple::~CEntityConditionSimple() {}

CEntityCondition::CEntityCondition(CEntityAlive* object) : CEntityConditionSimple()
{
    VERIFY(object);

    m_object = object;

    m_use_limping_state = false;
    m_iLastTimeCalled = 0;
    m_bTimeValid = false;

    m_fPowerMax = MAX_POWER;
    m_fRadiationMax = MAX_RADIATION;
    m_fPsyHealthMax = MAX_PSY_HEALTH;
    m_fEntityMorale = m_fEntityMoraleMax = 1.f;

    m_fPower = MAX_POWER;
    m_fRadiation = 0;
    m_fPsyHealth = MAX_PSY_HEALTH;

    m_fMinWoundSize = 0.00001f;

    m_fPowerHitPart = 0.5f;

    m_fBoostBurnImmunity = 0.f;
    m_fBoostShockImmunity = 0.f;
    m_fBoostRadiationImmunity = 0.f;
    m_fBoostTelepaticImmunity = 0.f;
    m_fBoostChemicalBurnImmunity = 0.f;
    m_fBoostExplImmunity = 0.f;
    m_fBoostStrikeImmunity = 0.f;
    m_fBoostFireWoundImmunity = 0.f;
    m_fBoostWoundImmunity = 0.f;
    m_fBoostRadiationProtection = 0.f;
    m_fBoostTelepaticProtection = 0.f;
    m_fBoostChemicalBurnProtection = 0.f;
    m_fBoostTimeFactor = 0.f;

    m_fDeltaHealth = 0;
    m_fDeltaPower = 0;
    m_fDeltaRadiation = 0;
    m_fDeltaPsyHealth = 0;

    m_fHealthLost = 0.f;
    m_pWho = NULL;
    m_iWhoID = 0;

    m_WoundVector.clear();

    m_fHitBoneScale = 1.f;
    m_fWoundBoneScale = 1.f;

    m_bIsBleeding = false;
    m_bCanBeHarmed = true;
}

CEntityCondition::~CEntityCondition(void) { ClearWounds(); }

void CEntityCondition::ClearWounds()
{
    for (WOUND_VECTOR_IT it = m_WoundVector.begin(); m_WoundVector.end() != it; ++it)
        xr_delete(*it);
    m_WoundVector.clear();

    m_bIsBleeding = false;
}

void CEntityCondition::LoadCondition(LPCSTR entity_section)
{
    LPCSTR section = READ_IF_EXISTS(pSettings, r_string, entity_section, "condition_sect", entity_section);

    m_change_v.load(section, "");

    m_fMinWoundSize = pSettings->r_float(section, "min_wound_size");
    m_fPowerHitPart = pSettings->r_float(section, "power_hit_part");
    float fHealthHitPart = READ_IF_EXISTS(pSettings, r_float, section, "health_hit_part", 1.f);
    for (int hit_type = 0; hit_type < (int)ALife::eHitTypeMax; ++hit_type)
    {
        LPCSTR hit_name = ALife::g_cafHitType2String((ALife::EHitType)hit_type);
        xr_string s("health_");
        s += hit_name;
        s += "_hit_part";
        m_fHealthHitPart[hit_type] = READ_IF_EXISTS(pSettings, r_float, section, s.c_str(), fHealthHitPart);
    }

    m_use_limping_state = !!(READ_IF_EXISTS(pSettings, r_bool, section, "use_limping_state", FALSE));
    m_limping_threshold = READ_IF_EXISTS(pSettings, r_float, section, "limping_threshold", .5f);
}

void CEntityCondition::reinit()
{
    m_iLastTimeCalled = 0;
    m_bTimeValid = false;

    max_health() = MAX_HEALTH;
    m_fPowerMax = MAX_POWER;
    m_fRadiationMax = MAX_RADIATION;
    m_fPsyHealthMax = MAX_PSY_HEALTH;

    m_fEntityMorale = m_fEntityMoraleMax = 1.f;

    health() = MAX_HEALTH;
    m_fPower = MAX_POWER;
    m_fRadiation = 0;
    m_fPsyHealth = MAX_PSY_HEALTH;

    m_fDeltaHealth = 0;
    m_fDeltaPower = 0;
    m_fDeltaRadiation = 0;
    m_fDeltaEntityMorale = 0;
    m_fDeltaPsyHealth = 0;

    m_fHealthLost = 0.f;
    m_pWho = NULL;
    m_iWhoID = NULL;

    ClearWounds();
}

void CEntityCondition::ChangeEntityMorale(float value) { m_fDeltaEntityMorale += value; }

void CEntityCondition::ChangeHealth(float value)
{
    VERIFY(_valid(value));
    m_fDeltaHealth += (CanBeHarmed() || (value > 0)) ? value : 0;
}

void CEntityCondition::ChangePower(float value) { m_fDeltaPower += value; }

void CEntityCondition::ChangeRadiation(float value) { m_fDeltaRadiation += value; }

void CEntityCondition::ChangePsyHealth(float value) { m_fDeltaPsyHealth += value; }

void CEntityCondition::ChangeBleeding(float percent)
{
    //затянуть раны
    for (WOUND_VECTOR_IT it = m_WoundVector.begin(); m_WoundVector.end() != it; ++it)
    {
        (*it)->Incarnation(percent, m_fMinWoundSize);
        if (0 == (*it)->TotalSize())
            (*it)->SetDestroy(true);
    }
}

bool RemoveWoundPred(CWound* pWound)
{
    if (pWound->GetDestroy())
    {
        xr_delete(pWound);
        return true;
    }
    return false;
}

void CEntityCondition::UpdateWounds()
{
    //убрать все зашившие раны из списка
    m_WoundVector.erase(std::remove_if(m_WoundVector.begin(), m_WoundVector.end(), &RemoveWoundPred), m_WoundVector.end());
}

void CEntityCondition::UpdateConditionTime()
{
    u64 _cur_time = Level().GetGameTime();

    if (m_bTimeValid)
    {
        if (_cur_time > m_iLastTimeCalled)
        {
            float x = float(_cur_time - m_iLastTimeCalled) / 1000.0f;
            SetConditionDeltaTime(x);
        }
        else
            SetConditionDeltaTime(0.0f);
    }
    else
    {
        SetConditionDeltaTime(0.0f);
        m_bTimeValid = true;

        m_fDeltaHealth = 0;
        m_fDeltaPower = 0;
        m_fDeltaRadiation = 0;
        m_fDeltaEntityMorale = 0;
    }

    m_iLastTimeCalled = _cur_time;
}

//вычисление параметров с ходом игрового времени
void CEntityCondition::UpdateCondition()
{
    if (GetHealth() <= 0)
        return;

    bool CriticalHealth = false;

    if (m_fDeltaHealth + GetHealth() <= 0)
    {
        CriticalHealth = true;
    }

    UpdateHealth();

    if (!CriticalHealth && m_fDeltaHealth + GetHealth() <= 0)
    {
        CriticalHealth = true;
    };

    UpdatePower();
    UpdateRadiation();

    if (!CriticalHealth && m_fDeltaHealth + GetHealth() <= 0)
    {
        CriticalHealth = true;
    };

    UpdatePsyHealth();

    UpdateEntityMorale();

    health() += m_fDeltaHealth;

    m_fPower += m_fDeltaPower;
    m_fPsyHealth += m_fDeltaPsyHealth;
    m_fEntityMorale += m_fDeltaEntityMorale;
    m_fRadiation += m_fDeltaRadiation;

    m_fDeltaHealth = 0;
    m_fDeltaPower = 0;
    m_fDeltaRadiation = 0;
    m_fDeltaPsyHealth = 0;
    m_fDeltaEntityMorale = 0;

    clamp(health(), MIN_HEALTH, max_health());
    clamp(m_fPower, 0.0f, m_fPowerMax);
    clamp(m_fRadiation, 0.0f, m_fRadiationMax);
    clamp(m_fEntityMorale, 0.0f, m_fEntityMoraleMax);
    clamp(m_fPsyHealth, 0.0f, m_fPsyHealthMax);
}

float CEntityCondition::HitOutfitEffect(float hit_power, ALife::EHitType hit_type, s16 element, float AP)
{
    CInventoryOwner* pInvOwner = smart_cast<CInventoryOwner*>(m_object);
    if (!pInvOwner)
        return hit_power;

    CCustomOutfit* pOutfit = (CCustomOutfit*)pInvOwner->inventory().m_slots[OUTFIT_SLOT].m_pIItem;
    if (!pOutfit)
        return hit_power;

    float new_hit_power = hit_power;

    if (hit_type == ALife::eHitTypeFireWound)
        new_hit_power = pOutfit->HitThruArmour(hit_power, element, AP);
    else
        new_hit_power *= pOutfit->GetHitTypeProtection(hit_type, element);

    //увеличить изношенность костюма
    pOutfit->Hit(hit_power, hit_type);

    return new_hit_power;
}

float CEntityCondition::HitPowerEffect(float power_loss)
{
    CInventoryOwner* pInvOwner = smart_cast<CInventoryOwner*>(m_object);
    if (!pInvOwner)
        return power_loss;

    CCustomOutfit* pOutfit = (CCustomOutfit*)pInvOwner->inventory().m_slots[OUTFIT_SLOT].m_pIItem;
    if (!pOutfit)
        return power_loss;

    return power_loss * pOutfit->GetPowerLoss();
}

CWound* CEntityCondition::AddWound(float hit_power, ALife::EHitType hit_type, u16 element)
{
    //максимальное число косточек 64
    VERIFY(element < 64 || BI_NONE == element);

    //запомнить кость по которой ударили и силу удара
    WOUND_VECTOR_IT it = m_WoundVector.begin();
    for (; it != m_WoundVector.end(); ++it)
    {
        if ((*it)->GetBoneNum() == element)
            break;
    }

    CWound* pWound = NULL;

    //новая рана
    if (it == m_WoundVector.end())
    {
        pWound = xr_new<CWound>(element);
        pWound->AddHit(hit_power * ::Random.randF(0.5f, 1.5f), hit_type);
        m_WoundVector.push_back(pWound);
    }
    //старая
    else
    {
        pWound = *it;
        pWound->AddHit(hit_power * ::Random.randF(0.5f, 1.5f), hit_type);
    }

    VERIFY(pWound);
    return pWound;
}

CWound* CEntityCondition::ConditionHit(SHit* pHDS)
{
    //кто нанес последний хит
    m_pWho = pHDS->who;
    m_iWhoID = (NULL != pHDS->who) ? pHDS->who->ID() : 0;

    float hit_power_org = pHDS->damage();
    float hit_power = hit_power_org;
    hit_power = HitOutfitEffect(hit_power, pHDS->hit_type, pHDS->boneID, pHDS->ap);

    bool bAddWound = true;
    switch (pHDS->hit_type)
    {
    case ALife::eHitTypeTelepatic:
        hit_power -= m_fBoostTelepaticProtection;
        hit_power *= m_HitTypeK[pHDS->hit_type] - m_fBoostTelepaticImmunity;
        ChangePsyHealth(-hit_power);
        bAddWound = false;
        break;
    case ALife::eHitTypeChemicalBurn:
        hit_power -= m_fBoostChemicalBurnProtection;
        hit_power *= m_HitTypeK[pHDS->hit_type] - m_fBoostChemicalBurnProtection;
        break;
    case ALife::eHitTypeShock:
        hit_power *= m_HitTypeK[pHDS->hit_type] - m_fBoostShockImmunity;
        m_fHealthLost = hit_power * m_fHealthHitPart[pHDS->hit_type];
        m_fDeltaHealth -= CanBeHarmed() ? m_fHealthLost : 0;
        m_fDeltaPower -= hit_power * m_fPowerHitPart;
        bAddWound = false;
        break;
    case ALife::eHitTypeRadiation:
        hit_power -= m_fBoostRadiationProtection;
        m_fDeltaRadiation += hit_power;
        return NULL;
        break;
    case ALife::eHitTypeExplosion:
        hit_power *= m_HitTypeK[pHDS->hit_type] - m_fBoostExplImmunity;
        m_fHealthLost = hit_power * m_fHealthHitPart[pHDS->hit_type];
        m_fDeltaHealth -= CanBeHarmed() ? m_fHealthLost : 0;
        m_fDeltaPower -= hit_power * m_fPowerHitPart;
        break;
    case ALife::eHitTypeStrike:
    case ALife::eHitTypePhysicStrike:
        hit_power *= m_HitTypeK[pHDS->hit_type] - m_fBoostStrikeImmunity;
        m_fHealthLost = hit_power * m_fHealthHitPart[pHDS->hit_type];
        m_fDeltaHealth -= CanBeHarmed() ? m_fHealthLost : 0;
        m_fDeltaPower -= hit_power * m_fPowerHitPart;
        break;
    case ALife::eHitTypeBurn:
    case ALife::eHitTypeFireWound:
    case ALife::eHitTypeWound:
        if (pHDS->hit_type == ALife::eHitTypeBurn)
            hit_power *= m_HitTypeK[pHDS->hit_type] - m_fBoostBurnImmunity;
        else
            hit_power *= m_HitTypeK[pHDS->hit_type] - m_fBoostFireWoundImmunity;
        m_fHealthLost = hit_power * m_fHealthHitPart[pHDS->hit_type] * m_fHitBoneScale;
        m_fDeltaHealth -= CanBeHarmed() ? m_fHealthLost : 0;
        m_fDeltaPower -= hit_power * m_fPowerHitPart;
        break;
    default: 
    {
        R_ASSERT2(0, "unknown hit type");
    }
    break;
    }

    if (bDebug)
        Msg("%s hitted in %s with %f[%f]", m_object->Name(), smart_cast<IKinematics*>(m_object->Visual())->LL_BoneName(pHDS->boneID), m_fHealthLost * 100.0f, hit_power_org);

    //раны добавляются только живому
    if (bAddWound && GetHealth() > 0)
        return AddWound(hit_power * m_fWoundBoneScale, pHDS->hit_type, pHDS->boneID);
    else
        return NULL;
}

float CEntityCondition::BleedingSpeed()
{
    float bleeding_speed = 0;

    for (WOUND_VECTOR_IT it = m_WoundVector.begin(); m_WoundVector.end() != it; ++it)
        bleeding_speed += (*it)->TotalSize();

    return (m_WoundVector.empty() ? 0.f : bleeding_speed / m_WoundVector.size());
}

void CEntityCondition::UpdateHealth()
{
    float bleeding_speed = BleedingSpeed() * m_fDeltaTime * m_change_v.m_fV_Bleeding;
    m_bIsBleeding = fis_zero(bleeding_speed) ? false : true;
    m_fDeltaHealth -= CanBeHarmed() ? bleeding_speed : 0;
    m_fDeltaHealth += m_fDeltaTime * m_change_v.m_fV_HealthRestore;

    VERIFY(_valid(m_fDeltaHealth));
    ChangeBleeding(m_change_v.m_fV_WoundIncarnation * m_fDeltaTime);
}

void CEntityCondition::UpdatePower() {}

void CEntityCondition::UpdatePsyHealth(float k)
{
    if (m_fPsyHealth > 0)
    {
        m_fDeltaPsyHealth += m_change_v.m_fV_PsyHealth * k * m_fDeltaTime;
    }
}

void CEntityCondition::UpdateRadiation(float k)
{
    if (m_fRadiation > 0)
    {
        m_fDeltaRadiation -= m_change_v.m_fV_Radiation * k * m_fDeltaTime;
        m_fDeltaHealth -= CanBeHarmed() ? m_change_v.m_fV_RadiationHealth * m_fRadiation * m_fDeltaTime : 0.0f;
    }
}

void CEntityCondition::UpdateEntityMorale()
{
    if (m_fEntityMorale < m_fEntityMoraleMax)
    {
        m_fDeltaEntityMorale += m_change_v.m_fV_EntityMorale * m_fDeltaTime;
    }
}

void CEntityCondition::save(NET_Packet& output_packet)
{
    u8 is_alive = (GetHealth() > 0.f) ? 1 : 0;

    output_packet.w_u8(is_alive);
    if (is_alive)
    {
        save_data(m_fPower, output_packet);
        save_data(m_fRadiation, output_packet);
        save_data(m_fEntityMorale, output_packet);
        save_data(m_fPsyHealth, output_packet);

        output_packet.w_u8((u8)m_WoundVector.size());
        for (WOUND_VECTOR_IT it = m_WoundVector.begin(); m_WoundVector.end() != it; it++)
            (*it)->save(output_packet);
    }
}

void CEntityCondition::load(IReader& input_packet)
{
    m_bTimeValid = false;

    u8 is_alive = input_packet.r_u8();
    if (is_alive)
    {
        load_data(m_fPower, input_packet);
        load_data(m_fRadiation, input_packet);
        load_data(m_fEntityMorale, input_packet);
        load_data(m_fPsyHealth, input_packet);

        ClearWounds();
        m_WoundVector.resize(input_packet.r_u8());
        if (!m_WoundVector.empty())
            for (u32 i = 0; i < m_WoundVector.size(); i++)
            {
                CWound* pWound = xr_new<CWound>(BI_NONE);
                pWound->load(input_packet);
                m_WoundVector[i] = pWound;
            }
    }
}

constexpr LPCSTR CCV_NAMES[] = {"radiation_v", "radiation_health_v", "morale_v", "psy_health_v", "bleeding_v", "wound_incarnation_v", "health_restore_v"};

float& CEntityCondition::SConditionChangeV::value(LPCSTR name)
{
    float* values[] = {&m_fV_Radiation, &m_fV_RadiationHealth, &m_fV_EntityMorale, &m_fV_PsyHealth, &m_fV_Bleeding, &m_fV_WoundIncarnation, &m_fV_HealthRestore};
    for (int i = 0; i < PARAMS_COUNT; i++)
        if (strstr(name, CCV_NAMES[i]))
            return *values[i];

    static float fake = 0;
    return fake;
}

void CEntityCondition::SConditionChangeV::load(LPCSTR sect, LPCSTR prefix)
{
    string256 str;
    for (int i = 0; i < PARAMS_COUNT; i++)
    {
        strconcat(sizeof(str), str, CCV_NAMES[i], prefix);
        float v = READ_IF_EXISTS(pSettings, r_float, sect, str, 0.0f);
        value(CCV_NAMES[i]) = v;
    }
}

float CEntityCondition::GetParamByName(LPCSTR name)
{
    const static LPCSTR PARAM_NAMES[] = {"health", "power", "radiation", "psy_health", "morale", "max_health", "power_max", "radiation_max", "psy_health_max", "morale_max"};
    float* values[] = {&health(), &m_fPower, &m_fRadiation, &m_fPsyHealth, &m_fEntityMorale, &max_health(), &m_fPowerMax, &m_fRadiationMax, &m_fPsyHealthMax, &m_fEntityMoraleMax};
    for (int i = 0; i < 10; i++)
        if (strstr(name, PARAM_NAMES[i]))
            return *values[i];

    return m_change_v.value(name);
}

void CEntityCondition::remove_links(const CObject* object)
{
    if (m_pWho != object)
        return;

    m_pWho = m_object;
    m_iWhoID = m_object->ID();
}

bool CEntityCondition::ApplyInfluence(const SMedicineInfluenceValues& V, const shared_str& sect)
{
    ChangeHealth(V.fHealth);
    ChangePower(V.fPower);
    ChangeSatiety(V.fSatiety);
    ChangeRadiation(V.fRadiation);
    ChangeBleeding(V.fWoundsHeal);
    SetMaxPower(GetMaxPower() + V.fMaxPowerUp);
    ChangeAlcohol(V.fAlcohol);
    ChangeThirst(V.fThirst);
    ChangePsyHealth(V.fPsyHealth);
    return true;
}

bool CEntityCondition::ApplyBooster(const SBooster& B, const shared_str& sect)
{
    return true;
}

void SMedicineInfluenceValues::Load(const shared_str& sect)
{
    fHealth = pSettings->r_float(sect.c_str(), "eat_health");
    fPower = pSettings->r_float(sect.c_str(), "eat_power");
    fSatiety = pSettings->r_float(sect.c_str(), "eat_satiety");
    fThirst = pSettings->r_float(sect.c_str(), "eat_thirst");
    fRadiation = pSettings->r_float(sect.c_str(), "eat_radiation");
    fPsyHealth = pSettings->r_float(sect.c_str(), "eat_psy_health");
    fWoundsHeal = pSettings->r_float(sect.c_str(), "wounds_heal_perc");
    clamp(fWoundsHeal, 0.f, 1.f);
    fMaxPowerUp = READ_IF_EXISTS(pSettings, r_float, sect.c_str(), "eat_max_power", 0.0f);
    fAlcohol = READ_IF_EXISTS(pSettings, r_float, sect.c_str(), "eat_alcohol", 0.0f);
    fTimeTotal = READ_IF_EXISTS(pSettings, r_float, sect.c_str(), "apply_time_sec", -1.0f);
}

void SBooster::Load(const shared_str& sect, EBoostParams type)
{
    fBoostTime = pSettings->r_float(sect.c_str(), "boost_time");
    m_type = type;
    switch (type)
    {
    case eBoostHpRestore: fBoostValue = pSettings->r_float(sect.c_str(), "boost_health_restore"); break;
    case eBoostPowerRestore: fBoostValue = pSettings->r_float(sect.c_str(), "boost_power_restore"); break;
    case eBoostRadiationRestore: fBoostValue = pSettings->r_float(sect.c_str(), "boost_radiation_restore"); break;
    case eBoostBleedingRestore: fBoostValue = pSettings->r_float(sect.c_str(), "boost_bleeding_restore"); break;
    case eBoostMaxWeight: fBoostValue = pSettings->r_float(sect.c_str(), "boost_max_weight"); break;
    case eBoostBurnImmunity: fBoostValue = pSettings->r_float(sect.c_str(), "boost_burn_immunity"); break;
    case eBoostShockImmunity: fBoostValue = pSettings->r_float(sect.c_str(), "boost_shock_immunity"); break;
    case eBoostRadiationImmunity: fBoostValue = pSettings->r_float(sect.c_str(), "boost_radiation_immunity"); break;
    case eBoostTelepaticImmunity: fBoostValue = pSettings->r_float(sect.c_str(), "boost_telepat_immunity"); break;
    case eBoostChemicalBurnImmunity: fBoostValue = pSettings->r_float(sect.c_str(), "boost_chemburn_immunity"); break;
    case eBoostExplImmunity: fBoostValue = pSettings->r_float(sect.c_str(), "boost_explosion_immunity"); break;
    case eBoostStrikeImmunity: fBoostValue = pSettings->r_float(sect.c_str(), "boost_strike_immunity"); break;
    case eBoostFireWoundImmunity: fBoostValue = pSettings->r_float(sect.c_str(), "boost_fire_wound_immunity"); break;
    case eBoostWoundImmunity: fBoostValue = pSettings->r_float(sect.c_str(), "boost_wound_immunity"); break;
    case eBoostRadiationProtection: fBoostValue = pSettings->r_float(sect.c_str(), "boost_radiation_protection"); break;
    case eBoostTelepaticProtection: fBoostValue = pSettings->r_float(sect.c_str(), "boost_telepat_protection"); break;
    case eBoostChemicalBurnProtection: fBoostValue = pSettings->r_float(sect.c_str(), "boost_chemburn_protection"); break;
    case eBoostSatietyRestore: fBoostValue = pSettings->r_float(sect.c_str(), "boost_satiety_restore"); break;
    case eBoostThirstRestore: fBoostValue = pSettings->r_float(sect.c_str(), "boost_thirst_restore"); break;
    case eBoostPsyHealthRestore: fBoostValue = pSettings->r_float(sect.c_str(), "boost_psy_health_restore"); break;
    case eBoostAlcoholRestore: fBoostValue = pSettings->r_float(sect.c_str(), "boost_alcohol_restore"); break;
    case eBoostTimeFactor: fBoostValue = pSettings->r_float(sect.c_str(), "boost_time_factor"); break;
    default: NODEFAULT;
    }
}

using namespace luabind;

void set_entity_health(CEntityCondition* E, float h) { E->health() = h; }
void set_entity_max_health(CEntityCondition* E, float h) { E->health() = h; }

bool get_entity_crouch(CEntity::SEntityState* S) { return S->bCrouch; }
bool get_entity_fall(CEntity::SEntityState* S) { return S->bFall; }
bool get_entity_jump(CEntity::SEntityState* S) { return S->bJump; }
bool get_entity_sprint(CEntity::SEntityState* S) { return S->bSprint; }

void CEntityCondition::script_register(lua_State* L)
{
    module(L)[class_<CEntity::SEntityState>("SEntityState")
                  .property("crouch", &get_entity_crouch)
                  .property("fall", &get_entity_fall)
                  .property("jump", &get_entity_jump)
                  .property("sprint", &get_entity_sprint)
                  .def_readonly("velocity", &CEntity::SEntityState::fVelocity)
                  .def_readonly("a_velocity", &CEntity::SEntityState::fAVelocity)
              ,
              class_<CEntityCondition>("CEntityCondition")
                  .def("AddWound", &CEntityCondition::AddWound)
                  .def("ClearWounds", &CEntityCondition::ClearWounds)
                  .def("GetWhoHitLastTimeID", &CEntityCondition::GetWhoHitLastTimeID)
                  .def("GetPower", &CEntityCondition::GetPower)
                  .def("SetPower", &CEntityCondition::SetPower)
                  .def("GetRadiation", &CEntityCondition::GetRadiation)
                  .def("GetPsyHealth", &CEntityCondition::GetPsyHealth)
                  .def("GetSatiety", &CEntityCondition::GetSatiety)
                  .def("GetEntityMorale", &CEntityCondition::GetEntityMorale)
                  .def("GetHealthLost", &CEntityCondition::GetHealthLost)
                  .def("ChangeSatiety", &CEntityCondition::ChangeSatiety)
                  .def("ChangeHealth", &CEntityCondition::ChangeHealth)
                  .def("ChangePower", &CEntityCondition::ChangePower)
                  .def("ChangeRadiation", &CEntityCondition::ChangeRadiation)
                  .def("ChangePsyHealth", &CEntityCondition::ChangePsyHealth)
                  .def("ChangeAlcohol", &CEntityCondition::ChangeAlcohol)
                  .def("SetMaxPower", &CEntityCondition::SetMaxPower)
                  .def("GetMaxPower", &CEntityCondition::GetMaxPower)
                  .def("ChangeEntityMorale", &CEntityCondition::ChangeEntityMorale)
                  .def("ChangeBleeding", &CEntityCondition::ChangeBleeding)
                  .def("BleedingSpeed", &CEntityCondition::BleedingSpeed)
                  .def("fdelta_time", &CEntityCondition::fdelta_time)
                  .def_readonly("has_valid_time", &CEntityCondition::m_bTimeValid)
                  .def_readwrite("power", &CEntityCondition::m_fPower)
                  .def_readwrite("power_max", &CEntityCondition::m_fPowerMax)
                  .def_readwrite("psy_health", &CEntityCondition::m_fPsyHealth)
                  .def_readwrite("psy_health_max", &CEntityCondition::m_fPsyHealthMax)
                  .def_readwrite("radiation", &CEntityCondition::m_fRadiation)
                  .def_readwrite("radiation_max", &CEntityCondition::m_fRadiationMax)
                  .def_readwrite("morale", &CEntityCondition::m_fEntityMorale)
                  .def_readwrite("morale_max", &CEntityCondition::m_fEntityMoraleMax)
                  .def_readwrite("min_wound_size", &CEntityCondition::m_fMinWoundSize)
                  .def_readonly("is_bleeding", &CEntityCondition::m_bIsBleeding)
                  .def_readwrite("power_hit_part", &CEntityCondition::m_fPowerHitPart)				
                  .property("health", &CEntityCondition::GetHealth, &set_entity_health)
                  .property("max_health", &CEntityCondition::GetMaxHealth, &set_entity_max_health)
                  .enum_("EBoostParams")
                  [
                        value("eBoostHpRestore", int(EBoostParams::eBoostHpRestore)),
                        value("eBoostPowerRestore", int(EBoostParams::eBoostPowerRestore)),
                        value("eBoostRadiationRestore", int(EBoostParams::eBoostRadiationRestore)),
                        value("eBoostBleedingRestore", int(EBoostParams::eBoostBleedingRestore)),
                        value("eBoostMaxWeight", int(EBoostParams::eBoostMaxWeight)),
                        value("eBoostRadiationProtection", int(EBoostParams::eBoostRadiationProtection)),
                        value("eBoostTelepaticProtection", int(EBoostParams::eBoostTelepaticProtection)),
                        value("eBoostChemicalBurnProtection", int(EBoostParams::eBoostChemicalBurnProtection)),
                        value("eBoostBurnImmunity", int(EBoostParams::eBoostBurnImmunity)),
                        value("eBoostShockImmunity", int(EBoostParams::eBoostShockImmunity)),
                        value("eBoostRadiationImmunity", int(EBoostParams::eBoostRadiationImmunity)),
                        value("eBoostTelepaticImmunity", int(EBoostParams::eBoostTelepaticImmunity)),
                        value("eBoostChemicalBurnImmunity", int(EBoostParams::eBoostChemicalBurnImmunity)),
                        value("eBoostExplImmunity", int(EBoostParams::eBoostExplImmunity)),
                        value("eBoostStrikeImmunity", int(EBoostParams::eBoostStrikeImmunity)),
                        value("eBoostFireWoundImmunity", int(EBoostParams::eBoostFireWoundImmunity)),
                        value("eBoostWoundImmunity", int(EBoostParams::eBoostWoundImmunity)),

                        value("eBoostSatietyRestore", int(EBoostParams::eBoostSatietyRestore)),
                        value("eBoostThirstRestore", int(EBoostParams::eBoostThirstRestore)),
                        value("eBoostPsyHealthRestore", int(EBoostParams::eBoostPsyHealthRestore)),
                        value("eBoostAlcoholRestore", int(EBoostParams::eBoostAlcoholRestore)),
                        value("eBoostTimeFactor", int(EBoostParams::eBoostTimeFactor))
                  ]
    ];
}
