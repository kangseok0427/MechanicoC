import torch
import torch.nn as nn
import numpy as np
import random
import os
import sys
import time
import json

sys.path.insert(0, os.path.dirname(__file__))
from env import MechanicoEnv, get_valid_mask, STATE_SIZE, ACTION_SIZE
from dqn_model import DQN
from replay_buffer import ReplayBuffer
from renderer import render

# ── 하이퍼파라미터 ──
GAMMA       = 0.97
LR          = 1e-4
BATCH       = 64
BUFFER_SIZE = 50_000
TAU         = 0.005
EPS_START   = 1.0
EPS_END     = 0.05
EPS_DECAY   = 5_000
SAVE_EVERY  = 200

# 방송용 딜레이 (초)
DELAY_BATTLE  = 0.5
DELAY_DUNGEON = 0.2
DELAY_HUB     = 0.3
DELAY_DONE    = 1.0

CURRICULUM = [
    {"name": "stage1", "target_reward": 1.0},
    {"name": "stage2", "target_reward": 3.0},
    {"name": "stage3", "target_reward": 6.0},
    {"name": "stage4", "target_reward": 10.0},
    {"name": "stage5", "target_reward": 15.0},
]

CHECKPOINT_PATH = "checkpoints/train_state.json"
MODEL_PATH      = "checkpoints/model_latest.pt"
STATUS_PATH     = "checkpoints/mechanico_status.json"   # 가온이 참고용

DEVICE = torch.device("mps" if torch.backends.mps.is_available() else "cpu")
print(f"Device: {DEVICE}")


def soft_update(online, target, tau):
    for p_o, p_t in zip(online.parameters(), target.parameters()):
        p_t.data.copy_(tau * p_o.data + (1 - tau) * p_t.data)

def get_epsilon(step):
    return max(EPS_END, EPS_START - (EPS_START - EPS_END) * step / EPS_DECAY)


# ── 학습 상태 저장/로드 ──
def save_train_state(episode, total_steps, curriculum_idx, recent_rewards, loss_val, online, optimizer):
    torch.save(online.state_dict(), MODEL_PATH)
    state = {
        "episode":        episode,
        "total_steps":    total_steps,
        "curriculum_idx": curriculum_idx,
        "recent_rewards": recent_rewards,
        "loss_val":       loss_val,
        "epsilon":        get_epsilon(total_steps),
    }
    with open(CHECKPOINT_PATH, "w") as f:
        json.dump(state, f, indent=2)

def load_train_state(online, optimizer):
    if not os.path.exists(CHECKPOINT_PATH) or not os.path.exists(MODEL_PATH):
        print("[체크포인트] 없음 → 처음부터 시작")
        return 0, 0, 0, [], 0.0
    with open(CHECKPOINT_PATH) as f:
        state = json.load(f)
    online.load_state_dict(torch.load(MODEL_PATH, map_location=DEVICE))
    ep    = state["episode"]
    steps = state["total_steps"]
    cidx  = state["curriculum_idx"]
    rews  = state["recent_rewards"]
    loss  = state["loss_val"]
    print(f"[체크포인트] 로드 완료 → EP:{ep} STEPS:{steps} STAGE:{cidx+1} EPS:{get_epsilon(steps):.3f}")
    return ep, steps, cidx, rews, loss


# ── 가온이용 상태 저장 ──
def update_gaon_status(episode, total_steps, curriculum_idx, recent_rewards,
                        loss_val, game_state, event_type):
    avg = float(np.mean(recent_rewards)) if recent_rewards else 0.0
    stage = CURRICULUM[min(curriculum_idx, len(CURRICULUM)-1)]

    party = game_state.get("party", [])
    alive = [m["name"] for m in party if m and m.get("alive")]
    ko    = [m["name"] for m in party if m and not m.get("alive")]

    status = {
        # 학습 정보
        "episode":     episode,
        "total_steps": total_steps,
        "stage":       stage["name"],
        "epsilon":     round(get_epsilon(total_steps), 3),
        "loss":        round(loss_val, 4),
        "avg_reward":  round(avg, 3),
        "recent_rewards": recent_rewards[-5:],   # 최근 5개만

        # 게임 정보
        "event":       event_type,
        "phase":       game_state.get("phase", ""),
        "zone":        game_state.get("zone", 1),
        "cleared":     game_state.get("cleared", 0),
        "gold":        game_state.get("gold", 0),
        "party_alive": alive,
        "party_ko":    ko,
    }

    # 전투 중이면 적 정보도
    b = game_state.get("battle") or {}
    if b.get("enemies"):
        alive_enemies = [e["name"] for e in b["enemies"] if e.get("alive")]
        status["enemies_alive"] = alive_enemies
        status["battle_turn"]   = b.get("turn", 0)

    with open(STATUS_PATH, "w", encoding="utf-8") as f:
        json.dump(status, f, ensure_ascii=False, indent=2)


def train():
    env    = MechanicoEnv()
    online = DQN(STATE_SIZE, ACTION_SIZE).to(DEVICE)
    target = DQN(STATE_SIZE, ACTION_SIZE).to(DEVICE)
    optimizer = torch.optim.Adam(online.parameters(), lr=LR)
    buffer    = ReplayBuffer(BUFFER_SIZE)
    os.makedirs("checkpoints", exist_ok=True)

    # 이어하기
    episode, total_steps, curriculum_idx, recent_rewards, loss_val = \
        load_train_state(online, optimizer)

    target.load_state_dict(online.state_dict())
    target.eval()

    while True:
        stage = CURRICULUM[min(curriculum_idx, len(CURRICULUM)-1)]
        obs, state = env.reset()
        ep_reward = 0.0

        while True:
            eps  = get_epsilon(total_steps)
            mask = torch.tensor(get_valid_mask(state), device=DEVICE)

            if random.random() < eps:
                valid_idxs = mask.nonzero(as_tuple=True)[0].cpu().numpy()
                action = int(np.random.choice(valid_idxs))
            else:
                s_t = torch.tensor(obs, device=DEVICE)
                action = online.select_action(s_t, mask)

            ai_info = {
                "ep": episode,
                "avg": float(np.mean(recent_rewards)) if recent_rewards else 0.0,
                "epsilon": eps,
                "loss": loss_val,
                "stage": curriculum_idx + 1,
                "steps": total_steps,
            }
            render(state, ai_info)

            next_obs, reward, done, next_state = env.step(action)

            # 이벤트별 딜레이
            event = next_state.get("event", "")
            phase = next_state.get("phase", "")
            if done:
                time.sleep(DELAY_DONE)
            elif event == "your_turn" and phase == "battle":
                time.sleep(DELAY_BATTLE)
            elif phase == "dungeon":
                time.sleep(DELAY_DUNGEON)
            else:
                time.sleep(DELAY_HUB)

            # 주요 이벤트마다 가온이 상태 갱신
            if event in ("battle_start", "battle_win", "battle_lose",
                         "dungeon_clear", "gameover"):
                update_gaon_status(episode, total_steps, curriculum_idx,
                                   recent_rewards, loss_val, next_state, event)

            buffer.push(obs, action, reward, next_obs, float(done))
            obs        = next_obs
            state      = next_state
            ep_reward += reward
            total_steps += 1

            # 학습
            if len(buffer) >= BATCH:
                s, a, r, ns, d = buffer.sample(BATCH)
                s  = torch.tensor(s,  device=DEVICE)
                a  = torch.tensor(a,  device=DEVICE)
                r  = torch.tensor(r,  device=DEVICE)
                ns = torch.tensor(ns, device=DEVICE)
                d  = torch.tensor(d,  device=DEVICE)

                with torch.no_grad():
                    next_q    = target(ns).max(dim=1).values
                    td_target = r + GAMMA * next_q * (1 - d)

                q_val = online(s).gather(1, a.unsqueeze(1)).squeeze(1)
                loss  = nn.functional.smooth_l1_loss(q_val, td_target)
                loss_val = loss.item()

                optimizer.zero_grad()
                loss.backward()
                nn.utils.clip_grad_norm_(online.parameters(), 10.0)
                optimizer.step()
                soft_update(online, target, TAU)

            if done:
                break

        episode += 1
        recent_rewards.append(ep_reward)
        if len(recent_rewards) > 20: recent_rewards.pop(0)
        avg = np.mean(recent_rewards)

        # 에피소드 끝마다 학습 상태 저장 (이어하기용)
        save_train_state(episode, total_steps, curriculum_idx,
                         recent_rewards, loss_val, online, optimizer)

        # 에피소드 끝 가온이 상태 갱신
        update_gaon_status(episode, total_steps, curriculum_idx,
                           recent_rewards, loss_val, state, "episode_end")

        print(f"[EP {episode:04d}] stage={stage['name']}  "
              f"reward={ep_reward:+.2f}  avg={avg:+.2f}  "
              f"eps={get_epsilon(total_steps):.3f}  loss={loss_val:.4f}")

        if episode % SAVE_EVERY == 0:
            path = f"checkpoints/{stage['name']}_ep{episode}.pt"
            torch.save(online.state_dict(), path)
            print(f"  >> saved {path}")

        if len(recent_rewards) >= 20 and avg >= stage["target_reward"]:
            print(f"\n*** {stage['name']} cleared! avg={avg:.2f} ***\n")
            torch.save(online.state_dict(), f"checkpoints/{stage['name']}_final.pt")
            if curriculum_idx < len(CURRICULUM) - 1:
                curriculum_idx += 1
            else:
                print("*** 모든 스테이지 클리어! 계속 학습 중... ***")

    env.close()

if __name__ == "__main__":
    train()