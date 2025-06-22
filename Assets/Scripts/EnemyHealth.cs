using UnityEngine;

namespace ProjectAuroraZeta
{
    /// <summary>
    /// Basic health component for enemies. Destroy the GameObject when health reaches zero.
    /// </summary>
    public class EnemyHealth : MonoBehaviour
    {
        public int maxHealth = 3;
        private int _current;

        void Awake()
        {
            _current = maxHealth;
            if (GameManager.Instance != null)
            {
                float mult = GameManager.Instance.GetEnemyHealthMultiplier();
                _current = Mathf.CeilToInt(_current * mult);
            }
        }

        public void TakeDamage(int amount)
        {
            _current -= amount;
            if (_current <= 0)
            {
                if (GameManager.Instance != null)
                {
                    GameManager.Instance.RegisterKill();
                }
                Destroy(gameObject);
            }
        }
    }
}
