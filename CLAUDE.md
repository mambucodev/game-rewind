# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Game Rewind is a Qt6 C++ application for managing game save backups on Linux. It automatically detects installed games -- both Linux-native and Proton/Wine -- via the Ludusavi manifest and Steam appmanifests, allows users to create compressed backups of save files, and restore them when needed.

## Build System

The project uses CMake as its build system.

### Build Commands

```bash
# Configure and build
mkdir build && cd build
cmake ..
make

# Build in debug mode
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Install
sudo make install

# Run from build directory
./game-rewind
```

## Source Tree Layout

```
src/
  main.cpp                  -- Application entry point
  core/                     -- Data types, database, backup logic
    gameinfo.{h,cpp}        -- GameInfo and BackupInfo structs
    database.{h,cpp}        -- SQLite database (custom games, hidden games)
    savemanager.{h,cpp}     -- Backup create/restore/delete via tar
  steam/                    -- Steam and game detection
    steamutils.{h,cpp}      -- Steam path detection, VDF parsing, Proton prefix lookup
    manifestmanager.{h,cpp} -- Ludusavi manifest download, caching, YAML parsing
    gamedetector.{h,cpp}    -- Two-phase game detection (custom DB + manifest)
  ui/                       -- All UI classes
    mainwindow.{h,cpp,ui}   -- Main window with game tree and backup list
    gamecarddelegate.{h,cpp} -- Custom paint delegate for game tree items
    backupitemdelegate.{h,cpp} -- Custom paint delegate for backup list items
    backupdialog.{h,cpp}    -- Dialog for backup name/notes
    addgamedialog.{h,cpp}   -- Dialog for adding custom games
    gameconfigdialog.{h,cpp} -- Config editor dialog (manage custom games)
    gameicon.{h,cpp}        -- Icon/capsule image provider
docs/
  configs/                  -- Documentation for JSON config format
```

### Include Conventions

- Cross-directory includes use qualified paths: `#include "core/gameinfo.h"`, `#include "steam/steamutils.h"`
- Same-directory includes use bare names: `#include "steamutils.h"`
- All three subdirectories (`src/core`, `src/steam`, `src/ui`) are in the include path via CMakeLists.txt

## Architecture

### Game Detection (Two-Phase)

**Phase 1 -- Custom Games** (`gamedetector.cpp`):
- Loads user-defined games from SQLite database
- Expands path variables (`~`, `$HOME`, `$STEAM`) and checks existence
- Skips games in the hidden set

**Phase 2 -- Manifest Games** (`gamedetector.cpp` + `manifestmanager.cpp`):
- Scans all Steam library folders for installed `appmanifest_*.acf` files
- Looks up each game in the Ludusavi manifest (indexed by Steam App ID)
- Tries Linux-native save paths first (`<xdgData>`, `<xdgConfig>`, `<home>`, etc.)
- Falls back to Proton prefix paths if no Linux save found (`compatdata/<appId>/pfx/drive_c/...`)
- Strips glob patterns from manifest paths (`*.dat`, `*/Level.bin`) via while-loop
- Skips games in the hidden set

### Ludusavi Manifest (`manifestmanager.{h,cpp}`)

- Downloads and caches a ~16MB YAML file from GitHub (mtkennerly/ludusavi-manifest)
- Uses ETag-based HTTP caching to avoid redundant downloads
- Parses with yaml-cpp; indexes ~52k games by Steam App ID
- Provides two path resolution methods:
  - `getLinuxSavePaths()` -- expands Linux placeholders (`<xdgData>`, `<xdgConfig>`, `<home>`)
  - `getProtonSavePaths()` -- expands Windows placeholders into Proton prefix paths

### Database (`database.{h,cpp}`)

SQLite database at `~/.local/share/game-rewind/games.db` with WAL mode:
- `schema_version` -- tracks migration state (current: v2)
- `custom_games` -- user-added game definitions (id, name, platform, steam_app_id, save_paths as JSON)
- `hidden_games` -- games hidden from the UI (game_id, name)

On first run, migrates legacy JSON configs from `~/.local/share/game-rewind/configs/` if they exist.

### Backup System (`savemanager.{h,cpp}`)

- Location: `~/.local/share/game-rewind/games/<game-id>/<timestamp>.tar.gz`
- Each backup has a matching `<timestamp>.tar.gz.json` metadata file
- Metadata: backup ID, display name, notes, timestamp, size, game name
- Uses system `tar` via `QProcess` for compression/extraction

### Steam Utilities (`steamutils.{h,cpp}`)

- `findSteamPath()` -- checks `~/.steam/steam`, `~/.local/share/Steam`, `/usr/share/steam`
- `getLibraryFolders()` -- parses `libraryfolders.vdf`, deduplicates by canonical path (handles symlinks)
- `scanInstalledGames()` -- reads all `appmanifest_*.acf` files across library folders
- `findProtonPrefix()` -- checks `compatdata/<appId>/pfx` across library folders
- `getSteamUserId()` -- reads `userdata/` directory, picks most recently modified if multiple
- Full VDF parser for Steam's key-value format

### UI Components

- **MainWindow**: Toolbar-driven (no menu bar), splitter with game tree (left) and backup list (right)
- **GameCardDelegate**: Custom-painted game cards with capsule art, backup count, size, platform badge
- **BackupItemDelegate**: HTML-rendered backup items with rich text
- **Context Menu**: Right-click game -> "Hide Game" (persisted in database)
- **Hidden Games Dialog**: Inline dialog to view/unhide hidden games
- **Keyboard Shortcuts**: Ctrl+B (backup), Ctrl+R (restore), Delete, F5 (refresh)

### Game Card Roles (`gamecarddelegate.h`)

Custom `Qt::UserRole` values used to store data on tree widget items:
- `GameIdRole`, `GameNameRole`, `GameIconRole`, `BackupCountRole`
- `TotalSizeRole`, `SavePathRole`, `PlatformRole`, `IsCategoryRole`

## Dependencies

- **Qt6**: Core, Widgets, Network, Sql
- **yaml-cpp**: Ludusavi manifest parsing
- **System**: `tar` (must be in PATH)

## Key Technical Details

### Path Expansion

The `GameDetector::expandPath()` method handles:
- `~` -> user home directory
- `$HOME` -> user home directory
- `$STEAM` -> detected Steam installation path

The `ManifestManager::expandManifestPath()` method handles Ludusavi placeholders:
- `<home>`, `<osUserName>`, `<xdgData>`, `<xdgConfig>`
- `<base>`, `<root>`, `<game>`, `<storeUserId>`

The `ManifestManager::expandProtonPath()` method maps Windows placeholders:
- `<winAppData>` -> `pfx/drive_c/users/steamuser/AppData/Roaming`
- `<winLocalAppData>` -> `.../AppData/Local`
- `<winLocalAppDataLow>` -> `.../AppData/LocalLow`
- `<winDocuments>` -> `.../Documents`
- `<home>` (Proton context) -> `pfx/drive_c/users/steamuser`

### Glob Pattern Handling

Manifest paths may contain glob patterns (`*.dat`, `*/Level.bin`). These are stripped via a while-loop to get the deepest non-glob parent directory, since `QFileInfo::exists()` treats `*` as a literal character.

## Common Development Tasks

When modifying the UI:
1. Edit `src/ui/mainwindow.ui` in Qt Designer or manually
2. CMake will automatically run `uic` to generate `ui_mainwindow.h`
3. Rebuild to see changes

When adding new source files:
1. Place in the appropriate subdirectory (`core/`, `steam/`, or `ui/`)
2. Add to the corresponding `SOURCES`/`HEADERS` list in `CMakeLists.txt`
3. Use qualified includes for cross-directory references

When debugging game detection:
- Check console output for detection debug messages (qDebug)
- Verify Steam paths match system installation
- Check manifest cache at `~/.local/share/game-rewind/manifest.yaml`
- Test path expansion with different variables
