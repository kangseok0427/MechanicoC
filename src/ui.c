#include "game.h"
#include <stdio.h>
#include <string.h>

void cls(void) {
    printf("\033[2J\033[H"); fflush(stdout);
}

void print_bar(int cur, int mx, int width) {
    int filled = mx > 0 ? cur * width / mx : 0;
    printf("[");
    for (int i = 0; i < width; i++) printf("%c", i < filled ? '|' : '.');
    printf("] %3d/%3d", cur, mx);
}

void print_dungeon_map(GameState *gs) {
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

void print_ai_status(void) {
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

void print_party(GameState *gs) {
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

void print_enemies(GameState *gs) {
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

void draw_screen(GameState *gs, int ai_mode) {
    cls();
    printf("\n  ==================================================\n");
    if (ai_mode) {
        printf("  MECHANICO  zone:%d  cleared:%d  gold:%dG  [AI MODE]\n",
               gs->zone_current, gs->dungeons_cleared, gs->party.gold);
        print_ai_status();
    } else {
        printf("  MECHANICO  zone:%d  cleared:%d  gold:%dG\n",
               gs->zone_current, gs->dungeons_cleared, gs->party.gold);
    }
    printf("  ==================================================\n");
}

void manual_battle_input(GameState *gs, int actor_idx, int ai_mode) {
    (void)ai_mode;
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
