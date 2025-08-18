using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Graphics;

namespace AuroraZetaGame;

public class Bullet
{
    private readonly Texture2D _texture;
    public Vector2 Position;
    private readonly Vector2 _direction;
    private const float Speed = 400f;
    public bool IsAlive { get; private set; } = true;

    public Bullet(Texture2D texture, Vector2 startPosition, Vector2 direction)
    {
        _texture = texture;
        Position = startPosition;
        _direction = direction;
    }

    public void Update(GameTime gameTime)
    {
        Position += _direction * Speed * (float)gameTime.ElapsedGameTime.TotalSeconds;
        if (Position.X < -10 || Position.Y < -10 || Position.X > 2000 || Position.Y > 2000)
        {
            IsAlive = false;
        }
    }

    public void Kill() => IsAlive = false;

    public Rectangle Bounds => new Rectangle((int)Position.X, (int)Position.Y, 4, 4);

    public void Draw(SpriteBatch spriteBatch)
    {
        spriteBatch.Draw(_texture, Bounds, Color.Yellow);
    }
}
