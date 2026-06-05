import subprocess
import json
import numpy as np
import os

MECHANICO_BIN = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'mechanico')

BATTLE_ACTIONS  = [
    "attack 0", "attack 1", "attack 2", "attack 3",
    "skill_0 0", "skill_0 1", "skill_0 2", "skill_0 3",
    "skill_1 0", "skill_1 1", "skill_1 2", "skill_1 3",
    "skill_2 0", "skill_2 1", "skill_2 2", "skill_2 3",
    "defend", "flee",
]
DUNGEON_ACTIONS = ["move_n", "move_s", "move_e", "move_w"]
HUB_ACTIONS     = ["dungeon", "rest"]
ACTION_SPACE    = BATTLE_ACTIONS + DUNGEON_ACTIONS + HUB_ACTIONS
ACTION_SIZE     = len(ACTION_SPACE)
STATE_SIZE      = 39


def state_to_vector(state: dict) -> np.ndarray:
    vec = np.zeros(STATE_SIZE, dtype=np.float32)
    idx = 0
    for m in (state.get("party") or [])[:3]:
        if not m: idx += 6; continue
        vec[idx]   = m["hp"] / m["hp_max"] if m["hp_max"] > 0 else 0
        vec[idx+1] = m["atk"] / 15.0
        vec[idx+2] = m["def"] / 15.0
        vec[idx+3] = m["spd"] / 15.0
        vec[idx+4] = float(m["alive"])
        vec[idx+5] = float(m["stunned"])
        idx += 6
    enemies = (state.get("battle") or {}).get("enemies") or []
    for i in range(4):
        if i >= len(enemies): idx += 4; continue
        e = enemies[i]
        vec[idx]   = e["hp"] / e["hp_max"] if e["hp_max"] > 0 else 0
        vec[idx+1] = e["atk"] / 15.0
        vec[idx+2] = e["def"] / 15.0
        vec[idx+3] = float(e["alive"])
        idx += 4
    phase = state.get("phase", "hub")
    vec[idx]   = state.get("zone", 1) / 5.0
    vec[idx+1] = state.get("cleared", 0) / 20.0
    vec[idx+2] = (state.get("battle") or {}).get("turn", 0) / 100.0
    vec[idx+3] = state.get("gold", 0) / 500.0
    vec[idx+4] = 1.0 if phase == "battle" else (0.5 if phase == "dungeon" else 0.0)
    return vec


def get_valid_mask(state: dict) -> np.ndarray:
    mask  = np.zeros(ACTION_SIZE, dtype=bool)
    valid = set((state.get("battle") or {}).get("valid_actions") or [])
    phase = state.get("phase", "hub")
    for i, act in enumerate(ACTION_SPACE):
        cmd = act.split()[0]
        if   phase == "battle"  and cmd in valid:           mask[i] = True
        elif phase == "dungeon" and act in DUNGEON_ACTIONS:  mask[i] = True
        elif phase == "hub"     and act in HUB_ACTIONS:      mask[i] = True
    if not mask.any(): mask[0] = True
    return mask


class MechanicoEnv:
    def __init__(self):
        self.proc  = None
        self.state = {}

    def _start(self):
        self.proc = subprocess.Popen(
            [MECHANICO_BIN, "--ai"],
            stdin=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdout=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )

    def _read(self) -> dict:
        """is_your_turn=True인 JSON이 올 때까지 읽음"""
        while True:
            line = self.proc.stderr.readline()
            if not line:
                ret = self.proc.poll()
                if ret is not None:
                    return {}
                continue
            try:
                state = json.loads(line.strip())
            except json.JSONDecodeError:
                continue

            # 항상 최신 상태 저장 (렌더링용)
            self.state = state

            # is_your_turn=True면 이게 Python이 응답해야 할 타이밍
            if state.get("is_your_turn"):
                return state
            # False면 그냥 계속 읽음 (적 턴, 이벤트 등)

    def _send(self, action_str: str):
        try:
            self.proc.stdin.write(f"ACTION {action_str}\n")
            self.proc.stdin.flush()
        except BrokenPipeError:
            pass

    def reset(self):
        self.close()
        self._start()
        state = self._read()
        return state_to_vector(state), state

    def step(self, action_idx: int):
        prev = self.state
        self._send(ACTION_SPACE[action_idx])
        next_s = self._read()
        if not next_s:
            return np.zeros(STATE_SIZE, dtype=np.float32), -5.0, True, {}
        reward = self._reward(prev, next_s)
        done   = next_s.get("phase") in ("gameover", "clear")
        return state_to_vector(next_s), reward, done, next_s

    def _party_hp(self, s):
        return sum(m["hp"] for m in (s.get("party") or []) if m)

    def _reward(self, prev: dict, curr: dict) -> float:
        r = 0.0
        phase    = curr.get("phase", "")
        last_dmg = (curr.get("battle") or {}).get("last_dmg", 0)
        if prev.get("phase") == "battle":
            r += last_dmg * 0.01
            r -= (self._party_hp(prev) - self._party_hp(curr)) * 0.05
        if phase == "battle_win":  r += 1.0
        if phase == "battle_lose": r -= 0.5
        if phase == "gameover":    r -= 5.0
        if curr.get("cleared", 0) > prev.get("cleared", 0):
            r += 2.0 * curr.get("cleared", 1)
        r += 0.001
        return r

    def close(self):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            self.proc.wait()
        self.proc = None