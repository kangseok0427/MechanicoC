import os

def clear():
    print("\033[2J\033[H", end="", flush=True)

def bar(cur, mx, width=12):
    filled = int(cur / mx * width) if mx > 0 else 0
    return "[" + "|" * filled + "." * (width - filled) + f"] {cur}/{mx}"

def render(state: dict, ai_info: dict):
    clear()
    phase = state.get("phase", "?")
    event = state.get("event", "")
    ep    = ai_info.get("ep", 0)
    avg   = ai_info.get("avg", 0.0)
    eps   = ai_info.get("epsilon", 1.0)
    loss  = ai_info.get("loss", 0.0)
    stage = ai_info.get("stage", 1)
    steps = ai_info.get("steps", 0)

    w = 58
    print("=" * w)
    print(f"  MECHANICO   EP:{ep:<5} stage:{stage}  steps:{steps}")
    print(f"  avg:{avg:+.2f}  eps:{eps:.2f}  loss:{loss:.4f}")
    print(f"  phase:{phase}  event:{event}")
    print("=" * w)

    print("\n  PARTY")
    for m in (state.get("party") or []):
        if not m: continue
        status = ""
        if m.get("stunned"):   status += "[STN]"
        if m.get("counter"):   status += "[CTR]"
        if m.get("defending"): status += "[GRD]"
        alive = m.get("alive")
        hp_cur = m["hp"] if alive else 0
        ko = "  -- KO --" if not alive else ""
        print(f"    {m['name']:<6} {bar(hp_cur, m['hp_max']):<28}"
              f" ATK:{m['atk']} DEF:{m['def']} SPD:{m['spd']}"
              f"  {status}{ko}")

    b = state.get("battle") or {}
    enemies = b.get("enemies") or []
    if enemies and phase in ("battle", "battle_win", "battle_lose"):
        boss = "  *** BOSS ***" if b.get("is_boss") else ""
        print(f"\n  ENEMIES  (turn {b.get('turn', 0)}){boss}")
        for e in enemies:
            ko = "  DEFEATED" if not e.get("alive") else ""
            print(f"    {e['name']:<12} {bar(e['hp'], e['hp_max']):<28}{ko}")
        actor = b.get("actor", "")
        if actor:
            print(f"\n  현재 액터: {actor}")

    if phase == "dungeon":
        d = state.get("dungeon") or {}
        print(f"\n  DUNGEON  zone:{state.get('zone', 1)}  "
              f"cleared:{state.get('cleared', 0)}  "
              f"pos:({d.get('x', '?')},{d.get('y', '?')})")

    valid = (b.get("valid_actions") or state.get("valid_actions") or [])
    if valid:
        print(f"\n  actions: {', '.join(valid)}")

    print("\n" + "-" * w, flush=True)