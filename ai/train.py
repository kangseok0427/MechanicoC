import torch
import torch.nn as nn
import numpy as np
import random
import os
import sys
import time

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
    {"name": "stage1", "target_reward": 0.5},
    {"name": "stage2", "target_reward": 1.5},
    {"name": "stage3", "target_reward": 3.0},
]

DEVICE = torch.device("mps" if torch.backends.mps.is_available() else "cpu")
print(f"Device: {DEVICE}")

def soft_update(online, target, tau):
    for p_o, p_t in zip(online.parameters(), target.parameters()):
        p_t.data.copy_(tau * p_o.data + (1 - tau) * p_t.data)

def get_epsilon(step):
    return max(EPS_END, EPS_START - (EPS_START - EPS_END) * step / EPS_DECAY)

def train():
    env    = MechanicoEnv()
    online = DQN(STATE_SIZE, ACTION_SIZE).to(DEVICE)
    target = DQN(STATE_SIZE, ACTION_SIZE).to(DEVICE)
    target.load_state_dict(online.state_dict())
    target.eval()

    optimizer = torch.optim.Adam(online.parameters(), lr=LR)
    buffer    = ReplayBuffer(BUFFER_SIZE)
    os.makedirs("checkpoints", exist_ok=True)

    total_steps    = 0
    episode        = 0
    curriculum_idx = 0
    recent_rewards = []
    loss_val       = 0.0

    while curriculum_idx < len(CURRICULUM):
        stage = CURRICULUM[curriculum_idx]
        obs, state = env.reset()
        ep_reward  = 0.0
        ep_steps   = 0

        while True:
            eps  = get_epsilon(total_steps)
            mask = torch.tensor(get_valid_mask(state), device=DEVICE)

            if random.random() < eps:
                valid_idxs = mask.nonzero(as_tuple=True)[0].cpu().numpy()
                action = int(np.random.choice(valid_idxs))
            else:
                s_t = torch.tensor(obs, device=DEVICE)
                action = online.select_action(s_t, mask)

            # 렌더링
            ai_info = {
                "ep": episode, "avg": float(np.mean(recent_rewards)) if recent_rewards else 0.0,
                "epsilon": eps, "loss": loss_val,
                "stage": curriculum_idx + 1, "steps": total_steps,
            }
            render(state, ai_info)

            next_obs, reward, done, next_state = env.step(action)

            # 이벤트 기반 딜레이
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

            buffer.push(obs, action, reward, next_obs, float(done))

            obs        = next_obs
            state      = next_state
            ep_reward += reward
            ep_steps  += 1
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

        if episode % SAVE_EVERY == 0:
            path = f"checkpoints/{stage['name']}_ep{episode}.pt"
            torch.save(online.state_dict(), path)
            print(f"\n  >> saved {path}")

        if len(recent_rewards) >= 20 and avg >= stage["target_reward"]:
            print(f"\n*** {stage['name']} cleared! avg={avg:.2f} ***\n")
            torch.save(online.state_dict(), f"checkpoints/{stage['name']}_final.pt")
            curriculum_idx += 1

    torch.save(online.state_dict(), "checkpoints/final.pt")
    env.close()

if __name__ == "__main__":
    train()