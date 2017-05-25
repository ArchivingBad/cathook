/*
 * helpers.cpp
 *
 *  Created on: Oct 8, 2016
 *      Author: nullifiedcat
 */

#include "common.h"
#include "hooks.h"
#include "sdk.h"
#include "profiler.h"

#include <link.h>
#include <pwd.h>
#include <sys/mman.h>

std::vector<ConVar*> g_ConVars;
FILE* hConVarsFile = 0;
void BeginConVars() {
	FILE *hAutoexec, *hMatchexec;

	hConVarsFile = fopen(strfmt("%scfg/cat_defaults.cfg", g_pszTFPath), "w");
	hAutoexec = fopen(strfmt("%scfg/cat_autoexec.cfg", g_pszTFPath), "r+");
	if (!hAutoexec) {
		hAutoexec = fopen(strfmt("%scfg/cat_autoexec.cfg", g_pszTFPath), "w");
		if (hAutoexec) {
			fprintf(hAutoexec, "// Put your custom cathook settings in this file\n// This script will be executed EACH TIME YOU INJECT CATHOOK\n");
			fclose(hAutoexec);
		}
	} else fclose(hAutoexec);
	hMatchexec = fopen(strfmt("%scfg/cat_matchexec.cfg", g_pszTFPath), "r+");
	if (!hMatchexec) {
		hMatchexec = fopen(strfmt("%scfg/cat_matchexec.cfg", g_pszTFPath), "w");
		if (hMatchexec) {
			fprintf(hMatchexec, "// Put your custom cathook settings in this file\n// This script will be executed EACH TIME YOU JOIN A MATCH\n");
			fclose(hMatchexec);
		}
	} else fclose(hMatchexec);
	if (hConVarsFile) fprintf(hConVarsFile, "// THIS FILE IS AUTO-GENERATED BY CATHOOK\n// DO NOT EDIT IT, ALL CHANGES WILL BE UNDONE!\n// If you want to change default settings, add changed convars to cat_autoexec.cfg\n");
	SetCVarInterface(g_ICvar);
}

void EndConVars() {
	RegisterCatVars();
	RegisterCatCommands();
	if (hConVarsFile) {
		//fprintf(hConVarsFile, "\nexec cat_autoexec\n");
		fprintf(hConVarsFile, "cat_killsay_reload\ncat_spam_reload\n");
		fclose(hConVarsFile);
	}
	ConVar_Register();
}

// StackOverflow copypasta xddd
void ReplaceString(char* target, char* what, char* with_what) {
	char buffer[1024] = { 0 };
	char *insert_point;
	const char *tmp, *p;
	size_t needle_len, repl_len;
	buffer[0] = 0;
	insert_point = &buffer[0];
	tmp = target;
	needle_len = strlen(what);
	repl_len = strlen(with_what);
	while (1) {
		p = strstr(tmp, what);
		if (p == NULL) {
			strcpy(insert_point, tmp);
			break;
		}
		memcpy(insert_point, tmp, p - tmp);
		insert_point += p - tmp;
		memcpy(insert_point, with_what, repl_len);
		insert_point += repl_len;
		tmp = p + needle_len;
	}
	strcpy(target, buffer);
}

ConVar* CreateConVar(std::string name, std::string value, std::string help) {
	char* namec = new char[256];
	char* valuec = new char[256];
	char* helpc = new char[256];
	strncpy(namec, name.c_str(), 255);
	strncpy(valuec, value.c_str(), 255);
	strncpy(helpc, help.c_str(), 255);
	//logging::Info("Creating ConVar: %s %s %s", namec, valuec, helpc);
	ConVar* ret = new ConVar((const char*)namec, (const char*)valuec, 0, (const char*)helpc);
	if (hConVarsFile)
		fprintf(hConVarsFile, "%s \"%s\"\n", name.c_str(), value.c_str());
	g_ICvar->RegisterConCommand(ret);
	g_ConVars.push_back(ret);
	return ret;
}

ConCommand* CreateConCommand(const char* name, FnCommandCallback_t callback, const char* help) {
	ConCommand* ret = new ConCommand(name, callback, help);
	g_ICvar->RegisterConCommand(ret);
	return ret;
}

const char* GetBuildingName(CachedEntity* ent) {
	if (!ent) return "[NULL]";
	if (ent->m_iClassID == CL_CLASS(CObjectSentrygun)) return "Sentry";
	if (ent->m_iClassID == CL_CLASS(CObjectDispenser)) return "Dispenser";
	if (ent->m_iClassID == CL_CLASS(CObjectTeleporter)) return "Teleporter";
	return "[NULL]";
}

void format_internal(std::stringstream& stream) {}

std::string WordWrap(std::string& in, int max, unsigned long font) {
	std::stringstream result, line, wordstream, next;
	std::string word;
	char ch;
	for (int i = 0; i < in.size(); i++) {
		ch = in.at(i);
		if (ch == ' ' || ch == '\n') {
			word = wordstream.str();
			//logging::Info("got word: '%s'", word.c_str());
			wordstream.str("");
			auto size = draw::GetStringLength(font, line.str() + word);
			if (size.first >= max) {
				//logging::Info("wrapping: '%s'", line.str().c_str());
				result << line.str() << '\n';
				line.str("");
			}
			line << word << ch;
		} else {
			wordstream << ch;
		}
	}
	word = wordstream.str();
	wordstream.str("");
	auto size = draw::GetStringLength(fonts::MENU, line.str() + word);
	if (size.first >= max) {
		result << line.str() << '\n';
		line.str(word);
	} else {
		line << word;
	}
	result << line.str();
	return result.str();
}

void ReplaceString(std::string& input, const std::string& what, const std::string& with_what) {
	size_t index;
	index = input.find(what);
	while (index != std::string::npos) {
		input.replace(index, what.size(), with_what);
		index = input.find(what, index + with_what.size());
	}
}

powerup_type GetPowerupOnPlayer(CachedEntity* player) {
	if (CE_BAD(player)) return powerup_type::not_powerup;
	if (!HasCondition<TFCond_HasRune>(player)) return powerup_type::not_powerup;
	if (HasCondition<TFCond_RuneStrength>(player)) return powerup_type::strength;
	if (HasCondition<TFCond_RuneHaste>(player)) return powerup_type::haste;
	if (HasCondition<TFCond_RuneRegen>(player)) return powerup_type::regeneration;
	if (HasCondition<TFCond_RuneResist>(player)) return powerup_type::resistance;
	if (HasCondition<TFCond_RuneVampire>(player)) return powerup_type::vampire;
	if (HasCondition<TFCond_RuneWarlock>(player)) return powerup_type::reflect;
	if (HasCondition<TFCond_RunePrecision>(player)) return powerup_type::precision;
	if (HasCondition<TFCond_RuneAgility>(player)) return powerup_type::agility;
	if (HasCondition<TFCond_RuneKnockout>(player)) return powerup_type::knockout;
	if (HasCondition<TFCond_KingRune>(player)) return powerup_type::king;
	if (HasCondition<TFCond_PlagueRune>(player)) return powerup_type::plague;
	if (HasCondition<TFCond_SupernovaRune>(player)) return powerup_type::supernova;
	return powerup_type::not_powerup;
}

bool HasDarwins(CachedEntity* ent) {
	int *hWeapons;
	if (CE_INT(ent, netvar.iClass) != tf_sniper) return false;
	hWeapons = (int*)((unsigned)(RAW_ENT(ent) + netvar.hMyWeapons));
	for (int i = 0; i < 4; i++) {
		IClientEntity* weapon = g_IEntityList->GetClientEntityFromHandle(hWeapons[i]);
		if (weapon && NET_INT(weapon, netvar.iItemDefinitionIndex) == 231) return true;
	}
	return false;
}

void VectorTransform (const float *in1, const matrix3x4_t& in2, float *out) {
	out[0] = (in1[0] * in2[0][0] + in1[1] * in2[0][1] + in1[2] * in2[0][2]) + in2[0][3];
	out[1] = (in1[0] * in2[1][0] + in1[1] * in2[1][1] + in1[2] * in2[1][2]) + in2[1][3];
	out[2] = (in1[0] * in2[2][0] + in1[1] * in2[2][1] + in1[2] * in2[2][2]) + in2[2][3];
}

bool GetHitbox(CachedEntity* entity, int hb, Vector& out) {
	CachedHitbox *box;

	if (CE_BAD(entity)) return false;
	box = entity->hitboxes.GetHitbox(hb);
	if (!box) out = entity->m_vecOrigin;
	else out = box->center;
	return true;
}

void VectorAngles(Vector &forward, Vector &angles) {
	float tmp, yaw, pitch;

	if(forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if(forward[2] > 0)
			pitch = 270;
		else
			pitch = 90;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / PI);
		if(yaw < 0)
			yaw += 360;

		tmp = sqrt((forward[0] * forward[0] + forward[1] * forward[1]));
		pitch = (atan2(-forward[2], tmp) * 180 / PI);
		if(pitch < 0)
			pitch += 360;
	}

	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}

char GetUpperChar(ButtonCode_t button) {
	switch (button) {
	case KEY_0:
		return ')';
	case KEY_1:
		return '!';
	case KEY_2:
		return '@';
	case KEY_3:
		return '#';
	case KEY_4:
		return '$';
	case KEY_5:
		return '%';
	case KEY_6:
		return '^';
	case KEY_7:
		return '&';
	case KEY_8:
		return '*';
	case KEY_9:
		return '(';
	case KEY_LBRACKET:
		return '{';
	case KEY_RBRACKET:
		return '}';
	case KEY_SEMICOLON:
		return ':';
	case KEY_BACKQUOTE:
		return '~';
	case KEY_APOSTROPHE:
		return '"';
	case KEY_COMMA:
		return '<';
	case KEY_PERIOD:
		return '>';
	case KEY_SLASH:
		return '?';
	case KEY_BACKSLASH:
		return '|';
	case KEY_MINUS:
		return '_';
	case KEY_EQUAL:
		return '+';
	default:
		if (strlen(g_IInputSystem->ButtonCodeToString(button)) != 1) return 0;
		return toupper(*g_IInputSystem->ButtonCodeToString(button));
	}
}

char GetChar(ButtonCode_t button) {
	switch (button) {
	case KEY_PAD_DIVIDE:
		return '/';
	case KEY_PAD_MULTIPLY:
		return '*';
	case KEY_PAD_MINUS:
		return '-';
	case KEY_PAD_PLUS:
		return '+';
	case KEY_SEMICOLON:
		return ';';
	default:
		if (button >= KEY_PAD_0 && button <= KEY_PAD_9) {
			return button - KEY_PAD_0 + '0';
		}
		if (strlen(g_IInputSystem->ButtonCodeToString(button)) != 1) return 0;
		return *g_IInputSystem->ButtonCodeToString(button);
	}
}

void FixMovement(CUserCmd& cmd, Vector& viewangles) {
	Vector movement, ang;
	float speed, yaw;
	movement.x = cmd.forwardmove;
	movement.y = cmd.sidemove;
	movement.z = cmd.upmove;
	speed = sqrt(movement.x * movement.x + movement.y * movement.y);
	VectorAngles(movement, ang);
	yaw = DEG2RAD(ang.y - viewangles.y + cmd.viewangles.y);
	cmd.forwardmove = cos(yaw) * speed;
	cmd.sidemove = sin(yaw) * speed;
}

bool AmbassadorCanHeadshot() {
	if (IsAmbassador(g_pLocalPlayer->weapon())) {
		if ((g_GlobalVars->curtime - CE_FLOAT(g_pLocalPlayer->weapon(), netvar.flLastFireTime)) <= 1.0) {
			return false;
		}
	}
	return true;
}

float RandFloatRange(float min, float max) {
    return (min + 1) + (((float) rand()) / (float) RAND_MAX) * (max - (min + 1));
}

bool IsEntityVisible(CachedEntity* entity, int hb) {
	Vector hit;
	if (g_Settings.bInvalid) return false;
	if (entity == g_pLocalPlayer->entity) return true;
	if (hb == -1) {
		return IsEntityVectorVisible(entity, entity->m_vecOrigin);
	} else {
		return entity->hitboxes.VisibilityCheck(hb);
	}

}

bool IsEntityVectorVisible(CachedEntity* entity, Vector endpos) {
	trace_t trace_object;
	Ray_t ray;

	if (g_Settings.bInvalid) return false;
	if (entity == g_pLocalPlayer->entity) return true;
	if (CE_BAD(g_pLocalPlayer->entity)) return false;
	if (CE_BAD(entity)) return false;
	trace::filter_default.SetSelf(RAW_ENT(g_pLocalPlayer->entity));
	ray.Init(g_pLocalPlayer->v_Eye, endpos);
	{
		PROF_SECTION(IEVV_TraceRay);
		g_ITrace->TraceRay(ray, MASK_SHOT_HULL, &trace::filter_default, &trace_object);
	}
	return (trace_object.fraction >= 0.99f || (((IClientEntity*)trace_object.m_pEnt)) == RAW_ENT(entity));
}

Vector GetBuildingPosition(CachedEntity* ent) {
	Vector res;
	res = ent->m_vecOrigin;
	if (ent->m_iClassID == CL_CLASS(CObjectDispenser)) res.z += 30;
	if (ent->m_iClassID == CL_CLASS(CObjectTeleporter)) res.z += 8;
	if (ent->m_iClassID == CL_CLASS(CObjectSentrygun)) {
		switch (CE_INT(ent, netvar.iUpgradeLevel)) {
		case 1:
			res.z += 30;
			break;
		case 2:
			res.z += 50;
			break;
		case 3:
			res.z += 60;
			break;
		}
	}
	return res;
}

bool IsBuildingVisible(CachedEntity* ent) {
	return IsEntityVectorVisible(ent, GetBuildingPosition(ent));
}

void fClampAngle(Vector& qaAng) {
	while(qaAng[0] > 89)
		qaAng[0] -= 180;

	while(qaAng[0] < -89)
		qaAng[0] += 180;

	while(qaAng[1] > 180)
		qaAng[1] -= 360;

	while(qaAng[1] < -180)
		qaAng[1] += 360;

	qaAng.z = 0;
}

float DistToSqr(CachedEntity* entity) {
	if (CE_BAD(entity)) return 0.0f;
	return g_pLocalPlayer->v_Origin.DistToSqr(entity->m_vecOrigin);
}

void Patch(void* address, void* patch, size_t length) {
	void* page = (void*)((uintptr_t)address &~ 0xFFF);
	mprotect(page, 0xFFF, PROT_WRITE | PROT_EXEC);
	memcpy(address, patch, length);
	mprotect(page, 0xFFF, PROT_EXEC);
}

bool IsProjectileCrit(CachedEntity* ent) {
	if (ent->m_bGrenadeProjectile)
		return CE_BYTE(ent, netvar.Grenade_bCritical);
	return CE_BYTE(ent, netvar.Rocket_bCritical);
}

weaponmode GetWeaponMode() {
	int weapon_handle, slot;
	CachedEntity *weapon;

	if (CE_BAD(LOCAL_E)) return weapon_invalid;
	weapon_handle = CE_INT(LOCAL_E, netvar.hActiveWeapon);
	if (IDX_BAD((weapon_handle & 0xFFF))) {
		//logging::Info("IDX_BAD: %i", weapon_handle & 0xFFF);
		return weaponmode::weapon_invalid;
	}
	weapon = (ENTITY(weapon_handle & 0xFFF));
	if (CE_BAD(weapon)) return weaponmode::weapon_invalid;
	slot = vfunc<int(*)(IClientEntity*)>(RAW_ENT(g_pLocalPlayer->weapon()), 395, 0)(RAW_ENT(g_pLocalPlayer->weapon()));
	if (slot == 2) return weaponmode::weapon_melee;
	if (slot > 2) {
		return weaponmode::weapon_pda;
	} else if (weapon->m_iClassID == CL_CLASS(CTFLunchBox) ||
			weapon->m_iClassID == CL_CLASS(CTFLunchBox_Drink) ||
			weapon->m_iClassID == CL_CLASS(CTFBuffItem)) {
		return weaponmode::weapon_consumable;
	} else if ( weapon->m_iClassID == CL_CLASS(CTFRocketLauncher_DirectHit) ||
				weapon->m_iClassID == CL_CLASS(CTFRocketLauncher) ||
				weapon->m_iClassID == CL_CLASS(CTFGrenadeLauncher) ||
				weapon->m_iClassID == CL_CLASS(CTFPipebombLauncher) ||
				weapon->m_iClassID == CL_CLASS(CTFCompoundBow) ||
				weapon->m_iClassID == CL_CLASS(CTFBat_Wood) ||
				weapon->m_iClassID == CL_CLASS(CTFBat_Giftwrap) ||
				weapon->m_iClassID == CL_CLASS(CTFFlareGun) ||
				weapon->m_iClassID == CL_CLASS(CTFFlareGun_Revenge) ||
				weapon->m_iClassID == CL_CLASS(CTFSyringeGun)) {
		return weaponmode::weapon_projectile;
	} else if (weapon->m_iClassID == CL_CLASS(CTFJar) ||
			   weapon->m_iClassID == CL_CLASS(CTFJarMilk)) {
		return weaponmode::weapon_throwable;
	} else if (weapon->m_iClassID == CL_CLASS(CWeaponMedigun)) {
		return weaponmode::weapon_medigun;
	}
	return weaponmode::weapon_hitscan;
}

bool LineIntersectsBox(Vector& bmin, Vector& bmax, Vector& lmin, Vector& lmax) {
	if (lmax.x < bmin.x && lmin.x < bmin.x) return false;
	if (lmax.y < bmin.y && lmin.y < bmin.y) return false;
	if (lmax.z < bmin.z && lmin.z < bmin.z) return false;
	if (lmax.x > bmax.x && lmin.x > bmax.x) return false;
	if (lmax.y > bmax.y && lmin.y > bmax.y) return false;
	if (lmax.z > bmax.z && lmin.z > bmax.z) return false;
	return true;
}

// TODO FIX this function
bool GetProjectileData(CachedEntity* weapon, float& speed, float& gravity) {
	float rspeed, rgrav;

	IF_GAME (!IsTF()) return false;

	if (CE_BAD(weapon)) return false;
	rspeed = 0.0f;
	rgrav = 0.0f;
	typedef float(GetProjectileData)(IClientEntity*);

	if (weapon->m_iClassID == CL_CLASS(CTFRocketLauncher_DirectHit)) {
		rspeed = 1980.0f;
	} else if (weapon->m_iClassID == CL_CLASS(CTFRocketLauncher)) {
		rspeed = 1100.0f;
	} else if (weapon->m_iClassID == CL_CLASS(CTFGrenadeLauncher)) {
		IF_GAME (IsTF2()) {
			rspeed = vfunc<GetProjectileData*>(RAW_ENT(weapon), 527)(RAW_ENT(weapon));
			// TODO Wrong grenade launcher gravity
			rgrav = 0.5f;
		} else IF_GAME (IsTF2C()) {
			rspeed = 1100.0f;
			rgrav = 0.5f;
		}
	} else if (weapon->m_iClassID == CL_CLASS(CTFCompoundBow)) {
		rspeed = vfunc<GetProjectileData*>(RAW_ENT(weapon), 527)(RAW_ENT(weapon));
		rgrav = vfunc<GetProjectileData*>(RAW_ENT(weapon), 528)(RAW_ENT(weapon));
	} else if (weapon->m_iClassID == CL_CLASS(CTFBat_Wood)) {
		rspeed = 3000.0f;
		rgrav = 0.5f;
	} else if (weapon->m_iClassID == CL_CLASS(CTFFlareGun)) {
		rspeed = 2000.0f;
		rgrav = 0.25f;
	} else if (weapon->m_iClassID == CL_CLASS(CTFSyringeGun)) {
		rgrav = 0.2f;
		rspeed = 990.0f;
	}
	speed = rspeed;
	gravity = rgrav;
	return (rspeed || rgrav);
}

constexpr unsigned developer_list[] = { 306902159, 347272825, 401679596 };

bool Developer(CachedEntity* ent) {
	for (int i = 0; i < sizeof(developer_list) / sizeof(unsigned); i++) {
		if (developer_list[i] == ent->player_info.friendsID) return true;
	}
	return false;
}

/*const char* MakeInfoString(IClientEntity* player) {
	char* buf = new char[256]();
	player_info_t info;
	if (!engineClient->GetPlayerInfo(player->entindex(), &info)) return (const char*)0;
	logging::Info("a");
	int hWeapon = NET_INT(player, netvar.hActiveWeapon);
	if (NET_BYTE(player, netvar.iLifeState)) {
		sprintf(buf, "%s is dead %s", info.name, tfclasses[NET_INT(player, netvar.iClass)]);
		return buf;
	}
	if (hWeapon) {
		IClientEntity* weapon = ENTITY(hWeapon & 0xFFF);
		sprintf(buf, "%s is %s with %i health using %s", info.name, tfclasses[NET_INT(player, netvar.iClass)], NET_INT(player, netvar.iHealth), weapon->GetClientClass()->GetName());
	} else {
		sprintf(buf, "%s is %s with %i health", info.name, tfclasses[NET_INT(player, netvar.iClass)], NET_INT(player, netvar.iHealth));
	}
	logging::Info("Result: %s", buf);
	return buf;
}*/

bool IsVectorVisible(Vector origin, Vector target) {
	trace_t trace_visible;
	Ray_t ray;

	trace::filter_no_player.SetSelf(RAW_ENT(g_pLocalPlayer->entity));
	ray.Init(origin, target);
	g_ITrace->TraceRay(ray, MASK_SHOT_HULL, &trace::filter_no_player, &trace_visible);
	return (trace_visible.fraction == 1.0f);
}

void WhatIAmLookingAt(int* result_eindex, Vector* result_pos) {
	Ray_t ray;
	Vector forward;
	float sp, sy, cp, cy;
	QAngle angle;
	trace_t trace;

	trace::filter_default.SetSelf(RAW_ENT(g_pLocalPlayer->entity));
	g_IEngine->GetViewAngles(angle);
	sy = sinf(DEG2RAD(angle[1]));
	cy = cosf(DEG2RAD(angle[1]));
	sp = sinf(DEG2RAD(angle[0]));
	cp = cosf(DEG2RAD(angle[0]));
	forward.x = cp * cy;
	forward.y = cp * sy;
	forward.z = -sp;
	forward = forward * 8192.0f + g_pLocalPlayer->v_Eye;
	ray.Init(g_pLocalPlayer->v_Eye, forward);
	g_ITrace->TraceRay(ray, 0x4200400B, &trace::filter_default, &trace);
	if (result_pos)
		*result_pos = trace.endpos;
	if (result_eindex) {
		*result_eindex = 0;
	}
	if (trace.m_pEnt && result_eindex) {
		*result_eindex = ((IClientEntity*)(trace.m_pEnt))->entindex();
	}
}

bool IsSentryBuster(CachedEntity* entity) {
	return (entity->m_Type == EntityType::ENTITY_PLAYER &&
			CE_INT(entity, netvar.iClass) == tf_class::tf_demoman &&
			g_pPlayerResource->GetMaxHealth(entity) == 2500);
}

bool IsAmbassador(CachedEntity* entity) {
	IF_GAME (!IsTF2()) return false;
	if (entity->m_iClassID != CL_CLASS(CTFRevolver)) return false;
	const int& defidx = CE_INT(entity, netvar.iItemDefinitionIndex);
	return (defidx == 61 || defidx == 1006);
}

bool IsPlayerInvulnerable(CachedEntity* player) {
	return HasConditionMask<KInvulnerabilityMask.cond_0, KInvulnerabilityMask.cond_1, KInvulnerabilityMask.cond_2, KInvulnerabilityMask.cond_3>(player);
}

bool IsPlayerCritBoosted(CachedEntity* player) {
	return HasConditionMask<KCritBoostMask.cond_0, KCritBoostMask.cond_1, KCritBoostMask.cond_2, KCritBoostMask.cond_3>(player);
}

bool IsPlayerInvisible(CachedEntity* player) {
	return HasConditionMask<KInvisibilityMask.cond_0, KInvisibilityMask.cond_1, KInvisibilityMask.cond_2, KInvisibilityMask.cond_3>(player);
}

// F1 c&p
Vector CalcAngle(Vector src, Vector dst) {
	Vector AimAngles, delta;
	float hyp;
	delta = src - dst;
	hyp = sqrtf((delta.x * delta.x) + (delta.y * delta.y)); //SUPER SECRET IMPROVEMENT CODE NAME DONUT STEEL
	AimAngles.x = atanf(delta.z / hyp) * RADPI;
	AimAngles.y = atanf(delta.y / delta.x) * RADPI;
	AimAngles.z = 0.0f;
	if(delta.x >= 0.0)
		AimAngles.y += 180.0f;
	return AimAngles;
}

void MakeVector(Vector angle, Vector& vector) {
	float pitch, yaw, tmp;
	pitch = float(angle[0] * PI / 180);
	yaw = float(angle[1] * PI / 180);
	tmp = float(cos(pitch));
	vector[0] = float(-tmp * -cos(yaw));
	vector[1] = float(sin(yaw)*tmp);
	vector[2] = float(-sin(pitch));
}

float GetFov(Vector angle, Vector src, Vector dst) {
	Vector ang, aim;
	float mag, u_dot_v;
	ang = CalcAngle(src, dst);

	MakeVector(angle, aim);
	MakeVector(ang, ang);

	mag = sqrtf(pow(aim.x, 2) + pow(aim.y, 2) + pow(aim.z, 2));
	u_dot_v = aim.Dot(ang);

	return RAD2DEG(acos(u_dot_v / (pow(mag, 2))));
}

bool CanHeadshot() {
	return (g_pLocalPlayer->flZoomBegin > 0.0f && (g_GlobalVars->curtime - g_pLocalPlayer->flZoomBegin > 0.2f));
}

bool CanShoot() {
	float servertime, nextattack;

	servertime = (float)(CE_INT(g_pLocalPlayer->entity, netvar.nTickBase)) * g_GlobalVars->interval_per_tick;
	nextattack = CE_FLOAT(g_pLocalPlayer->weapon(), netvar.flNextPrimaryAttack);
	return nextattack <= servertime;
}

QAngle VectorToQAngle(Vector in) {
	return *(QAngle*)&in;
}

Vector QAngleToVector(QAngle in) {
	return *(Vector*)&in;
}

void AimAt(Vector origin, Vector target, CUserCmd* cmd) {
	Vector angles, tr;
	tr = (target - origin);
	VectorAngles(tr, angles);
	fClampAngle(angles);
	cmd->viewangles = angles;
}

void AimAtHitbox(CachedEntity* ent, int hitbox, CUserCmd* cmd) {
	Vector r;
	r = ent->m_vecOrigin;
	GetHitbox(ent, hitbox, r);
	AimAt(g_pLocalPlayer->v_Eye, r, cmd);
}

bool IsEntityVisiblePenetration(CachedEntity* entity, int hb) {
	trace_t trace_visible;
	Ray_t ray;
	Vector hit;
	int ret;
	bool correct_entity;
	IClientEntity *ent;
	trace::filter_penetration.SetSelf(RAW_ENT(g_pLocalPlayer->entity));
	trace::filter_penetration.Reset();
	ret = GetHitbox(entity, hb, hit);
	if (ret) {
		return false;
	}
	ray.Init(g_pLocalPlayer->v_Origin + g_pLocalPlayer->v_ViewOffset, hit);
	g_ITrace->TraceRay(ray, MASK_SHOT_HULL, &trace::filter_penetration, &trace_visible);
	correct_entity = false;
	if (trace_visible.m_pEnt) {
		correct_entity = ((IClientEntity*)trace_visible.m_pEnt) == RAW_ENT(entity);
	}
	if (!correct_entity) return false;
	g_ITrace->TraceRay(ray, 0x4200400B, &trace::filter_default, &trace_visible);
	if (trace_visible.m_pEnt) {
		ent = (IClientEntity*)trace_visible.m_pEnt;
		if (ent) {
			if (ent->GetClientClass()->m_ClassID == RCC_PLAYER) {
				if (ent == RAW_ENT(entity)) return false;
				if (trace_visible.hitbox >= 0) {
					return true;
				}
			}
		}
	}
	return false;
}

void RunEnginePrediction(IClientEntity* ent, CUserCmd *ucmd) {
    if (!ent) return;

    typedef void(*SetupMoveFn)(IPrediction*, IClientEntity *, CUserCmd *, class IMoveHelper *, CMoveData *);
    typedef void(*FinishMoveFn)(IPrediction*, IClientEntity *, CUserCmd*, CMoveData*);

    void **predictionVtable = *((void ***)g_IPrediction);
    SetupMoveFn oSetupMove = (SetupMoveFn)(*(unsigned*)(predictionVtable + 19));
    FinishMoveFn oFinishMove = (FinishMoveFn)(*(unsigned*)(predictionVtable + 20));

    CMoveData *pMoveData = (CMoveData*)(sharedobj::client->lmap->l_addr + 0x1F69C0C);

    float frameTime = g_GlobalVars->frametime;
    float curTime = g_GlobalVars->curtime;

    CUserCmd defaultCmd;
    if(ucmd == NULL) {
        ucmd = &defaultCmd;
    }

    NET_VAR(ent, 4188, CUserCmd*) = ucmd;

    g_GlobalVars->curtime =  g_GlobalVars->interval_per_tick * NET_INT(ent, netvar.nTickBase);
    g_GlobalVars->frametime = g_GlobalVars->interval_per_tick;

    *g_PredictionRandomSeed = MD5_PseudoRandom(g_pUserCmd->command_number) & 0x7FFFFFFF;
    g_IGameMovement->StartTrackPredictionErrors(reinterpret_cast<CBasePlayer*>(ent));
    oSetupMove(g_IPrediction, ent, ucmd, NULL, pMoveData);
    g_IGameMovement->ProcessMovement(reinterpret_cast<CBasePlayer*>(ent), pMoveData);
    oFinishMove(g_IPrediction, ent, ucmd, pMoveData);
    g_IGameMovement->FinishTrackPredictionErrors(reinterpret_cast<CBasePlayer*>(ent));

    NET_VAR(ent, 4188, CUserCmd*) = nullptr;

    g_GlobalVars->frametime = frameTime;
    g_GlobalVars->curtime = curTime;

    return;
}


class CMoveData;

/*void RunEnginePrediction(IClientEntity* ent, CUserCmd *ucmd) {
	// we are going to require some helper functions for this to work
	// notably SetupMove, FinishMove and ProcessMovement


	// setup the types of the functions
	typedef void(*SetupMoveFn)(IClientEntity *, CUserCmd *, class IMoveHelper *, CMoveData *);
	typedef void(*FinishMoveFn)(IClientEntity *, CUserCmd*, CMoveData*);
	typedef void(*ProcessMovementFn)(IClientEntity *, CMoveData *);
	typedef void(*StartTrackPredictionErrorsFn)(IClientEntity *);
	typedef void(*FinishTrackPredictionErrorsFn)(IClientEntity *);

	// get the vtable
	void **predictionVtable = *(void ***)prediction;
	logging::Info("predictionVtable 0x%08x", predictionVtable);
	// get the functions
	SetupMoveFn oSetupMove = (SetupMoveFn) predictionVtable[19];
	FinishMoveFn oFinishMove = (FinishMoveFn) predictionVtable[20];

	// get the vtable
	void **gameMovementVtable = *(void ***)gamemovement;
	logging::Info("gameMovementVtable 0x%08x", gameMovementVtable);
	// get the functions
	ProcessMovementFn oProcessMovement = (ProcessMovementFn) gameMovementVtable[2];
	StartTrackPredictionErrorsFn oStartTrackPredictionErrors = (StartTrackPredictionErrorsFn) gameMovementVtable[3];
	FinishTrackPredictionErrorsFn oFinishTrackPredictionErrors = (FinishTrackPredictionErrorsFn) gameMovementVtable[4];

	// use this as movedata (should be big enough - otherwise the stack will die!)
	unsigned char moveData[2048];
	CMoveData *pMoveData = (CMoveData *)&(moveData[0]);
	logging::Info("pMoveData 0x%08x", pMoveData);

	// back up globals
	float frameTime = gvars->frametime;
	float curTime = gvars->curtime;

	CUserCmd defaultCmd;
	if(ucmd == NULL)
	{
		ucmd = &defaultCmd;
	}

	// set the current command
	NET_VAR(ent, 0x105C, void*) = ucmd;

	// set up the globals
	gvars->curtime =  gvars->interval_per_tick * NET_INT(ent, netvar.nTickBase);
	gvars->frametime = gvars->interval_per_tick;

	oStartTrackPredictionErrors(ent);

	logging::Info("StartTrackPredictionErrors(ent)");
	oSetupMove(ent, ucmd, NULL, pMoveData);
	logging::Info("oSetupMove");
	oProcessMovement(ent, pMoveData);
	logging::Info("oProcessMovement");
	oFinishMove(ent, ucmd, pMoveData);
	logging::Info("oFinishMove");

	oFinishTrackPredictionErrors(ent);
	logging::Info("oFinishTrackPredictionErrors");
	// reset the current command
	NET_VAR(ent, 0x105C, void *) = 0;

	// restore globals
	gvars->frametime = frameTime;
	gvars->curtime = curTime;

	return;
}

float oldCurtime;
float oldFrametime;

void StartPrediction(CUserCmd* cmd) {
	oldCurtime = gvars->curtime;
	oldFrametime = gvars->frametime;
	gvars->curtime = NET_INT(g_pLocalPlayer->entity, netvar.nTickBase) * gvars->interval_per_tick;
	gvars->frametime = gvars->interval_per_tick;
	//gamemovement->
}

void EndPrediction() {
	gvars->curtime = oldCurtime;
	gvars->frametime = oldFrametime;
}*/


// You are responsible for delete[]'ing the resulting string.
char* strfmt(const char* fmt, ...) {
	char* buf = new char[1024];
	va_list list;
	va_start(list, fmt);
	vsprintf(buf, fmt, list);
	va_end(list);
	return buf;
}

const char* powerups[] = {
	"STRENGTH",
	"RESISTANCE",
	"VAMPIRE",
	"REFLECT",
	"HASTE",
	"REGENERATION",
	"PRECISION",
	"AGILITY",
	"KNOCKOUT",
	"KING",
	"PLAGUE",
	"SUPERNOVA",
	"CRITS"
};

const std::string classes[] = {
	"Scout",
	"Sniper",
	"Soldier",
	"Demoman",
	"Medic",
	"Heavy",
	"Pyro",
	"Spy",
	"Engineer"
};
