#include <QApplication>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <ctime>
#include "quickbattlesplash.h"
#include "purchasedialog.h"
#include "combatdialog.h"
#include "player.h"
#include "gamepiece.h"
#include "mapwidget.h"
#include "building.h"

// Create game pieces from purchase result
void createPiecesFromPurchase(Player *player, const PurchaseResult &result, const QString &territory)
{
    Position dummyPos = {0, 0};

    // Create infantry
    for (int i = 0; i < result.infantry; ++i) {
        InfantryPiece *infantry = new InfantryPiece(player->getId(), dummyPos, player);
        infantry->setTerritoryName(territory);
        player->addInfantry(infantry);
    }

    // Create cavalry
    for (int i = 0; i < result.cavalry; ++i) {
        CavalryPiece *cavalry = new CavalryPiece(player->getId(), dummyPos, player);
        cavalry->setTerritoryName(territory);
        player->addCavalry(cavalry);
    }

    // Create catapults
    for (int i = 0; i < result.catapults; ++i) {
        CatapultPiece *catapult = new CatapultPiece(player->getId(), dummyPos, player);
        catapult->setTerritoryName(territory);
        player->addCatapult(catapult);
    }
}

// Log battle result to CSV file for AI training
void logBattleResult(int budget,
                     int attackerInfantry, int attackerCavalry, int attackerCatapults,
                     int defenderInfantry, int defenderCavalry, int defenderCatapults,
                     bool defenderHasCity, bool defenderHasFortification,
                     CombatDialog::CombatResult result)
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    // Create directory if it doesn't exist
    QDir dir(appDataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString logPath = appDataPath + "/battle_log.csv";

    QFile file(logPath);
    bool fileExists = file.exists();

    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);

        // Write header if new file
        if (!fileExists) {
            stream << "timestamp,budget,"
                   << "atk_infantry,atk_cavalry,atk_catapults,atk_total_strength,"
                   << "def_infantry,def_cavalry,def_catapults,def_total_strength,"
                   << "def_has_city,def_has_fortification,def_has_caesar,"
                   << "result,winner\n";
        }

        // Calculate total strength (infantry=1, cavalry=2, catapult=3 combat value)
        int atkStrength = attackerInfantry + attackerCavalry * 2 + attackerCatapults * 3;
        int defStrength = defenderInfantry + defenderCavalry * 2 + defenderCatapults * 3;

        // Determine result string and winner
        QString resultStr, winner;
        switch (result) {
            case CombatDialog::CombatResult::AttackerWins:
                resultStr = "AttackerWins";
                winner = "Attacker";
                break;
            case CombatDialog::CombatResult::DefenderWins:
                resultStr = "DefenderWins";
                winner = "Defender";
                break;
            case CombatDialog::CombatResult::AttackerRetreats:
                resultStr = "AttackerRetreats";
                winner = "Defender";
                break;
        }

        // Defender has Caesar only if fortified city
        bool defenderHasCaesar = defenderHasCity && defenderHasFortification;

        stream << QDateTime::currentDateTime().toString(Qt::ISODate) << ","
               << budget << ","
               << attackerInfantry << "," << attackerCavalry << "," << attackerCatapults << "," << atkStrength << ","
               << defenderInfantry << "," << defenderCavalry << "," << defenderCatapults << "," << defStrength << ","
               << (defenderHasCity ? 1 : 0) << "," << (defenderHasFortification ? 1 : 0) << ","
               << (defenderHasCaesar ? 1 : 0) << ","
               << resultStr << "," << winner << "\n";

        file.close();
        qDebug() << "Battle logged to:" << logPath;
    } else {
        qWarning() << "Failed to open log file:" << logPath;
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Quick Battle");

    // Seed random number generator for AI purchases
    srand(static_cast<unsigned>(time(nullptr)));

    // Create a MapWidget (needed by CombatDialog for territory lookups)
    // We don't show it, just use it for the combat
    MapWidget mapWidget;

    Position dummyPos = {0, 0};
    bool keepPlaying = true;

    while (keepPlaying) {
        // Show splash screen
        QuickBattleSplash splash;
        if (splash.exec() != QDialog::Accepted) {
            break;  // User quit
        }

        int budget = splash.getBudget();
        bool attackerIsAI = splash.isAttackerAI();
        bool defenderIsAI = splash.isDefenderAI();
        bool defenderHasCity = splash.defenderHasCity();
        bool defenderHasFortification = splash.defenderHasFortification();

        // Create players - use "Battlefield" as home province so pieces are placed there
        // Use minimalSetup=true to only create Caesar (no generals/infantry/city)
        Player *attacker = new Player('A', "Battlefield", nullptr, true);
        Player *defender = new Player('D', "Battlefield", nullptr, true);

        // Replace auto-created Caesars with appropriate leaders
        Position dummyPos = {0, 0};

        // Attacker uses a General
        if (!attacker->getCaesars().isEmpty()) {
            CaesarPiece *caesar = attacker->getCaesars().first();
            attacker->removeCaesar(caesar);
            delete caesar;
        }
        GeneralPiece *attackerGeneral = new GeneralPiece(attacker->getId(), dummyPos, 1, nullptr);
        attackerGeneral->setTerritoryName("Battlefield");
        attacker->addGeneral(attackerGeneral);

        // Defender uses Caesar only if defending a fortified city, otherwise General
        GeneralPiece *defenderGeneral = nullptr;
        CaesarPiece *defenderCaesar = nullptr;
        if (defenderHasCity && defenderHasFortification) {
            // Keep Caesar for fortified city defense
            defenderCaesar = defender->getCaesars().isEmpty() ? nullptr : defender->getCaesars().first();
            qDebug() << "Defender Caesar:" << (defenderCaesar ? defenderCaesar->getTerritoryName() : "NONE");
        } else {
            // Replace Caesar with General
            if (!defender->getCaesars().isEmpty()) {
                CaesarPiece *caesar = defender->getCaesars().first();
                defender->removeCaesar(caesar);
                delete caesar;
            }
            defenderGeneral = new GeneralPiece(defender->getId(), dummyPos, 1, nullptr);
            defenderGeneral->setTerritoryName("Battlefield");
            defender->addGeneral(defenderGeneral);
            qDebug() << "Defender General:" << (defenderGeneral ? defenderGeneral->getTerritoryName() : "NONE");
        }

        qDebug() << "Attacker General:" << (attackerGeneral ? attackerGeneral->getTerritoryName() : "NONE");

        // Attacker purchase phase
        qDebug() << "Starting attacker purchase phase";
        PurchaseResult attackerPurchase = {};
        {
            PurchaseDialog attackerDialog(
                'A', budget, 1,
                {}, {}, {},  // No cities/fortifications/galleys
                0, 99, 99, 99, 0,  // Unlimited troops available
                nullptr, true  // combatUnitsOnly = true
            );
            attackerDialog.setWindowTitle("Attacker - Build Your Army");

            // If AI controlled, set up auto-mode with random purchases
            if (attackerIsAI) {
                QMap<QString, int> aiPurchases;
                int remaining = budget;
                int infantry = 0, cavalry = 0, catapults = 0;

                // Randomly buy units, but reserve enough for at least some variety
                while (remaining >= 30) {  // While we can afford any unit
                    int choice = rand() % 3;
                    if (choice == 0) { infantry++; remaining -= 10; }
                    else if (choice == 1) { cavalry++; remaining -= 20; }
                    else { catapults++; remaining -= 30; }
                }
                // Spend any remainder on cheaper units
                while (remaining >= 20) { cavalry++; remaining -= 20; }
                while (remaining >= 10) { infantry++; remaining -= 10; }

                aiPurchases["Infantry"] = infantry;
                aiPurchases["Cavalry"] = cavalry;
                aiPurchases["Catapults"] = catapults;
                attackerDialog.setupAIAutoMode(500, aiPurchases);
            }

            if (attackerDialog.exec() != QDialog::Accepted) {
                delete attacker;
                delete defender;
                continue;  // Go back to splash
            }
            attackerPurchase = attackerDialog.getPurchaseResult();
        }

        // Create attacker's pieces
        createPiecesFromPurchase(attacker, attackerPurchase, "Battlefield");

        // Add troops to General's legion
        if (attackerGeneral) {
            QList<int> attackerLegion;
            for (GamePiece *piece : attacker->getAllPieces()) {
                if (piece->getType() != GamePiece::Type::Caesar && piece->getType() != GamePiece::Type::General) {
                    attackerLegion.append(piece->getUniqueId());
                }
            }
            attackerGeneral->setLegion(attackerLegion);
            qDebug() << "Attacker legion:" << attackerLegion.size() << "troops";
        }

        // Defender purchase phase
        qDebug() << "Starting defender purchase phase";
        PurchaseResult defenderPurchase = {};
        {
            PurchaseDialog defenderDialog(
                'D', budget, 1,
                {}, {}, {},
                0, 99, 99, 99, 0,
                nullptr, true
            );
            defenderDialog.setWindowTitle("Defender - Build Your Army");

            // If AI controlled, set up auto-mode with random purchases
            if (defenderIsAI) {
                QMap<QString, int> aiPurchases;
                int remaining = budget;
                int infantry = 0, cavalry = 0, catapults = 0;

                // Randomly buy units while we can afford the most expensive
                while (remaining >= 30) {
                    int choice = rand() % 3;
                    if (choice == 0) { infantry++; remaining -= 10; }
                    else if (choice == 1) { cavalry++; remaining -= 20; }
                    else { catapults++; remaining -= 30; }
                }
                // Spend any remainder on cheaper units
                while (remaining >= 20) { cavalry++; remaining -= 20; }
                while (remaining >= 10) { infantry++; remaining -= 10; }

                aiPurchases["Infantry"] = infantry;
                aiPurchases["Cavalry"] = cavalry;
                aiPurchases["Catapults"] = catapults;
                defenderDialog.setupAIAutoMode(500, aiPurchases);
            }

            if (defenderDialog.exec() != QDialog::Accepted) {
                delete attacker;
                delete defender;
                continue;
            }
            defenderPurchase = defenderDialog.getPurchaseResult();
        }

        // Create defender's pieces
        createPiecesFromPurchase(defender, defenderPurchase, "Battlefield");

        // Add troops to defender's leader's legion
        QList<int> defenderLegion;
        for (GamePiece *piece : defender->getAllPieces()) {
            if (piece->getType() != GamePiece::Type::Caesar && piece->getType() != GamePiece::Type::General) {
                defenderLegion.append(piece->getUniqueId());
            }
        }
        if (defenderCaesar) {
            defenderCaesar->setLegion(defenderLegion);
        } else if (defenderGeneral) {
            defenderGeneral->setLegion(defenderLegion);
        }
        qDebug() << "Defender legion:" << defenderLegion.size() << "troops";

        // Add city/fortification if selected
        if (defenderHasCity) {
            City *city = new City('D', dummyPos, "Battlefield", defenderHasFortification, defender);
            defender->addCity(city);
        }

        // Show army summary before combat
        qDebug() << "About to show army summary";
        QString attackerSummary = QString("Attacker: 1 General, %1 Infantry, %2 Cavalry, %3 Catapults")
            .arg(attackerPurchase.infantry)
            .arg(attackerPurchase.cavalry)
            .arg(attackerPurchase.catapults);
        QString defenderLeader = defenderCaesar ? "Caesar" : "General";
        QString defenderSummary = QString("Defender: 1 %1, %2 Infantry, %3 Cavalry, %4 Catapults%5%6")
            .arg(defenderLeader)
            .arg(defenderPurchase.infantry)
            .arg(defenderPurchase.cavalry)
            .arg(defenderPurchase.catapults)
            .arg(defenderHasCity ? ", City" : "")
            .arg(defenderHasFortification ? " (Fortified)" : "");

        QMessageBox battleMsg;
        battleMsg.setWindowTitle("Battle Starting");
        battleMsg.setText(QString("%1\n%2\n\nLet the battle begin!").arg(attackerSummary).arg(defenderSummary));
        battleMsg.setIconPixmap(QPixmap(":/images/combatIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        // Auto-close after delay if both players are AI
        if (attackerIsAI && defenderIsAI) {
            QTimer::singleShot(2000, &battleMsg, &QMessageBox::accept);
        }

        battleMsg.exec();

        // Run combat
        qDebug() << "Creating CombatDialog...";
        CombatDialog combatDialog(attacker, defender, "Battlefield", &mapWidget);
        qDebug() << "CombatDialog created, about to exec()...";

        // Set up AI control if selected
        if (attackerIsAI || defenderIsAI) {
            combatDialog.setupAIMode(attackerIsAI, defenderIsAI, 1000);
        }

        combatDialog.exec();

        // Log battle result for AI training
        logBattleResult(budget,
                        attackerPurchase.infantry, attackerPurchase.cavalry, attackerPurchase.catapults,
                        defenderPurchase.infantry, defenderPurchase.cavalry, defenderPurchase.catapults,
                        defenderHasCity, defenderHasFortification,
                        combatDialog.getCombatResult());

        // Show result with appropriate icon
        QString resultText;
        QString resultIcon;
        switch (combatDialog.getCombatResult()) {
            case CombatDialog::CombatResult::AttackerWins:
                resultText = "The Attacker is victorious!";
                resultIcon = ":/images/victoryIcon.png";
                break;
            case CombatDialog::CombatResult::DefenderWins:
                resultText = "The Defender holds the field!";
                resultIcon = ":/images/deadIcon.png";
                break;
            case CombatDialog::CombatResult::AttackerRetreats:
                resultText = "The Attacker has retreated!";
                resultIcon = ":/images/retreatIcon.png";
                break;
        }

        QMessageBox resultBox;
        resultBox.setWindowTitle("Battle Complete");
        resultBox.setText(resultText);
        resultBox.setIconPixmap(QPixmap(resultIcon).scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        QPushButton *playAgainButton = resultBox.addButton("Play Again", QMessageBox::AcceptRole);
        QPushButton *quitButton = resultBox.addButton("Quit", QMessageBox::RejectRole);
        Q_UNUSED(playAgainButton);

        resultBox.exec();
        if (resultBox.clickedButton() == quitButton) {
            keepPlaying = false;
        }

        // Cleanup
        delete attacker;
        delete defender;
    }

    return 0;
}
