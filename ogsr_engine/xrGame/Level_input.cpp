#include "stdafx.h"
#include <dinput.h>
#include "HUDmanager.h"
#include "..\xr_3da\XR_IOConsole.h"
#include "entity_alive.h"
#include "game_sv_single.h"
#include "alife_simulator.h"
#include "alife_simulator_header.h"
#include "level_graph.h"
#include "../xr_3da/fdemorecord.h"
#include "level.h"
#include "xr_level_controller.h"
#include "game_cl_base.h"
#include "stalker_movement_manager.h"
#include "Inventory.h"
#include "xrServer.h"

#include "actor.h"
#include "huditem.h"
#include "ui/UIDialogWnd.h"
#include "clsid_game.h"
#include "../xr_3da/xr_input.h"
#include "saved_game_wrapper.h"

#include "game_object_space.h"
#include "script_callback_ex.h"
#include "GamePersistent.h"
#include "MainMenu.h"

#ifdef DEBUG
#include "ai/monsters/BaseMonster/base_monster.h"
#endif

#ifdef DEBUG
extern void try_change_current_entity();
extern void restore_actor();
#endif

#include "embedded_editor/embedded_editor_main.h"

bool g_bDisableAllInput = false;

extern float g_fTimeFactor;

#define CURRENT_ENTITY() (game ? CurrentEntity() : nullptr)

void CLevel::IR_OnMouseWheel(int direction)
{
    if (Editor_MouseWheel(direction))
        return;

    if (g_bDisableAllInput)
        return;

    if (HUD().GetUI()->IR_OnMouseWheel(direction))
        return;
    if (Device.Paused())
        return;

    if (game && Game().IR_OnMouseWheel(direction))
        return;

    if (Actor())
        Actor()->callback(GameObject::eOnMouseWheel)(direction);

    if (HUD().GetUI()->MainInputReceiver())
        return;
    if (CURRENT_ENTITY())
    {
        IInputReceiver* IR = smart_cast<IInputReceiver*>(smart_cast<CGameObject*>(CURRENT_ENTITY()));
        if (IR)
            IR->IR_OnMouseWheel(direction);
    }
}

static int mouse_button_2_key[] = {MOUSE_1, MOUSE_2, MOUSE_3, MOUSE_4, MOUSE_5, MOUSE_6, MOUSE_7, MOUSE_8};

void CLevel::IR_OnMousePress(int btn) { IR_OnKeyboardPress(mouse_button_2_key[btn]); }

void CLevel::IR_OnMouseRelease(int btn) { IR_OnKeyboardRelease(mouse_button_2_key[btn]); }

void CLevel::IR_OnMouseHold(int btn) { IR_OnKeyboardHold(mouse_button_2_key[btn]); }

void CLevel::IR_OnMouseMove(int dx, int dy)
{
    if (Editor_MouseMove(dx, dy))
        return;

    if (g_bDisableAllInput)
        return;
    if (HUD().GetUI()->IR_OnMouseMove(dx, dy))
        return;
    if (Device.Paused())
        return;

    if (Actor())
        Actor()->callback(GameObject::eOnMouseMove)(dx, dy);

    if (CURRENT_ENTITY())
    {
        IInputReceiver* IR = smart_cast<IInputReceiver*>(smart_cast<CGameObject*>(CURRENT_ENTITY()));
        if (IR)
            IR->IR_OnMouseMove(dx, dy);
    }
}

// Обработка нажатия клавиш
extern bool g_block_pause;
extern bool g_block_all_except_movement;

void CLevel::IR_OnKeyboardPress(int key)
{
    if (Editor_KeyPress(key))
        return;

    if (GamePersistent().OnKeyboardPress(key))
        return;

    EGameActions _curr = get_binded_action(key);

    if (m_blocked_actions.find(_curr) != m_blocked_actions.end())
        return; // Real Wolf. 14.10.2014

    const bool b_ui_exist = (Has_HUD() && HUD().GetUI());

    switch (_curr)
    {
    case kSCREENSHOT:
        Render->Screenshot();
        return;

    case kCONSOLE:
        Console->Show();
        return;

    case kQUIT: {
        if (b_ui_exist && HUD().GetUI()->MainInputReceiver())
        {
            if (HUD().GetUI()->MainInputReceiver()->IR_OnKeyboardPress(key))
                return; // special case for mp and main_menu

            if (MainMenu()->IsActive() || !Device.Paused())
                HUD().GetUI()->StartStopMenu(HUD().GetUI()->MainInputReceiver(), true);
        }
        else
            Console->Execute("main_menu");
        return;
        }

    case kPAUSE:
        if (!g_block_pause)
        {
            Device.Pause(!Device.Paused(), TRUE, TRUE, "li_pause_key");
        }
        return;
    }

    if (g_bDisableAllInput)
        return;
    
    if (g_block_all_except_movement)
    {
        if (!(_curr < kCAM_1 || _curr == kPAUSE || _curr == kSCREENSHOT || _curr == kQUIT || _curr == kCONSOLE))
            return;
    }

    if (!b_ui_exist)
        return;

    if (Actor())
    {
        Actor()->callback(GameObject::eOnKeyPress)(key, _curr);

        if (g_bDisableAllInput)
            return;
    }

    if (b_ui_exist && HUD().GetUI()->IR_OnKeyboardPress(key))
        return;

    if (Device.Paused())
        return;

    if (game && Game().IR_OnKeyboardPress(key))
        return;

    if (_curr == kQUICK_SAVE)
    {
        Console->Execute("save");
        return;
    }
    else if (_curr == kQUICK_LOAD)
    {
#ifdef DEBUG
        FS.get_path("$game_config$")->m_Flags.set(FS_Path::flNeedRescan, TRUE);
        FS.get_path("$game_scripts$")->m_Flags.set(FS_Path::flNeedRescan, TRUE);
        FS.rescan_pathes();
#endif // DEBUG
        string_path saved_game, command;
        strconcat(sizeof(saved_game), saved_game, Core.UserName, "_", "quicksave");
        if (!CSavedGameWrapper::valid_saved_game(saved_game))
            return;

        strconcat(sizeof(command), command, "load ", saved_game);
        Console->Execute(command);
        return;
    }

#ifdef DEBUG
    case DIK_RETURN:
    case DIK_NUMPADENTER: bDebug = !bDebug; return;

    case DIK_BACK: HW.Caps.SceneMode = (HW.Caps.SceneMode + 1) % 3; return;

    case DIK_F4: {
        if (pInput->iGetAsyncKeyState(DIK_LALT))
            break;

        if (pInput->iGetAsyncKeyState(DIK_RALT))
            break;

        bool bOk = false;
        u32 i = 0, j, n = Objects.o_count();
        if (pCurrentEntity)
            for (; i < n; ++i)
                if (Objects.o_get_by_iterator(i) == pCurrentEntity)
                    break;
        if (i < n)
        {
            j = i;
            bOk = false;
            for (++i; i < n; ++i)
            {
                CEntityAlive* tpEntityAlive = smart_cast<CEntityAlive*>(Objects.o_get_by_iterator(i));
                if (tpEntityAlive)
                {
                    bOk = true;
                    break;
                }
            }
            if (!bOk)
                for (i = 0; i < j; ++i)
                {
                    CEntityAlive* tpEntityAlive = smart_cast<CEntityAlive*>(Objects.o_get_by_iterator(i));
                    if (tpEntityAlive)
                    {
                        bOk = true;
                        break;
                    }
                }
            if (bOk)
            {
                CObject* tpObject = CurrentEntity();
                CObject* __I = Objects.o_get_by_iterator(i);
                CObject** I = &__I;

                SetEntity(*I);
                if (tpObject != *I)
                {
                    CActor* pActor = smart_cast<CActor*>(tpObject);
                    if (pActor)
                        pActor->inventory().Items_SetCurrentEntityHud(false);
                }
                if (tpObject)
                {
                    Engine.Sheduler.Unregister(tpObject);
                    Engine.Sheduler.Register(tpObject, TRUE);
                };
                Engine.Sheduler.Unregister(*I);
                Engine.Sheduler.Register(*I, TRUE);

                CActor* pActor = smart_cast<CActor*>(*I);
                if (pActor)
                {
                    pActor->inventory().Items_SetCurrentEntityHud(true);

                    CHudItem* pHudItem = smart_cast<CHudItem*>(pActor->inventory().ActiveItem());
                    if (pHudItem)
                    {
                        pHudItem->OnStateSwitch(pHudItem->GetState());
                    }
                }
            }
        }
        return;
    }
    case MOUSE_1: {
        if (pInput->iGetAsyncKeyState(DIK_LALT))
        {
            if (CurrentEntity()->CLS_ID == CLSID_OBJECT_ACTOR)
                try_change_current_entity();
            else
                restore_actor();
            return;
        }
        break;
    }
        /**/

    case DIK_DIVIDE:
        if (OnServer())
        {
            //			float NewTimeFactor				= pSettings->r_float("alife","time_factor");
            Server->game->SetGameTimeFactor(g_fTimeFactor);
        }
        break;
    case DIK_MULTIPLY:
        if (OnServer())
        {
            float NewTimeFactor = 1000.f;
            Server->game->SetGameTimeFactor(NewTimeFactor);
        }
        break;
#endif

    if (bindConsoleCmds.execute(key))
        return;

    if (b_ui_exist && HUD().GetUI()->MainInputReceiver())
        return;

    if (CURRENT_ENTITY())
    {
        IInputReceiver* IR = smart_cast<IInputReceiver*>(smart_cast<CGameObject*>(CURRENT_ENTITY()));
        if (IR)
            IR->IR_OnKeyboardPress(_curr);
    }

#ifdef DEBUG
    CObject* obj = Level().Objects.FindObjectByName("monster");
    if (obj)
    {
        CBaseMonster* monster = smart_cast<CBaseMonster*>(obj);
        if (monster)
            monster->debug_on_key(key);
    }
#endif
}

void CLevel::IR_OnKeyboardRelease(int key)
{
    if (Editor_KeyRelease(key))
        return;

    if (g_bDisableAllInput)
        return;

    EGameActions _curr = get_binded_action(key);

   if (m_blocked_actions.find(_curr) != m_blocked_actions.end())
        return; // Real Wolf. 14.10.2014

    if (g_block_all_except_movement)
    {
        if (!(_curr < kCAM_1 || _curr == kPAUSE || _curr == kSCREENSHOT || _curr == kQUIT || _curr == kCONSOLE))
            return;
    }

    const bool b_ui_exist = (Has_HUD() && HUD().GetUI());

    if (b_ui_exist && HUD().GetUI()->IR_OnKeyboardRelease(key))
        return;

    if (Device.Paused())
        return;

    if (game && Game().OnKeyboardRelease(_curr))
        return;

    if ((key != DIK_LALT) && (key != DIK_RALT) && (key != DIK_F4) && Actor())
        Actor()->callback(GameObject::eOnKeyRelease)(key, _curr);

    if (b_ui_exist && HUD().GetUI()->MainInputReceiver())
        return;

    if (CURRENT_ENTITY())
    {
        IInputReceiver* IR = smart_cast<IInputReceiver*>(smart_cast<CGameObject*>(CURRENT_ENTITY()));
        if (IR)
            IR->IR_OnKeyboardRelease(_curr);
    }
}

void CLevel::IR_OnKeyboardHold(int key)
{
    if (Editor_KeyHold(key))
        return;

    if (g_bDisableAllInput)
        return;

    EGameActions _curr = get_binded_action(key);

    if (m_blocked_actions.find(_curr) != m_blocked_actions.end())
        return; // Real Wolf. 14.10.2014

    if (g_block_all_except_movement)
    {
        if (!(_curr < kCAM_1 || _curr == kPAUSE || _curr == kSCREENSHOT || _curr == kQUIT || _curr == kCONSOLE))
            return;
    }

    const bool b_ui_exist = (Has_HUD() && HUD().GetUI());

    if (b_ui_exist && HUD().GetUI()->IR_OnKeyboardHold(key))
        return;
    if (b_ui_exist && HUD().GetUI()->MainInputReceiver())
        return;
    if (Device.Paused())
        return;

    if ((key != DIK_LALT) && (key != DIK_RALT) && (key != DIK_F4) && Actor())
        Actor()->callback(GameObject::eOnKeyHold)(key, _curr);

    if (CURRENT_ENTITY())
    {
        IInputReceiver* IR = smart_cast<IInputReceiver*>(smart_cast<CGameObject*>(CURRENT_ENTITY()));
        if (IR)
            IR->IR_OnKeyboardHold(_curr);
    }
}

void CLevel::IR_OnMouseStop(int /**axis/**/, int /**value/**/) {}

void CLevel::IR_OnActivate()
{
    if (!pInput)
        return;
    int i;
    for (i = 0; i < CInput::COUNT_KB_BUTTONS; i++)
    {
        if (pInput->iGetAsyncKeyState(i))
        {
            EGameActions action = get_binded_action(i);
            switch (action)
            {
            case kFWD:
            case kBACK:
            case kL_STRAFE:
            case kR_STRAFE:
            case kLEFT:
            case kRIGHT:
            case kUP:
            case kDOWN:
            case kCROUCH:
            case kACCEL:
            case kL_LOOKOUT:
            case kR_LOOKOUT:
            case kWPN_FIRE: {
                IR_OnKeyboardPress(i);
            }
            break;
            };
        };
    }
}

void CLevel::block_action(EGameActions cmd) { m_blocked_actions.insert(cmd); }

void CLevel::unblock_action(EGameActions cmd) { m_blocked_actions.erase(cmd); }