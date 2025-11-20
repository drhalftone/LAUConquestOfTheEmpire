#include <QApplication>
#include <QDebug>
#include "mapwidget.h"
#include "mapgraph.h"
#include "common.h"

// Test function to verify graph neighbors match grid neighbors
bool testGraphNeighbors()
{
    qDebug() << "=== Starting Graph Neighbor Verification Test ===\n";

    // Create a MapWidget (this will generate random map and build graph)
    MapWidget *mapWidget = new MapWidget();

    qDebug() << "Map generated with" << mapWidget->getGraph()->territoryCount() << "territories";
    qDebug() << "Grid size:" << MapWidget::ROWS << "rows x" << MapWidget::COLUMNS << "columns";
    qDebug() << "Expected territories:" << (MapWidget::ROWS * MapWidget::COLUMNS) << "\n";

    bool allTestsPassed = true;
    int totalTests = 0;
    int passedTests = 0;
    int failedTests = 0;

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

            // Compare the two lists
            bool neighborsMatch = true;

            // Check if counts match
            if (graphNeighbors.size() != expectedNeighbors.size()) {
                qDebug() << "FAIL: Territory" << territoryName << "at (" << row << "," << col << ")";
                qDebug() << "  Expected" << expectedNeighbors.size() << "neighbors, got" << graphNeighbors.size();
                neighborsMatch = false;
            }

            // Check if all expected neighbors are in graph neighbors
            for (const QString &expectedNeighbor : expectedNeighbors) {
                if (!graphNeighbors.contains(expectedNeighbor)) {
                    if (neighborsMatch) {  // Only print header once
                        qDebug() << "FAIL: Territory" << territoryName << "at (" << row << "," << col << ")";
                    }
                    qDebug() << "  Missing expected neighbor:" << expectedNeighbor;
                    neighborsMatch = false;
                }
            }

            // Check if graph has any unexpected neighbors
            for (const QString &graphNeighbor : graphNeighbors) {
                if (!expectedNeighbors.contains(graphNeighbor)) {
                    if (neighborsMatch) {  // Only print header once
                        qDebug() << "FAIL: Territory" << territoryName << "at (" << row << "," << col << ")";
                    }
                    qDebug() << "  Unexpected graph neighbor:" << graphNeighbor;
                    neighborsMatch = false;
                }
            }

            if (neighborsMatch) {
                passedTests++;
            } else {
                failedTests++;
                allTestsPassed = false;

                // Print details for failed test
                qDebug() << "  Expected neighbors:" << expectedNeighbors;
                qDebug() << "  Graph neighbors:   " << graphNeighbors;
                qDebug() << "";
            }
        }
    }

    // Print summary
    qDebug() << "\n=== Test Summary ===";
    qDebug() << "Total territories tested:" << totalTests;
    qDebug() << "Passed:" << passedTests;
    qDebug() << "Failed:" << failedTests;
    qDebug() << "Success rate:" << QString::number(100.0 * passedTests / totalTests, 'f', 1) << "%";

    if (allTestsPassed) {
        qDebug() << "\n✓ ALL TESTS PASSED - Graph neighbors match grid neighbors perfectly!";
    } else {
        qDebug() << "\n✗ SOME TESTS FAILED - Graph neighbors do not match grid neighbors";
    }

    delete mapWidget;

    return allTestsPassed;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Run the test
    bool success = testGraphNeighbors();

    // Return 0 if all tests passed, 1 if any failed
    return success ? 0 : 1;
}
