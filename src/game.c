#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ══════════════════════════════════════════
   JSON emit - 모든 이벤트마다 호출
   is_your_turn=1 이면 Python이 ACTION 보냄
   ══════════════════════════════════════════ */
void emit_event(GameState *gs, const char *event, int is_your_turn) {
    FILE *f = stderr;
    const char *phase_str =
        gs->phase == PHASE_HUB        ? "hub"        :
        gs->phase == PHASE_DUNGEON     ? "dungeon"    :
        gs->phase == PHASE_BATTLE      ? "battle"     :
        gs->phase == PHASE_BATTLE_WIN  ? "battle_win" :
        gs->phase == PHASE_BATTLE_LOSE ? "battle_lose":
        gs->phase == PHASE_GAMEOVER    ? "gameover"   : "clear";

    fprintf(f, "{\"phase\":\"%s\",\"event\":\"%s\",\"is_your_turn\":%d",
            phase_str, event, is_your_turn);
    fprintf(f, ",\"gold\":%d", gs->party.gold);
    fprintf(f, ",\"cleared\":%d", gs->dungeons_cleared);
    fprintf(f, ",\"zone\":%d", gs->zone_current);

    /* 파티 */
    fprintf(f, ",\"party\":[");
    for (int i = 0; i < PARTY_SIZE; i++) {
        const Character *m = &gs->party.members[i];
        fprintf(f, "%s{\"name\":\"%s\",\"hp\":%d,\"hp_max\":%d"
                ",\"atk\":%d,\"def\":%d,\"spd\":%d"
                ",\"alive\":%d,\"stunned\":%d,\"defending\":%d,\"counter\":%d"
                ",\"skills\":[",
                i ? "," : "", m->name,
                m->is_alive ? m->hp_cur : 0,  /* KO면 0 */
                m->hp_max,
                m->atk, m->def, m->spd,
                m->is_alive, m->is_stunned, m->is_defending, m->counter_active);
        for (int si = 0; si < MAX_SKILLS; si++)
            fprintf(f, "%s{\"name\":\"%s\",\"cd\":%d,\"cd_max\":%d}",
                    si ? "," : "",
                    m->skills[si].name,
                    m->skills[si].cooldown_cur,
                    m->skills[si].cooldown_max);
        fprintf(f, "]}");
    }
    fprintf(f, "]");

    /* 전투 중일 때만 적 정보 */
    if (gs->phase == PHASE_BATTLE || gs->phase == PHASE_BATTLE_WIN ||
        gs->phase == PHASE_BATTLE_LOSE) {
        BattleState *b = &gs->battle;
        fprintf(f, ",\"battle\":{\"turn\":%d,\"is_boss\":%d,\"last_dmg\":%d",
                b->turn, b->is_boss_fight, b->last_dmg);

        /* 현재 액터 이름 */
        if (b->current_actor < b->turn_order_size) {
            int ai = b->turn_order[b->current_actor];
            const char *aname = ai < PARTY_SIZE
                ? gs->party.members[ai].name
                : gs->battle.enemies[ai - PARTY_SIZE].base.name;
            fprintf(f, ",\"actor\":\"%s\"", aname);
        }

        /* valid_actions (파티 턴일때만) */
        fprintf(f, ",\"valid_actions\":[");
        if (is_your_turn) {
            int first = 1;
            fprintf(f, "\"attack\""); first = 0;
            for (int si = 0; si < MAX_SKILLS; si++) {
                int ai = b->turn_order[b->current_actor];
                if (ai < PARTY_SIZE) {
                    Character *actor = &gs->party.members[ai];
                    if (actor->skills[si].cooldown_cur == 0 &&
                        actor->skills[si].name[0]) {
                        fprintf(f, ",\"skill_%d\"", si);
                    }
                }
            }
            if (!first) {} /* suppress unused warning */
            fprintf(f, ",\"defend\",\"flee\"");
        }
        fprintf(f, "]");

        fprintf(f, ",\"enemies\":[");
        for (int i = 0; i < b->enemy_count; i++) {
            const Character *e = &b->enemies[i].base;
            fprintf(f, "%s{\"name\":\"%s\",\"hp\":%d,\"hp_max\":%d"
                    ",\"atk\":%d,\"def\":%d,\"spd\":%d,\"alive\":%d}",
                    i ? "," : "", e->name, e->hp_cur, e->hp_max,
                    e->atk, e->def, e->spd, e->is_alive);
        }
        fprintf(f, "]}");
    }

    /* 던전 좌표 */
    if (gs->phase == PHASE_DUNGEON) {
        fprintf(f, ",\"dungeon\":{\"x\":%d,\"y\":%d}",
                gs->dungeon.player_x, gs->dungeon.player_y);
        /* 이동 가능 방향 */
        if (is_your_turn) {
            static const int DX[] = {0,0,1,-1};
            static const int DY[] = {-1,1,0,0};
            static const char *DN[] = {"move_n","move_s","move_e","move_w"};
            fprintf(f, ",\"valid_actions\":[");
            int first = 1;
            for (int d = 0; d < 4; d++) {
                int nx = gs->dungeon.player_x + DX[d];
                int ny = gs->dungeon.player_y + DY[d];
                if (nx >= 0 && nx < DUNGEON_W && ny >= 0 && ny < DUNGEON_H) {
                    fprintf(f, "%s\"%s\"", first ? "" : ",", DN[d]);
                    first = 0;
                }
            }
            fprintf(f, "]");
        }
    }

    /* HUB */
    if (gs->phase == PHASE_HUB && is_your_turn)
        fprintf(f, ",\"valid_actions\":[\"dungeon\",\"rest\"]");

    fprintf(f, "}\n");
    fflush(f);
}

/* ══════════════════════════════════════════
   캐릭터 생성
   ══════════════════════════════════════════ */
static Character make_kael(void) {
    Character c; memset(&c, 0, sizeof(c));
    snprintf(c.name, MAX_NAME, "KAEL");
    c.role = ROLE_WARRIOR; c.hp_max = c.hp_cur = 60;
    c.atk = 9; c.def = 5; c.spd = 10; c.is_alive = 1; c.taunt_target = -1;
    /* Rapid Slash */
    snprintf(c.skills[0].name, MAX_NAME, "Rapid Slash");
    c.skills[0].type=SKILL_DAMAGE; c.skills[0].target=TARGET_SINGLE;
    c.skills[0].dmg_ratio=0.6f; c.skills[0].cooldown_max=2;
    c.skills[0].effect_type=EFFECT_NONE;
    /* Critical Strike */
    snprintf(c.skills[1].name, MAX_NAME, "Critical Strike");
    c.skills[1].type=SKILL_DAMAGE; c.skills[1].target=TARGET_SINGLE;
    c.skills[1].dmg_ratio=1.5f; c.skills[1].cooldown_max=3;
    c.skills[1].effect_type=EFFECT_STUN; c.skills[1].effect_duration=1;
    /* Berserker */
    snprintf(c.skills[2].name, MAX_NAME, "Berserker");
    c.skills[2].type=SKILL_BUFF; c.skills[2].target=TARGET_SELF;
    c.skills[2].cooldown_max=5; c.skills[2].effect_duration=3;
    return c;
}

static Character make_voss(void) {
    Character c; memset(&c, 0, sizeof(c));
    snprintf(c.name, MAX_NAME, "VOSS");
    c.role = ROLE_TANKER; c.hp_max = c.hp_cur = 100;
    c.atk = 5; c.def = 10; c.spd = 4; c.is_alive = 1; c.taunt_target = -1;
    snprintf(c.skills[0].name, MAX_NAME, "Furious Counter");
    c.skills[0].type=SKILL_COUNTER; c.skills[0].target=TARGET_SELF;
    c.skills[0].cooldown_max=3; c.skills[0].effect_duration=2;
    snprintf(c.skills[1].name, MAX_NAME, "Provoke");
    c.skills[1].type=SKILL_TAUNT; c.skills[1].target=TARGET_ALL_ENEMY;
    c.skills[1].cooldown_max=4; c.skills[1].effect_duration=1;
    snprintf(c.skills[2].name, MAX_NAME, "Iron Fortress");
    c.skills[2].type=SKILL_BUFF; c.skills[2].target=TARGET_ALL_ALLY;
    c.skills[2].cooldown_max=5; c.skills[2].effect_duration=2;
    return c;
}

static Character make_lyra(void) {
    Character c; memset(&c, 0, sizeof(c));
    snprintf(c.name, MAX_NAME, "LYRA");
    c.role = ROLE_SORCERER; c.hp_max = c.hp_cur = 50;
    c.atk = 10; c.def = 4; c.spd = 7; c.is_alive = 1; c.taunt_target = -1;
    snprintf(c.skills[0].name, MAX_NAME, "Plasma Cannon");
    c.skills[0].type=SKILL_DAMAGE; c.skills[0].target=TARGET_SINGLE;
    c.skills[0].dmg_ratio=2.2f; c.skills[0].cooldown_max=3;
    snprintf(c.skills[1].name, MAX_NAME, "Sys Disruption");
    c.skills[1].type=SKILL_DEBUFF; c.skills[1].target=TARGET_SINGLE;
    c.skills[1].dmg_ratio=0.8f; c.skills[1].cooldown_max=4;
    c.skills[1].effect_type=EFFECT_DEF_DOWN; c.skills[1].effect_value=3;
    c.skills[1].effect_duration=2;
    snprintf(c.skills[2].name, MAX_NAME, "Overload");
    c.skills[2].type=SKILL_AOE; c.skills[2].target=TARGET_ALL_ENEMY;
    c.skills[2].dmg_ratio=1.2f; c.skills[2].cooldown_max=6;
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

/* ══════════════════════════════════════════
   파티
   ══════════════════════════════════════════ */
int party_alive_count(const Party *p) {
    int n = 0;
    for (int i = 0; i < PARTY_SIZE; i++)
        if (p->members[i].is_alive) n++;
    return n;
}

void party_rest(Party *p, int cost) {
    if (p->gold < cost) return;
    p->gold -= cost;
    for (int i = 0; i < PARTY_SIZE; i++)
        p->members[i].hp_cur = p->members[i].hp_max;
}

/* ══════════════════════════════════════════
   전투 내부 함수
   ══════════════════════════════════════════ */
static int calc_damage(int atk, float ratio, int def) {
    int d = (int)((float)atk * ratio) - def;
    return d < 1 ? 1 : d;
}

static void apply_buff(Character *c, EffectType type, int value, int duration) {
    if (c->buff_count >= MAX_BUFFS) return;
    for (int i = 0; i < c->buff_count; i++) {
        if (c->buffs[i].type == type) {
            c->buffs[i].value = value; c->buffs[i].duration = duration; return;
        }
    }
    c->buffs[c->buff_count].type = type;
    c->buffs[c->buff_count].value = value;
    c->buffs[c->buff_count].duration = duration;
    c->buff_count++;
}

static void tick_buffs(Character *c) {
    for (int i = 0; i < c->buff_count; i++) c->buffs[i].duration--;
    int j = 0;
    for (int i = 0; i < c->buff_count; i++) {
        if (c->buffs[i].duration > 0) {
            c->buffs[j++] = c->buffs[i];
        } else {
            if (c->buffs[i].type == EFFECT_COUNTER_ON) c->counter_active = 0;
            if (c->buffs[i].type == EFFECT_TAUNT_ON)   c->taunt_target = -1;
        }
    }
    c->buff_count = j;
}

static int has_effect(const Character *c, EffectType type) {
    for (int i = 0; i < c->buff_count; i++)
        if (c->buffs[i].type == type) return 1;
    return 0;
}

static int get_effect_value(const Character *c, EffectType type) {
    for (int i = 0; i < c->buff_count; i++)
        if (c->buffs[i].type == type) return c->buffs[i].value;
    return 0;
}

static int eff_atk(const Character *c) {
    int a = c->atk + get_effect_value(c, EFFECT_ATK_UP)
                   - get_effect_value(c, EFFECT_ATK_DOWN);
    return a < 1 ? 1 : a;
}

static int eff_def(const Character *c) {
    int d = c->def + get_effect_value(c, EFFECT_DEF_UP)
                   - get_effect_value(c, EFFECT_DEF_DOWN);
    if (c->role == ROLE_TANKER) {
        float r = (float)c->hp_cur / (float)c->hp_max;
        float b = r < 0.25f ? 0.30f : r < 0.50f ? 0.20f : r < 0.75f ? 0.10f : 0.0f;
        d = (int)((float)d * (1.0f + b));
    }
    return d < 0 ? 0 : d;
}

static Character *get_actor(GameState *gs, int idx) {
    if (idx < PARTY_SIZE) return &gs->party.members[idx];
    return &gs->battle.enemies[idx - PARTY_SIZE].base;
}

static int passive_bonus(const Character *actor, const Character *target) {
    if (actor->role == ROLE_WARRIOR && actor->spd > target->spd)
        return (int)((float)eff_atk(actor) * 0.05f);
    if (actor->role == ROLE_SORCERER && has_effect(target, EFFECT_DEF_DOWN))
        return (int)((float)eff_atk(actor) * 0.07f);
    return 0;
}

static void resolve_counter(GameState *gs, int voss_idx, int attacker_idx) {
    Character *voss = &gs->party.members[voss_idx];
    if (!voss->counter_active) return;
    int dmg = (int)((float)eff_def(voss) * 1.5f + (float)voss->counter_dmg_taken * 0.3f);
    if (has_effect(voss, EFFECT_TAUNT_ON))
        dmg += (int)((float)eff_def(voss) * 0.5f);
    Character *atk = get_actor(gs, attacker_idx);
    if (atk && atk->is_alive) {
        atk->hp_cur -= dmg;
        if (atk->hp_cur <= 0) { atk->hp_cur = 0; atk->is_alive = 0; }
        gs->battle.last_dmg = dmg;
    }
    voss->counter_active = 0; voss->counter_dmg_taken = 0;
}

/* ══════════════════════════════════════════
   전투 공개 함수
   ══════════════════════════════════════════ */
void build_turn_order(GameState *gs) {
    BattleState *b = &gs->battle;
    int n = 0, spd[PARTY_SIZE + MAX_ENEMIES], idx[PARTY_SIZE + MAX_ENEMIES];
    for (int i = 0; i < PARTY_SIZE; i++) {
        if (gs->party.members[i].is_alive) { idx[n]=i; spd[n]=gs->party.members[i].spd; n++; }
    }
    for (int i = 0; i < b->enemy_count; i++) {
        if (b->enemies[i].base.is_alive) { idx[n]=PARTY_SIZE+i; spd[n]=b->enemies[i].base.spd; n++; }
    }
    for (int i = 0; i < n-1; i++)
        for (int j = i+1; j < n; j++)
            if (spd[j] > spd[i]) {
                int ts=spd[i]; spd[i]=spd[j]; spd[j]=ts;
                int ti=idx[i]; idx[i]=idx[j]; idx[j]=ti;
            }
    for (int i = 0; i < n; i++) b->turn_order[i] = idx[i];
    b->turn_order_size = n;
    b->current_actor   = 0;
}

void do_attack(GameState *gs, int actor_idx, int target_idx) {
    Character *a = get_actor(gs, actor_idx);
    Character *t = get_actor(gs, target_idx);
    if (!a || !t || !a->is_alive || !t->is_alive) return;
    int dmg = calc_damage(eff_atk(a) + passive_bonus(a, t), 1.0f,
                          t->is_defending ? (int)(eff_def(t)*1.5f) : eff_def(t));
    if (t->counter_active) t->counter_dmg_taken += dmg;
    t->hp_cur -= dmg;
    if (t->hp_cur <= 0) { t->hp_cur = 0; t->is_alive = 0; }
    gs->battle.last_dmg = dmg;
    if (t->counter_active && t->is_alive && target_idx < PARTY_SIZE)
        resolve_counter(gs, target_idx, actor_idx);
}

void do_skill(GameState *gs, int actor_idx, int skill_idx, int target_idx) {
    Character *a = get_actor(gs, actor_idx);
    if (!a || !a->is_alive) return;
    Skill *sk = &a->skills[skill_idx];
    if (sk->cooldown_cur > 0) return;
    sk->cooldown_cur = sk->cooldown_max;

    switch (sk->type) {
    case SKILL_DAMAGE: {
        Character *t = get_actor(gs, target_idx);
        if (!t || !t->is_alive) break;
        int hits = (skill_idx == 0 && a->role == ROLE_WARRIOR) ? 2 : 1;
        int total = 0;
        for (int h = 0; h < hits; h++) {
            int d = calc_damage(eff_atk(a) + passive_bonus(a, t), sk->dmg_ratio,
                                t->is_defending ? (int)(eff_def(t)*1.5f) : eff_def(t));
            if (t->counter_active) t->counter_dmg_taken += d;
            t->hp_cur -= d; total += d;
        }
        if (t->hp_cur <= 0) { t->hp_cur = 0; t->is_alive = 0; }
        if (sk->effect_type == EFFECT_STUN) {
            float chance = (float)a->atk * 0.03f;
            if ((float)rand()/RAND_MAX < chance) t->is_stunned = sk->effect_duration;
        }
        gs->battle.last_dmg = total;
        if (t->counter_active && t->is_alive && target_idx < PARTY_SIZE)
            resolve_counter(gs, target_idx, actor_idx);
        break;
    }
    case SKILL_AOE: {
        int total = 0;
        for (int i = 0; i < gs->battle.enemy_count; i++) {
            Character *t = &gs->battle.enemies[i].base;
            if (!t->is_alive) continue;
            int d = calc_damage(eff_atk(a) + passive_bonus(a, t), sk->dmg_ratio, eff_def(t));
            t->hp_cur -= d;
            if (t->hp_cur <= 0) { t->hp_cur = 0; t->is_alive = 0; }
            total += d;
        }
        gs->battle.last_dmg = total;
        break;
    }
    case SKILL_DEBUFF: {
        Character *t = get_actor(gs, target_idx);
        if (!t || !t->is_alive) break;
        apply_buff(t, sk->effect_type, sk->effect_value, sk->effect_duration);
        int d = calc_damage(eff_atk(a) + passive_bonus(a, t), sk->dmg_ratio, eff_def(t));
        t->hp_cur -= d;
        if (t->hp_cur <= 0) { t->hp_cur = 0; t->is_alive = 0; }
        gs->battle.last_dmg = d;
        break;
    }
    case SKILL_BUFF:
        if (a->role == ROLE_WARRIOR) {
            apply_buff(a, EFFECT_ATK_UP, 4, 3);
            apply_buff(a, EFFECT_DEF_DOWN, 2, 3);
        } else if (a->role == ROLE_TANKER) {
            int bonus = (int)((float)eff_def(a) * 0.3f);
            for (int i = 0; i < PARTY_SIZE; i++)
                if (gs->party.members[i].is_alive)
                    apply_buff(&gs->party.members[i], EFFECT_DEF_UP, bonus, sk->effect_duration);
        }
        break;
    case SKILL_COUNTER:
        a->counter_active = 1; a->counter_dmg_taken = 0;
        apply_buff(a, EFFECT_COUNTER_ON, 0, sk->effect_duration);
        break;
    case SKILL_TAUNT:
        for (int i = 0; i < gs->battle.enemy_count; i++) {
            gs->battle.enemies[i].base.taunt_target = actor_idx;
            apply_buff(&gs->battle.enemies[i].base, EFFECT_TAUNT_ON, 0, 1);
        }
        apply_buff(a, EFFECT_TAUNT_ON, 0, 1);
        a->counter_active = 1;
        break;
    default: break;
    }
}

void do_defend(GameState *gs, int actor_idx) {
    Character *a = get_actor(gs, actor_idx);
    if (a) a->is_defending = 1;
}

void do_flee(GameState *gs) {
    int ps=0, pn=0, es=0, en=0;
    for (int i = 0; i < PARTY_SIZE; i++)
        if (gs->party.members[i].is_alive) { ps+=gs->party.members[i].spd; pn++; }
    for (int i = 0; i < gs->battle.enemy_count; i++)
        if (gs->battle.enemies[i].base.is_alive) { es+=gs->battle.enemies[i].base.spd; en++; }
    if (!pn || !en) return;
    float chance = 0.5f + 0.05f * ((ps/pn) - (es/en));
    if (chance > 0.9f) chance = 0.9f;
    if (chance < 0.1f) chance = 0.1f;
    if ((float)rand()/RAND_MAX < chance) {
        gs->phase = PHASE_DUNGEON;
        snprintf(gs->last_event, 64, "flee_success");
    } else {
        snprintf(gs->last_event, 64, "flee_fail");
    }
}

void enemy_turn(GameState *gs, int enemy_idx) {
    Character *e = &gs->battle.enemies[enemy_idx].base;
    if (!e->is_alive) return;
    if (e->is_stunned) { e->is_stunned--; return; }
    int tidx = -1;
    if (e->taunt_target >= 0) {
        tidx = e->taunt_target;
    } else {
        int alive[PARTY_SIZE], an = 0;
        for (int i = 0; i < PARTY_SIZE; i++)
            if (gs->party.members[i].is_alive) alive[an++] = i;
        if (an == 0) return;
        tidx = alive[rand() % an];
    }
    do_attack(gs, PARTY_SIZE + enemy_idx, tidx);
}

void battle_start(GameState *gs, Enemy *enemies, int count, int is_boss) {
    BattleState *b = &gs->battle;
    memset(b, 0, sizeof(BattleState));
    b->enemy_count   = count;
    b->is_boss_fight = is_boss;
    b->turn          = 1;
    for (int i = 0; i < count && i < MAX_ENEMIES; i++) b->enemies[i] = enemies[i];
    for (int i = 0; i < PARTY_SIZE; i++) gs->party.members[i].is_defending = 0;
    build_turn_order(gs);
    gs->phase = PHASE_BATTLE;
    snprintf(gs->last_event, 64, is_boss ? "boss_appear" : "battle_start");
}

int battle_check_end(GameState *gs) {
    int all_dead = 1;
    for (int i = 0; i < gs->battle.enemy_count; i++)
        if (gs->battle.enemies[i].base.is_alive) { all_dead = 0; break; }
    if (all_dead) return 1;
    int party_dead = 1;
    for (int i = 0; i < PARTY_SIZE; i++)
        if (gs->party.members[i].is_alive) { party_dead = 0; break; }
    if (party_dead) return -1;
    return 0;
}

void battle_end_turn(GameState *gs) {
    BattleState *b = &gs->battle;
    b->current_actor++;
    if (b->current_actor >= b->turn_order_size)
        b->current_actor = 0;
    if (b->current_actor == 0) {
        b->turn++; gs->turn_total++;
        for (int i = 0; i < PARTY_SIZE; i++) {
            Character *c = &gs->party.members[i];
            tick_buffs(c); c->is_defending = 0;
            for (int si = 0; si < MAX_SKILLS; si++)
                if (c->skills[si].cooldown_cur > 0) c->skills[si].cooldown_cur--;
        }
        for (int i = 0; i < gs->battle.enemy_count; i++) {
            Character *c = &gs->battle.enemies[i].base;
            tick_buffs(c);
            for (int si = 0; si < MAX_SKILLS; si++)
                if (c->skills[si].cooldown_cur > 0) c->skills[si].cooldown_cur--;
        }
        build_turn_order(gs);
    }
}

/* ══════════════════════════════════════════
   던전
   ══════════════════════════════════════════ */
static const int DX[] = { 0,  0, 1, -1};
static const int DY[] = {-1,  1, 0,  0};

void dungeon_generate(Dungeon *d, int zone, int floor) {
    memset(d, 0, sizeof(Dungeon));
    d->zone = zone; d->floor = floor;
    for (int y = 0; y < DUNGEON_H; y++)
        for (int x = 0; x < DUNGEON_W; x++) {
            int r = rand() % 10;
            d->tiles[y][x].type = r < 3 ? TILE_BATTLE : TILE_EMPTY;
        }
    d->tiles[2][2].type = TILE_EMPTY;
    d->tiles[4][4].type = TILE_EXIT;
    d->tiles[0][4].type = TILE_BOSS;
    d->player_x = 2; d->player_y = 2;
    d->boss_defeated = 0;
}

int dungeon_valid_dirs(const Dungeon *d, int *out_dirs) {
    int n = 0;
    for (int dir = 0; dir < 4; dir++) {
        int nx = d->player_x + DX[dir];
        int ny = d->player_y + DY[dir];
        if (nx >= 0 && nx < DUNGEON_W && ny >= 0 && ny < DUNGEON_H)
            out_dirs[n++] = dir;
    }
    return n;
}

int dungeon_move(GameState *gs, int dir) {
    Dungeon *d = &gs->dungeon;
    int nx = d->player_x + DX[dir];
    int ny = d->player_y + DY[dir];
    if (nx < 0 || nx >= DUNGEON_W || ny < 0 || ny >= DUNGEON_H) return 0;
    d->tiles[d->player_y][d->player_x].visited = 1;
    d->player_x = nx; d->player_y = ny;
    Tile *tile = &d->tiles[ny][nx];
    if (tile->visited) return 1;
    switch (tile->type) {
    case TILE_BATTLE: {
        Enemy enemies[MAX_ENEMIES]; int count = 0;
        get_enemies_for_zone(d->zone, enemies, &count, 0);
        battle_start(gs, enemies, count, 0);
        return 2;
    }
    case TILE_BOSS:
        if (!d->boss_defeated) {
            Enemy enemies[MAX_ENEMIES]; int count = 0;
            get_enemies_for_zone(d->zone, enemies, &count, 1);
            battle_start(gs, enemies, count, 1);
            return 5;
        }
        return 1;
    case TILE_EXIT:
        gs->dungeons_cleared++;
        gs->phase = PHASE_HUB;
        snprintf(gs->last_event, 64, "dungeon_clear");
        return 6;
    default: return 1;
    }
}

/* ══════════════════════════════════════════
   적 생성
   ══════════════════════════════════════════ */
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
        case 1: out[0]=make_enemy("GUARD-MK1",  80*s,6*s,5*s,4,80, 120,ENEMY_BOSS); break;
        case 2: out[0]=make_enemy("WARDEN-X",  100*s,8*s,6*s,5,150,200,ENEMY_BOSS); break;
        case 3: out[0]=make_enemy("SIEGE-9",   120*s,9*s,7*s,6,220,300,ENEMY_BOSS); break;
        case 4: out[0]=make_enemy("APEX-NULL", 150*s,10*s,8*s,7,300,400,ENEMY_BOSS); break;
        default:out[0]=make_enemy("NEXUS-CORE",200*s,12*s,10*s,8,500,600,ENEMY_BOSS); break;
        }
        return;
    }
    *count = 2;
    switch (zone) {
    case 1: out[0]=make_enemy("DRONE-A",30*s,4*s,2*s,5,20,30,ENEMY_NORMAL);
            out[1]=make_enemy("DRONE-B",30*s,4*s,2*s,4,20,30,ENEMY_NORMAL); break;
    case 2: out[0]=make_enemy("STALKER", 40*s,5*s,3*s,6,35,50,ENEMY_NORMAL);
            out[1]=make_enemy("ENFORCER",55*s,6*s,4*s,5,55,70,ENEMY_ELITE); break;
    case 3: out[0]=make_enemy("REAPER",60*s,7*s,4*s,7,70,90,ENEMY_ELITE);
            out[1]=make_enemy("REAPER",60*s,7*s,4*s,6,70,90,ENEMY_ELITE); break;
    case 4: out[0]=make_enemy("PHANTOM",70*s,8*s,5*s,8,90,120,ENEMY_ELITE);
            out[1]=make_enemy("PHANTOM",70*s,8*s,5*s,7,90,120,ENEMY_ELITE); break;
    default:out[0]=make_enemy("VOIDMECH",80*s,9*s,6*s,9,120,150,ENEMY_ELITE);
            out[1]=make_enemy("VOIDMECH",80*s,9*s,6*s,8,120,150,ENEMY_ELITE); break;
    }
}