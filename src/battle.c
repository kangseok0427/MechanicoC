#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── 내부 헬퍼 ── */
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
    c->buffs[c->buff_count].type     = type;
    c->buffs[c->buff_count].value    = value;
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
            if (c->buffs[i].type == EFFECT_TAUNT_ON)   c->taunt_target   = -1;
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
    int dmg = (int)((float)eff_def(voss) * 1.5f
                  + (float)voss->counter_dmg_taken * 0.3f);
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

/* ── 공개 함수 ── */
void build_turn_order(GameState *gs) {
    BattleState *b = &gs->battle;
    int n = 0, spd[PARTY_SIZE + MAX_ENEMIES], idx[PARTY_SIZE + MAX_ENEMIES];
    for (int i = 0; i < PARTY_SIZE; i++)
        if (gs->party.members[i].is_alive)
            { idx[n]=i; spd[n]=gs->party.members[i].spd; n++; }
    for (int i = 0; i < b->enemy_count; i++)
        if (b->enemies[i].base.is_alive)
            { idx[n]=PARTY_SIZE+i; spd[n]=b->enemies[i].base.spd; n++; }
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
