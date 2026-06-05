import torch
import torch.nn as nn

class DQN(nn.Module):
    def __init__(self, state_dim: int, action_dim: int):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(state_dim, 128),
            nn.ReLU(),
            nn.Linear(128, 128),
            nn.ReLU(),
            nn.Linear(128, action_dim),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)

    def select_action(self, state: torch.Tensor, mask: torch.Tensor) -> int:
        """마스킹 적용 후 greedy 선택"""
        with torch.no_grad():
            q = self.forward(state)
            q[~mask] = -float('inf')
            return int(q.argmax().item())