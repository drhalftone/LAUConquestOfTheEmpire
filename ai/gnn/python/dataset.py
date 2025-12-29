"""
Dataset loading utilities for GNN training.

Loads game state snapshots exported from C++ and converts them
to PyTorch Geometric Data objects.
"""

import json
import os
from pathlib import Path
from typing import List, Dict, Optional, Tuple

import torch
from torch.utils.data import Dataset
from torch_geometric.data import Data, InMemoryDataset

from model import INPUT_FEATURES, FEATURE_NAMES


class GameStateDataset(Dataset):
    """
    Simple dataset that loads JSON snapshots on-demand.

    Good for initial development and small datasets.
    """

    def __init__(self, data_dir: str, transform=None):
        """
        Initialize dataset.

        Args:
            data_dir: Directory containing game subdirectories with snapshots
            transform: Optional transform to apply to each sample
        """
        self.data_dir = Path(data_dir)
        self.transform = transform
        self.samples: List[Path] = []

        # Find all snapshot files
        self._scan_directory()

    def _scan_directory(self):
        """Scan for all snapshot JSON files."""
        if not self.data_dir.exists():
            print(f"Warning: Data directory {self.data_dir} does not exist")
            return

        # Look for game subdirectories
        for game_dir in self.data_dir.iterdir():
            if game_dir.is_dir():
                # Find snapshot files in this game
                for snapshot_file in game_dir.glob("snapshot_*.json"):
                    self.samples.append(snapshot_file)

        print(f"Found {len(self.samples)} snapshots in {self.data_dir}")

    def __len__(self) -> int:
        return len(self.samples)

    def __getitem__(self, idx: int) -> Data:
        """
        Load a single game state snapshot.

        Args:
            idx: Sample index

        Returns:
            PyTorch Geometric Data object
        """
        filepath = self.samples[idx]
        data = load_snapshot(filepath)

        if self.transform is not None:
            data = self.transform(data)

        return data


class GameStateInMemoryDataset(InMemoryDataset):
    """
    In-memory dataset that pre-loads all snapshots.

    Better performance for training on small-medium datasets.
    """

    def __init__(self, root: str, transform=None, pre_transform=None):
        """
        Initialize dataset.

        Args:
            root: Root directory (data in root/raw, processed in root/processed)
            transform: Transform to apply at access time
            pre_transform: Transform to apply during processing
        """
        super().__init__(root, transform, pre_transform)
        self.data, self.slices = torch.load(self.processed_paths[0])

    @property
    def raw_file_names(self) -> List[str]:
        """List of raw files to look for."""
        # We look for any JSON files in raw directory
        raw_dir = Path(self.raw_dir)
        if raw_dir.exists():
            return [f.name for f in raw_dir.glob("**/*.json") if "snapshot" in f.name]
        return []

    @property
    def processed_file_names(self) -> List[str]:
        """List of processed files."""
        return ['data.pt']

    def download(self):
        """No download needed - data comes from game."""
        pass

    def process(self):
        """Process raw JSON files into PyTorch Geometric format."""
        data_list = []

        raw_dir = Path(self.raw_dir)
        for snapshot_file in raw_dir.glob("**/*.json"):
            if "snapshot" not in snapshot_file.name:
                continue

            try:
                data = load_snapshot(snapshot_file)
                if self.pre_transform is not None:
                    data = self.pre_transform(data)
                data_list.append(data)
            except Exception as e:
                print(f"Error loading {snapshot_file}: {e}")

        if not data_list:
            print("Warning: No valid snapshots found")
            # Create a dummy data point
            data_list = [Data(
                x=torch.zeros(1, INPUT_FEATURES),
                edge_index=torch.zeros(2, 0, dtype=torch.long),
                y=torch.zeros(1)
            )]

        data, slices = self.collate(data_list)
        torch.save((data, slices), self.processed_paths[0])
        print(f"Processed {len(data_list)} snapshots")


def load_snapshot(filepath: str) -> Data:
    """
    Load a single snapshot JSON and convert to PyTorch Geometric Data.

    Args:
        filepath: Path to snapshot JSON file

    Returns:
        Data object with x (features), edge_index, y (labels)
    """
    with open(filepath, 'r') as f:
        snapshot = json.load(f)

    # Extract node features
    node_features = snapshot['node_features']
    num_nodes = len(node_features)

    # Create feature tensor (excluding label columns)
    x = torch.tensor([row[:INPUT_FEATURES] for row in node_features], dtype=torch.float)

    # Extract labels (risk_score is at index 27)
    y = torch.tensor([row[27] for row in node_features], dtype=torch.float)

    # Extract edge index
    edge_index = torch.tensor(snapshot['edge_index'], dtype=torch.long)

    # Create Data object
    data = Data(x=x, edge_index=edge_index, y=y)

    # Add metadata
    data.node_names = snapshot.get('node_names', [])
    data.num_nodes = num_nodes

    return data


def load_combat_outcomes(game_dir: str) -> List[Dict]:
    """
    Load combat outcome labels for a game.

    Args:
        game_dir: Directory containing game data

    Returns:
        List of combat outcome dicts
    """
    filepath = Path(game_dir) / "combat_outcomes.json"
    if not filepath.exists():
        return []

    with open(filepath, 'r') as f:
        data = json.load(f)

    return data.get('combat_outcomes', [])


def load_territory_captures(game_dir: str) -> List[Dict]:
    """
    Load territory capture events for a game.

    Args:
        game_dir: Directory containing game data

    Returns:
        List of capture event dicts
    """
    filepath = Path(game_dir) / "territory_captures.json"
    if not filepath.exists():
        return []

    with open(filepath, 'r') as f:
        data = json.load(f)

    return data.get('territory_captures', [])


def split_dataset(dataset: Dataset,
                  train_ratio: float = 0.7,
                  val_ratio: float = 0.15,
                  seed: int = 42) -> Tuple[Dataset, Dataset, Dataset]:
    """
    Split dataset into train/validation/test sets.

    Args:
        dataset: Full dataset
        train_ratio: Fraction for training
        val_ratio: Fraction for validation
        seed: Random seed for reproducibility

    Returns:
        (train_dataset, val_dataset, test_dataset)
    """
    from torch.utils.data import random_split

    n = len(dataset)
    train_size = int(n * train_ratio)
    val_size = int(n * val_ratio)
    test_size = n - train_size - val_size

    generator = torch.Generator().manual_seed(seed)
    return random_split(dataset, [train_size, val_size, test_size], generator=generator)


if __name__ == "__main__":
    import sys

    # Test loading
    if len(sys.argv) > 1:
        data_dir = sys.argv[1]
    else:
        data_dir = "../../../training_data"

    print(f"Loading data from: {data_dir}")
    dataset = GameStateDataset(data_dir)
    print(f"Dataset size: {len(dataset)}")

    if len(dataset) > 0:
        sample = dataset[0]
        print(f"\nSample 0:")
        print(f"  Nodes: {sample.num_nodes}")
        print(f"  Edges: {sample.edge_index.shape[1]}")
        print(f"  Features shape: {sample.x.shape}")
        print(f"  Labels shape: {sample.y.shape}")
        print(f"  Node names: {sample.node_names[:5]}...")
