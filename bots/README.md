# bots/

- `New/` : active work. Each project is a directory containing `bot.toml` plus its C++ files,
  named `bot-v13.<n>-hNNN` (the hypothesis it tests). Lane agents create bots here.
- `Old/` : frozen history. Never edit. It holds the champion, the panel opponents listed in
  `battlecode-automation/panel.json`, and older versions kept for reference:
  `Old/current` (recent champions), `Old/Kushagra` (v12 line), `Old/alan` (alan line),
  `Old/archive` (v1 to v11.1), `Old/candidate` (v9 experiments, lowest value).

A bot project is any directory with a `bot.toml`. To start a new bot, copy the champion
directory into `New/` under a new name; do not edit it in place under `Old/`.
