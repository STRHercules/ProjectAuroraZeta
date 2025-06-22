using UnityEngine;

namespace ProjectAuroraZeta
{
    /// <summary>
    /// Handles firing projectiles from the player.
    /// </summary>
    public class Gun : MonoBehaviour
    {
        public GameObject bulletPrefab;
        public Transform firePoint;
        public float fireRate = 0.25f;
        public int bulletDamage = 1;
        public int damageUpgradeCost = 10;
        private float _nextFire;

        void Update()
        {
            if (Input.GetButton("Fire1") && Time.time >= _nextFire)
            {
                var bullet = Instantiate(bulletPrefab, firePoint.position, firePoint.rotation);
                var bulletComp = bullet.GetComponent<Bullet>();
                if (bulletComp != null)
                {
                    bulletComp.damage = bulletDamage;
                }
                _nextFire = Time.time + fireRate;
            }

            if (Input.GetKeyDown(KeyCode.U))
            {
                if (GameManager.Instance != null && GameManager.Instance.SpendMoney(damageUpgradeCost))
                {
                    bulletDamage++;
                }
            }
        }
    }
}
