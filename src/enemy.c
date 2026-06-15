#include "game.h"
#include <string.h>
#include <stdio.h>

static Enemy make_enemy(const char *name, int hp, int atk, int def,
                        int spd, int exp, int gold, EnemyGrade grade) {
    Enemy e; memset(&e, 0, sizeof(e));
    snprintf(e.base.name, MAX_NAME, "%s", name);
    e.base.hp_max = hp; e.base.hp_cur = hp;
    e.base.atk = atk; e.base.def = def; e.base.spd = spd;
    e.base.is_alive = 1; e.base.taunt_target = -1;
    e.grade = grade; e.exp_reward = exp; e.gold_reward = gold;
    return e;
}

void get_enemies_for_zone(int zone, Enemy *out, int *count, int is_boss) {
    int s = zone;
    if (is_boss) {
        *count = 1;
        switch (zone) {
        case 1: out[0]=make_enemy("GUARD-MK1",  80*s, 6*s, 5*s,4, 80, 120,ENEMY_BOSS); break;
        case 2: out[0]=make_enemy("WARDEN-X",  100*s, 8*s, 6*s,5,150, 200,ENEMY_BOSS); break;
        case 3: out[0]=make_enemy("SIEGE-9",   120*s, 9*s, 7*s,6,220, 300,ENEMY_BOSS); break;
        case 4: out[0]=make_enemy("APEX-NULL", 150*s,10*s, 8*s,7,300, 400,ENEMY_BOSS); break;
        default:out[0]=make_enemy("NEXUS-CORE",200*s,12*s,10*s,8,500, 600,ENEMY_BOSS); break;
        }
        return;
    }
    *count = 2;
    switch (zone) {
    case 1: out[0]=make_enemy("DRONE-A",  30*s,4*s,2*s,5,20, 30,ENEMY_NORMAL);
            out[1]=make_enemy("DRONE-B",  30*s,4*s,2*s,4,20, 30,ENEMY_NORMAL); break;
    case 2: out[0]=make_enemy("STALKER",  40*s,5*s,3*s,6,35, 50,ENEMY_NORMAL);
            out[1]=make_enemy("ENFORCER", 55*s,6*s,4*s,5,55, 70,ENEMY_ELITE);  break;
    case 3: out[0]=make_enemy("REAPER",   60*s,7*s,4*s,7,70, 90,ENEMY_ELITE);
            out[1]=make_enemy("REAPER",   60*s,7*s,4*s,6,70, 90,ENEMY_ELITE);  break;
    case 4: out[0]=make_enemy("PHANTOM",  70*s,8*s,5*s,8,90,120,ENEMY_ELITE);
            out[1]=make_enemy("PHANTOM",  70*s,8*s,5*s,7,90,120,ENEMY_ELITE);  break;
    default:out[0]=make_enemy("VOIDMECH", 80*s,9*s,6*s,9,120,150,ENEMY_ELITE);
            out[1]=make_enemy("VOIDMECH", 80*s,9*s,6*s,8,120,150,ENEMY_ELITE); break;
    }
}
