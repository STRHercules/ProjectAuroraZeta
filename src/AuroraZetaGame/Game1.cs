using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;

namespace AuroraZetaGame;

public class Game1 : Game
{
    private GraphicsDeviceManager _graphics;
    private SpriteBatch _spriteBatch;
    private Texture2D _pixel;
    private Player _player;
    private WaveManager _waveManager;
    private SpriteFont _font;
    private int _killCount;

    public Game1()
    {
        _graphics = new GraphicsDeviceManager(this);
        Content.RootDirectory = "Content";
        IsMouseVisible = true;
    }

    protected override void Initialize()
    {
        // TODO: Add your initialization logic here

        base.Initialize();
    }

    protected override void LoadContent()
    {
        _spriteBatch = new SpriteBatch(GraphicsDevice);

        _pixel = new Texture2D(GraphicsDevice, 1, 1);
        _pixel.SetData(new[] { Color.White });

        _player = new Player(_pixel, _pixel, new Vector2(400, 300));
        _waveManager = new WaveManager(_pixel);
        _font = Content.Load<SpriteFont>("DefaultFont");
    }

    protected override void Update(GameTime gameTime)
    {
        if (GamePad.GetState(PlayerIndex.One).Buttons.Back == ButtonState.Pressed || Keyboard.GetState().IsKeyDown(Keys.Escape))
            Exit();

        _player.Update(gameTime);
        _waveManager.Update(gameTime, _player.Position + new Vector2(10, 10));

        var enemies = _waveManager.Enemies;
        foreach (var bullet in _player.Bullets)
        {
            for (int i = enemies.Count - 1; i >= 0; i--)
            {
                if (bullet.Bounds.Intersects(enemies[i].Bounds))
                {
                    bullet.Kill();
                    enemies.RemoveAt(i);
                    _killCount++;
                }
            }
        }

        base.Update(gameTime);
    }

    protected override void Draw(GameTime gameTime)
    {
        GraphicsDevice.Clear(Color.CornflowerBlue);

        _spriteBatch.Begin();
        _player.Draw(_spriteBatch);
        _waveManager.Draw(_spriteBatch);
        _spriteBatch.DrawString(_font, $"Kills: {_killCount}", new Vector2(10, 10), Color.White);
        _spriteBatch.DrawString(_font, $"Wave: {_waveManager.CurrentWave}", new Vector2(10, 30), Color.White);
        _spriteBatch.End();

        base.Draw(gameTime);
    }
}
