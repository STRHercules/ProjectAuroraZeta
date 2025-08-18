using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Graphics;

namespace AuroraZetaGame;

public class Enemy
{
    private readonly Texture2D _texture;
    public Vector2 Position;
    private const float Speed = 100f;

    public Enemy(Texture2D texture, Vector2 startPosition)
    {
        _texture = texture;
        Position = startPosition;
    }

    public void Update(GameTime gameTime, Vector2 target)
    {
        Vector2 dir = target - Position;
        if (dir != Vector2.Zero)
            dir.Normalize();
        Position += dir * Speed * (float)gameTime.ElapsedGameTime.TotalSeconds;
    }

    public Rectangle Bounds => new Rectangle((int)Position.X, (int)Position.Y, 20, 20);

    public void Draw(SpriteBatch spriteBatch)
    {
        spriteBatch.Draw(_texture, Bounds, Color.Red);
    }
}
