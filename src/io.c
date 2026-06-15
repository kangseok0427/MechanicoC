#include "game.h"
#include <string.h>
#include <stdio.h>

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
                m->is_alive ? m->hp_cur : 0,
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

        if (b->current_actor < b->turn_order_size) {
            int ai = b->turn_order[b->current_actor];
            const char *aname = ai < PARTY_SIZE
                ? gs->party.members[ai].name
                : gs->battle.enemies[ai - PARTY_SIZE].base.name;
            fprintf(f, ",\"actor\":\"%s\"", aname);
        }

        fprintf(f, ",\"valid_actions\":[");
        if (is_your_turn) {
            int first = 1;
            fprintf(f, "\"attack\""); first = 0;
            for (int si = 0; si < MAX_SKILLS; si++) {
                int ai = b->turn_order[b->current_actor];
                if (ai < PARTY_SIZE) {
                    Character *actor = &gs->party.members[ai];
                    if (actor->skills[si].cooldown_cur == 0 &&
                        actor->skills[si].name[0])
                        fprintf(f, ",\"skill_%d\"", si);
                }
            }
            if (!first) {}
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

    if (gs->phase == PHASE_HUB && is_your_turn)
        fprintf(f, ",\"valid_actions\":[\"dungeon\",\"rest\"]");

    fprintf(f, "}\n");
    fflush(f);
}

int parse_cmd(AICmd *out) {
    char line[256];
    memset(out, 0, sizeof(AICmd));
    if (!fgets(line, sizeof(line), stdin)) return -1;
    char prefix[16];
    if (sscanf(line, "%15s %31s %d %d",
               prefix, out->cmd, &out->args[0], &out->args[1]) < 2) return 0;
    return strcmp(prefix, "ACTION") == 0 ? 1 : 0;
}
