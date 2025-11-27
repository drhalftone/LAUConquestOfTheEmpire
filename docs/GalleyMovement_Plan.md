# Galley Movement Implementation Plan

## Original Rules Summary

### Rule 1: Legion Boarding/Disembarking Restriction
- A legion **cannot move before boarding** a galley - must already be in a territory adjacent to the sea zone with the galley
- A legion **cannot move after disembarking** - once off the galley, that's it for the turn
- Boarding → sailing → disembarking is the **entire movement** for that legion

### Rule 2: Galley 2-Movement System
Galleys have **2 movement points per turn**. Valid movement patterns:

1. **Coast → Sea → Sea**: Enter sea from coastal province, sail to adjacent sea zone (with/without legion)
2. **Sea → Sea → Coast**: Sail from one sea zone to adjacent sea zone, then land on coast (with/without legion)
3. **Sea → Coast → Same Sea** (pickup maneuver): Land on coast, pick up a legion, return to the **same** sea zone you came from
4. **Sea → Sea → Sea**: Sail through 2 sea zones. **Cannot land** if doing this.

Movement costs:
- Entering/exiting coast = 1 movement
- Moving between sea zones = 1 movement

### Rule 3: Overnight Coast-to-Coast Traversal
- If a galley ends its turn landed on a coast, next turn it can set sail from **any coast** of that province
- Effectively "crosses" the province overnight to reach the other side

**Example:**
- Turn 1: Hispania → Mare Beliaricum → land on **west** coast of Caesariensis
- Turn 2: Set sail from **east** coast of Caesariensis → Mare Numidia → land on Sicilia

**Exceptions (broken coastlines):**
- **Hispania**, **Italia**, **Macedonia** - cannot land on one coast then sail from a non-adjacent coast
- These provinces have coastlines separated by other provinces

### Rule 4: Same-Turn Docking Restriction
- If a galley lands on a coast **during a turn**, it can only set sail back into the **same** sea zone it came from
- Cannot land and then sail out to a different adjacent sea zone in the same turn

### Rule 5: No Coast-to-Coast Hopping
- Cannot move directly from one province's coast to an adjacent province's coast
- Must enter the sea zone first
- Example: From coast of Caesariensis → cannot go directly to coast of Numidia, must enter Mare Numidia first

### Rule 6: Mandatory Galley Crossings
These straits require galley transport - no land route exists:
- Baetica ↔ Tingitana
- Sardinia ↔ Corsica
- Thracia ↔ Asia

---

## Implementation Plan

### 1. Data Model Changes

**GalleyPiece** - add new state tracking:
- `m_movesRemaining` (0, 1, or 2)
- `m_lastSeaZone` - sea zone galley came from (for same-turn docking rule)
- `m_hasDockedThisTurn` - whether galley landed on coast this turn
- `m_dockedCoast` - which coast/province galley is docked at

**Legion/Leader tracking**:
- Track if legion has moved before attempting to board
- Track if legion has disembarked (prevent further movement)

### 2. Map Graph Changes

**Coastal adjacency data**:
- Define which sea zones are adjacent to which province coasts
- Define connected coasts within each province
- Mark Hispania/Italia/Macedonia as having broken coastlines

**Mandatory crossings**:
- Flag Baetica↔Tingitana, Sardinia↔Corsica, Thracia↔Asia as galley-only routes

### 3. Movement Validation Logic

**Galley movement validator** - enforce the 4 patterns:
- Coast → Sea → Sea (2 moves)
- Sea → Sea → Coast (2 moves)
- Sea → Coast → Same Sea (pickup, 2 moves)
- Sea → Sea (no landing allowed if 2 sea moves)

**Boarding validation**:
- Check legion has 0 moves used
- Check legion is adjacent to galley's sea zone

**Disembark validation**:
- Mark legion as "cannot move" after disembarking

**Coast traversal validation**:
- Same turn: can only return to same sea zone
- Next turn: can sail from any connected coast (except broken coastlines)

### 4. UI Changes

- Update movement highlighting to reflect valid galley destinations
- Show galley movement points remaining
- Prevent invalid boarding/disembarking actions

---

## Naval Combat Rules

### Differences from Land Combat

**1. No Retreats**
- Neither side can retreat in naval combat
- Combat continues until one side is eliminated

**2. Galleys as Combat Units**
- Galleys can attack and defend in naval combat
- Hit on rolls of 3, 4, 5, or 6 (no advantage modifiers apply)
- Combat continues as long as at least one galley remains on each side

**3. Troop Protection** (already implemented)
- Galleys cannot be targeted until all troops aboard are destroyed
- Generals don't count as "troops" - they can swim
- Once troops are eliminated, galley becomes targetable

### Galleys in Land Combat

**Docked Galleys are Passive:**
- Galleys docked on coastal provinces **cannot attack or defend** in land battles
- They are spectators during land combat
- If their side loses the land battle, **all docked galleys are automatically destroyed** after combat ends

### Implementation Notes

**Naval combat detection:**
- Check if combat territory is a sea zone
- If naval: disable retreat button
- If naval: allow galley attacks (not just troop attacks)
- Victory condition: one side has zero galleys remaining

**Land combat with docked galleys:**
- Galleys do not participate in combat
- After combat resolution, destroy all galleys belonging to losing side in that territory
