# AI Risk Assessment System Plan

This document outlines the plan for improving AI decision-making through reachability analysis and risk assessment.

---

## Part 1: Single-Turn Risk Assessment (Phase 1 Implementation)

The immediate goal is to help the AI understand which territories are safe to take versus which are contested.

### Core Insight

> "A territory that can only be reached by our general this turn but not any other general from any other army means we can take it with a single general without risk of losing that general."

### Step 1: Calculate Our Reachability

For each of our leaders, find all territories they can reach **this turn**:

**For each General/Caesar:**
1. Start from current territory
2. Add all territories within movement range (typically 2 for leaders)
3. Add all road-connected territories (if starting from a city on our road network)
4. Add territories reachable via galley (if galley available in adjacent sea zone)

**Output:**
```
Map<Territory, List<OurLeaders that can reach it>>
```

### Step 2: Calculate Enemy Reachability

Same calculation, but for each enemy player:

**Output:**
```
Map<Territory, List<EnemyLeaders that can reach it>>
```

### Step 3: Risk Classification

For each territory we can reach:

| We Can Reach | Enemy Can Reach | Risk |
|--------------|-----------------|------|
| Yes | No | **SAFE** - Take freely |
| Yes | Yes | **CONTESTED** - Need force comparison |
| No | Yes | (Not our concern - we can't reach it) |
| No | No | (Not relevant this turn) |

### Step 4: Force Comparison (for CONTESTED territories)

Simple comparison:
- **Our max force** = Sum of troops from all our leaders that can reach
- **Enemy max force** = Sum of troops from all enemy leaders that can reach

### Data Structures

```cpp
struct ReachableTerritory {
    QString territoryName;
    QList<Leader*> leadersWhoCanReach;
    int maxTroopStrength;  // Total troops those leaders could bring
};

struct TerritoryRisk {
    QString territoryName;
    int ourMaxForce;
    int enemyMaxForce;
    RiskLevel risk;  // SAFE, CONTESTED_ADVANTAGE, CONTESTED_EQUAL, CONTESTED_DISADVANTAGE
};
```

### The Core Algorithm (Pseudocode)

```
1. For each player (us and enemies):
     For each leader:
       reachable = getReachableTerritories(leader)
       store in playerReachabilityMap

2. For each territory in ourReachabilityMap:
     enemyCanReach = check if in any enemy's reachability
     if (!enemyCanReach):
       risk = SAFE
     else:
       compare forces -> risk level
```

---

## Part 2: Long-Range Strategic Planning System (Future Implementation)

This is the full strategic AI system to implement after Part 1 is working.

### Phase 1: Reachability Map (Per Player)

For each player, calculate:
```
Territory -> List of (Leader, TroopsWithLeader, MovesRemaining)
```

**Inputs:**
- All leaders (Caesar, Generals) and their current positions
- Each leader's legion (troops traveling with them)
- Road network (owned cities)
- Galleys and their positions

**Output:**
- Every territory that player can reach this turn
- Which leaders can reach it
- What force each leader brings

### Phase 2: Force Projection Map

Aggregate Phase 1 into:
```
Territory -> MaxForceWeCanBring
```

Where "force" could be measured as:
- Simple troop count (Infantry + Cavalry + Catapults)
- Weighted combat strength (Cavalry worth more than Infantry?)
- Include leader bonuses?

### Phase 3: Enemy Threat Map

Run Phase 1 & 2 for **every other player**:
```
Territory -> MaxEnemyForce (worst case across all enemies)
```

### Phase 4: Risk Assessment

For each territory we can reach, calculate:
```
Risk Level = Compare(OurMaxForce, EnemyMaxForce)
```

**Risk Categories:**

| Our Force | Enemy Force | Risk Level | Recommendation |
|-----------|-------------|------------|----------------|
| Any | 0 | **Safe** | Take with 1 general, no troops needed |
| Superior | Some | **Low** | Attack with advantage |
| Equal | Equal | **Medium** | Only if territory is valuable |
| Inferior | Superior | **High** | Avoid or bring full army |
| 0 | Any | **Unreachable** | N/A |

### Phase 5: Strategic Decision Making

Using the risk map, AI prioritizes:

1. **Safe high-value territories** - Easy expansion, grab first
2. **Low-risk valuable territories** - Attack with confidence
3. **Defensive positioning** - Don't leave generals exposed in high-risk zones
4. **Force concentration** - If attacking contested territory, bring enough troops

---

## Movement Considerations

### Leaders (Caesar/Generals)
- Base movement: 2 land territories per turn
- Can use roads: instant travel between owned cities connected by roads
- Can board/disembark galleys for naval transport

### Galleys
- Move 2 sea zones per turn
- Can pick up a leader (costs 0.5 moves for galley)
- Can drop off a leader to adjacent land
- Beaching rules (can only beach in certain territories)

### Troops (Infantry/Cavalry/Catapults)
- Don't move independently - they travel WITH leaders in legions
- So troop reachability = leader reachability

### Road Network Bonus
- If you have cities connected by roads, a leader can traverse the entire road network as 1 move
- This dramatically extends reach

---

## Future Considerations (Not Yet Planned)

These items are noted but not part of the current plan:

1. **Multi-turn lookahead** - Enemy can't reach Roma this turn, but could next turn
2. **Territory valuation** - Tax value vs strategic position (chokepoints, road connections)
3. **Naval threat assessment** - Galley invasion potential
4. **Fortification effects** - Fortified cities are harder to take
5. **Battle outcome prediction** - Win/lose probability and expected troop losses (separate plan exists)

---

## Implementation Order

1. Implement `getReachableTerritories(leader)` function
2. Build reachability map for current player
3. Build reachability map for all enemy players
4. Create risk classification for each reachable territory
5. Integrate into AI decision-making loop

---

*Created: November 2024*
*Status: Planning Phase*
