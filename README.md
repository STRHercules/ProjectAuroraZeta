# Project Aurora Zeta

This repository contains the beginnings of a Unity-based top down shooter. The concept is that players select heroes and survive waves of enemies on the planet **Zeta**. This project will build out a wave-based system, player controls, and other mechanics.

## Development

1. Open the repository in Unity 2022 or later.
2. Ensure you have the `.NET Standard 2.1` compatibility level enabled.
3. Scripts are located in `Assets/Scripts`.

## Directory Layout

```
Assets/
  Scripts/
    WaveManager.cs   - Handles basic wave spawning.
    PlayerController.cs - Top down player movement and rotation.
    SimpleEnemy.cs   - Basic enemy AI that chases the player.
    Gun.cs           - Fires bullets when the player presses Fire1 and can upgrade damage.
    Bullet.cs        - Simple projectile that destroys enemies on contact.
    EnemyHealth.cs   - Tracks enemy hit points and scales with difficulty.
    PlayerHealth.cs  - Handles player hit points.
    CameraController.cs - Moves and zooms the camera.
    GameManager.cs   - Tracks kills, waves, currency and player health.
    MenuManager.cs   - Handles menu buttons for starting and quitting the game.
    (GameManager also stores difficulty settings.)
  Scenes/
    Main.unity       - Example scene.
    Menu.unity       - Simple main menu.
```

## Getting Started

1. Open `Scenes/Menu.unity` in Unity for a simple starting menu or load `Main.unity` directly.
2. Attach the `WaveManager` script to an empty GameObject.
3. Create a player GameObject and attach `PlayerController` and `PlayerHealth` with a `Rigidbody2D`. Set its tag to `Player`.
4. Add a child GameObject to the player with a `Gun` component and assign a bullet prefab.
5. Configure enemy prefabs with `SimpleEnemy` and `EnemyHealth` components and set spawn points in the `WaveManager` inspector. Adjust `attackDamage` on the enemy as desired and ensure they are tagged `Enemy`.
6. Attach `CameraController` to your main camera so you can pan with WASD/arrow keys and zoom with the mouse wheel.
7. Create a UI Canvas with Text elements for kills, wave count, player HP and currency. Optionally add a panel that will show when the game ends.
8. Add a GameObject with the `GameManager` script and assign the Texts (including the currency Text) and panel.
9. Press Play and use the left mouse button or Ctrl to shoot. When the player dies, the game over panel becomes visible and time is paused.
10. Optionally create a menu canvas in `Menu.unity` with buttons that call `MenuManager.StartGame` or `MenuManager.QuitGame`.
11. Add a restart button to your game over panel that calls `MenuManager.RestartGame`.
12. Both `StartGame` and `RestartGame` reset the kill and wave counters through `GameManager.ResetGame`.
13. Press **U** during gameplay to spend currency and increase bullet damage by one.
14. Set the game difficulty in the `GameManager` inspector or create menu buttons that call `MenuManager.StartGameWithDifficulty` with an index.

This is a work in progress.
