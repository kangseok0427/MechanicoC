#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_ai_mode = 0;

/* -- battle loop -- */
static void run_battle(GameState *gs) {
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
            draw_screen(gs, g_ai_mode);
            printf("\n  ** Battle Win! +%dG **\n", gold);
            print_party(gs);
            emit_event(gs, "battle_win", 0);
            if (!g_ai_mode) {
                char tmp[8]; printf("  [Enter]"); fflush(stdout);
                fgets(tmp, sizeof(tmp), stdin);
            }
            gs->phase = PHASE_DUNGEON;
            return;
        }
        if (result == -1) {
            gs->phase = PHASE_BATTLE_LOSE;
            draw_screen(gs, g_ai_mode);
            printf("\n  ** Battle Lost... **\n");
            emit_event(gs, "battle_lose", 0);
            if (!g_ai_mode) {
                char tmp[8]; printf("  [Enter]"); fflush(stdout);
                fgets(tmp, sizeof(tmp), stdin);
            }
            gs->phase = PHASE_GAMEOVER;
            return;
        }

        int actor_idx = gs->battle.turn_order[gs->battle.current_actor];

        /* enemy turn */
        if (actor_idx >= PARTY_SIZE) {
            int eidx = actor_idx - PARTY_SIZE;
            Character *e = &gs->battle.enemies[eidx].base;
            enemy_turn(gs, eidx);
            draw_screen(gs, g_ai_mode);
            print_party(gs);
            print_enemies(gs);
            printf("\n  -> %s attacks! %d dmg\n", e->name, gs->battle.last_dmg);
            emit_event(gs, "enemy_action", 0);
            if (!g_ai_mode) {
                char tmp[8]; printf("  [Enter]"); fflush(stdout);
                fgets(tmp, sizeof(tmp), stdin);
            }
            battle_end_turn(gs);
            continue;
        }

        /* party - dead/stunned */
        Character *actor = &gs->party.members[actor_idx];
        if (!actor->is_alive) { battle_end_turn(gs); continue; }
        if (actor->is_stunned) {
            actor->is_stunned--;
            draw_screen(gs, g_ai_mode);
            print_party(gs);
            print_enemies(gs);
            printf("\n  -> %s is stunned!\n", actor->name);
            emit_event(gs, "stunned", 0);
            if (!g_ai_mode) {
                char tmp[8]; printf("  [Enter]"); fflush(stdout);
                fgets(tmp, sizeof(tmp), stdin);
            }
            battle_end_turn(gs);
            continue;
        }

        /* party - action */
        emit_event(gs, "your_turn", 1);

        if (g_ai_mode) {
            draw_screen(gs, g_ai_mode);
            print_party(gs);
            print_enemies(gs);
            printf("\n  -> AI thinking...\n"); fflush(stdout);

            AICmd cmd; int r;
            do { r = parse_cmd(&cmd); } while (r == 0);
            if (r < 0) { gs->phase = PHASE_GAMEOVER; return; }

            if      (!strcmp(cmd.cmd, "attack"))  do_attack(gs, actor_idx, cmd.args[0]);
            else if (!strcmp(cmd.cmd, "skill_0")) do_skill(gs, actor_idx, 0, cmd.args[0]);
            else if (!strcmp(cmd.cmd, "skill_1")) do_skill(gs, actor_idx, 1, cmd.args[0]);
            else if (!strcmp(cmd.cmd, "skill_2")) do_skill(gs, actor_idx, 2, cmd.args[0]);
            else if (!strcmp(cmd.cmd, "defend"))  do_defend(gs, actor_idx);
            else if (!strcmp(cmd.cmd, "flee"))    { do_flee(gs); return; }

            printf("  -> %s: %s  dmg:%d\n",
                   actor->name, cmd.cmd, gs->battle.last_dmg);
            fflush(stdout);
        } else {
            cls();
            print_party(gs);
            print_enemies(gs);
            manual_battle_input(gs, actor_idx, g_ai_mode);
            if (gs->phase == PHASE_DUNGEON) return;
        }

        emit_event(gs, "action_result", 0);
        battle_end_turn(gs);
    }
}

/* -- hub loop -- */
static void run_hub(GameState *gs) {
    while (gs->phase == PHASE_HUB) {
        emit_event(gs, "hub", 1);
        draw_screen(gs, g_ai_mode);
        print_party(gs);
        printf("\n  [1] Enter Dungeon\n");
        printf("  [2] Rest (50G, full HP)\n");
        printf("  > "); fflush(stdout);

        int sel = -1;
        if (g_ai_mode) {
            AICmd cmd; int r;
            do { r = parse_cmd(&cmd); } while (r == 0);
            if (r < 0) return;
            if      (!strcmp(cmd.cmd, "dungeon")) sel = 0;
            else if (!strcmp(cmd.cmd, "rest"))    sel = 1;
        } else {
            char line[64];
            if (!fgets(line, sizeof(line), stdin)) return;
            sel = (line[0] == '2') ? 1 : 0;
        }

        if (sel == 0) {
            dungeon_generate(&gs->dungeon, gs->zone_current, gs->dungeons_cleared + 1);
            gs->phase = PHASE_DUNGEON;
        } else {
            if (gs->party.gold >= 50) {
                party_rest(&gs->party, 50);
                printf("  -> HP restored! (-50G)\n"); fflush(stdout);
                snprintf(gs->last_event, 64, "rested");
            } else {
                printf("  -> Not enough gold!\n"); fflush(stdout);
                snprintf(gs->last_event, 64, "no_gold");
            }
        }
    }
}

/* -- dungeon loop -- */
static void run_dungeon(GameState *gs) {
    while (gs->phase == PHASE_DUNGEON) {
        emit_event(gs, "dungeon", 1);
        draw_screen(gs, g_ai_mode);
        print_dungeon_map(gs);
        print_party(gs);

        Dungeon *d = &gs->dungeon;
        static const int DX[] = {0,0,1,-1};
        static const int DY[] = {-1,1,0,0};
        static const char *DNAME[] = {"North(W)", "South(S)", "East(D)", "West(A)"};
        static const char *DKEY[]  = {"w","s","d","a"};
        printf("\n  Move:\n");
        for (int dir = 0; dir < 4; dir++) {
            int nx = d->player_x + DX[dir];
            int ny = d->player_y + DY[dir];
            if (nx >= 0 && nx < DUNGEON_W && ny >= 0 && ny < DUNGEON_H) {
                Tile *t = &d->tiles[ny][nx];
                const char *info = !t->visited ?
                    (t->type == TILE_BATTLE ? "(battle)" :
                     t->type == TILE_BOSS   ? "(BOSS!)"  :
                     t->type == TILE_EXIT   ? "(exit)"   : "") : "(visited)";
                printf("  [%s] %s %s\n", DKEY[dir], DNAME[dir], info);
            }
        }
        if (g_ai_mode)
            printf("  -> AI choosing...\n");
        else
            printf("  dir > ");
        fflush(stdout);

        int dir = -1;
        if (g_ai_mode) {
            AICmd cmd; int r;
            do { r = parse_cmd(&cmd); } while (r == 0);
            if (r < 0) return;
            if      (!strcmp(cmd.cmd, "move_n")) dir = 0;
            else if (!strcmp(cmd.cmd, "move_s")) dir = 1;
            else if (!strcmp(cmd.cmd, "move_e")) dir = 2;
            else if (!strcmp(cmd.cmd, "move_w")) dir = 3;
            else continue;
        } else {
            char line[64];
            if (!fgets(line, sizeof(line), stdin)) return;
            if      (line[0]=='w') dir=0;
            else if (line[0]=='s') dir=1;
            else if (line[0]=='d') dir=2;
            else if (line[0]=='a') dir=3;
            else { printf("  Use w/s/d/a\n"); continue; }
        }

        int result = dungeon_move(gs, dir);
        if (result == 0) {
            printf("  -> Cannot move!\n"); fflush(stdout);
        } else if (result == 2 || result == 5) {
            run_battle(gs);
            if (gs->phase == PHASE_GAMEOVER) return;
        } else if (result == 6) {
            draw_screen(gs, g_ai_mode);
            printf("\n  ** Dungeon Clear! **\n");
            print_party(gs);
            emit_event(gs, "dungeon_clear", 0);
            if (!g_ai_mode) {
                char tmp[8]; printf("  [Enter]"); fflush(stdout);
                fgets(tmp, sizeof(tmp), stdin);
            }
            return;
        }
    }
}

/* -- main -- */
int main(int argc, char *argv[]) {
    srand((unsigned)time(NULL));
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--ai")) g_ai_mode = 1;

    if (!g_ai_mode) freopen("/dev/null", "w", stderr);

    GameState gs;
    game_init(&gs);

    while (1) {
        switch (gs.phase) {
        case PHASE_HUB:     run_hub(&gs);     break;
        case PHASE_DUNGEON: run_dungeon(&gs); break;
        case PHASE_GAMEOVER:
            emit_event(&gs, "gameover", 0);
            draw_screen(&gs, g_ai_mode);
            printf("\n  == GAME OVER ==\n");
            printf("  cleared:%d  gold:%dG\n",
                   gs.dungeons_cleared, gs.party.gold);
            fflush(stdout);
            return 0;
        default:
            gs.phase = PHASE_HUB;
            break;
        }
    }
}
