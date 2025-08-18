using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;
using System.Collections.Generic;

namespace AuroraZetaGame;

public class Player
{
    public Vector2 Position;
    private readonly Texture2D _texture;
    private readonly Texture2D _bulletTexture;
    private readonly List<Bullet> _bullets = new();
    private const float Speed = 200f;

    public IEnumerable<Bullet> Bullets => _bullets;

    public Player(Texture2D texture, Texture2D bulletTexture, Vector2 startPosition)
    {
        _texture = texture;
        _bulletTexture = bulletTexture;
        Position = startPosition;
    }

    public void Update(GameTime gameTime)
    {
        var keyboard = Keyboard.GetState();
        Vector2 direction = Vector2.Zero;
        if (keyboard.IsKeyDown(Keys.W)) direction.Y -= 1;
        if (keyboard.IsKeyDown(Keys.S)) direction.Y += 1;
        if (keyboard.IsKeyDown(Keys.A)) direction.X -= 1;
        if (keyboard.IsKeyDown(Keys.D)) direction.X += 1;

        if (direction != Vector2.Zero)
        {
            direction.Normalize();
            Position += direction * Speed * (float)gameTime.ElapsedGameTime.TotalSeconds;
        }

        if (Mouse.GetState().LeftButton == ButtonState.Pressed)
        {
            FireBullet();
        }

        for (int i = _bullets.Count - 1; i >= 0; i--)
        {
            _bullets[i].Update(gameTime);
            if (!_bullets[i].IsAlive)
                _bullets.RemoveAt(i);
        }
    }

    private void FireBullet()
    {
        Vector2 mousePos = Mouse.GetState().Position.ToVector2();
        Vector2 dir = mousePos - Position;
        if (dir != Vector2.Zero)
            dir.Normalize();
        _bullets.Add(new Bullet(_bulletTexture, Position + new Vector2(10,10), dir));
    }

    public void Draw(SpriteBatch spriteBatch)
    {
        spriteBatch.Draw(_texture, new Rectangle((int)Position.X, (int)Position.Y, 20, 20), Color.Cyan);
        foreach (var bullet in _bullets)
        {
            bullet.Draw(spriteBatch);
        }
    }
}
