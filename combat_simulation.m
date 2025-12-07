% Combat Simulation: Catapult vs Infantry vs Cavalry
% Monte Carlo simulation to evaluate troop value differences

clear; clc;

numSimulations = 10000;

%% Hit thresholds (need to roll this or higher on d6)
% Infantry: 4+, Cavalry: 5+, Catapult: 6+
INFANTRY_THRESHOLD = 4;
CAVALRY_THRESHOLD = 5;
CATAPULT_THRESHOLD = 6;

%% Simulation 1: Catapult vs Infantry
fprintf('=== SIMULATION 1: Catapult (attacker) vs Infantry (defender) ===\n\n');

% Attacker: 5 infantry + 1 catapult
% Defender: 6 infantry
attackerWins1 = 0;
defenderWins1 = 0;
attackerTroopsLeft1 = [];
defenderTroopsLeft1 = [];

for sim = 1:numSimulations
    % [infantry, cavalry, catapult]
    attacker = [5, 0, 1];
    defender = [6, 0, 0];

    isAttackerTurn = true;

    while sum(attacker) > 0 && sum(defender) > 0
        if isAttackerTurn
            % Attacker attacks defender
            advantage = attacker(3) - defender(3); % catapult difference
            advantage = max(0, advantage);

            % Choose target: catapult > cavalry > infantry
            if defender(3) > 0
                target = 3; threshold = CATAPULT_THRESHOLD;
            elseif defender(2) > 0
                target = 2; threshold = CAVALRY_THRESHOLD;
            else
                target = 1; threshold = INFANTRY_THRESHOLD;
            end

            roll = randi(6) + advantage;
            if roll >= threshold
                defender(target) = defender(target) - 1;
            end
        else
            % Defender attacks attacker
            advantage = defender(3) - attacker(3);
            advantage = max(0, advantage);

            % Choose target: catapult > cavalry > infantry
            if attacker(3) > 0
                target = 3; threshold = CATAPULT_THRESHOLD;
            elseif attacker(2) > 0
                target = 2; threshold = CAVALRY_THRESHOLD;
            else
                target = 1; threshold = INFANTRY_THRESHOLD;
            end

            roll = randi(6) + advantage;
            if roll >= threshold
                attacker(target) = attacker(target) - 1;
            end
        end

        isAttackerTurn = ~isAttackerTurn;
    end

    if sum(attacker) > 0
        attackerWins1 = attackerWins1 + 1;
        attackerTroopsLeft1(end+1) = sum(attacker);
    else
        defenderWins1 = defenderWins1 + 1;
        defenderTroopsLeft1(end+1) = sum(defender);
    end
end

fprintf('Attacker (5 inf + 1 cat) vs Defender (6 inf):\n');
fprintf('  Attacker wins: %.1f%% (avg %.2f troops left)\n', ...
    100*attackerWins1/numSimulations, mean(attackerTroopsLeft1));
fprintf('  Defender wins: %.1f%% (avg %.2f troops left)\n\n', ...
    100*defenderWins1/numSimulations, mean(defenderTroopsLeft1));

%% Simulation 2: Swap roles - Infantry attacker vs Catapult defender
attackerWins2 = 0;
defenderWins2 = 0;
attackerTroopsLeft2 = [];
defenderTroopsLeft2 = [];

for sim = 1:numSimulations
    % Attacker: 6 infantry
    % Defender: 5 infantry + 1 catapult
    attacker = [6, 0, 0];
    defender = [5, 0, 1];

    isAttackerTurn = true;

    while sum(attacker) > 0 && sum(defender) > 0
        if isAttackerTurn
            advantage = attacker(3) - defender(3);
            advantage = max(0, advantage);

            if defender(3) > 0
                target = 3; threshold = CATAPULT_THRESHOLD;
            elseif defender(2) > 0
                target = 2; threshold = CAVALRY_THRESHOLD;
            else
                target = 1; threshold = INFANTRY_THRESHOLD;
            end

            roll = randi(6) + advantage;
            if roll >= threshold
                defender(target) = defender(target) - 1;
            end
        else
            advantage = defender(3) - attacker(3);
            advantage = max(0, advantage);

            if attacker(3) > 0
                target = 3; threshold = CATAPULT_THRESHOLD;
            elseif attacker(2) > 0
                target = 2; threshold = CAVALRY_THRESHOLD;
            else
                target = 1; threshold = INFANTRY_THRESHOLD;
            end

            roll = randi(6) + advantage;
            if roll >= threshold
                attacker(target) = attacker(target) - 1;
            end
        end

        isAttackerTurn = ~isAttackerTurn;
    end

    if sum(attacker) > 0
        attackerWins2 = attackerWins2 + 1;
        attackerTroopsLeft2(end+1) = sum(attacker);
    else
        defenderWins2 = defenderWins2 + 1;
        defenderTroopsLeft2(end+1) = sum(defender);
    end
end

fprintf('Attacker (6 inf) vs Defender (5 inf + 1 cat):\n');
fprintf('  Attacker wins: %.1f%% (avg %.2f troops left)\n', ...
    100*attackerWins2/numSimulations, mean(attackerTroopsLeft2));
fprintf('  Defender wins: %.1f%% (avg %.2f troops left)\n\n', ...
    100*defenderWins2/numSimulations, mean(defenderTroopsLeft2));

%% Simulation 3: Catapult vs Cavalry
fprintf('=== SIMULATION 2: Catapult (attacker) vs Cavalry (defender) ===\n\n');

attackerWins3 = 0;
defenderWins3 = 0;
attackerTroopsLeft3 = [];
defenderTroopsLeft3 = [];

for sim = 1:numSimulations
    % Attacker: 5 infantry + 1 catapult
    % Defender: 5 infantry + 1 cavalry
    attacker = [5, 0, 1];
    defender = [5, 1, 0];

    isAttackerTurn = true;

    while sum(attacker) > 0 && sum(defender) > 0
        if isAttackerTurn
            advantage = attacker(3) - defender(3);
            advantage = max(0, advantage);

            if defender(3) > 0
                target = 3; threshold = CATAPULT_THRESHOLD;
            elseif defender(2) > 0
                target = 2; threshold = CAVALRY_THRESHOLD;
            else
                target = 1; threshold = INFANTRY_THRESHOLD;
            end

            roll = randi(6) + advantage;
            if roll >= threshold
                defender(target) = defender(target) - 1;
            end
        else
            advantage = defender(3) - attacker(3);
            advantage = max(0, advantage);

            if attacker(3) > 0
                target = 3; threshold = CATAPULT_THRESHOLD;
            elseif attacker(2) > 0
                target = 2; threshold = CAVALRY_THRESHOLD;
            else
                target = 1; threshold = INFANTRY_THRESHOLD;
            end

            roll = randi(6) + advantage;
            if roll >= threshold
                attacker(target) = attacker(target) - 1;
            end
        end

        isAttackerTurn = ~isAttackerTurn;
    end

    if sum(attacker) > 0
        attackerWins3 = attackerWins3 + 1;
        attackerTroopsLeft3(end+1) = sum(attacker);
    else
        defenderWins3 = defenderWins3 + 1;
        defenderTroopsLeft3(end+1) = sum(defender);
    end
end

fprintf('Attacker (5 inf + 1 cat) vs Defender (5 inf + 1 cav):\n');
fprintf('  Attacker wins: %.1f%% (avg %.2f troops left)\n', ...
    100*attackerWins3/numSimulations, mean(attackerTroopsLeft3));
fprintf('  Defender wins: %.1f%% (avg %.2f troops left)\n\n', ...
    100*defenderWins3/numSimulations, mean(defenderTroopsLeft3));

%% Simulation 4: Swap roles - Cavalry attacker vs Catapult defender
attackerWins4 = 0;
defenderWins4 = 0;
attackerTroopsLeft4 = [];
defenderTroopsLeft4 = [];

for sim = 1:numSimulations
    % Attacker: 5 infantry + 1 cavalry
    % Defender: 5 infantry + 1 catapult
    attacker = [5, 1, 0];
    defender = [5, 0, 1];

    isAttackerTurn = true;

    while sum(attacker) > 0 && sum(defender) > 0
        if isAttackerTurn
            advantage = attacker(3) - defender(3);
            advantage = max(0, advantage);

            if defender(3) > 0
                target = 3; threshold = CATAPULT_THRESHOLD;
            elseif defender(2) > 0
                target = 2; threshold = CAVALRY_THRESHOLD;
            else
                target = 1; threshold = INFANTRY_THRESHOLD;
            end

            roll = randi(6) + advantage;
            if roll >= threshold
                defender(target) = defender(target) - 1;
            end
        else
            advantage = defender(3) - attacker(3);
            advantage = max(0, advantage);

            if attacker(3) > 0
                target = 3; threshold = CATAPULT_THRESHOLD;
            elseif attacker(2) > 0
                target = 2; threshold = CAVALRY_THRESHOLD;
            else
                target = 1; threshold = INFANTRY_THRESHOLD;
            end

            roll = randi(6) + advantage;
            if roll >= threshold
                attacker(target) = attacker(target) - 1;
            end
        end

        isAttackerTurn = ~isAttackerTurn;
    end

    if sum(attacker) > 0
        attackerWins4 = attackerWins4 + 1;
        attackerTroopsLeft4(end+1) = sum(attacker);
    else
        defenderWins4 = defenderWins4 + 1;
        defenderTroopsLeft4(end+1) = sum(defender);
    end
end

fprintf('Attacker (5 inf + 1 cav) vs Defender (5 inf + 1 cat):\n');
fprintf('  Attacker wins: %.1f%% (avg %.2f troops left)\n', ...
    100*attackerWins4/numSimulations, mean(attackerTroopsLeft4));
fprintf('  Defender wins: %.1f%% (avg %.2f troops left)\n\n', ...
    100*defenderWins4/numSimulations, mean(defenderTroopsLeft4));

%% Summary
fprintf('=== SUMMARY ===\n\n');
fprintf('Catapult advantage over Infantry:\n');
fprintf('  As attacker: %.1f%% vs %.1f%% (delta: +%.1f%%)\n', ...
    100*attackerWins1/numSimulations, 100*attackerWins2/numSimulations, ...
    100*(attackerWins1 - attackerWins2)/numSimulations);
fprintf('  As defender: %.1f%% vs %.1f%% (delta: +%.1f%%)\n\n', ...
    100*defenderWins2/numSimulations, 100*defenderWins1/numSimulations, ...
    100*(defenderWins2 - defenderWins1)/numSimulations);

fprintf('Catapult advantage over Cavalry:\n');
fprintf('  As attacker: %.1f%% vs %.1f%% (delta: +%.1f%%)\n', ...
    100*attackerWins3/numSimulations, 100*attackerWins4/numSimulations, ...
    100*(attackerWins3 - attackerWins4)/numSimulations);
fprintf('  As defender: %.1f%% vs %.1f%% (delta: +%.1f%%)\n', ...
    100*defenderWins4/numSimulations, 100*defenderWins3/numSimulations, ...
    100*(defenderWins4 - defenderWins3)/numSimulations);
