#include "stdafx.h"

#include "bone.h"
#include "gamemtllib.h"

//////////////////////////////////////////////////////////////////////////
// BoneInstance methods

void ENGINE_API CBoneInstance::set_param(u32 idx, float data)
{
    VERIFY(idx < MAX_BONE_PARAMS);
    param[idx] = data;
}

float ENGINE_API CBoneInstance::get_param(u32 idx)
{
    VERIFY(idx < MAX_BONE_PARAMS);
    return param[idx];
}

#ifdef DEBUG
void ENGINE_API CBoneData::DebugQuery(BoneDebug& L)
{
    for (u32 i = 0; i < children.size(); i++)
    {
        L.push_back(SelfID);
        L.push_back(children[i]->SelfID);
        children[i]->DebugQuery(L);
    }
}
#endif

void ENGINE_API CBoneData::CalculateM2B(const Fmatrix& parent)
{
    // Build matrix
    m2b_transform.mul_43(parent, bind_transform);

    // Calculate children
    for (auto& C : children)
        C->CalculateM2B(m2b_transform);

    m2b_transform.invert();
}
