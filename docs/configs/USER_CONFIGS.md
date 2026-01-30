# Game Rewind Config Files

This file will be copied to `~/.local/share/game-rewind/configs/` on first run.

## Location

All game configuration files are stored in:
```
~/.local/share/game-rewind/configs/
```

## Usage

You can freely edit, add, or remove JSON files in this directory. Changes take effect after restarting Game Rewind or clicking "Refresh Games".

## Adding Custom Games

Create a new JSON file (e.g., `my-games.json`) or edit existing ones:

```json
{
  "games": [
    {
      "id": "my-custom-game",
      "name": "My Custom Game",
      "platform": "custom",
      "savePaths": [
        "~/.config/my-game/saves"
      ]
    }
  ]
}
```

## Fields

- `id`: Unique identifier (used internally, must be unique)
- `name`: Display name in the UI
- `platform`: `steam`, `native`, or `custom`
- `steamAppId`: Steam App ID (optional, for Steam games)
- `savePaths`: Array of possible save locations (first match wins)

## Path Variables

- `~` or `$HOME`: Your home directory
- `$STEAM`: Steam installation directory

## Examples

See `steam-games.json` and `native-games.json` for examples.
