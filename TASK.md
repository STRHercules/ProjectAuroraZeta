# StarCraft II–Style Fog of War (Flat 2D, No Height) – Codex Implementation Spec

Implement a StarCraft II–style fog of war system for a **flat 2D, top-down, tile-based** RTS game in C++17 using SDL2.

Important constraints for this spec:

- The game world is **strictly 2D**.
- There is **no terrain height**, no ramps, no cliffs, no elevation bonuses.
- All tiles are on a single plane; vision is computed only in 2D using distances on the map grid.
- For this version, **no line-of-sight occlusion** is implemented; vision is a simple 2D circle around each unit.

## Map and Fog Data

- The map is tile-based, with width and height in tiles.
- There is **no third dimension**; every tile is just `(x, y)` on a flat grid.
- For each player, maintain a `FogOfWarLayer`:

  ```cpp
  enum class FogState : uint8_t
  {
      Unexplored = 0,   // Black, never seen
      Fogged     = 1,   // Explored but not currently visible
      Visible    = 2    // Currently visible
  };

  struct FogOfWarLayer
  {
      int width;
      int height;

      // Current fog state per tile
      std::vector<FogState> fogState;   // size = width * height

      // Permanent exploration memory
      std::vector<bool> explored;       // size = width * height
  };

  inline int index(int x, int y, int width)
  {
      return y * width + x;
  }
  ```

- `FogState` has three states:
  - `Unexplored`: tile has never been seen (black).
  - `Fogged`: tile has been seen in the past but is not currently visible.
  - `Visible`: tile is currently inside the 2D vision radius of at least one friendly unit or building.

- The fog is **per-player**: each player has a separate `FogOfWarLayer` instance.

## Units and Vision (Pure 2D)

Assume there is a list of units:

```cpp
struct Unit
{
    float x;              // world position in pixels (2D)
    float y;
    float visionRadius;   // in tiles (2D radial distance)
    bool  isAlive;
    int   ownerPlayerId;  // which player owns this unit
};
```

Additional assumptions:

- `tileSize` is the size of a tile in pixels.
- To convert from world pixels to tiles:

  ```cpp
  inline int worldToTile(float pos, int tileSize)
  {
      return static_cast<int>(pos) / tileSize;
  }
  ```

Vision characteristics for this spec:

- Vision is **circular in 2D**, centered on the unit's tile.
- There is **no height advantage or penalty**.
- There is **no 3D line-of-sight**; vision depends only on 2D distance on the tile grid.
- Optional blocking tiles or walls can be added later, but are **not part of this base spec**.

## Fog Behavior (Per Frame, Per Player)

Each frame, for each player:

1. **Reset previous visibility**

   For every tile:

   - If `fogState` is `Visible`, then:
     - If `explored` is `true`, set it to `Fogged`.
     - Otherwise, set it to `Unexplored`.

2. **Reveal tiles based on unit vision (2D circle)**

   For each unit that:

   - `isAlive == true`, and
   - `ownerPlayerId == currentPlayerId`,

   do the following:

   - Convert its world position to tile coordinates:

     ```cpp
     int tileX = worldToTile(unit.x, tileSize);
     int tileY = worldToTile(unit.y, tileSize);
     ```

   - Reveal a **2D circular area** around `(tileX, tileY)` with radius `unit.visionRadius` (in tiles).
   - For each tile inside that circle:
     - Set `fogState[tile] = FogState::Visible`.
     - Set `explored[tile] = true`.

For this version, use a simple **filled circle in 2D**, with **no height, no ramps, and no occlusion**.

## FogOfWarLayer API

Implement the following methods:

```cpp
class FogOfWarLayer
{
public:
    FogOfWarLayer(int width, int height);

    void resetVisibility();

    // Simple circular reveal with radius in tiles (2D)
    void revealCircle(int centerX, int centerY, float radiusTiles);

    int getWidth() const;
    int getHeight() const;

    FogState getState(int x, int y) const;
    bool isExplored(int x, int y) const;

    // Optional helpers for direct access
    std::vector<FogState>& data();
    const std::vector<FogState>& data() const;

private:
    int width;
    int height;
    std::vector<FogState> fogState;
    std::vector<bool> explored;
};
```

Implementation rules:

- `FogOfWarLayer(int width, int height)`:
  - Initialize `fogState` to `FogState::Unexplored` for all tiles.
  - Initialize `explored` to `false` for all tiles.

- `void resetVisibility()`:
  - For each tile:
    - If `fogState == FogState::Visible`:
      - If `explored[tile] == true`, set `fogState = FogState::Fogged`.
      - Else set `fogState = FogState::Unexplored`.

- `void revealCircle(int centerX, int centerY, float radiusTiles)`:
  - Use a filled circle algorithm on the 2D grid:
    - Let `r = ceil(radiusTiles)`.
    - For `dx` from `-r` to `+r` and `dy` from `-r` to `+r`:
      - Compute `tx = centerX + dx`, `ty = centerY + dy`.
      - Skip if tile is outside bounds.
      - Compute squared distance: `dist2 = dx*dx + dy*dy`.
      - If `dist2 <= radiusTiles * radiusTiles`:
        - Set `fogState[tx,ty] = FogState::Visible`.
        - Set `explored[tx,ty] = true`.

All calculations are purely 2D, with no height or z-axis involved.

## High-Level Update Function

Provide a helper function:

```cpp
void updateFogOfWar(
    FogOfWarLayer& fog,
    const std::vector<Unit>& units,
    int playerId,
    int tileSize)
{
    fog.resetVisibility();

    for (const Unit& u : units)
    {
        if (!u.isAlive) continue;
        if (u.ownerPlayerId != playerId) continue;

        int tileX = worldToTile(u.x, tileSize);
        int tileY = worldToTile(u.y, tileSize);
        float radiusTiles = u.visionRadius;

        fog.revealCircle(tileX, tileY, radiusTiles);
    }
}
```

This function is called once per frame per player. All positions and distances are in flat 2D (x–y plane only).

## Rendering with SDL2 (2D Top-Down)

Assume:

- The terrain and units are already rendered in 2D world space.
- The game uses a camera defined by:

  ```cpp
  int camX, camY;   // top-left of camera in world pixels (2D)
  int camW, camH;   // width and height of the viewport in pixels
  ```

- There is a `tileSize` in pixels.
- There is a `fogTexture`: a 1x1 RGBA white texture created at startup.

There is **no height-based rendering**; the fog is simply drawn as 2D quads over the tiles.

### Fog Overlay Rendering

Implement:

```cpp
void renderFog(SDL_Renderer* renderer,
               const FogOfWarLayer& fog,
               int tileSize,
               int camX, int camY,
               int camW, int camH,
               SDL_Texture* fogTexture);
```

Behavior:

1. Compute the range of tiles intersecting the camera rectangle:

   ```cpp
   int startTileX = camX / tileSize;
   int startTileY = camY / tileSize;
   int endTileX   = (camX + camW) / tileSize + 1;
   int endTileY   = (camY + camH) / tileSize + 1;

   startTileX = std::max(0, startTileX);
   startTileY = std::max(0, startTileY);
   endTileX   = std::min(fog.getWidth(),  endTileX);
   endTileY   = std::min(fog.getHeight(), endTileY);
   ```

2. For each tile `(tx, ty)` in that range:

   - Get `FogState state = fog.getState(tx, ty);`
   - If `state == FogState::Visible`: draw nothing.
   - If `state == FogState::Fogged`:
     - Draw a semi-transparent dark overlay.
   - If `state == FogState::Unexplored`:
     - Draw an opaque black overlay.

3. To draw an overlay, compute the destination rectangle in screen space:

   ```cpp
   SDL_Rect dst;
   dst.x = tx * tileSize - camX;
   dst.y = ty * tileSize - camY;
   dst.w = tileSize;
   dst.h = tileSize;
   ```

4. Use `fogTexture` scaled over `dst`:

   ```cpp
   // For fogged tiles (semi-transparent)
   SDL_SetTextureColorMod(fogTexture, 0, 0, 0);
   SDL_SetTextureAlphaMod(fogTexture, 160); // tweak alpha as needed
   SDL_RenderCopy(renderer, fogTexture, nullptr, &dst);

   // For unexplored tiles (fully opaque)
   SDL_SetTextureColorMod(fogTexture, 0, 0, 0);
   SDL_SetTextureAlphaMod(fogTexture, 255);
   SDL_RenderCopy(renderer, fogTexture, nullptr, &dst);
   ```

All rendering is basic 2D; there is no layering or change in fog based on height.

### Unit Rendering Rules

- When drawing enemy units for the viewing player:
  - Convert the unit's position to tile coordinates `(tileX, tileY)`.
  - Look up `FogState state = fog.getState(tileX, tileY);`
  - Only draw the enemy unit if `state == FogState::Visible`.

- For the player's own units:
  - Draw them regardless of fog state (typical RTS behavior), or
  - Apply your game-specific rule if different.

Again, everything is strictly 2D; unit rendering does not depend on height.

## Summary

- `FogOfWarLayer` tracks per-tile fog state and exploration per player on a **flat 2D grid**.
- There is **no height**, no ramps, no elevation levels; all tiles are on a single plane.
- Each frame:
  - Reset fog visibility.
  - Reveal **2D circular areas** around each living unit for that player.
- Rendering:
  - Draw terrain and units first (2D).
  - Hide enemy units in non-visible tiles.
  - Overlay fog in 2D:
    - Semi-transparent dark for `Fogged`.
    - Solid black for `Unexplored`.
    - No overlay for `Visible`.

Use the structures and functions defined in this spec to generate clean C++17 and SDL2-compatible code, with headers and sources separated where appropriate. This implementation assumes a **purely flat 2D world with no height dimension.**
