using UnityEngine;

namespace ProjectAuroraZeta
{
    /// <summary>
    /// Simple health component for the player. Destroys the GameObject when
    /// health reaches zero.
    /// </summary>
    public class PlayerHealth : MonoBehaviour
    {
        public int maxHealth = 5;
        private int _current;

        void Awake()
        {
            _current = maxHealth;
            if (GameManager.Instance != null)
            {
                GameManager.Instance.UpdatePlayerHealth(_current);
            }
        }

        public void TakeDamage(int amount)
        {
            _current -= amount;
            if (GameManager.Instance != null)
            {
                GameManager.Instance.UpdatePlayerHealth(_current);
            }

            if (_current <= 0)
            {
                Debug.Log("Player died");
                if (GameManager.Instance != null)
                {
                    GameManager.Instance.PlayerDied();
                }
                Destroy(gameObject);
            }
        }
    }
}
