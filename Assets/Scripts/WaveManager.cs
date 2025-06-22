using UnityEngine;
using System.Collections.Generic;

namespace ProjectAuroraZeta
{
    /// <summary>
    /// Basic wave spawning logic. Spawns enemies at timed intervals.
    /// Attach this to an empty GameObject in the scene.
    /// </summary>
    public class WaveManager : MonoBehaviour
    {
        [Tooltip("List of enemy prefabs to spawn")] public List<GameObject> enemyPrefabs;
        [Tooltip("Where enemies spawn from")] public Transform[] spawnPoints;
        public float timeBetweenSpawns = 5f;
        private float _timer;
        private int _wave;

        void Start()
        {
            if (GameManager.Instance != null)
            {
                _wave = GameManager.Instance.GetStartingWave() - 1;
            }
        }

        void Update()
        {
            _timer += Time.deltaTime;
            if (_timer >= timeBetweenSpawns)
            {
                SpawnWave();
                _timer = 0f;
            }
        }

        void SpawnWave()
        {
            _wave++;
            if (GameManager.Instance != null)
            {
                GameManager.Instance.NextWave(_wave);
            }
            foreach (var spawn in spawnPoints)
            {
                var prefab = enemyPrefabs[Random.Range(0, enemyPrefabs.Count)];
                Instantiate(prefab, spawn.position, spawn.rotation);
            }
            Debug.Log($"Wave {_wave} spawned");
        }
    }
}
