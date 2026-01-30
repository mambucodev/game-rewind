# Game Rewind

A game save backup manager for Linux. Automatically detects installed Steam games (including Proton/Wine titles), lets you create compressed backups of save files, and restore them at any time.

## Features

- **Automatic game detection** -- finds installed Steam games via the [Ludusavi manifest](https://github.com/mtkennerly/ludusavi-manifest) (~52k games indexed)
- **Proton/Wine support** -- detects save files inside Wine prefixes for Windows games running through Proton
- **Custom games** -- manually add any game with a save path
- **Compressed backups** -- each backup is a `.tar.gz` archive with metadata
- **Multiple backup slots** -- keep as many snapshots per game as you want
- **Backup notes** -- annotate backups (e.g., "Before final boss", "100% completion")
- **Hide/unhide games** -- hide irrelevant games from the detected list, restore them anytime
- **Native Qt6 UI** -- integrates with your system theme (Breeze, Adwaita, etc.)
- **Keyboard shortcuts** -- Ctrl+B (backup), Ctrl+R (restore), Delete, F5 (refresh)

## Screenshots

*Coming soon*

## Building

### Requirements

- CMake 3.16+
- C++17 compiler (GCC, Clang)
- Qt6 (Core, Widgets, Network, Sql)
- yaml-cpp
- tar

### Build from source

```bash
git clone https://github.com/mambucodev/game-rewind.git
cd game-rewind
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Install system-wide

```bash
sudo make install
```

### Run from build directory

```bash
./game-rewind
```

### Package dependencies

**Arch Linux:**
```bash
sudo pacman -S cmake qt6-base yaml-cpp
```

**Ubuntu/Debian:**
```bash
sudo apt install cmake g++ qt6-base-dev libyaml-cpp-dev
```

**Fedora:**
```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel yaml-cpp-devel
```

## Usage

### Getting started

1. Launch `game-rewind`
2. On first run, the Ludusavi manifest is downloaded and cached (~16MB)
3. Installed Steam games with detected save paths appear in the left panel
4. Select a game and click **Create Backup** (or press Ctrl+B)
5. To restore, select a backup from the right panel and click **Restore** (or press Ctrl+R)

### Adding custom games

There are two ways to add games that aren't auto-detected:

**Quick add:** Click the **Add Game** button in the toolbar. Pick a platform, enter the game name, and browse to the save folder.

**Config editor:** Click **Manage Configs** in the toolbar to open the full editor. Here you can add, edit, and delete game definitions with multiple save paths.

### Hiding games

Right-click any detected game and select **Hide Game** to remove it from the list. To restore hidden games, click **Hidden Games** in the toolbar.

### How detection works

1. **Custom games** from the database are checked first -- each save path is tested for existence
2. **Steam games** are scanned from installed `appmanifest_*.acf` files across all Steam library folders
3. Each Steam game is looked up in the Ludusavi manifest for known save paths
4. Linux-native paths are tried first (`~/.local/share/...`, `~/.config/...`)
5. If no Linux save is found, the Proton prefix is checked (`compatdata/<appId>/pfx/drive_c/...`)

### Data locations

| What | Where |
|------|-------|
| Backups | `~/.local/share/game-rewind/games/<game-id>/` |
| Database | `~/.local/share/game-rewind/games.db` |
| Manifest cache | `~/.local/share/game-rewind/manifest.yaml` |

Each backup consists of a `.tar.gz` archive and a `.tar.gz.json` metadata file containing the backup name, notes, timestamp, and size.

## Project Structure

```
src/
  main.cpp
  core/           Data types, database, backup logic
  steam/          Steam integration, manifest parsing, game detection
  ui/             Qt widgets, delegates, dialogs
docs/             Config format documentation
.github/          CI workflows
```

## License

[MIT](LICENSE)
