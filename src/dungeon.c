#include "game.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
        gs->zone_current++;
        gs->phase = PHASE_HUB;
        snprintf(gs->last_event, 64, "dungeon_clear");
        return 6;
    default: return 1;
    }
}