#!/usr/bin/env bash
# Usage: ./run_all.sh N [BOT_A] [BOT_B]
# Plays every map in maps/ N times, BOT_A as team A and BOT_B as team B,
# prints each result and a running A - B score (a draw counts half to each).
set -u

N="${1:?usage: $0 N [BOT_A] [BOT_B]}"

A="${2:-bots/New/current/bot-v26.1.0-omar-portals}"
B="${3:-bots/New/current/bot-v25.0.0-omar}"

a=0; b=0; d=0; games=0
for ((pass = 1; pass <= N; pass++)); do
    for map in maps/*; do
        out=$(unswbc run "$map" "$A" "$B" --no-replay 2>&1 | sed 's/\x1b\[[0-9;]*m//g')
        line=$(grep -E '^(team [AB] wins|draw) after' <<<"$out" | tail -1)
        games=$((games + 1))
        case "$line" in
            "team A wins"*) a=$((a + 1)) ;;
            "team B wins"*) b=$((b + 1)) ;;
            draw*)          d=$((d + 1)) ;;
            *)              line="NO RESULT: $(tail -1 <<<"$out")" ;;
        esac
        printf 'pass %d  %-36s %s\n' "$pass" "$(basename "$map")" "$line"
        printf '         score A - B: %s - %s  (draws %d, games %d)\n' \
            "$(awk "BEGIN{print $a + $d/2}")" "$(awk "BEGIN{print $b + $d/2}")" "$d" "$games"
    done
done

echo
echo "A = $A"
echo "B = $B"
echo "final A - B: $a - $b, draws $d, over $games games"
