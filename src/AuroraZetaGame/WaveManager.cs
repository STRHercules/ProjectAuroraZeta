using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Graphics;
using System;
using System.Collections.Generic;

namespace AuroraZetaGame;

public class WaveManager
{
    private readonly Texture2D _enemyTexture;
    private readonly List<Enemy> _enemies = new();
    private readonly Random _random = new();
    private double _spawnTimer;
    private int _enemiesToSpawn;
    private const double SpawnInterval = 0.5;
    private const int BaseEnemies = 5;

    public int CurrentWave { get; private set; } = 1;
    public List<Enemy> Enemies => _enemies;

    public WaveManager(Texture2D enemyTexture)
    {
        _enemyTexture = enemyTexture;
        PrepareWave();
    }

    public void Update(GameTime gameTime, Vector2 playerPos)
    {
        if (_enemiesToSpawn > 0)
        {
            _spawnTimer += gameTime.ElapsedGameTime.TotalSeconds;
            if (_spawnTimer >= SpawnInterval)
            {
                SpawnEnemy();
                _spawnTimer = 0;
                _enemiesToSpawn--;
            }
        }

        foreach (var enemy in _enemies)
            enemy.Update(gameTime, playerPos);

        if (_enemies.Count == 0 && _enemiesToSpawn == 0)
        {
            CurrentWave++;
            PrepareWave();
        }
    }

    private void PrepareWave()
    {
        _enemiesToSpawn = BaseEnemies + (CurrentWave - 1) * 2;
    }

    private void SpawnEnemy()
    {
        var spawnPos = new Vector2(_random.Next(0, 800), _random.Next(0, 600));
        _enemies.Add(new Enemy(_enemyTexture, spawnPos));
    }

    public void Draw(SpriteBatch spriteBatch)
    {
        foreach (var enemy in _enemies)
            enemy.Draw(spriteBatch);
    }
}
