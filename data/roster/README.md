# Character roster data

Planned character definitions should use stable character IDs matching `assets/chr/<character_id>/`. A future roster entry can reference the default costume and the character's available jutsu IDs.

Example fields to support when a parser is implemented: `id`, `display_name`, `default_costume`, `jutsu_ids`, `ultimate_id`, and `awakening_id`.

The current roster and move logic remain in `src/main.c`; these folders do not change gameplay until a data loader is implemented.
