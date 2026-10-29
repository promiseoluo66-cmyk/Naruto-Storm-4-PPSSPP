# Character folder template

Copy this structure when adding a character. Replace `CHARACTER_ID` with a stable lowercase ID and remove this template folder from runtime content if it is not needed.

```
CHARACTER_ID/
├── model/       # Character mesh files in a documented supported format
├── textures/    # Body, face, hair, and material textures
├── costumes/    # Outfit variants and their texture/model overrides
└── jutsu/       # Character-specific move definitions and references
```

The current runtime does not automatically load all of these folders. Implement and test the relevant loaders before expecting these files to affect gameplay.
