# Move Enumeration Feature Plan

**Status: IMPLEMENTED** - See `ai/moveenumerator.h` and `ai/moveenumerator.cpp`

## Overview

This plan describes a system to enumerate all possible moves that a player could make on a given turn. The output is a list of **move pairs** for each general, where each pair represents two transitions (source → sink, source → sink).

## Key Concepts

### Move Representation
Each general has **2 move actions** per turn. A complete general move is represented as a **pair of transitions**:

```
GeneralMove = (Move1, Move2)
where:
  Move1 = (source1, sink1)
  Move2 = (source2, sink2)

Constraint: source2 == sink1 (you start move 2 where move 1 ended)
```

### Examples
For a general starting at "Macedonia":
- **Stay both moves**: `(Macedonia → Macedonia, Macedonia → Macedonia)`
- **Move once, then stay**: `(Macedonia → Moesia, Moesia → Moesia)`
- **Move twice same direction**: `(Macedonia → Moesia, Moesia → Dacia)`
- **Move and return**: `(Macedonia → Moesia, Moesia → Macedonia)`

## Data Structures

### New Structs (in ai/moveenumerator.h)

```cpp
/**
 * @brief A single transition from one territory to another (or same)
 */
struct Transition {
    QString source;
    QString sink;

    bool isStay() const { return source == sink; }
};

/**
 * @brief A complete move for one general (2 transitions)
 */
struct GeneralMove {
    GamePiece *general = nullptr;
    Transition move1;
    Transition move2;

    // Derived info
    QString startingTerritory() const { return move1.source; }
    QString endingTerritory() const { return move2.sink; }
    int totalMoves() const; // 0, 1, or 2 (count non-stay moves)
};

/**
 * @brief All possible moves for a single general
 */
struct GeneralMoveSet {
    GamePiece *general = nullptr;
    QString startTerritory;
    QList<GeneralMove> possibleMoves;

    int count() const { return possibleMoves.size(); }
};

/**
 * @brief Complete enumeration of all possible moves for a player's turn
 */
struct TurnMoveEnumeration {
    Player *player = nullptr;
    QList<GeneralMoveSet> generalMoveSets;  // One per general

    // Statistics
    int totalGenerals() const;
    int totalMoveCount() const;  // Sum of all possible moves across generals
};
```

## Class Design

### MoveEnumerator (ai/moveenumerator.h, ai/moveenumerator.cpp)

```cpp
class MoveEnumerator {
public:
    MoveEnumerator();

    /**
     * @brief Enumerate all possible moves for a single general
     * @param general The general to enumerate moves for
     * @param player The player who owns the general
     * @param graph The map graph
     * @return GeneralMoveSet with all possible (move1, move2) pairs
     */
    GeneralMoveSet enumerateGeneralMoves(
        GamePiece *general,
        Player *player,
        MapGraph *graph
    );

    /**
     * @brief Enumerate all possible moves for all of a player's generals
     * @param player The player
     * @param graph The map graph
     * @return TurnMoveEnumeration containing all generals' move options
     */
    TurnMoveEnumeration enumerateAllMoves(
        Player *player,
        MapGraph *graph
    );

    /**
     * @brief Get territories reachable in exactly 1 move from a territory
     * Considers: land adjacency, galley transport, road network
     * @return List of reachable territory names (includes staying in place)
     */
    QStringList getReachableIn1Move(
        const QString &from,
        Player *player,
        MapGraph *graph
    );

private:
    /**
     * @brief Get adjacent land territories (basic adjacency)
     */
    QStringList getAdjacentLandTerritories(
        const QString &from,
        MapGraph *graph
    );

    /**
     * @brief Get territories reachable via galley this turn
     */
    QStringList getGalleyReachableFrom(
        const QString &from,
        Player *player,
        MapGraph *graph
    );

    /**
     * @brief Get territories reachable via road network
     */
    QStringList getRoadReachableFrom(
        const QString &from,
        Player *player,
        MapGraph *graph
    );
};
```

## Algorithm

### enumerateGeneralMoves(general, player, graph)

```
1. Get starting territory: start = general->getTerritory()

2. Get all territories reachable in 1 move from start:
   reachable1 = getReachableIn1Move(start, player, graph)
   (includes staying at start itself)

3. For each possible move1 destination (d1) in reachable1:
   a. Get all territories reachable in 1 move from d1:
      reachable2 = getReachableIn1Move(d1, player, graph)

   b. For each possible move2 destination (d2) in reachable2:
      Create GeneralMove:
        move1 = (start, d1)
        move2 = (d1, d2)
      Add to result list

4. Return GeneralMoveSet with all moves
```

### getReachableIn1Move(from, player, graph)

```
1. Start with result = { from }  // Can always stay

2. Add adjacent land territories:
   for each neighbor in graph->getNeighbors(from):
     if graph->isLandTerritory(neighbor):
       result.add(neighbor)

3. Add road-reachable territories:
   for each roadDest in graph->getRoadConnectedTerritories(from, player):
     result.add(roadDest)

4. Add galley-reachable territories:
   for each galley owned by player:
     if galley can transport from 'from' territory:
       for each destination galley can reach:
         result.add(destination)

5. Return unique list (deduplicated)
```

## Move Count Analysis

For a general with N reachable territories (including staying):
- Move 1 options: N
- Move 2 options: varies by where Move 1 ends (call it M for each destination)
- Total moves: Σ(M_i) for i in 1..N

Worst case (well-connected territory):
- If each territory has ~6 neighbors: roughly 6 * 6 = 36 moves per general
- With galleys/roads: could be higher

Expected typical range: 20-50 moves per general

## Special Cases to Handle

### 1. Galley Transport
- Galley can move 2 sea zones, so it can drop off at various coastal territories
- Need to check which galleys the player owns and where they can reach
- A general using galley for move1 might end up at a coastal territory far from start

### 2. Road Network
- Roads let a general move between cities in one move (any distance)
- Only works if player owns cities in both source and destination
- Road move counts as 1 move, can do normal move afterward

### 3. Sea Territories
- Generals cannot move to sea territories (only galleys can)
- Filter out sea neighbors when enumerating moves

### 4. Enemy Presence
- For enumeration purposes, we list ALL possible moves
- Whether a move is legal (blocked by enemy) is a separate concern
- The enumerator just lists physical possibilities

## Integration Points

### With AIDecisionMaker
The enumerator provides raw move options. AIDecisionMaker can:
1. Call `enumerateAllMoves()` to get all possibilities
2. Score each GeneralMove based on:
   - Final territory value
   - Threat assessment
   - Strategic goals
3. Select optimal moves

### Future: Minimax/Game Tree
Move enumeration is a prerequisite for:
- Game tree search (look-ahead AI)
- Minimax with alpha-beta pruning
- Monte Carlo Tree Search (MCTS)

## File Structure

```
ai/
  moveenumerator.h    - New header with structs and class
  moveenumerator.cpp  - Implementation
  aidecisionmaker.h   - No changes needed initially
  aidecisionmaker.cpp - Can optionally use enumerator later
```

## Testing Approach

1. **Unit tests**: Verify move counts for known positions
2. **Edge cases**:
   - General with no valid moves (surrounded by sea)
   - General at territory with galley access
   - General at city with road connections
3. **Performance**: Ensure enumeration completes quickly (<10ms per general)

## Implementation Steps

1. Create `ai/moveenumerator.h` with structs and class declaration
2. Create `ai/moveenumerator.cpp` with implementation
3. Add to CMakeLists.txt / .pro file
4. Test with sample positions
5. (Future) Integrate with AIDecisionMaker for improved planning

## Troop Movement

Troops move by "riding the train" - they hop on a general's transition. They don't need to track roads/galleys themselves; they just follow the general.

### Infantry and Catapults (1 transition per turn)

**Options:**
1. **Stay in place**: `(T → T)` - always valid
2. **Ride a general**: Any general transition (move1 OR move2) where `source == troop's territory`

**Algorithm:**
```
For each infantry/catapult at territory T:
  transitions = [(T, T)]  // staying is always an option

  For each general's move:
    if move1.source == T:
      transitions.add(move1)
    if move2.source == T:
      transitions.add(move2)

  return unique(transitions)
```

**Example - Infantry at Macedonia:**
- General's move: `(Macedonia → Moesia, Moesia → Dacia)`
- Infantry options:
  1. `(Macedonia → Macedonia)` - stay ✓
  2. `(Macedonia → Moesia)` - ride general's move1 ✓

**Example - Infantry at Moesia:**
- General's move: `(Macedonia → Moesia, Moesia → Dacia)`
- Infantry options:
  1. `(Moesia → Moesia)` - stay ✓
  2. `(Moesia → Dacia)` - ride general's move2 ✓

### Cavalry (2 transitions per turn)

Cavalry make **independent choices** at each transition:

**Transition 1 options:**
- Stay in place: `(T → T)`
- Ride any general's **move1** where `move1.source == T`

**Transition 2 options:**
- Stay in place: `(T2 → T2)` where T2 is where cavalry ended after transition 1
- Ride any general's **move2** where `move2.source == T2`

**Algorithm:**
```
For each cavalry at territory T:
  // Build transition1 options
  trans1_options = [(T, T)]  // stay
  For each general's move:
    if move1.source == T:
      trans1_options.add(move1)

  // For each transition1 outcome, build transition2 options
  result = []
  For each t1 in trans1_options:
    T2 = t1.sink  // where cavalry ends up after transition 1

    trans2_options = [(T2, T2)]  // stay
    For each general's move:
      if move2.source == T2:
        trans2_options.add(move2)

    For each t2 in trans2_options:
      result.add((t1, t2))

  return result
```

**Example - Cavalry at Macedonia:**
- General's move: `(Macedonia → Moesia, Moesia → Dacia)`
- Cavalry options:
  1. `(Macedonia → Macedonia, Macedonia → Macedonia)` - stay both ✓
  2. `(Macedonia → Moesia, Moesia → Moesia)` - ride move1, stay move2 ✓
  3. `(Macedonia → Moesia, Moesia → Dacia)` - ride both ✓

**Example - Cavalry at Macedonia with general staying:**
- General's move: `(Macedonia → Macedonia, Macedonia → Thessalonica)`
- Cavalry options:
  1. `(Macedonia → Macedonia, Macedonia → Macedonia)` - stay both ✓
  2. `(Macedonia → Macedonia, Macedonia → Thessalonica)` - stay move1, ride move2 ✓

### Data Structures for Troops

```cpp
/**
 * @brief A troop's possible moves (1 transition for infantry/catapult)
 */
struct TroopMove {
    GamePiece *troop = nullptr;
    Transition transition;

    QString startingTerritory() const { return transition.source; }
    QString endingTerritory() const { return transition.sink; }
};

/**
 * @brief A cavalry's possible moves (2 transitions)
 */
struct CavalryMove {
    GamePiece *cavalry = nullptr;
    Transition transition1;
    Transition transition2;

    QString startingTerritory() const { return transition1.source; }
    QString endingTerritory() const { return transition2.sink; }
};

/**
 * @brief All possible moves for a single troop
 */
struct TroopMoveSet {
    GamePiece *troop = nullptr;
    QString startTerritory;
    QList<TroopMove> possibleMoves;      // For infantry/catapults
    QList<CavalryMove> possibleCavalryMoves;  // For cavalry

    bool isCavalry() const;
    int count() const;
};
```

## Validation: Generals Need Troop Escort

A general **cannot** enter unclaimed or enemy territory alone. At least one troop must accompany them on that transition. This requires validating general moves and potentially removing invalid ones.

### Escort Rules

**For move1 into unclaimed/enemy territory:**
- Valid if: Any troop at move1.source can ride move1

**For move2 into unclaimed/enemy territory:**
- Valid if:
  - **Cavalry** that rode move1 (started at move1.source, now at move2.source) can ride move2, OR
  - **Any troop** originally at move2.source can ride move2

Note: Infantry/catapults that rode move1 cannot escort move2 (they only have 1 transition).

### Validation Algorithm

```
Step 1: Validate move2 transitions

  For each general move pair (move1, move2):
    If move2.sink is unclaimed or enemy territory:

      escortAvailable = false

      // Check cavalry that rode move1 and can continue
      For each cavalry at move1.source:
        If cavalry can ride move1 AND cavalry can ride move2:
          escortAvailable = true
          break

      // Check troops originally at move2.source (intermediate territory)
      If not escortAvailable:
        For each troop at move2.source:
          If troop can ride move2:
            escortAvailable = true
            break

      If not escortAvailable:
        // Replace move2 with stay-in-place
        move2 = (move2.source → move2.source)


Step 2: Validate move1 transitions

  For each general move pair (move1, move2):
    If move1.sink is unclaimed or enemy territory:

      escortAvailable = false

      For each troop at move1.source:
        If troop can ride move1:
          escortAvailable = true
          break

      If not escortAvailable:
        // Remove entire move pair - it's invalid
        Remove (move1, move2) from general's possible moves


Step 3: Cascade cleanup of troop transitions

  For each troop:
    For each troop transition:
      If transition references a general move that was removed:
        Remove this troop transition
```

### Example 1: No troops at general's location

- General at Roma, no troops at Roma
- Gallia is enemy territory
- General's move: `(Roma → Gallia, Gallia → Hispania)`

**Validation:**
- Move1 `Roma → Gallia` goes to enemy territory
- No troops at Roma can ride move1
- **Remove entire move pair**

### Example 2: Infantry can escort move1 only

- General at Roma, infantry at Roma
- Gallia and Hispania are enemy territories
- General's move: `(Roma → Gallia, Gallia → Hispania)`

**Validation:**
- Move2 `Gallia → Hispania` goes to enemy territory
  - Cavalry at Roma that rides move1? No cavalry at Roma
  - Troops originally at Gallia? None
  - **No escort for move2** → Replace with `(Gallia → Gallia)`
- Move1 `Roma → Gallia` goes to enemy territory
  - Infantry at Roma can ride move1
  - **Valid**

**Result:** `(Roma → Gallia, Gallia → Gallia)` - general moves to Gallia with infantry, then stays

### Example 3: Cavalry can escort both moves

- General at Roma, cavalry at Roma
- Gallia and Hispania are enemy territories
- General's move: `(Roma → Gallia, Gallia → Hispania)`

**Validation:**
- Move2 `Gallia → Hispania` goes to enemy territory
  - Cavalry at Roma can ride move1 to Gallia
  - That same cavalry can then ride move2 to Hispania
  - **Valid**
- Move1 `Roma → Gallia` goes to enemy territory
  - Cavalry at Roma can ride move1
  - **Valid**

**Result:** `(Roma → Gallia, Gallia → Hispania)` - remains valid

### Example 4: Troops at intermediate territory

- General at Roma, no troops at Roma, infantry at Gallia
- Gallia is friendly, Hispania is enemy
- General's move: `(Roma → Gallia, Gallia → Hispania)`

**Validation:**
- Move2 `Gallia → Hispania` goes to enemy territory
  - Cavalry riding move1? No cavalry at Roma
  - Troops at Gallia? Yes, infantry at Gallia can ride move2
  - **Valid**
- Move1 `Roma → Gallia` goes to friendly territory
  - **No escort needed**

**Result:** `(Roma → Gallia, Gallia → Hispania)` - general picks up infantry at Gallia

## Two-Turn Projection

For looking ahead 2 turns, we extend the model to have **4 slots** for generals and corresponding troop movement rules.

### Slot Structure

```
Turn 1: Slot 1 (move1), Slot 2 (move2)
Turn 2: Slot 3 (move3), Slot 4 (move4)

Constraints:
  - slot2.source == slot1.sink (within turn 1)
  - slot3.source == slot2.sink (turn boundary - start turn 2 where turn 1 ended)
  - slot4.source == slot3.sink (within turn 2)
```

### General Movement (4 transitions)

A general's 2-turn move is represented as 4 chained transitions:

```cpp
struct GeneralMove2Turn {
    GamePiece *general = nullptr;
    Transition slot1;  // Turn 1, move 1
    Transition slot2;  // Turn 1, move 2
    Transition slot3;  // Turn 2, move 1
    Transition slot4;  // Turn 2, move 2

    QString startingTerritory() const { return slot1.source; }
    QString endOfTurn1Territory() const { return slot2.sink; }
    QString endingTerritory() const { return slot4.sink; }
};
```

**Example - General at Macedonia over 2 turns:**
- `(Macedonia → Moesia, Moesia → Dacia, Dacia → Dacia, Dacia → Thracia)`
  - Turn 1: Move to Moesia, then Dacia
  - Turn 2: Stay at Dacia, then move to Thracia

### Infantry/Catapult Movement (2 transitions over 2 turns)

Infantry and catapults get **1 transition per turn**, so they have 2 transitions total:

```cpp
struct InfantryMove2Turn {
    GamePiece *troop = nullptr;
    Transition turn1;  // Can ride ONE of slots 1-2
    Transition turn2;  // Can ride ONE of slots 3-4

    QString startingTerritory() const { return turn1.source; }
    QString endOfTurn1Territory() const { return turn1.sink; }
    QString endingTerritory() const { return turn2.sink; }
};
```

**Rules:**
- **Turn 1**: Infantry can ride slot 1 OR slot 2 (whichever starts at their territory)
- **Turn 2**: Infantry can ride slot 3 OR slot 4 (whichever starts at their end-of-turn-1 territory)
- Infantry can always stay (transition to same territory)

**Algorithm:**
```
For infantry at territory T:

  // Turn 1 options: stay OR ride slot1 OR ride slot2
  turn1_options = [(T, T)]  // stay
  For each general's 2-turn move:
    if slot1.source == T:
      turn1_options.add(slot1)
    if slot2.source == T:
      turn1_options.add(slot2)

  // For each turn1 outcome, build turn2 options
  result = []
  For each t1 in turn1_options:
    T2 = t1.sink  // where infantry is at start of turn 2

    turn2_options = [(T2, T2)]  // stay
    For each general's 2-turn move:
      // Only consider generals whose slot2.sink == T2 (they're at same spot)
      if slot2.sink == T2:
        if slot3.source == T2:
          turn2_options.add(slot3)
        if slot4.source == T2:
          turn2_options.add(slot4)

    For each t2 in turn2_options:
      result.add((t1, t2))

  return unique(result)
```

**Example - Infantry at Macedonia:**
- General's 2-turn move: `(Macedonia → Moesia, Moesia → Dacia, Dacia → Thracia, Thracia → Byzantium)`
- Infantry options:
  1. Stay both turns: `(Macedonia → Macedonia, Macedonia → Macedonia)`
  2. Ride slot1, stay turn 2: `(Macedonia → Moesia, Moesia → Moesia)`
  3. Ride slot1, ride slot3: `(Macedonia → Moesia, Moesia → Dacia)` - WAIT, this doesn't work!
     - Infantry rode slot1 to Moesia
     - But slot3 starts at Dacia (where general is after turn 1)
     - Infantry is at Moesia, not Dacia
     - **Infantry cannot ride slot3 because they're not at slot3.source**

**Key insight**: Infantry can only ride turn 2 slots if they ended turn 1 at the same place as the general. This happens when:
- Infantry rode slot2 (ending at slot2.sink = slot3.source), OR
- Infantry was already at slot2.sink and stayed

### Cavalry Movement (4 transitions over 2 turns)

Cavalry get **2 transitions per turn**, so they have 4 transitions total - one for each slot:

```cpp
struct CavalryMove2Turn {
    GamePiece *cavalry = nullptr;
    Transition slot1;  // Turn 1, move 1
    Transition slot2;  // Turn 1, move 2
    Transition slot3;  // Turn 2, move 1
    Transition slot4;  // Turn 2, move 2

    QString startingTerritory() const { return slot1.source; }
    QString endOfTurn1Territory() const { return slot2.sink; }
    QString endingTerritory() const { return slot4.sink; }
};
```

**Rules:**
- Each slot: cavalry can ride (if at slot's source) OR stay
- Cavalry makes independent choice at each slot
- Must track position through all 4 slots

**Algorithm:**
```
For cavalry at territory T:

  // Slot 1 options
  slot1_options = [(T, T)]  // stay
  For each general's 2-turn move:
    if slot1.source == T:
      slot1_options.add(slot1)

  result = []
  For each s1 in slot1_options:
    T1 = s1.sink

    // Slot 2 options
    slot2_options = [(T1, T1)]  // stay
    For each general's 2-turn move:
      if slot2.source == T1:
        slot2_options.add(slot2)

    For each s2 in slot2_options:
      T2 = s2.sink  // End of turn 1 position

      // Slot 3 options
      slot3_options = [(T2, T2)]  // stay
      For each general's 2-turn move:
        // Only generals who ended turn 1 at T2
        if slot2.sink == T2 AND slot3.source == T2:
          slot3_options.add(slot3)

      For each s3 in slot3_options:
        T3 = s3.sink

        // Slot 4 options
        slot4_options = [(T3, T3)]  // stay
        For each general's 2-turn move:
          if slot4.source == T3:
            slot4_options.add(slot4)

        For each s4 in slot4_options:
          result.add((s1, s2, s3, s4))

  return unique(result)
```

### Escort Validation for 2 Turns

The escort rules extend to all 4 slots:

**Slot 1 into enemy/unclaimed:**
- Need any troop at slot1.source

**Slot 2 into enemy/unclaimed:**
- Need cavalry that rode slot1 (now at slot2.source), OR
- Need any troop originally at slot2.source

**Slot 3 into enemy/unclaimed:**
- Need any troop at slot3.source at start of turn 2
- This includes:
  - Cavalry that ended turn 1 at slot3.source (rode slot1 and/or slot2)
  - Infantry/catapult that ended turn 1 at slot3.source (rode slot1 or slot2)
  - Troops that were already at slot3.source and stayed turn 1

**Slot 4 into enemy/unclaimed:**
- Need cavalry that rode slot3 (now at slot4.source), OR
- Need any troop at slot4.source that can ride slot4
  - Infantry/catapult that used their turn2 transition to get to slot4.source via slot3
  - Troops already at slot4.source

### Data Structures for 2-Turn Projection

```cpp
/**
 * @brief A general's 2-turn move (4 transitions)
 */
struct GeneralMove2Turn {
    GamePiece *general = nullptr;
    Transition slot1;
    Transition slot2;
    Transition slot3;
    Transition slot4;

    QString startingTerritory() const { return slot1.source; }
    QString endOfTurn1() const { return slot2.sink; }
    QString endingTerritory() const { return slot4.sink; }
};

/**
 * @brief Infantry/catapult 2-turn move (2 transitions)
 */
struct InfantryMove2Turn {
    GamePiece *troop = nullptr;
    Transition turn1;
    Transition turn2;

    QString startingTerritory() const { return turn1.source; }
    QString endOfTurn1() const { return turn1.sink; }
    QString endingTerritory() const { return turn2.sink; }
};

/**
 * @brief Cavalry 2-turn move (4 transitions)
 */
struct CavalryMove2Turn {
    GamePiece *cavalry = nullptr;
    Transition slot1;
    Transition slot2;
    Transition slot3;
    Transition slot4;

    QString startingTerritory() const { return slot1.source; }
    QString endOfTurn1() const { return slot2.sink; }
    QString endingTerritory() const { return slot4.sink; }
};

/**
 * @brief Complete 2-turn enumeration
 */
struct TwoTurnMoveEnumeration {
    Player *player = nullptr;
    QList<GeneralMoveSet2Turn> generalMoveSets;
    QList<InfantryMoveSet2Turn> infantryMoveSets;
    QList<CavalryMoveSet2Turn> cavalryMoveSets;

    QMap<QString, int> getMaxReachabilityMap2Turn() const;
};
```

### Complexity Analysis

For 2-turn projection:
- Generals: If N destinations per move, then N^4 combinations per general
  - With ~6 neighbors: 6^4 = 1,296 combinations per general
  - With 6 generals: ~7,776 general move combinations

- Infantry: For each of ~N turn1 options, ~N turn2 options = N^2
  - Much smaller because they can only ride slots where they're at the source

- Cavalry: N^4 like generals, but filtered by position matching

**Optimization needed**: Build lookup maps like we did for 1-turn to avoid O(n^2) loops.

### Implementation Steps

1. Add `GeneralMove2Turn`, `InfantryMove2Turn`, `CavalryMove2Turn` structs
2. Add `enumerateGeneralMoves2Turn()` - enumerate all 4-slot combinations
3. Add `enumerateTroopMoves2Turn()` - build troop moves from general moves
4. Add `validateGeneralMoves2Turn()` - escort validation for all 4 slots
5. Add `getMaxReachabilityMap2Turn()` - count troops reaching each territory
6. Update heat map to use 2-turn projection

## Implementation Status

The following has been implemented in `ai/moveenumerator.h` and `ai/moveenumerator.cpp`:

### Completed ✓
- [x] Core data structures (Transition, GeneralMove, TroopMove, CavalryMove, GalleyMove)
- [x] `enumerateGeneralMoves()` - enumerate all 2-transition move pairs for generals
- [x] `enumerateTroopMoves()` - infantry/catapult single transitions
- [x] `enumerateCavalryMoves()` - cavalry 2-transition moves
- [x] `enumerateGalleyMoves()` - galley movement including troops aboard
- [x] Road network support
- [x] Galley transport support
- [x] Escort validation (generals need troops for enemy/unclaimed territories)
- [x] 2-turn projection (`enumerateGeneralMoves2Turn()`, `getMaxReachabilityMap2Turn()`)
- [x] Heat map integration for force/threat projection
- [x] `MoveEnumeratorWidget` for visualization/debugging

### Open Questions

1. **Opponent moves**: Should we enumerate opponent moves for threat analysis?
2. **Combined moves**: Should we enumerate combinations of all generals' moves together? (Exponential in number of generals)
3. **Performance**: 2-turn projection has O(N^4) complexity - may need caching or lazy evaluation
