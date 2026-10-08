#!/usr/bin/env bash
# Usage: ./run_all.sh [-j JOBS] [-s] N [BOT_A] [BOT_B]
# Plays every map in maps/ N times, BOT_A as team A and BOT_B as team B, JOBS games at a time
# (default: one per CPU core). Prints each result as it finishes with a running A - B score
# (a draw counts half to each), then a breakdown of how each bot lost: by elimination, on the
# queen (its queen dead, or alive but shorter), on the longest dragon, or on total length.
#
# -s  swap sides every other pass: passes 1, 3, 5... play BOT_A as team A, passes 2, 4, 6... play
#     BOT_A as team B, on the same seed as the pass before (so use an even N). The bots are then
#     reported as 1 (BOT_A) and 2 (BOT_B), with each bot's score as team A and as team B.
#
# Bots that stop their searches on wall-clock deadlines play worse on a busy machine: keep JOBS
# at or below the number of cores, and run nothing else meanwhile.
set -u

usage() { echo "usage: $0 [-j JOBS] [-s] N [BOT_A] [BOT_B]" >&2; exit 2; }

JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
SWAP=0
while getopts "j:s" opt; do
    case "$opt" in
        j) JOBS="$OPTARG" ;;
        s) SWAP=1 ;;
        *) usage ;;
    esac
done
shift $((OPTIND - 1))
[ $# -ge 1 ] || usage
case "$JOBS" in ''|*[!0-9]*|0) echo "JOBS must be a positive number" >&2; exit 2 ;; esac
case "$1" in ''|*[!0-9]*) echo "N must be a number" >&2; exit 2 ;; esac

N="$1"
A="${2:-bots/New/current/bot-v26.1.0-omar-portals}"
B="${3:-bots/New/current/bot-v25.0.0-omar}"
if [ "$SWAP" = 1 ]; then
    L1=1; L2=2
    [ $((N % 2)) -eq 0 ] || echo "note: with -s an odd N leaves the last pass unswapped" >&2
else
    L1=A; L2=B
fi

# How a game was decided, from the reason the engine prints in brackets after the round count:
#   "by elimination" | "longer queen, X to Y" | "longest dragon, X to Y" | "total length, X to Y"
#   | "both teams eliminated" | "equal length: queen Q, longest L, total T each"
CONDS="elim queen_dead queen_shorter longest total other"
label() {
    case "$1" in
        elim)          echo "elimination" ;;
        queen_dead)    echo "queen (its queen dead)" ;;
        queen_shorter) echo "queen (its queen shorter)" ;;
        longest)       echo "longest dragon" ;;
        total)         echo "total length" ;;
        *)             echo "other" ;;
    esac
}
for c in $CONDS; do eval "lost_1_$c=0; lost_2_$c=0"; done
inc() { eval "$1=\$(( $1 + 1 ))"; }

classify() { # reason -> condition
    case "$1" in
        "by elimination") echo elim ;;
        "longer queen, "*)
            # "longer queen, X to Y": Y is the loser's queen, 0 when she is dead
            if [[ "$1" =~ to\ ([0-9]+)$ ]] && [ "${BASH_REMATCH[1]}" -eq 0 ]; then echo queen_dead
            else echo queen_shorter; fi ;;
        "longest dragon, "*) echo longest ;;
        "total length, "*) echo total ;;
        *) echo other ;;
    esac
}

maps=(maps/*)
nmaps=${#maps[@]}
[ -e "${maps[0]}" ] || { echo "no maps in maps/" >&2; exit 1; }
total=$((N * nmaps))

# -s: is job k's pass a swapped one (BOT_A plays as team B)?
swapped() { [ "$SWAP" = 1 ] && [ $(( (($1 / nmaps) % 2) )) -eq 1 ] && [ $(( $1 / nmaps + 1 )) -le $(( N - N % 2 )) ]; }

TMPD=$(mktemp -d "${TMPDIR:-/tmp}/run_all.XXXXXX") || exit 1
pids=(); state=(); seeds=()   # per job: its process, and 1 while it runs, 2 once recorded; -s: seed per pair
killtree() {        # a process and everything under it (the engine and the bots of a game)
    local c
    for c in $(pgrep -P "$1" 2>/dev/null); do killtree "$c"; done
    kill "$1" 2>/dev/null
}
cleanup() {         # stop the games still running
    local k
    for ((k = 0; k < ${#state[@]}; k++)); do
        if [ "${state[k]:-0}" = 1 ]; then killtree "${pids[k]}"; fi
    done
    rm -rf "$TMPD"
}
trap cleanup EXIT
trap 'echo; echo "interrupted"; exit 130' INT TERM

# One game: its result line goes to $TMPD/<job> (written whole, then renamed into place).
play() { # job map [seed]
    local out line
    if [ $# -ge 3 ]; then
        if swapped "$1"; then out=$(unswbc run "$2" "$B" "$A" --seed "$3" --no-replay 2>&1 | sed 's/\x1b\[[0-9;]*m//g')
        else out=$(unswbc run "$2" "$A" "$B" --seed "$3" --no-replay 2>&1 | sed 's/\x1b\[[0-9;]*m//g'); fi
    else
        out=$(unswbc run "$2" "$A" "$B" --no-replay 2>&1 | sed 's/\x1b\[[0-9;]*m//g')
    fi
    line=$(grep -E '^(team [AB] wins|draw) after' <<<"$out" | tail -1)
    [ -n "$line" ] || line="NO RESULT: $(tail -1 <<<"$out")"
    printf '%s\n' "$line" > "$TMPD/$1.part" && mv "$TMPD/$1.part" "$TMPD/$1"
}

w1=0; w2=0; d=0; games=0; draw_elim=0; draw_equal=0; no_result=0
s1_A=0; g1_A=0; s1_B=0; g1_B=0   # bot 1's score (doubled, so draws stay whole) and games, by its side
record() { # job line
    local k="$1" line="$2" reason side winner
    reason=$(sed -nE 's/^.* after [0-9]+ rounds \(([^)]*)\).*$/\1/p' <<<"$line")
    if swapped "$k"; then side=B; else side=A; fi
    games=$((games + 1))
    case "$line" in
        "team A wins"*|"team B wins"*)
            winner=${line:5:1}                       # the winning team
            if [ "$winner" = "$side" ]; then         # bot 1 won
                w1=$((w1 + 1)); inc "lost_2_$(classify "$reason")"; inc "s1_$side"; inc "s1_$side"
            else
                w2=$((w2 + 1)); inc "lost_1_$(classify "$reason")"
            fi
            inc "g1_$side" ;;
        draw*)
            d=$((d + 1)); inc "s1_$side"; inc "g1_$side"
            if [ "$reason" = "both teams eliminated" ]; then draw_elim=$((draw_elim + 1))
            else draw_equal=$((draw_equal + 1)); fi ;;
        *)  no_result=$((no_result + 1)) ;;
    esac
    if [ "$SWAP" = 1 ]; then
        printf 'pass %d  %-36s [1 is team %s] %s\n' $((k / nmaps + 1)) "$(basename "${maps[k % nmaps]}")" "$side" "$line"
    else
        printf 'pass %d  %-36s %s\n' $((k / nmaps + 1)) "$(basename "${maps[k % nmaps]}")" "$line"
    fi
    printf '         score %s - %s: %s - %s  (draws %d, games %d/%d)\n' "$L1" "$L2" \
        "$(awk "BEGIN{print $w1 + $d/2}")" "$(awk "BEGIN{print $w2 + $d/2}")" "$d" "$games" "$total"
}

echo "$total games, $JOBS at a time$([ "$SWAP" = 1 ] && echo ", sides swapped every other pass")"
next=0; running=0
while [ "$games" -lt "$total" ]; do
    while [ "$running" -lt "$JOBS" ] && [ "$next" -lt "$total" ]; do
        if [ "$SWAP" = 1 ]; then
            pair=$(( (next / nmaps / 2) * nmaps + next % nmaps ))
            [ -n "${seeds[pair]:-}" ] || seeds[pair]=$(( (RANDOM << 45) | (RANDOM << 30) | (RANDOM << 15) | RANDOM ))
            play "$next" "${maps[next % nmaps]}" "${seeds[pair]}" &
        else
            play "$next" "${maps[next % nmaps]}" &
        fi
        pids[next]=$!
        state[next]=1
        next=$((next + 1)); running=$((running + 1))
    done
    for ((k = 0; k < next; k++)); do
        if [ "${state[k]}" = 1 ] && [ -e "$TMPD/$k" ]; then
            wait "${pids[k]}" 2>/dev/null
            state[k]=2; running=$((running - 1))
            record "$k" "$(cat "$TMPD/$k")"
        elif [ "${state[k]}" = 1 ] && ! kill -0 "${pids[k]}" 2>/dev/null && [ ! -e "$TMPD/$k" ]; then
            state[k]=2; running=$((running - 1))   # died without writing a result
            record "$k" "NO RESULT: game process exited early"
        fi
    done
    [ "$games" -lt "$total" ] && sleep 0.2
done

echo
echo "$L1 = $A"
echo "$L2 = $B"
echo "final $L1 - $L2: $w1 - $w2, draws $d, over $games games"
if [ "$SWAP" = 1 ]; then
    for sd in A B; do
        sv="s1_$sd"; gv="g1_$sd"
        printf '1 as team %s: %s / %d\n' "$sd" "$(awk "BEGIN{print ${!sv}/2}")" "${!gv}"
    done
fi

echo
printf '%-28s %6s %6s\n' "lost by" "$L1" "$L2"
for c in $CONDS; do
    l1="lost_1_$c"; l2="lost_2_$c"
    # "other" only shows up if the engine printed a reason this script does not know
    if [ "$c" = other ] && [ "${!l1}" -eq 0 ] && [ "${!l2}" -eq 0 ]; then continue; fi
    printf '%-28s %6d %6d\n' "$(label "$c")" "${!l1}" "${!l2}"
done
printf '%-28s %6d %6d\n' "total losses" "$w2" "$w1"
echo "draws: $draw_elim with both teams eliminated, $draw_equal at the round limit with equal lengths"
if [ "$no_result" -gt 0 ]; then echo "games with no result (engine or bot error): $no_result"; fi
