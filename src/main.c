#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_ai_mode = 0;

/* ── ACTION 파싱 ── */
typedef struct { char cmd[32]; int args[4]; } AICmd;

static int parse_cmd(AICmd *out) {
    char line[256];
    memset(out, 0, sizeof(AICmd));
    if (!fgets(line, sizeof(line), stdin)) return -1;
    char prefix[16];
    if (sscanf(line, "%15s %31s %d %d",
               prefix, out->cmd, &out->args[0], &out->args[1]) < 2) return 0;
    return strcmp(prefix, "ACTION") == 0 ? 1 : 0;
}

/* ── 전투 루프 ── */
static void run_battle(GameState *gs) {
    /* 전투 시작 이벤트 (is_your_turn=0, 정보만) */
    emit_event(gs, "battle_start", 0);

    while (1) {
        int result = battle_check_end(gs);
        if (result == 1) {
            int gold = 0;
            for (int i = 0; i < gs->battle.enemy_count; i++)
                gold += gs->battle.enemies[i].gold_reward;
            gs->party.gold += gold;
            if (gs->battle.is_boss_fight) gs->dungeon.boss_defeated = 1;
            gs->phase = PHASE_BATTLE_WIN;
            emit_event(gs, "battle_win", 0);
            gs->phase = PHASE_DUNGEON;
            return;
        }
        if (result == -1) {
            gs->phase = PHASE_BATTLE_LOSE;
            emit_event(gs, "battle_lose", 0);
            gs->phase = PHASE_GAMEOVER;
            return;
        }

        int actor_idx = gs->battle.turn_order[gs->battle.current_actor];

        /* 적 턴 */
        if (actor_idx >= PARTY_SIZE) {
            enemy_turn(gs, actor_idx - PARTY_SIZE);
            emit_event(gs, "enemy_action", 0);  /* 정보만, 행동 불필요 */
            battle_end_turn(gs);
            continue;
        }

        /* 파티원 - 사망/스턴 */
        Character *actor = &gs->party.members[actor_idx];
        if (!actor->is_alive) { battle_end_turn(gs); continue; }
        if (actor->is_stunned) {
            actor->is_stunned--;
            emit_event(gs, "stunned", 0);
            battle_end_turn(gs);
            continue;
        }

        /* 파티원 - 행동 필요 → is_your_turn=1 */
        emit_event(gs, "your_turn", 1);

        if (g_ai_mode) {
            AICmd cmd;
            int r;
            do { r = parse_cmd(&cmd); } while (r == 0);
            if (r < 0) { gs->phase = PHASE_GAMEOVER; return; }

            if      (!strcmp(cmd.cmd, "attack"))  do_attack(gs, actor_idx, cmd.args[0]);
            else if (!strcmp(cmd.cmd, "skill_0")) do_skill(gs, actor_idx, 0, cmd.args[0]);
            else if (!strcmp(cmd.cmd, "skill_1")) do_skill(gs, actor_idx, 1, cmd.args[0]);
            else if (!strcmp(cmd.cmd, "skill_2")) do_skill(gs, actor_idx, 2, cmd.args[0]);
            else if (!strcmp(cmd.cmd, "defend"))  do_defend(gs, actor_idx);
            else if (!strcmp(cmd.cmd, "flee"))    { do_flee(gs); return; }
        } else {
            /* 수동 플레이: 키보드 입력 */
            char line[64];
            printf("[%s] a)ttack s0/s1/s2)kill d)efend f)lee > ", actor->name);
            fflush(stdout);
            if (!fgets(line, sizeof(line), stdin)) return;
            /* 타겟 선택 (attack/skill) */
            int tidx = PARTY_SIZE; /* 기본 첫 번째 살아있는 적 */
            for (int i = 0; i < gs->battle.enemy_count; i++) {
                if (gs->battle.enemies[i].base.is_alive) { tidx = PARTY_SIZE + i; break; }
            }
            if (line[0]=='a') do_attack(gs, actor_idx, tidx);
            else if (line[0]=='s' && line[1]=='0') do_skill(gs, actor_idx, 0, tidx);
            else if (line[0]=='s' && line[1]=='1') do_skill(gs, actor_idx, 1, tidx);
            else if (line[0]=='s' && line[1]=='2') do_skill(gs, actor_idx, 2, tidx);
            else if (line[0]=='d') do_defend(gs, actor_idx);
            else if (line[0]=='f') { do_flee(gs); return; }
            else do_attack(gs, actor_idx, tidx); /* 기본 공격 */
        }

        emit_event(gs, "action_result", 0);
        battle_end_turn(gs);
    }
}

/* ── HUB 루프 ── */
static void run_hub(GameState *gs) {
    while (gs->phase == PHASE_HUB) {
        emit_event(gs, "hub", 1);

        int sel = -1;
        if (g_ai_mode) {
            AICmd cmd;
            int r;
            do { r = parse_cmd(&cmd); } while (r == 0);
            if (r < 0) return;
            if (!strcmp(cmd.cmd, "dungeon")) sel = 0;
            else if (!strcmp(cmd.cmd, "rest")) sel = 1;
        } else {
            char line[64];
            printf("1)dungeon 2)rest > ");
            fflush(stdout);
            if (!fgets(line, sizeof(line), stdin)) return;
            sel = (line[0] == '2') ? 1 : 0;
        }

        if (sel == 0) {
            dungeon_generate(&gs->dungeon, gs->zone_current, gs->dungeons_cleared + 1);
            gs->phase = PHASE_DUNGEON;
        } else {
            if (gs->party.gold >= 50) {
                party_rest(&gs->party, 50);
                snprintf(gs->last_event, 64, "rested");
            } else {
                snprintf(gs->last_event, 64, "no_gold");
            }
        }
    }
}

/* ── 던전 루프 ── */
static void run_dungeon(GameState *gs) {
    while (gs->phase == PHASE_DUNGEON) {
        emit_event(gs, "dungeon", 1);

        int dir = -1;
        if (g_ai_mode) {
            AICmd cmd;
            int r;
            do { r = parse_cmd(&cmd); } while (r == 0);
            if (r < 0) return;
            if      (!strcmp(cmd.cmd, "move_n")) dir = 0;
            else if (!strcmp(cmd.cmd, "move_s")) dir = 1;
            else if (!strcmp(cmd.cmd, "move_e")) dir = 2;
            else if (!strcmp(cmd.cmd, "move_w")) dir = 3;
            else continue;
        } else {
            char line[64];
            printf("n/s/e/w > ");
            fflush(stdout);
            if (!fgets(line, sizeof(line), stdin)) return;
            if      (line[0]=='n') dir=0;
            else if (line[0]=='s') dir=1;
            else if (line[0]=='e') dir=2;
            else if (line[0]=='w') dir=3;
            else continue;
        }

        int result = dungeon_move(gs, dir);

        if (result == 2 || result == 5) {
            run_battle(gs);
            if (gs->phase == PHASE_GAMEOVER) return;
        } else if (result == 6) {
            /* 던전 클리어 → HUB로 */
            emit_event(gs, "dungeon_clear", 0);
            return;
        }
    }
}

/* ── Main ── */
int main(int argc, char *argv[]) {
    srand((unsigned)time(NULL));
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--ai")) g_ai_mode = 1;

    GameState gs;
    game_init(&gs);

    while (1) {
        switch (gs.phase) {
        case PHASE_HUB:     run_hub(&gs);     break;
        case PHASE_DUNGEON: run_dungeon(&gs); break;
        case PHASE_GAMEOVER:
            emit_event(&gs, "gameover", 0);
            return 0;
        default:
            gs.phase = PHASE_HUB;
            break;
        }
    }
}
