# Asset Legend

## Sprites
```
~/assets/Sprites/Characters/Assassin/
    Assassin.png
    Assassin_Combat.png

~/assets/Sprites/Characters/Builder/
    Builder.png
    Builder_Combat.png

~/assets/Sprites/Characters/Damage Dealer/
    Dps.png
    Dps_Combat.png

~/assets/Sprites/Characters/Summoner/
    Summoner.png
    Summoner_Combat.png

~/assets/Sprites/Characters/Druid/
    Druid.png
    Druid_Combat.png

~/assets/Sprites/Characters/Wizard/
    Wizard.png
    Wizard_Combat.png

~/assets/Sprites/Characters/ealer/
    Healer.png
    Healer_Combat.png

~/assets/Sprites/Characters/Special/
    Special.png
    Special_Combat.png
    Special_Combat_Thrust_with_AttackEffect.png
    Special_Combat_Thrust+Dash.png
    Special_Thrust_Effects.png

~/assets/Sprites/Characters/Support/
    Support.png
    Support_Combat.png

~/assets/Sprites/Characters/Tank/
    Tank.png
    Tank_Combat.png
```

These sheets are laid out similarly to the previous character spritesheets, this is how the noncombat sheets are laid out:
Row 1: Idle Right
Row 2: Idle Left
Row 3: Idle Front
Row 4: Idle Back
Row 5: Walk Right
Row 6: Walk Left
Row 7: Walk Front
Row 8: Walk Back
Row 9: Run Right
Row 10: Run Left
Row 11: Run Front
Row 12: Run Back
Row 13: Carry R
Row 14: Carry L
Row 15: Carry F
Row 16: Carry B
Row 17: Pickup R
Row 18: Pickup L
Row 19: Pickup F
Row 20: Pickup B
Row 21: Interact R
Row 22: Interact L
Row 23: Interact F
Row 24: Interact B
Row 25: Knockdown R
Row 26: Knockdown L
Row 27: Knowckdown F
Row 28: Knockdown B
Row 29: Climb
Row 30: Get up R
Row 31: Gut up L


The Combat sheets are 32x32 sprites on a sheet. They are laid out like so;
Row 1: Attack Right
Row 2: Attack Right Backhand
Row 3: Attack Left
Row 4: Attack Left Backhand
Row 5: Attack Front
Row 6: Attack Front Backhand
Row 7: Attack Back
Row 8: Attack Back Backhand
Row 9: Ready Right
Row 10: Ready Left
Row 11: Ready Front
Row 12: Ready Back
Row 13: Secondary Weapon Right
Row 14: Secondary Weapon Left
Row 15: Secondary Weapon Front
Row 16: Secondary Weapon Back
Row 17: Secondary Weapon Idle Right
Row 18: Secondary Weapon Idle Left
Row 19: Secondary Weapon Idle Front
Row 20: Secondary Weapon Idle Back


Some Spritesheets for combat remain 16x16, namely the Builder_Combat.png, Healer_Combat.png. These should follow the same row formula.

There are also new projectile assets;
`~/assets/Sprites/Misc/..
    Simple_Sling_Bullet.png
        (4 4x4 Sprites in a horizontal sheet, animated)
    Sling_Bullets.png
        (4 5x5 Sprites in a horizontal sheet, individual)
    Sling_Bullets_No_Outline.png
        (4 5x5 Sprites in a horizontal sheet, individual - no outline)
    Arrows.png
        (4 10x5 Arrow Sprites - Horizontal Orientation - Facing Right by Default)
    Arrows_Diagonal.png
        (4 9x9 Arrow Sprites - Diagonal Orientation - Facing Right/Down by Default)
    (The Arrow assets will need to be mirrored/flipped to face the right orientation it is fired so they always appear to be firing straight from the shooter)

Snake: movement 16x16, Combat is 24x24.
Beetle: movement 16x16, Combat is 16x22.
Bear: movement 24x24, combat 32x32, render base 24.
Wolf: movement 16x16, combat 24x24, render base 16.

The Bear asset legend:
    Movement:
        Row 1: Idle Right
        Row 2: Idle Left
        Row 3: Idle Front
        Row 4: Idle Back
        Row 5: Walk Right
        Row 6: Walk Left
        Row 7: Walk Back
        Row 8: Walk Front
        Row 9: Run Right
        Row 10: Run Left
        Row 11: Run Front
        Row 12: Run Back
        Run 13: Knockdown Right
        Row 14: Knockdown Left
    Combat:
        Row 1: Attack Right
        Row 2: Attack Left
        Row 3: Attack Front
        Row 4: Attack Back

The Wolf asset Legend:
    Movement:
        Row 1: Idle Right
        Row 2: Idle Left
        Row 3: Idle Front
        Row 4: Idle Back
        Row 5: Walk Right
        Row 6: Walk Left
        Row 7: Walk Front
        Row 8: Walk Back
        Row 9: Run Right
        Row 10: Run Left
        Row 11: Run Front
        Row 12: Run Back
        Run 13: Knockdown Right
        Row 14: Knockdown Left
    Combat:
        Row 1: Attack Right
        Row 2: Attack Left
        Row 3: Attack Front
        Row 4: Attack Back

The Beatle asset legend:
    Movement:
        Row 1: Idle Right
        Row 2: Idle Left
        Row 3: Idle Front
        Row 4: Idle Back
        Row 5: Walk Right
        Row 6: Walk Left
        Row 7: Walk Front
        Row 8: Walk Back
        Row 9: Run Right
        Row 10: Run Left
        Row 11: Run Front
        Row 12: Run Back
        Run 13: Knockdown Right
        Row 14: Knockdown Left
    Combat:
        Row 1: Attack Right
        Row 2: Attack Left
        Row 3: Attack Front
        Row 4: Attack Back

The Snake asset legend:
    Movement:
        Row 1: Idle Right
        Row 2: Idle Left
        Row 3: Walk Right
        Row 4: Walk Left
        Row 5: Walk Front
        Row 6: Walk Back
        Run 7: Knockdown Right
        Row 8: Knockdown Left
    Combat:
        Row 1: Attack Right
        Row 2: Attack Left
        Row 3: Attack Front
        Row 4: Attack Back

## Spells

`Elemental_Spellcasting_Effects_v1_Anti_Alias_glow_8x8.png`
This sheet contains 8x8 Sprites.
The projectile have multiple levels. 
    Row 1: Stage 1 Lightning
    Row 2: Stage 2 Lightning
    Row 3: Stage 3 Lightning
    Row 4: Stage 1 Dark
    Row 5: Stage 2 Dark
    Row 6: Stage 3 Dark
    Row 7: Stage 1 Fire
    Row 8: Stage 2 Fire
    Row 9: Stage 3 Fire
    Row 10: Stage 1 Ice
    Row 11: Stage 2 Ice
    Row 12: Stage 3 Ice pt 1
    Row 13: Stage 3 Ice pt 2
    Row 14: Stage 1 Wind
    Row 15: Stage 2 Wind
    Row 16: Stage 3 Wind pt 1
    Row 17: Stage 3 Wind pt 2
    Row 18: Stage 1 Earth
    Row 19: Stage 2 Earth
    Row 20: Stage 3 Earth
    Row 21: Stage 1 Blood
    Row 22: Stage 2 Blood
    Row 23: Stage 3 Blood
    Row 24: Stage 1 Cyclone
    Row 25: Stage 2 Cyclone
    Row 26: Stage 3 Cyclone

`Extra_Elemental_Spellcasting_Effects_Anti-Alias_glow_14x14.png`
This sheet contains 14x14 Sprites.
    Row 1: Stage 1 Rock
    Row 2: Stage 2 Rock
    Row 3: Stage 3 Rock
    Row 4: Stage 1 Matter
    Row 5: Stage 2 Matter
    Row 6: Stage 3 Matter
    Row 7: Stage 1 Dark Matter
    Row 8: Stage 2 Dark Matter
    Row 9: Stage 3 Dark Matter

`Fire_Explosion_Anti-Alias_glow.png`
This sheet contains 28x28 Sprites.
This sheet is horizontal, instead of vertical and play in sequence starting from the left.

`Fire_Explosion_ISOMETRIC_Anti-Alias_glow_28x28.png`
This sheet contains 28x28 Sprites.
This sheet is horizontal, instead of vertical and play in sequence starting from the left.

`Ice-Burst_crystal_48x48_Anti-Alias_glow.png`
`Ice-Burst_dark-blue_outline_48x48.png`
`Ice-Burst_light-grey_outline_48x48.png`
`Ice-Burst_transparent-blue_outline_48x48.png`
These sheets contains 48x48 Sprites.
These sheets are horizontal, instead of vertical and play in sequence starting from the left.

`Large_Fire_Anti-Alias_glow_28x28.png`
This sheet contains 28x28 Sprites.
This sheet plays in sequence from left to right, top to bottom. 
    0/1/2/3
    4/5/6/7
    8/9/10/11

`Lightning_Blast_Anti-Alias_glow_54x18.png`
`Red_Lightning_Blast_Anti-Alias_glow_54x18.png`
These sheets contains 54x18 Sprites.
These sheets are horizontal, instead of vertical and play in sequence starting from the left.

`Lightning_Energy_Anti-Alias_glow_48x48.png`
`Red_Energy_Anti-Alias_glow_48x48.png`
These sheets contains 48x48 Sprites.
These sheets are horizontal, instead of vertical and play in sequence starting from the left.

There are also non-aliased versions;
    `Elemental_Spellcasting_Effects_v2_8x8.png`
    `Extra_Elemental_Spellcasting_Effects_14x14.png`
    `Fire_Explosion_28x28.png`
    `Fire_Explosion_ISOMETRIC_28x28.png`
    `Ice-Burst_crystal_48x48.png`
    `Ice-Burst_dark-blue_outline_48x48.png`
    `Ice-Burst_light-grey_outline_48x48.png`
    `Ice-Burst_no_outline_48x48.png`
    `Large_Fire_28x28.png`
    `Lightning_Blast_54x18.png`
    `Lightning_Energy_48x48.png`
    `Red_Energy_48x48.png`
    `Red_Lightning_Blast_54x18.png`
These are identically shaped and formatted.
