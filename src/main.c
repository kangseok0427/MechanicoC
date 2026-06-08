#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_ai_mode = 0;

/* -- clear screen -- */
static void cls(void) { printf("\033[2J\033[H"); fflush(stdout); }

/* -- HP bar -- */
static void print_bar(int cur, int mx, int width) {
    int filled = mx > 0 ? cur * width / mx : 0;
    printf("[");
    for (int i = 0; i < width; i++) printf("%c", i < filled ? '|' : '.');
    printf("] %3d/%3d", cur, mx);
}

/* -- dungeon map (모든 타일 2칸으로 통일) -- */
static void print_dungeon_map(GameState *gs) {
    Dungeon *d = &gs->dungeon;
    printf("\n  +");
    for (int x = 0; x < DUNGEON_W; x++) printf("----");
    printf("+\n");
    for (int y = 0; y < DUNGEON_H; y++) {
        printf("  |");
        for (int x = 0; x < DUNGEON_W; x++) {
            if (x == d->player_x && y == d->player_y) {
                printf(" [@]");
            } else if (!d->tiles[y][x].visited) {
                printf(" [?]");
            } else {
                Tile *t = &d->tiles[y][x];
                switch (t->type) {
                case TILE_BATTLE: printf(" [!]"); break;
                case TILE_BOSS:   printf(" [*]"); break;
                case TILE_EXIT:   printf(" [E]"); break;
                default:          printf(" [ ]"); break;
                }
            }
        }
        printf(" |\n");
    }
    printf("  +");
    for (int x = 0; x < DUNGEON_W; x++) printf("----");
    printf("+\n");
    printf("  [@]=You  [!]=Battle  [*]=Boss  [E]=Exit  [?]=Unknown\n");
}

/* -- AI 상태 바 (AI 모드 전용) -- */
static void print_ai_status(void) {
    FILE *f = fopen("/tmp/mechanico_ai_status.json", "r");
    if (!f) return;
    int ep, stage, steps;
    float reward, avg, eps, loss;
    if (fscanf(f, "%d %f %f %f %f %d %d",
               &ep, &reward, &avg, &eps, &loss, &stage, &steps) == 7) {
        printf("  [AI] EP:%-4d  stage:%d  steps:%-6d  avg:%+.2f  eps:%.2f  loss:%.4f\n",
               ep, stage, steps, avg, eps, loss);
    }
    fclose(f);
}

/* -- party status -- */
static void print_party(GameState *gs) {
    printf("\n  +--------------------------------------------------+\n");
    printf("  |  PARTY                                           |\n");
    printf("  +--------------------------------------------------+\n");
    for (int i = 0; i < PARTY_SIZE; i++) {
        Character *m = &gs->party.members[i];
        if (!m->is_alive) {
            printf("  |  %-6s  -- KO --                                |\n", m->name);
            continue;
        }
        printf("  |  %-6s  ", m->name);
        print_bar(m->hp_cur, m->hp_max, 14);
        printf("\n");
        printf("  |         ATK:%2d  DEF:%2d  SPD:%2d", m->atk, m->def, m->spd);
        if (m->is_stunned)     printf("  [STN]");
        if (m->is_defending)   printf("  [DEF]");
        if (m->counter_active) printf("  [CTR]");
        printf("\n");
    }
    printf("  +--------------------------------------------------+\n");
}

/* -- enemy status -- */
static void print_enemies(GameState *gs) {
    BattleState *b = &gs->battle;
    printf("\n  +--------------------------------------------------+\n");
    printf("  |  ENEMIES  turn:%-3d%s\n",
           b->turn, b->is_boss_fight ? "  ** BOSS **" : "");
    printf("  +--------------------------------------------------+\n");
    for (int i = 0; i < b->enemy_count; i++) {
        Character *e = &b->enemies[i].base;
        printf("  |  [%d] %-12s  ", i, e->name);
        if (!e->is_alive) {
            printf("DEFEATED                         |\n");
        } else {
            print_bar(e->hp_cur, e->hp_max, 12);
            printf("\n  |      ATK:%2d  DEF:%2d  SPD:%2d", e->atk, e->def, e->spd);
            if (e->is_stunned) printf("  [STN]");
            printf("\n");
        }
    }
    printf("  +--------------------------------------------------+\n");
}

/* -- draw screen -- */
static void draw_screen(GameState *gs) {
    cls();
    printf("\n  ==================================================\n");
    if (g_ai_mode) {
        printf("  MECHANICO  zone:%d  cleared:%d  gold:%dG  [AI MODE]\n",
               gs->zone_current, gs->dungeons_cleared, gs->party.gold);
        print_ai_status();
    } else {
        printf("  MECHANICO  zone:%d  cleared:%d  gold:%dG\n",
               gs->zone_current, gs->dungeons_cleared, gs->party.gold);
    }
    printf("  ==================================================\n");
}

/* -- battle action (manual) -- */
static void manual_battle_input(GameState *gs, int actor_idx) {
    Character *actor = &gs->party.members[actor_idx];
    printf("\n  --------------------------------------------------\n");
    printf("  > %s's turn\n", actor->name);
    printf("  --------------------------------------------------\n");
    printf("  [1] Attack\n");
    for (int si = 0; si < MAX_SKILLS; si++) {
        Skill *sk = &actor->skills[si];
        if (!sk->name[0]) continue;
        if (sk->cooldown_cur > 0)
            printf("  [%d] %s  (cooldown: %d)\n", si+2, sk->name, sk->cooldown_cur);
        else
            printf("  [%d] %s\n", si+2, sk->name);
    }
    printf("  [5] Defend\n");
    printf("  [6] Flee\n");
    printf("  > "); fflush(stdout);

    char line[64];
    if (!fgets(line, sizeof(line), stdin)) return;
    int sel = line[0] - '0';

    int tidx = -1;
    int alive_enemies[MAX_ENEMIES], en = 0;
    for (int i = 0; i < gs->battle.enemy_count; i++)
        if (gs->battle.enemies[i].base.is_alive) alive_enemies[en++] = i;

    if (sel >= 1 && sel <= 4 && en > 0) {
        if (en == 1) {
            tidx = PARTY_SIZE + alive_enemies[0];
        } else {
            printf("\n  Select target:\n");
            for (int i = 0; i < en; i++)
                printf("  [%d] %s\n", i+1, gs->battle.enemies[alive_enemies[i]].base.name);
            printf("  > "); fflush(stdout);
            if (!fgets(line, sizeof(line), stdin)) return;
            int t = line[0] - '1';
            if (t < 0 || t >= en) t = 0;
            tidx = PARTY_SIZE + alive_enemies[t];
        }
    }

    if (sel == 1 && tidx >= 0) {
        do_attack(gs, actor_idx, tidx);
        printf("  -> Attack %s! %d dmg\n",
               gs->battle.enemies[tidx-PARTY_SIZE].base.name, gs->battle.last_dmg);
    } else if (sel == 2 && actor->skills[0].cooldown_cur == 0 && tidx >= 0) {
        do_skill(gs, actor_idx, 0, tidx);
        printf("  -> %s! %d dmg\n", actor->skills[0].name, gs->battle.last_dmg);
    } else if (sel == 3 && actor->skills[1].cooldown_cur == 0) {
        do_skill(gs, actor_idx, 1, tidx >= 0 ? tidx : PARTY_SIZE);
        printf("  -> %s!\n", actor->skills[1].name);
    } else if (sel == 4 && actor->skills[2].cooldown_cur == 0) {
        do_skill(gs, actor_idx, 2, tidx >= 0 ? tidx : PARTY_SIZE);
        printf("  -> %s!\n", actor->skills[2].name);
    } else if (sel == 5) {
        do_defend(gs, actor_idx);
        printf("  -> %s defends!\n", actor->name);
    } else if (sel == 6) {
        do_flee(gs);
        printf("  -> Attempting flee...\n");
        return;
    } else {
        if (tidx >= 0) {
            do_attack(gs, actor_idx, tidx);
            printf("  -> Attack %s! %d dmg\n",
                   gs->battle.enemies[tidx-PARTY_SIZE].base.name, gs->battle.last_dmg);
        }
    }
    printf("  [Enter]"); fflush(stdout);
    fgets(line, sizeof(line), stdin);
}

/* -- ACTION parse (AI mode) -- */
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
            draw_screen(gs);
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
            draw_screen(gs);
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
            draw_screen(gs);
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
            draw_screen(gs);
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
            /* AI 모드: 화면 그리고 ACTION 기다림 */
            draw_screen(gs);
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
            manual_battle_input(gs, actor_idx);
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
        draw_screen(gs);
        print_party(gs);
        printf("\n  [1] Enter Dungeon\n");
        printf("  [2] Rest (50G, full HP)\n");
        printf("  > "); fflush(stdout);

        int sel = -1;
        if (g_ai_mode) {
            AICmd cmd; int r;
            do { r = parse_cmd(&cmd); } while (r == 0);
            if (r < 0) return;
            if (!strcmp(cmd.cmd, "dungeon")) sel = 0;
            else if (!strcmp(cmd.cmd, "rest")) sel = 1;
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
        draw_screen(gs);
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
            draw_screen(gs);
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

    /* AI 모드에서도 stdout으로 화면, stderr로 JSON */
    if (!g_ai_mode) freopen("/dev/null", "w", stderr);

    GameState gs;
    game_init(&gs);

    while (1) {
        switch (gs.phase) {
        case PHASE_HUB:     run_hub(&gs);     break;
        case PHASE_DUNGEON: run_dungeon(&gs); break;
        case PHASE_GAMEOVER:
            emit_event(&gs, "gameover", 0);
            draw_screen(&gs);
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