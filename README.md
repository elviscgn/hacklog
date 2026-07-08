# hacklog

Terminal-native hackathon/project logbook. Track what you're in, what's due when, and your win/loss record — all from the command line.

Built in C with ncurses. No network calls, no databases, no dependencies beyond libc and ncurses.

## Features

- **TUI dashboard** — launch with `hack` for an interactive view with ASCII banner, stats, and entry list
- **Slash commands** — type `/add`, `/win`, `/lose` inside the TUI, same parser as shell commands
- **Calendar view** — month grid showing deadlines with urgency coloring and per-entry identity colors
- **Currency conversion** — static ZAR conversion with manually-maintained rates
- **Profiles** — switch to a decoy dataset for screenshots (`--profile demo`)
- **Flat-file storage** — human-readable TSV files in `~/.hacklog/`

## Build

```sh
make          # build the binary
make test     # run the test suite
make install  # install to ~/.local/bin or /usr/local/bin
make clean    # remove build artifacts
```

Requires: gcc (C11), ncurses, make.

On macOS ncurses is included with the system. On Ubuntu/Debian: `sudo apt install libncurses-dev`.

## Usage

### TUI (default)

```sh
hack                    # launch dashboard
hack --profile demo     # launch with demo data
```

Inside the TUI:
- `j`/`k` or arrow keys to navigate the entry list
- `/` to open the command line
- `Esc` to cancel a command
- `q` to quit

### Shell commands

```sh
hack add "HackMIT" --deadline 2026-09-15
hack add "TreeHacks" --deadline 2026-10-01 --notes "team of 4"
hack win "HackMIT" --prize $2500
hack lose "TreeHacks"
hack status "HackMIT" active
hack edit "HackMIT" --deadline 2026-09-20 --notes "updated plan"
hack delete "HackMIT"
hack undo
hack list
hack rate USD 18.50
hack cal
hack --profile demo list
```

### In-TUI slash commands

All shell commands work as slash commands with `/`:

```
/add "HackMIT" --deadline 2026-09-15
/win "HackMIT" --prize $2500
/w "HackMIT" --prize $2500     (alias)
/lose "HackMIT"
/l "HackMIT"                   (alias)
/status "HackMIT" active
/edit "HackMIT" --notes "new plan"
/delete "HackMIT"
/undo
/list
/rate EUR 19.80
/cal
/profile demo
```

## Command reference

| Command | Usage | Description |
|---------|-------|-------------|
| `add`   | `hack add "name" --deadline YYYY-MM-DD` | Add entry (defaults to `applied`) |
| `win`   | `hack win "name" [--prize $2500]` | Mark as won with optional prize |
| `lose`  | `hack lose "name"` | Mark as lost |
| `status`| `hack status "name" <status>` | Set status directly |
| `edit`  | `hack edit "name" [flags]` | Edit entry fields |
| `delete`| `hack delete "name"` | Delete (requires confirmation) |
| `undo`  | `hack undo` | Revert last action |
| `list`  | `hack list` | Print entries to stdout |
| `rate`  | `hack rate USD 18.50` | Update currency rate |
| `cal`   | `hack cal` | Print calendar |

### Aliases

| Alias | Expands to |
|-------|-----------|
| `/a`  | `/add`    |
| `/w`  | `/win`    |
| `/l`  | `/lose`   |
| `/d`  | `/delete` |
| `/e`  | `/edit`   |
| `/s`  | `/status` |

### Status values

| Status      | Meaning |
|-------------|---------|
| `applied`   | Waiting to hear back |
| `active`    | Currently competing |
| `submitted` | Done, awaiting results |
| `won`       | Won the hackathon |
| `lost`      | Didn't win |
| `rejected`  | Application declined |
| `cancelled` | You decided to bail |

### Win rate

```
win_rate = won / (won + lost)
```

Only `won` and `lost` count. Everything else is excluded.

## Currency

Prizes are parsed from symbols (`$`, `R`, `£`, `€`) or 3-letter codes (`USD`, `ZAR`, `GBP`, `EUR`).

Conversion rates are static and stored in `~/.hacklog/config`:

```
USD=18.50
GBP=23.10
EUR=19.80
```

ZAR is the base currency. Update rates with `hack rate USD 19.00`.

Converted values are frozen at time of entry — updating a rate won't change past prizes.

## Profiles

Profiles let you keep separate datasets. The main use case is switching to a fake dataset before taking screenshots so competitors can't see your real entries.

```sh
hack --profile demo     # launch with demo data
```

Inside the TUI:
```
/profile demo           # switch mid-session
/profile default        # switch back
```

The `demo` profile ships pre-seeded with realistic fabricated data.

All profiles live as `.db` files under `~/.hacklog/profiles/`.

## Data format

Entries are stored as tab-separated values in `~/.hacklog/profiles/<name>.db`. The format is human-editable:

```
# name	deadline	status	prize_amount	prize_currency	prize_zar	notes
HackMIT	2026-09-15	won	2500.00	USD	46250.00	team of 4, used Go
```

## License

MIT
