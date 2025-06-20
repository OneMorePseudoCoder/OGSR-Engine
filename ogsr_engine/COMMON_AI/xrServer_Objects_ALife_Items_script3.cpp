////////////////////////////////////////////////////////////////////////////
//	Module 		: xrServer_Objects_ALife_Items_script3.cpp
//	Created 	: 19.09.2002
//  Modified 	: 04.06.2003
//	Author		: Dmitriy Iassenev
//	Description : Server items for ALife simulator, script export, the second part
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "xrServer_Objects_ALife_Items.h"
#include "xrServer_script_macroses.h"

using namespace luabind;


void CSE_ALifeItemWeaponMagazinedWGL::script_register(lua_State* L)
{
    module(L)[luabind_class_item1(CSE_ALifeItemWeaponMagazinedWGL, "cse_alife_item_weapon_magazined_w_gl", CSE_ALifeItemWeaponMagazined)
                  .def_readwrite("ammo_type_2", &CSE_ALifeItemWeaponMagazinedWGL::ammo_type2)
                  .def_readwrite("ammo_elapsed_2", &CSE_ALifeItemWeaponMagazinedWGL::a_elapsed2)
                  .def_readwrite("gl_mode", &CSE_ALifeItemWeaponMagazinedWGL::m_bGrenadeMode)];
}