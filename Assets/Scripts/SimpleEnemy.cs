using UnityEngine;

namespace ProjectAuroraZeta
{
    /// <summary>
    /// Simple enemy AI that moves towards the player each frame.
    /// Requires a Rigidbody2D for physics based movement.
    /// </summary>
    [RequireComponent(typeof(Rigidbody2D))]
    public class SimpleEnemy : MonoBehaviour
    {
        public float moveSpeed = 3f;
        public int attackDamage = 1;
        private Rigidbody2D _rb;
        private Transform _player;

        void Awake()
        {
            _rb = GetComponent<Rigidbody2D>();
            var playerObj = GameObject.FindGameObjectWithTag("Player");
            if (playerObj != null)
            {
                _player = playerObj.transform;
            }
        }

        void FixedUpdate()
        {
            if (_player == null) return;
            Vector2 direction = ((Vector2)_player.position - _rb.position).normalized;
            _rb.velocity = direction * moveSpeed;
        }

        void OnCollisionEnter2D(Collision2D collision)
        {
            if (collision.collider.CompareTag("Player"))
            {
                var health = collision.collider.GetComponent<PlayerHealth>();
                if (health != null)
                {
                    health.TakeDamage(attackDamage);
                }
            }
        }
    }
}
