for map in maps/*; do
    unswbc run "$map" bots/current/bot-v11.1.0-omar bots/current/bot-v21.0.0
done