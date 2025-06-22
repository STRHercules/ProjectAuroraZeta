using UnityEngine;
using UnityEngine.UI;

namespace ProjectAuroraZeta
{
    /// <summary>
    /// Singleton game manager to track kills and waves.
    /// Displays information via simple UI Text elements.
    /// </summary>
    public class GameManager : MonoBehaviour
    {
        public static GameManager Instance { get; private set; }
        public Text killsText;
        public Text waveText;
        public Text healthText;
        public Text currencyText;
        public int moneyPerKill = 1;
        public GameObject gameOverPanel;
        public enum Difficulty
        {
            VeryEasy,
            Easy,
            Normal,
            Hard,
            Chaotic,
            Insane
        }

        public Difficulty currentDifficulty = Difficulty.Normal;
        public float[] difficultyHealthMultipliers = { 0.8f, 0.9f, 1f, 1.2f, 1.4f, 1.6f };
        public int[] difficultyStartWaves = { 1, 1, 1, 10, 20, 40 };

        private int _kills;
        private int _wave;
        private int _currency;

        /// <summary>
        /// Resets internal counters and hides the game over panel.
        /// Call this when starting or restarting the game.
        /// </summary>
        public void ResetGame()
        {
            _kills = 0;
            _wave = GetStartingWave() - 1;
            _currency = 0;
            if (gameOverPanel != null)
            {
                gameOverPanel.SetActive(false);
            }
            UpdateUI();
        }

        void Awake()
        {
            if (Instance != null && Instance != this)
            {
                Destroy(gameObject);
                return;
            }
            Instance = this;
            DontDestroyOnLoad(gameObject);
            UpdateUI();
        }

        public void SetDifficulty(Difficulty difficulty)
        {
            currentDifficulty = difficulty;
        }

        public float GetEnemyHealthMultiplier()
        {
            int index = (int)currentDifficulty;
            if (index >= 0 && index < difficultyHealthMultipliers.Length)
                return difficultyHealthMultipliers[index];
            return 1f;
        }

        public int GetStartingWave()
        {
            int index = (int)currentDifficulty;
            if (index >= 0 && index < difficultyStartWaves.Length)
                return difficultyStartWaves[index];
            return 1;
        }

        public void UpdatePlayerHealth(int health)
        {
            if (healthText != null)
            {
                healthText.text = $"HP: {health}";
            }
        }

        public void PlayerDied()
        {
            if (gameOverPanel != null)
            {
                gameOverPanel.SetActive(true);
            }
            Time.timeScale = 0f;
        }

        public void RegisterKill()
        {
            _kills++;
            AddMoney(moneyPerKill);
        }

        public void NextWave(int wave)
        {
            _wave = wave;
            UpdateUI();
        }

        public void AddMoney(int amount)
        {
            _currency += amount;
            UpdateUI();
        }

        public bool SpendMoney(int amount)
        {
            if (_currency < amount) return false;
            _currency -= amount;
            UpdateUI();
            return true;
        }

        private void UpdateUI()
        {
            if (killsText != null) killsText.text = $"Kills: {_kills}";
            if (waveText != null) waveText.text = $"Wave: {_wave}";
            if (currencyText != null) currencyText.text = $"$ {_currency}";
        }
    }
}
