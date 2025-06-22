using UnityEngine;

namespace ProjectAuroraZeta
{
    /// <summary>
    /// Simple projectile that travels forward and destroys
    /// itself after a short lifetime. Destroys enemies on hit.
    /// </summary>
    public class Bullet : MonoBehaviour
    {
        public float speed = 10f;
        public float lifetime = 2f;
        public int damage = 1;

        void Awake()
        {
            Destroy(gameObject, lifetime);
        }

        void Update()
        {
            transform.Translate(Vector2.up * speed * Time.deltaTime);
        }

        void OnTriggerEnter2D(Collider2D other)
        {
            if (other.CompareTag("Enemy"))
            {
                var health = other.GetComponent<EnemyHealth>();
                if (health != null)
                {
                    health.TakeDamage(damage);
                }
                else
                {
                    Destroy(other.gameObject);
                }
                Destroy(gameObject);
            }
        }
    }
}
