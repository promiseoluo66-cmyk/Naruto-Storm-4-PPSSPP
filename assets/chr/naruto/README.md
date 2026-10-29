# Naruto Uzumaki character family

This folder represents **one main roster entry: Naruto Uzumaki**. Selecting Naruto in the main roster should open a separate variant-selection screen. Variants such as Teenage Naruto, Sage Mode, Kurama Chakra Mode and Six Paths Sage Mode belong inside that screen rather than occupying separate main-roster slots.

## First variant: Teenage Naruto

- Variant ID: `teenage`
- Default costume: `orange-blue`
- Jutsu design: Rasengan, Shadow Clone Jutsu, Rasenshuriken
- Ultimate design: Tailed Beast Bomb
- Awakening design: Kurama Mode

## Folder map

- `model/` — original or properly licensed mesh files
- `textures/` — textures for the model
- `costumes/orange-blue/` — the first costume's model/texture variations
- `jutsu/` — move definitions, effect references and animation data
- `animations/` — character movement and combat animations

**Implementation status:** this is the data/folder specification only. The model, texture, costume selector, and family/variant menu are not yet implemented. The current game still renders fighters with procedural placeholder geometry. Do not put extracted commercial-game files here.
