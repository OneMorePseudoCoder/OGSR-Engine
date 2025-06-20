#pragma once

// refs
class CBone;

#define BI_NONE (u16(-1))

#define OGF_IKDATA_VERSION 0x0001

#define MAX_BONE_PARAMS 4

class ENGINE_API CBoneInstance;
// callback
typedef void BoneCallbackFunction(CBoneInstance* P);
typedef BoneCallbackFunction* BoneCallback;

//*** Bone Instance *******************************************************************************
#pragma pack(push, 8)
class ENGINE_API CBoneInstance
{
public:
    // data
    Fmatrix mTransform; // final x-form matrix (local to model)
    Fmatrix mRenderTransform; // final x-form matrix (model_base -> bone -> model)
    Fmatrix mRenderTransform_old; // final x-form matrix (model_base -> bone -> model) old
    Fmatrix mRenderTransform_tmp; // final x-form matrix (model_base -> bone -> model) temp
private:
    BoneCallback Callback{};
    void* Callback_Param{};
    BOOL Callback_overwrite{}; // performance hint - don't calc anims
    u32 Callback_type{};

public:
    float param[MAX_BONE_PARAMS]; //
    //
    // methods
public:
    IC BoneCallback callback() { return Callback; }
    IC void* callback_param() { return Callback_Param; }
    IC BOOL callback_overwrite() { return Callback_overwrite; } // performance hint - don't calc anims
    IC u32 callback_type() { return Callback_type; }

public:
    IC void construct()
    {
        mTransform.identity();
        mRenderTransform.identity();
        mRenderTransform_old.identity();
        mRenderTransform_tmp.identity();

        Callback = nullptr;
        Callback_Param = nullptr;
        Callback_overwrite = FALSE;
        Callback_type = 0;

        ZeroMemory(&param, sizeof(param));
    }

    void set_callback(u32 Type, BoneCallback C, void* Param, BOOL overwrite = FALSE)
    {
        Callback = C;
        Callback_Param = Param;
        Callback_overwrite = overwrite;
        Callback_type = Type;
    }

    void reset_callback()
    {
        Callback = nullptr;
        Callback_Param = nullptr;
        Callback_overwrite = FALSE;
        Callback_type = 0;
    }
    void set_callback_overwrite(BOOL v) { Callback_overwrite = v; }

    void set_param(u32 idx, float data);
    float get_param(u32 idx);

    u32 mem_usage() { return sizeof(*this); }
};
#pragma pack(pop)

#pragma pack(push, 2)
struct ENGINE_API vertBoned1W // (3+3+3+3+2+1)*4 = 15*4 = 60 bytes
{
    Fvector P;
    Fvector N;
    Fvector T;
    Fvector B;
    float u, v;
    u32 matrix;
    void get_pos(Fvector& p) const { p.set(P); }
#ifdef DEBUG
    static const u8 bones_count = 1;
    u16 get_bone_id(u8 bone) const
    {
        VERIFY(bone < bones_count);
        return u16(matrix);
    }
#endif
};
struct ENGINE_API vertBoned2W // (1+3+3 + 1+3+3 + 2)*4 = 16*4 = 64 bytes
{
    u16 matrix0;
    u16 matrix1;
    Fvector P;
    Fvector N;
    Fvector T;
    Fvector B;
    float w;
    float u, v;
    void get_pos(Fvector& p) const { p.set(P); }
#ifdef DEBUG
    static const u8 bones_count = 2;
    u16 get_bone_id(u8 bone) const
    {
        VERIFY(bone < bones_count);
        return bone == 0 ? matrix0 : matrix1;
    }
#endif
};
struct ENGINE_API vertBoned3W // 70 bytes
{
    u16 m[3];
    Fvector P;
    Fvector N;
    Fvector T;
    Fvector B;
    float w[2];
    float u, v;
    void get_pos(Fvector& p) const { p.set(P); }
#ifdef DEBUG
    static const u8 bones_count = 3;
    u16 get_bone_id(u8 bone) const
    {
        VERIFY(bone < bones_count);
        return m[bone];
    }
#endif
};
struct ENGINE_API vertBoned4W // 76 bytes
{
    u16 m[4];
    Fvector P;
    Fvector N;
    Fvector T;
    Fvector B;
    float w[3];
    float u, v;
    void get_pos(Fvector& p) const { p.set(P); }
#ifdef DEBUG
    static const u8 bones_count = 4;
    u16 get_bone_id(u8 bone) const
    {
        VERIFY(bone < bones_count);
        return m[bone];
    }
#endif
};
#pragma pack(pop)

#pragma pack(push, 1)
enum EJointType
{
    jtRigid,
    jtCloth,
    jtJoint,
    jtWheel,
    jtNone,
    jtSlider,
    jtForceU32 = u32(-1)
};

struct ECORE_API SJointLimit
{
    Fvector2 limit;
    float spring_factor;
    float damping_factor;
    SJointLimit() { Reset(); }
    void Reset()
    {
        limit.set(0.f, 0.f);
        spring_factor = 1.f;
        damping_factor = 1.f;
    }
};

struct ECORE_API SBoneShape
{
    enum EShapeType
    {
        stNone,
        stBox,
        stSphere,
        stCylinder,
        stForceU32 = u16(-1)
    };

    enum EShapeFlags
    {
        sfNoPickable = (1 << 0), // use only in RayPick
        sfRemoveAfterBreak = (1 << 1),
        sfNoPhysics = (1 << 2),
        sfNoFogCollider = (1 << 3),
    };

    u16 type; // 2
    Flags16 flags; // 2
    Fobb box; // 15*4
    Fsphere sphere; // 4*4
    Fcylinder cylinder; // 8*4
    SBoneShape() { Reset(); }

    void Reset()
    {
        flags.zero();
        type = stNone;
        box.invalidate();
        sphere.P.set(0.f, 0.f, 0.f);
        sphere.R = 0.f;
        cylinder.invalidate();
    }

    bool Valid()
    {
        switch (type)
        {
        case stBox: return !fis_zero(box.m_halfsize.x) && !fis_zero(box.m_halfsize.y) && !fis_zero(box.m_halfsize.z);
        case stSphere: return !fis_zero(sphere.R);
        case stCylinder: return !fis_zero(cylinder.m_height) && !fis_zero(cylinder.m_radius) && !fis_zero(cylinder.m_direction.square_magnitude());
        };
        return true;
    }
};

struct ECORE_API SJointIKData
{
    // IK
    EJointType type;
    SJointLimit limits[3]; // by [axis XYZ on joint] and[Z-wheel,X-steer on wheel]
    float spring_factor;
    float damping_factor;
    enum
    {
        flBreakable = (1 << 0),
    };
    Flags32 ik_flags;
    float break_force; // [0..+INF]
    float break_torque; // [0..+INF]

    float friction;

    SJointIKData() { Reset(); }
    void Reset()
    {
        limits[0].Reset();
        limits[1].Reset();
        limits[2].Reset();
        type = jtRigid;
        spring_factor = 1.f;
        damping_factor = 1.f;
        ik_flags.zero();
        break_force = 0.f;
        break_torque = 0.f;
    }

    void clamp_by_limits(Fvector& dest_xyz);

    void Export(IWriter& F)
    {
        F.w_u32(type);
        for (auto& limit : limits)
        {
            VERIFY(_min(-limits[k].limit.x, -limits[k].limit.y) == -limits[k].limit.y);
            VERIFY(_max(-limits[k].limit.x, -limits[k].limit.y) == -limits[k].limit.x);

            F.w_float(-limit.limit.y); // min (swap special for ODE)
            F.w_float(-limit.limit.x); // max (swap special for ODE)

            F.w_float(limit.spring_factor);
            F.w_float(limit.damping_factor);
        }
        F.w_float(spring_factor);
        F.w_float(damping_factor);

        F.w_u32(ik_flags.get());
        F.w_float(break_force);
        F.w_float(break_torque);

        F.w_float(friction);
    }

    bool Import(IReader& F, u16 vers)
    {
        type = (EJointType)F.r_u32();
        F.r(limits, sizeof(SJointLimit) * 3);
        spring_factor = F.r_float();
        damping_factor = F.r_float();
        ik_flags.flags = F.r_u32();
        break_force = F.r_float();
        break_torque = F.r_float();
        if (vers > 0)
        {
            friction = F.r_float();
        }
        return true;
    }
};
#pragma pack(pop)

class IBoneData
{
public:
    virtual const SJointIKData& get_IK_data() const = 0;
    virtual float lo_limit(u8 k) const = 0;
    virtual float hi_limit(u8 k) const = 0;
};

//  refs
class CBone;
DEFINE_VECTOR(CBone*, BoneVec, BoneIt);

class ECORE_API CBone : public CBoneInstance, public IBoneData
{
    shared_str name;
    shared_str parent_name;
    shared_str wmap;
    Fvector rest_offset;
    Fvector rest_rotate; // XYZ format (Game format)
    float rest_length;

    Fvector mot_offset;
    Fvector mot_rotate; // XYZ format (Game format)
    float mot_length;

    Fmatrix mot_transform;

    Fmatrix local_rest_transform;
    Fmatrix rest_transform;
    Fmatrix rest_i_transform;

public:
    int SelfID;
    CBone* parent;
    BoneVec children;

public:
    // editor part
    Flags8 flags;
    enum
    {
        flSelected = (1 << 0),
    };
    SJointIKData IK_data;
    shared_str game_mtl;

    float mass;
    Fvector center_of_mass;

public:
    CBone();
    virtual ~CBone();

    void SetName(const char* p)
    {
        name = p;
        xr_strlwr(name);
    }
    void SetParentName(const char* p)
    {
        parent_name = p;
        xr_strlwr(parent_name);
    }
    void SetWMap(const char* p) { wmap = p; }
    void SetRestParams(float length, const Fvector& offset, const Fvector& rotate)
    {
        rest_offset.set(offset);
        rest_rotate.set(rotate);
        rest_length = length;
    };

    shared_str Name() { return name; }
    shared_str ParentName() { return parent_name; }
    shared_str WMap() { return wmap; }
    IC CBone* Parent() { return parent; }
    IC BOOL IsRoot() { return (parent == nullptr); }
    shared_str& NameRef() { return name; }

    // IO
    void Save(IWriter& F);
    void Load_0(IReader& F);
    void Load_1(IReader& F);

    IC float engine_lo_limit(u8 k) const { return -IK_data.limits[k].limit.y; }
    IC float engine_hi_limit(u8 k) const { return -IK_data.limits[k].limit.x; }

    IC float editor_lo_limit(u8 k) const { return IK_data.limits[k].limit.x; }
    IC float editor_hi_limit(u8 k) const { return IK_data.limits[k].limit.y; }

    void SaveData(IWriter& F);
    void LoadData(IReader& F);
    void ResetData();
    void CopyData(CBone* bone);

private:
    const SJointIKData& get_IK_data() const { return IK_data; }
    float lo_limit(u8 k) const { return engine_lo_limit(k); }
    float hi_limit(u8 k) const { return engine_hi_limit(k); }
};

//*** Shared Bone Data ****************************************************************************
class CBoneData;
// t-defs
typedef xr_vector<CBoneData*> vecBones;
typedef vecBones::iterator vecBonesIt;

class ENGINE_API CBoneData : public IBoneData
{
protected:
    u16 SelfID;
    u16 ParentID;

public:
    shared_str name;

    Fobb obb;

    Fmatrix bind_transform;
    Fmatrix m2b_transform; // model to bone conversion transform
    SBoneShape shape;
    shared_str game_mtl_name;
    u16 game_mtl_idx;
    SJointIKData IK_data;
    float mass;
    Fvector center_of_mass;

    Fvector rotation{};
    Fvector position{};

    vecBones children; // bones which are slaves to this

    DEFINE_VECTOR(u16, FacesVec, FacesVecIt);
    DEFINE_VECTOR(FacesVec, ChildFacesVec, ChildFacesVecIt);
    ChildFacesVec child_faces; // shared
public:
    CBoneData(u16 ID) : SelfID(ID) { VERIFY(SelfID != BI_NONE); }
    virtual ~CBoneData() {}
#ifdef DEBUG
    typedef svector<int, 128> BoneDebug;
    void DebugQuery(BoneDebug& L);
#endif
    IC void SetParentID(u16 id) { ParentID = id; }

    IC u16 GetSelfID() const { return SelfID; }
    IC u16 GetParentID() const { return ParentID; }

    // assign face
    void AppendFace(u16 child_idx, u16 idx) { child_faces[child_idx].push_back(idx); }
    // Calculation
    void CalculateM2B(const Fmatrix& Parent);

private:
    const SJointIKData& get_IK_data() const { return IK_data; }
    float lo_limit(u8 k) const { return IK_data.limits[k].limit.x; }
    float hi_limit(u8 k) const { return IK_data.limits[k].limit.y; }

public:
    virtual u32 mem_usage()
    {
        u32 sz = sizeof(*this) + sizeof(vecBones::value_type) * children.size();
        for (ChildFacesVecIt c_it = child_faces.begin(); c_it != child_faces.end(); ++c_it)
            sz += c_it->size() * sizeof(FacesVec::value_type) + sizeof(*c_it);
        return sz;
    }
};

enum EBoneCallbackType
{
    bctDummy = u32(0), // 0 - required!!!
    bctPhysics,
    bctCustom,
};
