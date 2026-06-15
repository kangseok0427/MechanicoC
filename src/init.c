#include "game.h"
#include <string.h>
#include <stdio.h>

static Character make_kael(void) {
    Character c; memset(&c, 0, sizeof(c));
    snprintf(c.name, MAX_NAME, "KAEL");
    c.role = ROLE_WARRIOR; c.hp_max = c.hp_cur = 60;
    c.atk = 9; c.def = 5; c.spd = 10; c.is_alive = 1; c.taunt_target = -1;
    snprintf(c.skills[0].name, MAX_NAME, "Rapid Slash");
    c.skills[0].type = SKILL_DAMAGE; c.skills[0].target = TARGET_SINGLE;
    c.skills[0].dmg_ratio = 0.6f; c.skills[0].cooldown_max = 2;
    c.skills[0].effect_type = EFFECT_NONE;
    snprintf(c.skills[1].name, MAX_NAME, "Critical Strike");
    c.skills[1].type = SKILL_DAMAGE; c.skills[1].target = TARGET_SINGLE;
    c.skills[1].dmg_ratio = 1.5f; c.skills[1].cooldown_max = 3;
    c.skills[1].effect_type = EFFECT_STUN; c.skills[1].effect_duration = 1;
    snprintf(c.skills[2].name, MAX_NAME, "Berserker");
    c.skills[2].type = SKILL_BUFF; c.skills[2].target = TARGET_SELF;
    c.skills[2].cooldown_max = 5; c.skills[2].effect_duration = 3;
    return c;
}

static Character make_voss(void) {
    Character c; memset(&c, 0, sizeof(c));
    snprintf(c.name, MAX_NAME, "VOSS");
    c.role = ROLE_TANKER; c.hp_max = c.hp_cur = 80;
    c.atk = 5; c.def = 10; c.spd = 4; c.is_alive = 1; c.taunt_target = -1;
    snprintf(c.skills[0].name, MAX_NAME, "Furious Counter");
    c.skills[0].type = SKILL_COUNTER; c.skills[0].target = TARGET_SELF;
    c.skills[0].cooldown_max = 3; c.skills[0].effect_duration = 2;
    snprintf(c.skills[1].name, MAX_NAME, "Provoke");
    c.skills[1].type = SKILL_TAUNT; c.skills[1].target = TARGET_ALL_ENEMY;
    c.skills[1].cooldown_max = 4; c.skills[1].effect_duration = 1;
    snprintf(c.skills[2].name, MAX_NAME, "Iron Fortress");
    c.skills[2].type = SKILL_BUFF; c.skills[2].target = TARGET_ALL_ALLY;
    c.skills[2].cooldown_max = 5; c.skills[2].effect_duration = 2;
    return c;
}

static Character make_lyra(void) {
    Character c; memset(&c, 0, sizeof(c));
    snprintf(c.name, MAX_NAME, "LYRA");
    c.role = ROLE_SORCERER; c.hp_max = c.hp_cur = 50;
    c.atk = 10; c.def = 4; c.spd = 7; c.is_alive = 1; c.taunt_target = -1;
    snprintf(c.skills[0].name, MAX_NAME, "Plasma Cannon");
    c.skills[0].type = SKILL_DAMAGE; c.skills[0].target = TARGET_SINGLE;
    c.skills[0].dmg_ratio = 2.2f; c.skills[0].cooldown_max = 3;
    snprintf(c.skills[1].name, MAX_NAME, "Sys Disruption");
    c.skills[1].type = SKILL_DEBUFF; c.skills[1].target = TARGET_SINGLE;
    c.skills[1].dmg_ratio = 0.8f; c.skills[1].cooldown_max = 4;
    c.skills[1].effect_type = EFFECT_DEF_DOWN; c.skills[1].effect_value = 3;
    c.skills[1].effect_duration = 2;
    snprintf(c.skills[2].name, MAX_NAME, "Overload");
    c.skills[2].type = SKILL_AOE; c.skills[2].target = TARGET_ALL_ENEMY;
    c.skills[2].dmg_ratio = 1.2f; c.skills[2].cooldown_max = 6;
    return c;
}

void game_init(GameState *gs) {
    memset(gs, 0, sizeof(GameState));
    gs->party.members[0] = make_kael();
    gs->party.members[1] = make_voss();
    gs->party.members[2] = make_lyra();
    gs->party.gold = 100;
    gs->zone_current = 1;
    gs->phase = PHASE_HUB;
    snprintf(gs->last_event, 64, "game_start");
}