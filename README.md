# hacklog

[![CI](https://github.com/elviscgn/hacklog/actions/workflows/ci.yml/badge.svg)](https://github.com/elviscgn/hacklog/actions/workflows/ci.yml)

terminal-native hackathon and project logbook. track what you're in, what's due, and your win/loss record without leaving the terminal.

## features

- **tui dashboard**: type `hack` to open the terminal ui with a nice banner, quick stats, and list
- **slash commands**: `/add`, `/win`, `/lose` inside the tui (uses the exact same parser as the shell)
- **calendar view**: month grid view showing deadlines with urgency colors and legend
- **currency conversion**: local ZAR conversion with static rates in your config
- **flat-file storage**: just plain tsv files under `~/.hacklog/` - super easy to edit by hand

> [!NOTE]
> currency conversions are completely offline using rates in your config file. no api calls, no tracking.

## build

```sh
make          # build it
make test     # run tests
make install  # put it in your path
make clean    # clean up build files
```

needs: gcc (c11), ncurses, make.

on mac, ncurses is already installed. on linux: `sudo apt install libncurses-dev`.

## usage

### tui (default)

```sh
hack
```

inside the tui:
- `j`/`k` or arrow keys to move around
- `/` to type a command
- `Esc` to cancel
- `q` to exit

### shell mode

```sh
hack add "HackMIT" --deadline 2026-09-15
hack win "HackMIT" --prize $2500
hack list
hack cal
```

### in-tui commands

just prefix with `/`:

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
```

## command reference

| Command | Usage | Description |
|---------|-------|-------------|
| `add`   | `hack add "name" --deadline YYYY-MM-DD` | create entry (defaults to `applied`) |
| `win`   | `hack win "name" [--prize $2500]` | mark as won with optional prize |
| `lose`  | `hack lose "name"` | mark as lost |
| `status`| `hack status "name" <status>` | change status directly |
| `edit`  | `hack edit "name" [flags]` | edit fields of an entry |
| `delete`| `hack delete "name"` | delete entry (requires y/n confirm) |
| `undo`  | `hack undo` | revert last action |
| `list`  | `hack list` | print all entries to stdout |
| `rate`  | `hack rate USD 18.50` | update currency conversion rate |
| `cal`   | `hack cal` | show calendar view |

### aliases

| Alias | Expands to |
|-------|-----------|
| `/a`  | `/add`    |
| `/w`  | `/win`    |
| `/l`  | `/lose`   |
| `/d`  | `/delete` |
| `/e`  | `/edit`   |
| `/s`  | `/status` |

### status values

| Status      | Meaning |
|-------------|---------|
| `applied`   | waiting to hear back |
| `active`    | currently competing |
| `submitted` | done, waiting for results |
| `won`       | resolved, you won |
| `lost`      | competed but didn't win |
| `rejected`  | application was declined |
| `cancelled` | you decided to bail |

## license

mit
