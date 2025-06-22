using UnityEngine;

namespace ProjectAuroraZeta
{
    /// <summary>
    /// Simple top down controller for moving a player character with WASD/arrow keys.
    /// The player faces the mouse cursor.
    /// </summary>
    [RequireComponent(typeof(Rigidbody2D))]
    public class PlayerController : MonoBehaviour
    {
        public float moveSpeed = 5f;
        private Rigidbody2D _rb;

        void Awake()
        {
            _rb = GetComponent<Rigidbody2D>();
        }

        void Update()
        {
            // Look at mouse position in world space
            var mousePosition = Camera.main.ScreenToWorldPoint(Input.mousePosition);
            Vector2 direction = (mousePosition - transform.position).normalized;
            float angle = Mathf.Atan2(direction.y, direction.x) * Mathf.Rad2Deg - 90f;
            _rb.rotation = angle;
        }

        void FixedUpdate()
        {
            float h = Input.GetAxis("Horizontal");
            float v = Input.GetAxis("Vertical");
            Vector2 movement = new Vector2(h, v).normalized * moveSpeed;
            _rb.velocity = movement;
        }
    }
}
