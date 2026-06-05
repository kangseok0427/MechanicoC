#ifndef GAME_H
#define GAME_H

#include <stdio.h>

/* ── 상수 ── */
#define PARTY_SIZE   3
#define MAX_SKILLS   3
#define MAX_ENEMIES  4
#define MAX_BUFFS    8
#define DUNGEON_W    5
#define DUNGEON_H    5
#define MAX_NAME     32

/* ── Enum ── */
typedef enum { PHASE_HUB, PHASE_DUNGEON, PHASE_BATTLE,
               PHASE_BATTLE_WIN, PHASE_BATTLE_LOSE,
               PHASE_GAMEOVER, PHASE_CLEAR } GamePhase;

typedef enum { ROLE_WARRIOR, ROLE_TANKER, ROLE_SORCERER } CharRole;

typedef enum { SKILL_DAMAGE, SKILL_BUFF, SKILL_DEBUFF,
               SKILL_COUNTER, SKILL_TAUNT, SKILL_AOE } SkillType;

typedef enum { EFFECT_NONE, EFFECT_STUN, EFFECT_DEF_DOWN, EFFECT_ATK_DOWN,
               EFFECT_DEF_UP, EFFECT_ATK_UP, EFFECT_COUNTER_ON,
               EFFECT_TAUNT_ON, EFFECT_DMG_REDUCE } EffectType;

typedef enum { TILE_EMPTY, TILE_BATTLE, TILE_BOSS,
               TILE_EXIT, TILE_VISITED } TileType;

typedef enum { TARGET_SINGLE, TARGET_SELF,
               TARGET_ALL_ENEMY, TARGET_ALL_ALLY } TargetType;

typedef enum { ACTION_ATTACK, ACTION_SKILL_0, ACTION_SKILL_1,
               ACTION_SKILL_2, ACTION_DEFEND, ACTION_FLEE } ActionType;

/* ── 구조체 ── */
typedef struct { EffectType type; int value; int duration; } Buff;

typedef struct {
    char       name[MAX_NAME];
    SkillType  type;
    TargetType target;
    float      dmg_ratio;
    int        cooldown_max;
    int        cooldown_cur;
    EffectType effect_type;
    int        effect_value;
    int        effect_duration;
    float      effect_chance;
} Skill;

typedef struct {
    char     name[MAX_NAME];
    CharRole role;
    int      hp_max, hp_cur;
    int      atk, def, spd;
    Skill    skills[MAX_SKILLS];
    Buff     buffs[MAX_BUFFS];
    int      buff_count;
    int      counter_active;
    int      counter_dmg_taken;
    int      is_alive;
    int      is_defending;
    int      is_stunned;
    int      taunt_target;
} Character;

typedef enum { ENEMY_NORMAL, ENEMY_ELITE, ENEMY_BOSS } EnemyGrade;

typedef struct {
    Character  base;
    EnemyGrade grade;
    int        exp_reward;
    int        gold_reward;
} Enemy;

typedef struct {
    Character members[PARTY_SIZE];
    int       gold;
} Party;

typedef struct { TileType type; int visited; } Tile;

typedef struct {
    Tile tiles[DUNGEON_H][DUNGEON_W];
    int  player_x, player_y;
    int  zone, floor;
    int  boss_defeated;
} Dungeon;

typedef struct {
    Enemy      enemies[MAX_ENEMIES];
    int        enemy_count;
    int        turn;
    int        turn_order[PARTY_SIZE + MAX_ENEMIES];
    int        turn_order_size;
    int        current_actor;
    int        is_boss_fight;
    int        last_dmg;
} BattleState;

typedef struct {
    GamePhase   phase;
    Party       party;
    Dungeon     dungeon;
    BattleState battle;
    int         zone_current;
    int         zone_locked;
    int         dungeons_cleared;
    int         turn_total;
    char        last_event[64];
} GameState;

/* ── game.c 함수 선언 ── */
void game_init(GameState *gs);
void emit_event(GameState *gs, const char *event, int is_your_turn);

/* 파티 */
int  party_alive_count(const Party *p);
void party_rest(Party *p, int cost);

/* 전투 */
void build_turn_order(GameState *gs);
void do_attack(GameState *gs, int actor_idx, int target_idx);
void do_skill(GameState *gs, int actor_idx, int skill_idx, int target_idx);
void do_defend(GameState *gs, int actor_idx);
void do_flee(GameState *gs);
void enemy_turn(GameState *gs, int enemy_idx);
void battle_start(GameState *gs, Enemy *enemies, int count, int is_boss);
int  battle_check_end(GameState *gs);
void battle_end_turn(GameState *gs);

/* 던전 */
void dungeon_generate(Dungeon *d, int zone, int floor);
int  dungeon_valid_dirs(const Dungeon *d, int *out_dirs);
int  dungeon_move(GameState *gs, int dir);

/* 적 */
void get_enemies_for_zone(int zone, Enemy *out, int *count, int is_boss);

#endif
