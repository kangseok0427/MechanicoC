# MECHANICO

A turn-based console JRPG built in C with ncurses.

## Build

```bash
# macOS
brew install ncurses
make

# Linux
sudo apt install libncurses-dev
make
```

## Run

```bash
# Manual play
./mechanico

# AI mode (stdin/stdout bridge)
./mechanico --ai
```

## Controls

- **Arrow keys** or **number keys** to select actions
- **Enter / Space** to confirm

## Structure

```
src/
  state.h/c    - Game state + JSON emit (stderr)
  battle.h/c   - Turn-based combat engine
  dungeon.h/c  - Dungeon map generation + movement
  party.h/c    - Party management
  enemy.h/c    - Enemy definitions per zone
  ui.h/c       - ncurses rendering
  main.c       - Game loop + AI stdin bridge
```

## AI Bridge

Game state is emitted to **stderr** as JSON each turn.
AI sends commands to **stdin** in the format:

```
ACTION <cmd> [args...]

ACTION attack 0       # attack enemy 0
ACTION skill_0 1      # use skill 0 on enemy 1
ACTION skill_1 0
ACTION skill_2
ACTION defend
ACTION flee
ACTION move_n         # dungeon movement
ACTION move_s
ACTION move_e
ACTION move_w
ACTION dungeon        # hub: enter dungeon
ACTION rest           # hub: rest (50G)
```

## Characters

| Name | Role     | HP  | ATK | DEF | SPD |
|------|----------|-----|-----|-----|-----|
| KAEL | Warrior  | 60  | 9   | 5   | 10  |
| VOSS | Tanker   | 100 | 5   | 10  | 4   |
| LYRA | Sorcerer | 50  | 10  | 4   | 7   |
