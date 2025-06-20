//---------------------------------------------------------------------------
#pragma once

#include "../../xrCore/_stl_extensions.h"

struct PBool
{
    BOOL val;

    PBool() : val(FALSE) {}

    PBool(BOOL _val) : val(_val) {}

    void set(BOOL v) { val = v; }
};

struct PFloat
{
    float val;
    float mn;
    float mx;

    PFloat()
    {
        val = 0.f;
        mn = 0.f;
        mx = 0.f;
    }

    PFloat(float _val, float _mn, float _mx) : val(_val), mn(_mn), mx(_mx) {}

    void set(float v) { val = v; }
};

struct PInt
{
    int val;
    int mn;
    int mx;

    PInt()
    {
        val = 0;
        mn = 0;
        mx = 0;
    }

    PInt(int _val, int _mn, int _mx) : val(_val), mn(_mn), mx(_mx) {}

    void set(int v) { val = v; }
};

struct PVector
{
    Fvector val;
    float mn;
    float mx;

    enum EType
    {
        vNum,
        vAngle,
        vColor,

        _force_u32 = u32(-1),
    };

    EType type;

    PVector()
    {
        val.set(0, 0, 0);
        mn = 0.f;
        mx = 0.f;
    }

    PVector(EType t, Fvector _val, float _mn, float _mx) : type(t), val(_val), mn(_mn), mx(_mx) {}

    void set(const Fvector& v) { val.set(v); }
    void set(float x, float y, float z) { val.set(x, y, z); }
};

struct PDomain
{
public:
    PAPI::PDomainEnum type;

    union
    {
        float f[9];
        Fvector v[3];
    };

    enum EType
    {
        vNum,
        vAngle,
        vColor,

        _force_u32 = u32(-1),
    };

    enum
    {
        flRenderable = (1 << 0)
    };

    EType e_type;
    Flags32 flags;
    u32 clr;

public:
    PDomain() {}

    PDomain(EType et, BOOL renderable, u32 color = 0x00000000, PAPI::PDomainEnum type = PAPI::PDPoint
        , float inA0 = 0.0f, float inA1 = 0.0f, float inA2 = 0.0f, float inA3 = 0.0f
        , float inA4 = 0.0f, float inA5 = 0.0f, float inA6 = 0.0f, float inA7 = 0.0f, float inA8 = 0.0f);

    PDomain(const PDomain& in);
    ~PDomain();

    void Load(IReader& F);
    void Save(IWriter& F) const;

    void Load2(CInifile& ini, const shared_str& sect);
    void Save2(CInifile& ini, const shared_str& sect);
};

struct EParticleAction
{
    DEFINE_MAP(std::string, PDomain, PDomainMap, PDomainMapIt);
    DEFINE_MAP(std::string, PBool, PBoolMap, PBoolMapIt);
    DEFINE_MAP(std::string, PFloat, PFloatMap, PFloatMapIt);
    DEFINE_MAP(std::string, PInt, PIntMap, PIntMapIt);
    DEFINE_MAP(std::string, PVector, PVectorMap, PVectorMapIt);

    shared_str actionName;
    shared_str actionType;
    shared_str hint;

    enum
    {
        flEnabled = (1 << 0),
        flDraw = (1 << 1),
    };

    Flags32 flags{};
    PAPI::PActionEnum type{};

    PDomainMap domains;
    PBoolMap bools;
    PFloatMap floats;
    PIntMap ints;
    PVectorMap vectors;

    enum EValType
    {
        tpDomain,
        tpVector,
        tpFloat,
        tpBool,
        tpInt,
    };

    struct SOrder
    {
        EValType type{};
        std::string name;

        SOrder(EValType _type, std::string _name) : type(_type), name(_name) {}
    };

    DEFINE_VECTOR(SOrder, OrderVec, OrderVecIt);
    OrderVec orders;

    EParticleAction(PAPI::PActionEnum _type)
    {
        flags.assign(flEnabled);
        type = _type;
    }

public:
    void appendFloat(LPCSTR name, float v, float mn, float mx);
    void appendInt(LPCSTR name, int v, int mn = -P_MAXINT, int mx = P_MAXINT);
    void appendVector(LPCSTR name, PVector::EType type, float vx, float vy, float vz, float mn = -P_MAXFLOAT, float mx = P_MAXFLOAT);
    void appendDomain(LPCSTR name, PDomain v);
    void appendBool(LPCSTR name, BOOL v);

    PFloat& _float(LPCSTR name)
    {
        const PFloatMapIt it = floats.find(name);
        R_ASSERT(it != floats.end(), name);
        return it->second;
    }

    PInt& _int(LPCSTR name)
    {
        const PIntMapIt it = ints.find(name);
        R_ASSERT(it != ints.end(), name);
        return it->second;
    }

    PVector& _vector(LPCSTR name)
    {
        const PVectorMapIt it = vectors.find(name);
        R_ASSERT(it != vectors.end(), name);
        return it->second;
    }

    PDomain& _domain(LPCSTR name)
    {
        const PDomainMapIt it = domains.find(name);
        R_ASSERT(it != domains.end(), name);
        return it->second;
    }

    PBool& _bool(LPCSTR name)
    {
        const PBoolMapIt it = bools.find(name);
        R_ASSERT(it != bools.end(), name);
        return it->second;
    }

    PBool* _bool_safe(LPCSTR name)
    {
        const PBoolMapIt it = bools.find(name);
        return (it != bools.end()) ? &it->second : nullptr;
    }

public:
    virtual void Compile(IWriter& F) = 0;

    virtual bool Load(IReader& F);
    virtual void Save(IWriter& F);

    virtual void Load2(CInifile& ini, const shared_str& sect);
    virtual void Save2(CInifile& ini, const shared_str& sect);
};

struct EPAAvoid : public EParticleAction
{
    EPAAvoid();
    virtual void Compile(IWriter& F);
};

struct EPABounce : public EParticleAction
{
    EPABounce();
    virtual void Compile(IWriter& F);
};

struct EPACopyVertexB : public EParticleAction
{
    EPACopyVertexB();
    virtual void Compile(IWriter& F);
};

struct EPADamping : public EParticleAction
{
    EPADamping();
    virtual void Compile(IWriter& F);
};

struct EPAExplosion : public EParticleAction
{
    EPAExplosion();
    virtual void Compile(IWriter& F);
};

struct EPAFollow : public EParticleAction
{
    EPAFollow();
    virtual void Compile(IWriter& F);
};

struct EPAGravitate : public EParticleAction
{
    EPAGravitate();
    virtual void Compile(IWriter& F);
};

struct EPAGravity : public EParticleAction
{
    EPAGravity();
    virtual void Compile(IWriter& F);
};

struct EPAJet : public EParticleAction
{
    EPAJet();
    virtual void Compile(IWriter& F);
};

struct EPAKillOld : public EParticleAction
{
    EPAKillOld();
    virtual void Compile(IWriter& F);
};

struct EPAMatchVelocity : public EParticleAction
{
    EPAMatchVelocity();
    virtual void Compile(IWriter& F);
};

struct EPAMove : public EParticleAction
{
    EPAMove();
    virtual void Compile(IWriter& F);
};

struct EPAOrbitLine : public EParticleAction
{
    EPAOrbitLine();
    virtual void Compile(IWriter& F);
};

struct EPAOrbitPoint : public EParticleAction
{
    EPAOrbitPoint();
    virtual void Compile(IWriter& F);
};

struct EPARandomAccel : public EParticleAction
{
    EPARandomAccel();
    virtual void Compile(IWriter& F);
};

struct EPARandomDisplace : public EParticleAction
{
    EPARandomDisplace();
    virtual void Compile(IWriter& F);
};

struct EPARandomVelocity : public EParticleAction
{
    EPARandomVelocity();
    virtual void Compile(IWriter& F);
};

struct EPARestore : public EParticleAction
{
    EPARestore();
    virtual void Compile(IWriter& F);
};

struct EPAScatter : public EParticleAction
{
    EPAScatter();
    virtual void Compile(IWriter& F);
};

struct EPASink : public EParticleAction
{
    EPASink();
    virtual void Compile(IWriter& F);
};

struct EPASinkVelocity : public EParticleAction
{
    EPASinkVelocity();
    virtual void Compile(IWriter& F);
};

struct EPASpeedLimit : public EParticleAction
{
    EPASpeedLimit();
    virtual void Compile(IWriter& F);
};

struct EPASource : public EParticleAction
{
    EPASource();
    virtual void Compile(IWriter& F);
};

struct EPATargetColor : public EParticleAction
{
    EPATargetColor();
    virtual void Compile(IWriter& F);
};

struct EPATargetSize : public EParticleAction
{
    EPATargetSize();
    virtual void Compile(IWriter& F);
};

struct EPATargetRotate : public EParticleAction
{
    EPATargetRotate();
    virtual void Compile(IWriter& F);
};

struct EPATargetVelocity : public EParticleAction
{
    EPATargetVelocity();
    virtual void Compile(IWriter& F);
};

struct EPAVortex : public EParticleAction
{
    EPAVortex();
    virtual void Compile(IWriter& F);
};

struct EPATurbulence : public EParticleAction
{
    float*** nval;
    float age;

public:
    EPATurbulence();

    virtual void Compile(IWriter& F);
};

typedef EParticleAction* (*_CreateEAction)(PAPI::PActionEnum type);
extern ECORE_API _CreateEAction pCreateEAction;
