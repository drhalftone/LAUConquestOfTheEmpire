# GNN-Based Risk Assessment for AI Player

## Overview

This document outlines the plan to implement a Graph Neural Network (GNN) for territory risk assessment, replacing/augmenting the current heuristic-based system in `ReachabilityCalculator` and `AIDecisionMaker`.

## Why GNN Instead of U-Net

| Aspect | U-Net | GNN |
|--------|-------|-----|
| Data structure | Regular 2D grid | Irregular graph |
| Neighborhood | Fixed (3x3, 5x5 kernels) | Variable (2-8+ neighbors) |
| Spatial assumption | Euclidean | Non-Euclidean |
| Fit for game map | Poor (60 irregular territories) | Excellent |

The game map is fundamentally a **graph** with 60 territory nodes and irregular adjacency relationships. GNNs are specifically designed for this data structure.

## Architecture: Graph Attention Network (GAT)

```
┌─────────────────────────────────────────────────────────┐
│                    INPUT LAYER                          │
│  60 nodes × F features (ownership, troops, value, etc.) │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  GAT LAYER 1                            │
│  Multi-head attention (4 heads), 64 hidden units        │
│  Aggregates immediate neighbor information              │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  GAT LAYER 2                            │
│  Multi-head attention (4 heads), 64 hidden units        │
│  Sees 2-hop neighborhood (strategic context)            │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  GAT LAYER 3                            │
│  Multi-head attention (4 heads), 32 hidden units        │
│  Global strategic awareness                             │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  OUTPUT LAYER                           │
│  60 nodes × 1 risk score (sigmoid → 0.0 to 1.0)         │
└─────────────────────────────────────────────────────────┘
```

## Node Features (per territory)

### Ownership Features (one-hot encoded)
| Feature | Dim | Description |
|---------|-----|-------------|
| `is_mine` | 1 | Territory owned by current player |
| `is_enemy` | 1 | Territory owned by any enemy |
| `is_neutral` | 1 | Unclaimed territory |
| `owner_id` | 6 | One-hot encoding of owner (A-F) |

### Military Features
| Feature | Dim | Description |
|---------|-----|-------------|
| `my_infantry` | 1 | Count of my infantry here |
| `my_cavalry` | 1 | Count of my cavalry here |
| `my_catapults` | 1 | Count of my catapults here |
| `my_generals` | 1 | Count of my generals here |
| `my_caesar` | 1 | 1 if my Caesar is here |
| `enemy_infantry` | 1 | Total enemy infantry here |
| `enemy_cavalry` | 1 | Total enemy cavalry here |
| `enemy_catapults` | 1 | Total enemy catapults here |
| `enemy_generals` | 1 | Total enemy generals here |
| `enemy_caesar` | 1 | 1 if any enemy Caesar is here |

### Infrastructure Features
| Feature | Dim | Description |
|---------|-----|-------------|
| `has_city` | 1 | Territory has a city |
| `has_fortification` | 1 | City is fortified (+1 defense) |
| `on_road_network` | 1 | Connected to my road network |

### Strategic Features
| Feature | Dim | Description |
|---------|-----|-------------|
| `territory_value` | 1 | Tax value (normalized: 5/10/20 → 0.25/0.5/1.0) |
| `is_sea` | 1 | Sea territory flag |
| `is_home_province` | 1 | Starting province for any player |
| `dist_to_my_caesar` | 1 | Graph distance to my Caesar (normalized) |
| `dist_to_nearest_enemy` | 1 | Graph distance to nearest enemy unit |

### Heat Map Features (from existing system)
| Feature | Dim | Description |
|---------|-----|-------------|
| `my_force_1turn` | 1 | Max force I can project here in 1 turn |
| `my_force_2turn` | 1 | Max force I can project here in 2 turns |
| `enemy_threat_1turn` | 1 | Max enemy force that can reach here in 1 turn |
| `enemy_threat_2turn` | 1 | Max enemy force that can reach here in 2 turns |

**Total: ~30 features per node**

## Edge Features (optional, for edge-aware GNN)

| Feature | Description |
|---------|-------------|
| `is_land_adjacent` | Standard land adjacency |
| `is_sea_adjacent` | Sea zone adjacency |
| `has_road` | Road connection between cities |
| `is_galley_route` | Reachable via galley transport |

## Training Data

### Label Source: Current Heuristic System

The existing `AIDecisionMaker` already computes risk assessments. We can use these as training labels:

```cpp
// From ai/aidecisionmaker.cpp
TerritoryRisk classifyRisk(territory, ourForce, enemyThreat)
// Returns: SAFE, LOW_RISK, MEDIUM_RISK, HIGH_RISK, CONTESTED
```

### Label Encoding Options

**Option A: Classification (5 classes)**
- SAFE → 0
- LOW_RISK → 1
- MEDIUM_RISK → 2
- HIGH_RISK → 3
- CONTESTED → 4

**Option B: Regression (continuous 0-1)**
- SAFE → 0.0
- LOW_RISK → 0.25
- MEDIUM_RISK → 0.5
- HIGH_RISK → 0.75
- CONTESTED → 1.0

**Option C: Multi-task**
- Risk score (regression)
- Win probability if attacked (regression)
- Should attack? (binary)

### Data Collection Strategy

1. **Play games** with current heuristic AI
2. **Log game states** at each turn:
   - Node features for all 60 territories
   - Adjacency matrix
   - Heuristic risk labels
3. **Post-game labeling**:
   - Did territories change hands?
   - Did attacks succeed/fail?
   - Actual combat outcomes vs predicted

## Implementation Plan

### Phase 1: Data Infrastructure ✓ COMPLETE

- [x] **Task 1.1**: Create `GameStateSnapshot` class to capture full game state
- [x] **Task 1.2**: Create `GNNFeatureExtractor` class to convert game state → node features
- [x] **Task 1.3**: Create `TrainingDataLogger` to save game states during AI play
- [x] **Task 1.4**: Define data serialization format (JSON)

### Phase 2: PyTorch Model (Python) ✓ COMPLETE

- [x] **Task 2.1**: Set up Python environment with PyTorch Geometric
- [x] **Task 2.2**: Implement GAT model in Python (`model.py`)
- [x] **Task 2.3**: Create data loader for training data (`dataset.py`)
- [x] **Task 2.4**: Create training script (`train.py`)
- [ ] **Task 2.5**: Train model on collected data (requires game data)
- [ ] **Task 2.6**: Export model to TorchScript (automatic in train.py)

### Phase 3: C++ Integration (NEXT)

- [ ] **Task 3.1**: Integrate LibTorch (PyTorch C++ API) into Qt project
- [ ] **Task 3.2**: Create `GNNRiskAssessor` class to load and run model
- [ ] **Task 3.3**: Integrate with `AIDecisionMaker` as alternative risk source

### Phase 4: Training Loop

- [ ] **Task 4.1**: Enable TrainingDataLogger in main game
- [ ] **Task 4.2**: Implement self-play data generation
- [ ] **Task 4.3**: Add outcome-based labels (did predictions match reality?)
- [ ] **Task 4.4**: Iterative training: play → collect → train → deploy → repeat

### Phase 5: Evaluation & Tuning

- [ ] **Task 5.1**: Compare GNN vs heuristic risk assessments
- [ ] **Task 5.2**: Measure AI win rate with GNN vs without
- [ ] **Task 5.3**: Visualize learned attention weights (what does AI "look at"?)
- [ ] **Task 5.4**: Tune hyperparameters (layers, heads, hidden size)

## File Structure

```
ai/gnn/
├── GNN_RISK_ASSESSMENT_PLAN.md    # This document
├── python/
│   ├── model.py                   # GAT model definition ✓
│   ├── dataset.py                 # Data loading ✓
│   ├── train.py                   # Training script ✓
│   └── requirements.txt           # Python dependencies ✓
├── gamestatesnapshot.h            # Game state capture ✓
├── gamestatesnapshot.cpp          # ✓
├── gnnfeatureextractor.h          # Feature extraction ✓
├── gnnfeatureextractor.cpp        # ✓
├── trainingdatalogger.h           # Data collection ✓
├── trainingdatalogger.cpp         # ✓
├── gnnriskassessor.h              # Model inference (TODO)
├── gnnriskassessor.cpp            # (TODO)
└── models/                        # Saved model weights
    └── risk_model.pt              # (Generated after training)
```

## Dependencies

### Python (training)
- PyTorch >= 2.0
- PyTorch Geometric (torch_geometric)
- NumPy, Pandas

### C++ (inference)
- LibTorch (PyTorch C++ frontend)
- Or: ONNX Runtime (if using ONNX export)

## Questions to Resolve

1. **Training data volume**: How many game states do we need? (Estimate: 1000+ games)
2. **Feature normalization**: Min-max vs standard scaling?
3. **Class imbalance**: Most territories are SAFE most of the time
4. **Inference speed**: Can we run GNN fast enough for real-time AI? (Target: <10ms)
5. **Model size**: LibTorch adds ~150MB to binary - acceptable?

## Success Criteria

1. GNN risk predictions correlate >0.8 with heuristic predictions
2. GNN-based AI achieves similar or better win rate vs heuristic AI
3. Inference time <10ms per game state
4. Model learns meaningful patterns (interpretable attention weights)

## References

- [Graph Attention Networks (Veličković et al., 2018)](https://arxiv.org/abs/1710.10903)
- [PyTorch Geometric Documentation](https://pytorch-geometric.readthedocs.io/)
- [LibTorch C++ API](https://pytorch.org/cppdocs/)

---

*Created: December 2024*
*Status: Planning*
