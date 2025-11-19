# Conquest of the Empire

A Qt C++ implementation of the classic board game "Conquest of the Empire" (1984 edition).

## About

This is a digital recreation of the Milton Bradley board game from 1984. The game simulates ancient Roman conquest with players commanding armies across territories, managing resources, and engaging in strategic warfare.

## Features

- 6-player support with distinct colors
- Interactive game board with 12x8 grid
- Multiple piece types: Caesar, Generals, Infantry, Cavalry, Catapults, and Galleys
- Building system: Cities and Roads
- Economic system with taxes and purchasing
- Road network for faster movement
- City destruction mechanics
- Fortification system
- Territory ownership and conquest
- Save/load game functionality

## Building

### Requirements
- Qt 5.x or Qt 6.x
- C++ compiler (MSVC, GCC, or Clang)
- qmake or CMake

### Compilation
```bash
qmake ConquestOfTheEmpire.pro
make  # or nmake on Windows with MSVC
```

## How to Play

1. Each player starts with Caesar, 6 Generals, 4 Infantry, and a fortified city at their home province
2. Players take turns moving pieces, conquering territories, and managing resources
3. Collect taxes from owned territories at the end of each turn
4. Purchase additional units and buildings
5. Use roads to move leaders quickly across connected cities
6. Destroy your own cities to prevent enemy capture

## Game Mechanics

- **Movement**: Leaders (Caesar/Generals) have 2 movement points per turn
- **Roads**: Movement via roads only costs 1 movement point total
- **Cities**: Provide economic value and can be fortified for defense
- **Territories**: Each has a name and tax value (5 or 10 talents)
- **Combat**: Territories can have combat indicators for battles

## Development

Built with Qt C++ using:
- Model-View architecture
- Custom painting for game pieces
- Drag-and-drop support
- Context menus for piece actions
- Dialog-based UI for purchases and city destruction

## License

This is a fan implementation for educational purposes. The original "Conquest of the Empire" game is owned by its respective copyright holders.
