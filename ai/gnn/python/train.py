"""
Training script for the GNN Risk Assessment model.

Usage:
    python train.py --data_dir ../../../training_data --epochs 100
"""

import argparse
import os
from pathlib import Path
from datetime import datetime

import torch
import torch.nn as nn
import torch.optim as optim
from torch_geometric.loader import DataLoader

from model import create_model, INPUT_FEATURES
from dataset import GameStateDataset, split_dataset


def train_epoch(model, loader, optimizer, criterion, device):
    """Train for one epoch."""
    model.train()
    total_loss = 0
    num_samples = 0

    for batch in loader:
        batch = batch.to(device)
        optimizer.zero_grad()

        # Forward pass
        pred = model(batch.x, batch.edge_index)
        pred = pred.squeeze(-1)  # [num_nodes]

        # Compute loss
        loss = criterion(pred, batch.y)
        loss.backward()
        optimizer.step()

        total_loss += loss.item() * batch.num_nodes
        num_samples += batch.num_nodes

    return total_loss / num_samples


def evaluate(model, loader, criterion, device):
    """Evaluate model on a dataset."""
    model.eval()
    total_loss = 0
    total_correct = 0
    num_samples = 0

    with torch.no_grad():
        for batch in loader:
            batch = batch.to(device)

            # Forward pass
            pred = model(batch.x, batch.edge_index)
            pred = pred.squeeze(-1)

            # Compute loss
            loss = criterion(pred, batch.y)
            total_loss += loss.item() * batch.num_nodes

            # Compute accuracy (binary: risk > 0.5)
            pred_binary = (pred > 0.5).float()
            target_binary = (batch.y > 0.5).float()
            total_correct += (pred_binary == target_binary).sum().item()

            num_samples += batch.num_nodes

    avg_loss = total_loss / num_samples
    accuracy = total_correct / num_samples

    return avg_loss, accuracy


def train(args):
    """Main training function."""
    print(f"Loading data from: {args.data_dir}")

    # Load dataset
    dataset = GameStateDataset(args.data_dir)
    if len(dataset) == 0:
        print("Error: No data found. Run some AI games with logging enabled first.")
        return

    # Split dataset
    train_dataset, val_dataset, test_dataset = split_dataset(
        dataset,
        train_ratio=0.7,
        val_ratio=0.15,
        seed=args.seed
    )
    print(f"Train: {len(train_dataset)}, Val: {len(val_dataset)}, Test: {len(test_dataset)}")

    # Create data loaders
    train_loader = DataLoader(train_dataset, batch_size=args.batch_size, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=args.batch_size)
    test_loader = DataLoader(test_dataset, batch_size=args.batch_size)

    # Create model
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print(f"Using device: {device}")

    model = create_model(
        args.model_version,
        hidden_channels=args.hidden_dim,
        num_heads=args.num_heads,
        dropout=args.dropout
    ).to(device)

    print(f"Model parameters: {sum(p.numel() for p in model.parameters()):,}")

    # Loss and optimizer
    criterion = nn.MSELoss()  # Regression loss for risk score
    optimizer = optim.Adam(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(
        optimizer, mode='min', factor=0.5, patience=10, verbose=True
    )

    # Training loop
    best_val_loss = float('inf')
    best_epoch = 0
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"\nStarting training for {args.epochs} epochs...")
    print("-" * 60)

    for epoch in range(1, args.epochs + 1):
        # Train
        train_loss = train_epoch(model, train_loader, optimizer, criterion, device)

        # Evaluate
        val_loss, val_acc = evaluate(model, val_loader, criterion, device)

        # Update scheduler
        scheduler.step(val_loss)

        # Save best model
        if val_loss < best_val_loss:
            best_val_loss = val_loss
            best_epoch = epoch
            torch.save({
                'epoch': epoch,
                'model_state_dict': model.state_dict(),
                'optimizer_state_dict': optimizer.state_dict(),
                'val_loss': val_loss,
                'val_acc': val_acc,
            }, output_dir / 'best_model.pt')

        # Print progress
        if epoch % args.log_interval == 0:
            print(f"Epoch {epoch:4d} | Train Loss: {train_loss:.4f} | "
                  f"Val Loss: {val_loss:.4f} | Val Acc: {val_acc:.4f}")

    print("-" * 60)
    print(f"Training complete. Best val loss: {best_val_loss:.4f} at epoch {best_epoch}")

    # Final evaluation on test set
    print("\nEvaluating on test set...")
    checkpoint = torch.load(output_dir / 'best_model.pt')
    model.load_state_dict(checkpoint['model_state_dict'])
    test_loss, test_acc = evaluate(model, test_loader, criterion, device)
    print(f"Test Loss: {test_loss:.4f} | Test Accuracy: {test_acc:.4f}")

    # Save final model for TorchScript export
    model.eval()
    sample_batch = next(iter(test_loader)).to(device)
    traced_model = torch.jit.trace(
        model,
        (sample_batch.x, sample_batch.edge_index)
    )
    traced_model.save(str(output_dir / 'risk_model.pt'))
    print(f"\nTorchScript model saved to: {output_dir / 'risk_model.pt'}")

    # Save training config
    config = {
        'model_version': args.model_version,
        'hidden_dim': args.hidden_dim,
        'num_heads': args.num_heads,
        'dropout': args.dropout,
        'input_features': INPUT_FEATURES,
        'best_epoch': best_epoch,
        'best_val_loss': best_val_loss,
        'test_loss': test_loss,
        'test_accuracy': test_acc,
        'timestamp': datetime.now().isoformat(),
    }
    import json
    with open(output_dir / 'config.json', 'w') as f:
        json.dump(config, f, indent=2)


def main():
    parser = argparse.ArgumentParser(description='Train GNN Risk Assessment Model')

    # Data
    parser.add_argument('--data_dir', type=str, default='../../../training_data',
                        help='Directory containing training data')
    parser.add_argument('--output_dir', type=str, default='../models',
                        help='Directory to save model and logs')

    # Model
    parser.add_argument('--model_version', type=str, default='v1',
                        choices=['v1', 'v2'], help='Model architecture version')
    parser.add_argument('--hidden_dim', type=int, default=64,
                        help='Hidden dimension size')
    parser.add_argument('--num_heads', type=int, default=4,
                        help='Number of attention heads')
    parser.add_argument('--dropout', type=float, default=0.2,
                        help='Dropout rate')

    # Training
    parser.add_argument('--epochs', type=int, default=100,
                        help='Number of training epochs')
    parser.add_argument('--batch_size', type=int, default=32,
                        help='Batch size')
    parser.add_argument('--lr', type=float, default=0.001,
                        help='Learning rate')
    parser.add_argument('--weight_decay', type=float, default=1e-4,
                        help='Weight decay for regularization')
    parser.add_argument('--seed', type=int, default=42,
                        help='Random seed')

    # Logging
    parser.add_argument('--log_interval', type=int, default=10,
                        help='How often to log progress')

    args = parser.parse_args()

    # Set random seeds
    torch.manual_seed(args.seed)
    if torch.cuda.is_available():
        torch.cuda.manual_seed(args.seed)

    train(args)


if __name__ == '__main__':
    main()
