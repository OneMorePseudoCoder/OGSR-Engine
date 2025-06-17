#include "stdafx.h"

#include "SoundRender_Core.h"
#include "SoundRender_Emitter.h"
#include "SoundRender_Target.h"
#include "SoundRender_Source.h"

XRSOUND_API extern float psSoundCull;

void CSoundRender_Core::i_start(CSoundRender_Emitter* E)
{
    R_ASSERT(E);

    // Search lowest-priority target
    float Ptest = E->priority();
    float Ptarget = flt_max;
    float dist = flt_max;
    CSoundRender_Target* T = 0;
    for (u32 it = 0; it < s_targets.size(); it++)
    {
        CSoundRender_Target* Ttest = s_targets[it];
        float e_dist = flt_max;
        auto E = Ttest->get_emitter();
        if (E)
            e_dist = SoundRender->listener_position().distance_to(E->get_params()->position);

        if (Ttest->priority < Ptarget || (Ttest->priority == Ptarget && Ptarget < psSoundCull && E && e_dist > dist))
        {
            T = Ttest;
            Ptarget = Ttest->priority;
            if (Ptarget < 0)
                break;
            dist = e_dist;
        }
    }

    // Stop currently playing
    if (T->get_emitter())
    {
        if (Ptarget >= psSoundCull)
            MsgDbg("! [%s]: snd_targets[%u] limit reached: Ptest[%f] Ptarget[%f] psSoundCull[%f]", __FUNCTION__, s_targets.size(), Ptest, Ptarget, psSoundCull);
        T->get_emitter()->cancel();
    }

    // Associate
    E->target = T;
    E->target->start(E);
    T->priority = Ptest;
}

void CSoundRender_Core::i_stop(CSoundRender_Emitter* E)
{
    R_ASSERT(E);
    R_ASSERT(E == E->target->get_emitter());
    E->target->stop();
    E->target = NULL;
}

void CSoundRender_Core::i_rewind(CSoundRender_Emitter* E)
{
    R_ASSERT(E);
    R_ASSERT(E == E->target->get_emitter());
    E->target->rewind();
}

BOOL CSoundRender_Core::i_allow_play(CSoundRender_Emitter* E)
{
    // Search available target
    float Ptest = E->priority();
    for (u32 it = 0; it < s_targets.size(); it++)
    {
        CSoundRender_Target* T = s_targets[it];
        if (T->priority < Ptest)
            return TRUE;
    }
    return FALSE;
}
