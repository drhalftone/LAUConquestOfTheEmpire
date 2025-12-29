"""
Graph Attention Network (GAT) for Territory Risk Assessment

This model takes a game state graph and outputs per-territory risk scores.
"""

import torch
import torch.nn as nn
import torch.nn.functional as F
from torch_geometric.nn import GATConv, global_mean_pool


class RiskAssessmentGAT(nn.Module):
    """
    Graph Attention Network for territory risk assessment.

    Architecture:
        Input (30 features) -> GAT Layer 1 (64 hidden, 4 heads)
                            -> GAT Layer 2 (64 hidden, 4 heads)
                            -> GAT Layer 3 (32 hidden, 4 heads)
                            -> Output Layer (1 risk score per node)

    Uses multi-head attention to learn which neighboring territories
    are most relevant for risk assessment.
    """

    def __init__(self,
                 in_channels: int = 30,
                 hidden_channels: int = 64,
                 num_heads: int = 4,
                 dropout: float = 0.2):
        """
        Initialize the GAT model.

        Args:
            in_channels: Number of input features per node (default: 30)
            hidden_channels: Hidden dimension size (default: 64)
            num_heads: Number of attention heads (default: 4)
            dropout: Dropout rate (default: 0.2)
        """
        super(RiskAssessmentGAT, self).__init__()

        self.dropout = dropout

        # GAT Layer 1: in_channels -> hidden_channels
        # Output will be hidden_channels * num_heads due to concatenation
        self.conv1 = GATConv(
            in_channels=in_channels,
            out_channels=hidden_channels,
            heads=num_heads,
            dropout=dropout,
            concat=True  # Concatenate heads
        )

        # GAT Layer 2: hidden_channels * num_heads -> hidden_channels
        self.conv2 = GATConv(
            in_channels=hidden_channels * num_heads,
            out_channels=hidden_channels,
            heads=num_heads,
            dropout=dropout,
            concat=True
        )

        # GAT Layer 3: hidden_channels * num_heads -> hidden_channels // 2
        self.conv3 = GATConv(
            in_channels=hidden_channels * num_heads,
            out_channels=hidden_channels // 2,
            heads=num_heads,
            dropout=dropout,
            concat=False  # Average heads for final layer
        )

        # Output layer: hidden_channels // 2 -> 1 (risk score)
        self.output = nn.Linear(hidden_channels // 2, 1)

        # Batch normalization layers
        self.bn1 = nn.BatchNorm1d(hidden_channels * num_heads)
        self.bn2 = nn.BatchNorm1d(hidden_channels * num_heads)
        self.bn3 = nn.BatchNorm1d(hidden_channels // 2)

    def forward(self, x, edge_index, batch=None):
        """
        Forward pass.

        Args:
            x: Node features [num_nodes, in_channels]
            edge_index: Edge indices [2, num_edges]
            batch: Batch assignment for batched graphs (optional)

        Returns:
            risk_scores: Per-node risk scores [num_nodes, 1]
        """
        # Layer 1
        x = self.conv1(x, edge_index)
        x = self.bn1(x)
        x = F.elu(x)
        x = F.dropout(x, p=self.dropout, training=self.training)

        # Layer 2
        x = self.conv2(x, edge_index)
        x = self.bn2(x)
        x = F.elu(x)
        x = F.dropout(x, p=self.dropout, training=self.training)

        # Layer 3
        x = self.conv3(x, edge_index)
        x = self.bn3(x)
        x = F.elu(x)

        # Output layer (sigmoid for 0-1 risk score)
        risk_scores = torch.sigmoid(self.output(x))

        return risk_scores

    def get_attention_weights(self, x, edge_index):
        """
        Get attention weights from all layers for interpretability.

        Args:
            x: Node features
            edge_index: Edge indices

        Returns:
            Dict of attention weights per layer
        """
        attention_weights = {}

        # Layer 1
        x1, (edge_index1, alpha1) = self.conv1(x, edge_index, return_attention_weights=True)
        attention_weights['layer1'] = alpha1
        x1 = F.elu(self.bn1(x1))

        # Layer 2
        x2, (edge_index2, alpha2) = self.conv2(x1, edge_index, return_attention_weights=True)
        attention_weights['layer2'] = alpha2
        x2 = F.elu(self.bn2(x2))

        # Layer 3
        x3, (edge_index3, alpha3) = self.conv3(x2, edge_index, return_attention_weights=True)
        attention_weights['layer3'] = alpha3

        return attention_weights


class RiskAssessmentGATv2(nn.Module):
    """
    Alternative architecture with residual connections and
    separate heads for different risk aspects.
    """

    def __init__(self,
                 in_channels: int = 30,
                 hidden_channels: int = 64,
                 num_heads: int = 4,
                 dropout: float = 0.2):
        super(RiskAssessmentGATv2, self).__init__()

        self.dropout = dropout

        # Initial projection
        self.input_proj = nn.Linear(in_channels, hidden_channels)

        # GAT layers with residual connections
        self.conv1 = GATConv(hidden_channels, hidden_channels // num_heads,
                             heads=num_heads, concat=True, dropout=dropout)
        self.conv2 = GATConv(hidden_channels, hidden_channels // num_heads,
                             heads=num_heads, concat=True, dropout=dropout)
        self.conv3 = GATConv(hidden_channels, hidden_channels // num_heads,
                             heads=num_heads, concat=True, dropout=dropout)

        # Output heads for different risk aspects
        self.risk_head = nn.Sequential(
            nn.Linear(hidden_channels, hidden_channels // 2),
            nn.ReLU(),
            nn.Linear(hidden_channels // 2, 1),
            nn.Sigmoid()
        )

        # Auxiliary head: predicted force balance
        self.force_head = nn.Sequential(
            nn.Linear(hidden_channels, hidden_channels // 2),
            nn.ReLU(),
            nn.Linear(hidden_channels // 2, 1),
            nn.Tanh()  # -1 to 1 (enemy advantage to our advantage)
        )

        self.bn1 = nn.BatchNorm1d(hidden_channels)
        self.bn2 = nn.BatchNorm1d(hidden_channels)
        self.bn3 = nn.BatchNorm1d(hidden_channels)

    def forward(self, x, edge_index, return_auxiliary=False):
        """
        Forward pass with optional auxiliary outputs.

        Args:
            x: Node features
            edge_index: Edge indices
            return_auxiliary: If True, return force balance predictions too

        Returns:
            risk_scores or (risk_scores, force_balance)
        """
        # Project input
        x = self.input_proj(x)
        identity = x

        # Layer 1 with residual
        x = self.conv1(x, edge_index)
        x = self.bn1(x)
        x = F.elu(x) + identity
        x = F.dropout(x, p=self.dropout, training=self.training)
        identity = x

        # Layer 2 with residual
        x = self.conv2(x, edge_index)
        x = self.bn2(x)
        x = F.elu(x) + identity
        x = F.dropout(x, p=self.dropout, training=self.training)
        identity = x

        # Layer 3 with residual
        x = self.conv3(x, edge_index)
        x = self.bn3(x)
        x = F.elu(x) + identity

        # Output heads
        risk_scores = self.risk_head(x)

        if return_auxiliary:
            force_balance = self.force_head(x)
            return risk_scores, force_balance

        return risk_scores


# Feature indices (must match C++ GNNFeatureSpec)
FEATURE_NAMES = [
    "is_mine", "is_enemy", "is_neutral",
    "my_infantry", "my_cavalry", "my_catapults", "my_generals", "my_galleys", "my_caesar",
    "enemy_infantry", "enemy_cavalry", "enemy_catapults", "enemy_generals", "enemy_galleys", "enemy_caesar",
    "has_city", "has_fortification", "on_road_network",
    "value_normalized", "is_sea", "is_home_province",
    "dist_to_caesar", "dist_to_enemy",
    "my_force_1turn", "my_force_2turn", "enemy_threat_1turn", "enemy_threat_2turn",
    "risk_score", "risk_level", "unused"
]

# Number of input features (excluding labels)
INPUT_FEATURES = 27  # First 27 features are inputs, last 3 are labels


def create_model(model_version: str = "v1", **kwargs) -> nn.Module:
    """
    Factory function to create model instances.

    Args:
        model_version: "v1" or "v2"
        **kwargs: Additional arguments passed to model constructor

    Returns:
        Model instance
    """
    if model_version == "v1":
        return RiskAssessmentGAT(in_channels=INPUT_FEATURES, **kwargs)
    elif model_version == "v2":
        return RiskAssessmentGATv2(in_channels=INPUT_FEATURES, **kwargs)
    else:
        raise ValueError(f"Unknown model version: {model_version}")


if __name__ == "__main__":
    # Test model creation
    model = create_model("v1")
    print(f"Model v1 parameters: {sum(p.numel() for p in model.parameters()):,}")

    model2 = create_model("v2")
    print(f"Model v2 parameters: {sum(p.numel() for p in model2.parameters()):,}")

    # Test forward pass with dummy data
    num_nodes = 45  # Land territories only
    num_edges = 150  # Approximate adjacencies

    x = torch.randn(num_nodes, INPUT_FEATURES)
    edge_index = torch.randint(0, num_nodes, (2, num_edges))

    risk_scores = model(x, edge_index)
    print(f"Output shape: {risk_scores.shape}")  # Should be [45, 1]

    # Test attention weights
    attention = model.get_attention_weights(x, edge_index)
    print(f"Attention layers: {list(attention.keys())}")
