#!/bin/bash
# Équilibrage : simule des parties pour chaque stratégie de bot, sur plusieurs cartes,
# et affiche la vague moyenne atteinte. Usage : ./tools/equilibrage.sh [nombre de cartes]
cd "$(dirname "$0")/.." || exit 1
N=${1:-10}
gcc -std=c99 -O2 -Isrc -o tools/simulateur tools/simulateur.c $(ls src/*.c | grep -v main.c) -lm || exit 1
for strategie in sniper gatling mortier inferno mix banque spam-mortier spam-sniper spam-gatling spam-inferno; do
    vagues=$(for carte in $(seq 1 "$N"); do ./tools/simulateur "$strategie" "$carte" 80 | sed 's/.*vague=\([0-9]*\).*/\1/'; done)
    moyenne=$(echo "$vagues" | awk '{s += $1} END {printf "%.1f", s / NR}')
    printf "%-8s vague moyenne %5s   (%s)\n" "$strategie" "$moyenne" "$(echo $vagues | tr ' ' ',')"
done
