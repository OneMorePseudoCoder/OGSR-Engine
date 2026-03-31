// ActorCondition.h: класс состояния игрока
//

#pragma once

#include "EntityCondition.h"
#include "actor_defs.h"
#include "..\xr_3da\feel_touch.h"

template <typename _return_type>
class CScriptCallbackEx;

class CActor;

class CActorCondition : public CEntityCondition
{
    friend class CScriptActor;

public:
    typedef CEntityCondition inherited;
    enum
    {
        eCriticalPowerReached = (1 << 0),
        eCriticalMaxPowerReached = (1 << 1),
        eCriticalBleedingSpeed = (1 << 2),
        eCriticalSatietyReached = (1 << 3),
        eCriticalRadiationReached = (1 << 4),
        eWeaponJammedReached = (1 << 5),
        ePhyHealthMinReached = (1 << 6),
        eCantWalkWeight = (1 << 7),

        eLimping = (1 << 8),
        eCantWalk = (1 << 9),
        eCantSprint = (1 << 10),

        eCriticalThirstReached = (1 << 11),
    };
    Flags16 m_condition_flags;

private:
    CActor* m_object;
    void UpdateTutorialThresholds();
    void UpdateSatiety();
    void UpdateThirst();

public:
    CActorCondition(CActor* object);
    virtual ~CActorCondition(void);

    virtual void LoadCondition(LPCSTR section);
    virtual void reinit();

    virtual CWound* ConditionHit(SHit* pHDS);
    virtual void UpdateCondition();
            void UpdateBoosters();

    virtual void ChangeAlcohol(float value);
    virtual void ChangeSatiety(float value);
    virtual void ChangeThirst(float value);

    void BoostParameters(const SBooster& B, bool need_change_tf = true);
    void DisableBoostParameters(const SBooster& B);
    void WoundForEach(const luabind::functor<bool>& funct);
    void BoosterForEach(const luabind::functor<bool>& funct);
    bool ApplyBooster_script(const SBooster& B, LPCSTR sect);
    void ClearAllBoosters();
    void BoostMaxWeight(const float value);
    void BoostHpRestore(const float value);
    void BoostPowerRestore(const float value);
    void BoostRadiationRestore(const float value);
    void BoostBleedingRestore(const float value);
    void BoostBurnImmunity(const float value);
    void BoostShockImmunity(const float value);
    void BoostRadiationImmunity(const float value);
    void BoostTelepaticImmunity(const float value);
    void BoostChemicalBurnImmunity(const float value);
    void BoostExplImmunity(const float value);
    void BoostStrikeImmunity(const float value);
    void BoostFireWoundImmunity(const float value);
    void BoostWoundImmunity(const float value);
    void BoostRadiationProtection(const float value);
    void BoostTelepaticProtection(const float value);
    void BoostChemicalBurnProtection(const float value);
    void BoostTimeFactor(const float value);
    void BoostSatietyRestore(const float value);
    void BoostThirstRestore(const float value);
    void BoostPsyHealthRestore(const float value);
    void BoostAlcoholRestore(const float value);
    BOOSTER_MAP GetCurBoosterInfluences() { return m_booster_influences; };

    // хромание при потере сил и здоровья
    virtual bool IsLimping();
    virtual bool IsCantWalk();
    virtual bool IsCantWalkWeight();
    virtual bool IsCantSprint();
    virtual bool IsCantJump(float weight);

    void PowerHit(float power, bool apply_outfit);
    virtual void UpdatePower();

    void ConditionJump(float weight);
    void ConditionWalk(float weight, bool accel, bool sprint);
    void ConditionStand(float weight);

    float GetAlcohol() { return m_fAlcohol; }
    float GetPsy() { return 1.0f - GetPsyHealth(); }
    float GetSatiety() { return m_fSatiety; }
    float GetThirst() { return m_fThirst; }
    void SetMaxWalkWeight(float _weight) { m_MaxWalkWeight = _weight; }

    void AffectDamage_InjuriousMaterialAndMonstersInfluence();
    float GetInjuriousMaterialDamage();

public:
    IC CActor& object() const
    {
        VERIFY(m_object);
        return (*m_object);
    }
    virtual void save(NET_Packet& output_packet);
    virtual void load(IReader& input_packet);
    float m_MaxWalkWeight;

    float m_max_power_restore_speed;
    float m_max_wound_protection;
    float m_max_fire_wound_protection;

    bool DisableSprint(SHit* pHDS);
    float HitSlowmo(SHit* pHDS);

    virtual bool ApplyInfluence(const SMedicineInfluenceValues& V, const shared_str& sect);
    virtual bool ApplyBooster(const SBooster& B, const shared_str& sect);
    float GetMaxPowerRestoreSpeed() { return m_max_power_restore_speed; };
    float GetMaxWoundProtection() { return m_max_wound_protection; };
    float GetMaxFireWoundProtection() { return m_max_fire_wound_protection; };

public:
    SMedicineInfluenceValues m_curr_medicine_influence;

    float m_fAlcohol;
    float m_fV_Alcohol;
    //--
    float m_fSatiety;
    float m_fSatietyLightLimit;
    float m_fSatietyCriticalLimit;
    float m_fV_Satiety;
    float m_fV_SatietyPower;
    float m_fV_SatietyHealth;
    //--

    float m_fThirst;
    float m_fThirstLightLimit;
    float m_fThirstCriticalLimit;
    float m_fV_Thirst;
    float m_fV_ThirstPower;
    float m_fV_ThirstHealth;

    float m_fPowerLeakSpeed;
    float m_fV_Power;

    float m_fJumpPower;
    float m_fStandPower;
    float m_fWalkPower;
    float m_fJumpWeightPower;
    float m_fWalkWeightPower;
    float m_fOverweightWalkK;
    float m_fOverweightJumpK;
    float m_fAccelK;
    float m_fSprintK;

    bool m_bJumpRequirePower;

    float m_f_time_affected;

    //порог силы и здоровья меньше которого актер начинает хромать
    float m_fLimpingPowerBegin;
    float m_fLimpingPowerEnd;
    float m_fCantWalkPowerBegin;
    float m_fCantWalkPowerEnd;

    float m_fCantSprintPowerBegin;
    float m_fCantSprintPowerEnd;

    float m_fLimpingHealthBegin;
    float m_fLimpingHealthEnd;

protected:
    Feel::Touch* monsters_feel_touch;
    float monsters_aura_radius;

public:
    void net_Relcase(CObject* O);
    void set_monsters_aura_radius(float r)
    {
        if (r > monsters_aura_radius)
            monsters_aura_radius = r;
    };
    ref_sound m_use_sound;
    static void script_register(lua_State* L);
};
