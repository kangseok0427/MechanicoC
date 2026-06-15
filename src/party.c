#include "game.h"

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
