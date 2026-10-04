# for map in maps/*; do
#     unswbc run "$map" bots/New/current/bot-v21.0.0 bots/New/current/bot-v22.0.0 --sandbox -v | grep -E "bot [01] .*died|wins|points per turn"
# done
for map in maps/*; do
    unswbc run maps/big_empty.map bots/New/current/bot-v21.0.0 bots/New/current/bot-v22.0.0
done
# unswbc run --sandbox maps/unsw.map bots/New/current/bot-v21.0.0 bots/New/current/bot-v22.0.0
# unswbc run maps/default.map bots/New/current/bot-v21.0.0 bots/New/current/bot-v22.0.0 --sandbox -v | grep -E "bot [01] .*died|wins|points per turn"