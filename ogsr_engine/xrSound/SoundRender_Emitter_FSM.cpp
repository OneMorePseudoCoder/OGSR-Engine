#include "stdafx.h"

#include "SoundRender_Emitter.h"
#include "SoundRender_Core.h"
#include "SoundRender_Source.h"

XRSOUND_API extern float psSoundCull;
constexpr float TIME_TO_STOP_INFINITE = static_cast<float>(0xffffffff);

inline u32 calc_cursor(const float& fTimeStarted, float& fTime, const float& fTimeTotal, const float& fFreq, const WAVEFORMATEX& wfx)
{
    if (fTime < fTimeStarted)
        fTime = fTimeStarted; // Андрюха посоветовал, ассерт что ниже вылетел из за паузы как то хитро
    R_ASSERT((fTime - fTimeStarted) >= 0.0f);
    while ((fTime - fTimeStarted) > fTimeTotal / fFreq) // looped
    {
        fTime -= fTimeTotal / fFreq;
    }
    u32 curr_sample_num = iFloor((fTime - fTimeStarted) * fFreq * wfx.nSamplesPerSec);
    return curr_sample_num * (wfx.wBitsPerSample / 8) * wfx.nChannels;
}

void CSoundRender_Emitter::update(float dt)
{
    float fTime = SoundRender->fTimer_Value;
    float fDeltaTime = SoundRender->fTimer_Delta;

    VERIFY2(!!(owner_data) || (!(owner_data) && (m_current_state == stStopped)), "owner");
    VERIFY2(owner_data ? *(int*)(&owner_data->feedback) : 1, "owner");

    if (bRewind)
    {
        if (target)
            SoundRender->i_rewind(this);
        bRewind = FALSE;
    }

    switch (m_current_state)
    {
    case stStopped: break;
    case stStartingDelayed:
        if (iPaused)
            break;
        starting_delay -= dt;
        if (starting_delay <= 0)
            m_current_state = stStarting;
        break;
    case stStarting:
        if (iPaused)
            break;
        fTimeStarted = fTime;
        fTimeToStop = fTime + (get_length_sec() / p_source.freq); //--#SM+#--
        fTimeToPropagade = fTime;
        fade_volume = 1.f;
        occluder_volume = SoundRender->get_occlusion(p_source.position, .2f, occluder);
        smooth_volume = p_source.base_volume * p_source.volume * (owner_data->s_type == st_Effect ? psSoundVEffects * psSoundVFactor : psSoundVMusic) * (b2D ? 1.f : occluder_volume);

        if (update_culling(dt))
        {
            m_current_state = stPlaying;
            set_cursor(0);
            SoundRender->i_start(this);
        }
        else
            m_current_state = stSimulating;
        break;
    case stStartingLoopedDelayed:
        if (iPaused)
            break;
        starting_delay -= dt;
        if (starting_delay <= 0)
            m_current_state = stStartingLooped;
        break;
    case stStartingLooped:
        if (iPaused)
            break;
        fTimeStarted = fTime;
        fTimeToStop = TIME_TO_STOP_INFINITE;
        fTimeToPropagade = fTime;
        fade_volume = 1.f;
        occluder_volume = SoundRender->get_occlusion(p_source.position, .2f, occluder);
        smooth_volume = p_source.base_volume * p_source.volume * (owner_data->s_type == st_Effect ? psSoundVEffects * psSoundVFactor : psSoundVMusic) * (b2D ? 1.f : occluder_volume);
        
        if (update_culling(dt))
        {
            m_current_state = stPlayingLooped;
            set_cursor(0);
            SoundRender->i_start(this);
        }
        else
            m_current_state = stSimulatingLooped;
        break;
    case stPlaying:
        if (iPaused)
        {
            if (target)
            {
                SoundRender->i_stop(this);
                m_current_state = stSimulating;
            }
            fTimeStarted += fDeltaTime;
            fTimeToStop += fDeltaTime;
            fTimeToPropagade += fDeltaTime;
            break;
        }
        if (fTime >= fTimeToStop)
        {
            // STOP
            m_current_state = stStopped;
            SoundRender->i_stop(this);
        }
        else
        {
            if (!update_culling(dt))
            {
                // switch to: SIMULATE
                m_current_state = stSimulating; // switch state
                SoundRender->i_stop(this);
            }
            else
            {
                // We are still playing
                update_environment(dt);
            }
        }
        break;
    case stSimulating:
        if (iPaused)
        {
            fTimeStarted += fDeltaTime;
            fTimeToStop += fDeltaTime;
            fTimeToPropagade += fDeltaTime;
            break;
        }
        if (fTime >= fTimeToStop)
        {
            // STOP
            m_current_state = stStopped;
        }
        else
        {
            u32 ptr = calc_cursor(fTimeStarted, fTime, get_length_sec(), p_source.freq, source()->m_wformat); //--#SM+#--
            set_cursor(ptr);

            if (update_culling(dt))
            {
                // switch to: PLAY
                m_current_state = stPlaying;
                SoundRender->i_start(this);
            }
        }
        break;
    case stPlayingLooped:
        if (iPaused)
        {
            if (target)
            {
                SoundRender->i_stop(this);
                m_current_state = stSimulatingLooped;
            }
            fTimeStarted += fDeltaTime;
            fTimeToPropagade += fDeltaTime;
            break;
        }
        if (!update_culling(dt))
        {
            // switch to: SIMULATE
            m_current_state = stSimulatingLooped; // switch state
            SoundRender->i_stop(this);
        }
        else
        {
            // We are still playing
            update_environment(dt);
        }
        break;
    case stSimulatingLooped:
        if (iPaused)
        {
            fTimeStarted += fDeltaTime;
            fTimeToPropagade += fDeltaTime;
            break;
        }
        if (update_culling(dt))
        {
            // switch to: PLAY
            m_current_state = stPlayingLooped; // switch state
            u32 ptr = calc_cursor(fTimeStarted, fTime, get_length_sec(), p_source.freq, source()->m_wformat); //--#SM+#--
            set_cursor(ptr);

            SoundRender->i_start(this);
        }
        break;
    }

	//--#SM+# Begin--
    // hard rewind
    switch (m_current_state)
    {
    case stStarting:
    case stStartingLooped:
    case stPlaying:
    case stSimulating:
    case stPlayingLooped:
    case stSimulatingLooped:
        if (fTimeToRewind > 0.0f)
        {
            float fLength = get_length_sec();
            bool bLooped = (fTimeToStop == 0xffffffff);

            R_ASSERT2(fLength >= fTimeToRewind, "set_time: target time is bigger than length of sound");

            float fRemainingTime = (fLength - fTimeToRewind) / p_source.freq;
            float fPastTime = fTimeToRewind / p_source.freq;

            fTimeStarted = SoundRender->fTimer_Value - fPastTime;
            fTimeToPropagade = fTimeStarted; //--> For AI events

            if (fTimeStarted < 0.0f)
            {
                R_ASSERT2(fTimeStarted >= 0.0f, "Possible error in sound rewind logic! See log.");

                fTimeStarted = SoundRender->fTimer_Value;
                fTimeToPropagade = fTimeStarted;
            }

            if (!bLooped)
            {
                //--> Пересчитываем время, когда звук должен остановиться [recalculate stop time]
                fTimeToStop = SoundRender->fTimer_Value + fRemainingTime;
            }

            u32 ptr = calc_cursor(fTimeStarted, fTime, fLength, p_source.freq, source()->m_wformat);
            set_cursor(ptr);

            fTimeToRewind = 0.0f;
        }
    default: break;
    }
    //--#SM+# End--


    // if deffered stop active and volume==0 -> physically stop sound
    if (bStopping && fis_zero(fade_volume))
        i_stop();

    VERIFY2(!!(owner_data) || (!(owner_data) && (m_current_state == stStopped)), "owner");
    VERIFY2(owner_data ? *(int*)(owner_data->feedback) : 1, "owner");

    // footer
    bMoved = FALSE;
    if (m_current_state != stStopped)
    {
        if (fTime >= fTimeToPropagade)
            Event_Propagade();
    }
    else if (owner_data)
    {
        VERIFY(this == owner_data->feedback);
        owner_data->feedback = 0;
        owner_data = 0;
    }
}

IC void volume_lerp(float& c, float t, float s, float dt)
{
    float diff = t - c;
    float diff_a = _abs(diff);
    if (diff_a < EPS_S)
        return;
    float mot = s * dt;
    if (mot > diff_a)
        mot = diff_a;
    c += (diff / diff_a) * mot;
}

#include "..\COMMON_AI\ai_sounds.h"
BOOL CSoundRender_Emitter::update_culling(float dt)
{
    if (b2D)
    {
        occluder_volume = 1.f;
        fade_volume += dt * 10.f * (bStopping ? -1.f : 1.f);
    }
    else
    {
        // Check range
        float dist = SoundRender->listener_position().distance_to(p_source.position);
        if (dist > p_source.max_distance)
        {
            smooth_volume = 0;
            return FALSE;
        }

        // Calc attenuated volume
        float fade_scale = bStopping || (att() * p_source.base_volume * p_source.volume * (owner_data->s_type == st_Effect ? psSoundVEffects * psSoundVFactor : psSoundVMusic) < psSoundCull) ? -1.f : 1.f;
        fade_volume += dt * 10.f * fade_scale;

        // Update occlusion
        float occ = (owner_data->g_type == SOUND_TYPE_WORLD_AMBIENT) ? 1.0f : SoundRender->get_occlusion(p_source.position, .2f, occluder);
        volume_lerp(occluder_volume, occ, 1.f, dt);
        clamp(occluder_volume, 0.f, 1.f);
    }

    clamp(fade_volume, 0.f, 1.f);
    
    // Update smoothing
    smooth_volume = .9f * smooth_volume + .1f * (p_source.base_volume * p_source.volume * (owner_data->s_type == st_Effect ? psSoundVEffects * psSoundVFactor : psSoundVMusic) * occluder_volume * fade_volume);
    if (smooth_volume < psSoundCull)
        return FALSE; // allow volume to go up
    
    // Here we has enought "PRIORITY" to be soundable
    // If we are playing already, return OK
    // --- else check availability of resources
    if (target)
        return TRUE;
    else
        return SoundRender->i_allow_play(this);
}

float CSoundRender_Emitter::priority() { return smooth_volume * att() * priority_scale; }

float CSoundRender_Emitter::att()
{
    float dist = SoundRender->listener_position().distance_to(p_source.position);
    float rolloff_dist = psSoundRolloff * dist;

    // Calc linear fade --#SM+#--
    // https://www.desmos.com/calculator/lojovfugle
    const float fMinDistDiff = rolloff_dist - p_source.min_distance;
    float att;
    if (fMinDistDiff > 0.f)
    {
        const float fMaxDistDiff = p_source.max_distance - p_source.min_distance;
        att = pow(1.f - (fMinDistDiff / fMaxDistDiff), psSoundLinearFadeFactor);
    }
    else
        att = 1.f;

    clamp(att, 0.f, 1.f);

    return att;
}

void CSoundRender_Emitter::update_environment(float dt)
{}
