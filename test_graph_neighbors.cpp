#include <QApplication>
#include <QDebug>
#include "mapwidget.h"
#include "mapgraph.h"
#include "player.h"
#include "gamepiece.h"
#include "common.h"

// Helper function to get territory type as string
QString territoryTypeToString(TerritoryType type)
{
    switch (type) {
        case TerritoryType::Land: return "Land";
        case TerritoryType::Sea: return "Sea";
        case TerritoryType::Mountain: return "Mountain";
        case TerritoryType::Impassable: return "Impassable";
        default: return "Unknown";
    }
}

// Test function to verify graph neighbors match grid neighbors
bool testGraphNeighbors(bool verboseMode = false)
{
    qDebug() << "========================================";
    qDebug() << "  Graph Neighbor Verification Test";
    qDebug() << "========================================\n";

    // Create a MapWidget (this will generate random map and build graph)
    MapWidget *mapWidget = new MapWidget();

    qDebug() << "Map Configuration:";
    qDebug() << "  Grid size:" << MapWidget::ROWS << "rows x" << MapWidget::COLUMNS << "columns";
    qDebug() << "  Total territories:" << mapWidget->getGraph()->territoryCount();
    qDebug() << "  Land territories:" << mapWidget->getGraph()->countByType(TerritoryType::Land);
    qDebug() << "  Sea territories:" << mapWidget->getGraph()->countByType(TerritoryType::Sea);
    qDebug() << "";

    bool allTestsPassed = true;
    int totalTests = 0;
    int passedTests = 0;
    int failedTests = 0;

    // Statistics
    int landTerritories = 0;
    int seaTerritories = 0;
    int ownedTerritories = 0;
    int territoriesWithPieces = 0;

    // Type mismatch tracking
    int typeMismatches = 0;

    qDebug() << "========================================";
    qDebug() << "  Testing Territory Properties";
    qDebug() << "========================================\n";

    // Iterate through every grid position
    for (int row = 0; row < MapWidget::ROWS; ++row) {
        for (int col = 0; col < MapWidget::COLUMNS; ++col) {
            totalTests++;

            Position gridPos = {row, col};

            // Get territory name from grid position
            QString territoryName = mapWidget->positionToTerritoryName(gridPos);

            if (territoryName.isEmpty()) {
                qDebug() << "ERROR: No territory name for position (" << row << "," << col << ")";
                failedTests++;
                allTestsPassed = false;
                continue;
            }

            // === Get Territory Information ===

            // Territory type from grid
            bool isSeaGrid = mapWidget->isSeaTerritory(row, col);
            QString gridType = isSeaGrid ? "Sea" : "Land";

            // Territory type from graph
            TerritoryType graphType = mapWidget->getGraph()->getType(territoryName);
            QString graphTypeStr = territoryTypeToString(graphType);

            // Update statistics
            if (isSeaGrid) {
                seaTerritories++;
            } else {
                landTerritories++;
            }

            // Territory value
            int territoryValue = mapWidget->getTerritoryValueAt(row, col);

            // Ownership
            QChar owner = mapWidget->getTerritoryOwnerAt(row, col);
            QString ownerStr = (owner == '\0') ? "None" : QString(owner);
            if (owner != '\0') {
                ownedTerritories++;
            }

            // Count pieces (would need access to players to get actual pieces)
            // For now, just note if it has pieces based on ownership
            if (owner != '\0') {
                territoriesWithPieces++;
            }

            // Get neighbors from graph
            QList<QString> graphNeighbors = mapWidget->getGraph()->getNeighbors(territoryName);

            // Build expected neighbors from grid (4-directional)
            QList<QString> expectedNeighbors;

            // Up neighbor
            if (row > 0) {
                Position upPos = {row - 1, col};
                expectedNeighbors.append(mapWidget->positionToTerritoryName(upPos));
            }

            // Down neighbor
            if (row < MapWidget::ROWS - 1) {
                Position downPos = {row + 1, col};
                expectedNeighbors.append(mapWidget->positionToTerritoryName(downPos));
            }

            // Left neighbor
            if (col > 0) {
                Position leftPos = {row, col - 1};
                expectedNeighbors.append(mapWidget->positionToTerritoryName(leftPos));
            }

            // Right neighbor
            if (col < MapWidget::COLUMNS - 1) {
                Position rightPos = {row, col + 1};
                expectedNeighbors.append(mapWidget->positionToTerritoryName(rightPos));
            }

            // === Verification ===

            bool territoryPassed = true;
            QStringList issues;

            // Check if territory types match
            bool typeMatches = (isSeaGrid && graphType == TerritoryType::Sea) ||
                              (!isSeaGrid && graphType == TerritoryType::Land);
            if (!typeMatches) {
                issues << QString("Type mismatch: Grid=%1, Graph=%2").arg(gridType, graphTypeStr);
                territoryPassed = false;
                typeMismatches++;
            }

            // Check if neighbor counts match
            if (graphNeighbors.size() != expectedNeighbors.size()) {
                issues << QString("Neighbor count: Expected=%1, Got=%2")
                    .arg(expectedNeighbors.size())
                    .arg(graphNeighbors.size());
                territoryPassed = false;
            }

            // Check if all expected neighbors are in graph neighbors
            for (const QString &expectedNeighbor : expectedNeighbors) {
                if (!graphNeighbors.contains(expectedNeighbor)) {
                    issues << QString("Missing neighbor: %1").arg(expectedNeighbor);
                    territoryPassed = false;
                }
            }

            // Check if graph has any unexpected neighbors
            for (const QString &graphNeighbor : graphNeighbors) {
                if (!expectedNeighbors.contains(graphNeighbor)) {
                    issues << QString("Unexpected neighbor: %1").arg(graphNeighbor);
                    territoryPassed = false;
                }
            }

            // === Print Results ===

            if (!territoryPassed || verboseMode) {
                QString status = territoryPassed ? "[PASS]" : "[FAIL]";
                qDebug() << status << territoryName << "at (" << row << "," << col << ")";
                qDebug() << "  Grid Type:" << gridType;
                qDebug() << "  Graph Type:" << graphTypeStr;
                qDebug() << "  Territory Value:" << territoryValue;
                qDebug() << "  Owner:" << ownerStr;
                qDebug() << "  Neighbor Count:" << graphNeighbors.size() << "(expected:" << expectedNeighbors.size() << ")";

                // Print neighbor details
                qDebug() << "  Neighbors:";
                for (const QString &neighbor : graphNeighbors) {
                    // Get neighbor position and type
                    Position neighborPos = mapWidget->territoryNameToPosition(neighbor);
                    TerritoryType neighborType = mapWidget->getGraph()->getType(neighbor);
                    QString neighborTypeStr = territoryTypeToString(neighborType);
                    QString expectedMark = expectedNeighbors.contains(neighbor) ? "" : " [UNEXPECTED]";

                    qDebug() << "    -" << neighbor
                             << "(" << neighborPos.row << "," << neighborPos.col << ")"
                             << neighborTypeStr
                             << expectedMark;
                }

                // Print missing neighbors if any
                for (const QString &expected : expectedNeighbors) {
                    if (!graphNeighbors.contains(expected)) {
                        Position expectedPos = mapWidget->territoryNameToPosition(expected);
                        qDebug() << "    - [MISSING]" << expected
                                 << "(" << expectedPos.row << "," << expectedPos.col << ")";
                    }
                }

                if (!issues.isEmpty()) {
                    qDebug() << "  Issues:";
                    for (const QString &issue : issues) {
                        qDebug() << "    •" << issue;
                    }
                }

                qDebug() << "";
            }

            if (territoryPassed) {
                passedTests++;
            } else {
                failedTests++;
                allTestsPassed = false;
            }
        }
    }

    // === Print Summary ===

    qDebug() << "========================================";
    qDebug() << "  Test Results Summary";
    qDebug() << "========================================\n";

    qDebug() << "Territory Statistics:";
    qDebug() << "  Total territories:" << totalTests;
    qDebug() << "  Land territories:" << landTerritories;
    qDebug() << "  Sea territories:" << seaTerritories;
    qDebug() << "  Owned territories:" << ownedTerritories;
    qDebug() << "  Territories with pieces:" << territoriesWithPieces;
    qDebug() << "";

    qDebug() << "Test Results:";
    qDebug() << "  Passed:" << passedTests;
    qDebug() << "  Failed:" << failedTests;
    qDebug() << "  Success rate:" << QString::number(100.0 * passedTests / totalTests, 'f', 1) << "%";
    qDebug() << "";

    if (typeMismatches > 0) {
        qDebug() << "Issues Found:";
        qDebug() << "  Type mismatches (Grid vs Graph):" << typeMismatches;
        qDebug() << "";
    }

    if (allTestsPassed) {
        qDebug() << "✓ ✓ ✓ ALL TESTS PASSED ✓ ✓ ✓";
        qDebug() << "Graph neighbors match grid neighbors perfectly!";
        qDebug() << "Territory types match between grid and graph!";
    } else {
        qDebug() << "✗ ✗ ✗ TESTS FAILED ✗ ✗ ✗";
        qDebug() << "Issues detected in graph/grid synchronization.";
    }

    qDebug() << "";
    qDebug() << "========================================\n";

    delete mapWidget;

    return allTestsPassed;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Check for verbose flag
    bool verbose = false;
    if (argc > 1 && QString(argv[1]) == "-v") {
        verbose = true;
        qDebug() << "Running in VERBOSE mode (showing all territories)\n";
    } else {
        qDebug() << "Running in NORMAL mode (showing only failures)";
        qDebug() << "Use '-v' flag for verbose output (show all territories)\n";
    }

    // Run the test
    bool success = testGraphNeighbors(verbose);

    // Return 0 if all tests passed, 1 if any failed
    return success ? 0 : 1;
}
