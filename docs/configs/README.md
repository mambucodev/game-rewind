# Game Configuration Files

This directory contains JSON configuration files that define game save locations for automatic detection.

## Format

Each JSON file should have the following structure:

```json
{
  "games": [
    {
      "id": "unique-game-id",
      "name": "Game Display Name",
      "platform": "steam|native|custom",
      "steamAppId": "123456",
      "savePaths": [
        "~/.local/share/gamename",
        "$HOME/.config/gamename"
      ]
    }
  ]
}
```

## Fields

- `id`: Unique identifier for the game (used internally)
- `name`: Display name shown in the UI
- `platform`: Platform type (steam, native, custom)
- `steamAppId`: (Optional) Steam App ID for verification
- `savePaths`: Array of possible save locations (first match is used)

## Path Variables

The following variables are supported in save paths:

- `~` or `$HOME`: User's home directory
- `$STEAM`: Steam installation directory

## Adding New Games

To add a new game:

1. Create a new JSON file or edit an existing one
2. Add the game definition with correct save paths
3. Restart the application or click "Refresh Games"

## Finding Save Locations

Use the "Scan for Save Path" feature in the application to automatically search for save directories.

Common Linux save locations:
- `~/.local/share/`
- `~/.config/`
- `~/Documents/My Games/`
- `~/.steam/steam/steamapps/compatdata/` (for Proton games)
