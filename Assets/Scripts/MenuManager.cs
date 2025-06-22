using UnityEngine;
using UnityEngine.SceneManagement;

namespace ProjectAuroraZeta
{
    /// <summary>
    /// Simple menu controller for loading scenes and quitting the game.
    /// Attach this to buttons in the menu UI.
    /// </summary>
    public class MenuManager : MonoBehaviour
    {
        public void StartGameWithDifficulty(int difficulty)
        {
            if (GameManager.Instance != null)
            {
                GameManager.Instance.SetDifficulty((GameManager.Difficulty)difficulty);
            }
            StartGame();
        }

        public void StartGame()
        {
            SceneManager.LoadScene("Main");
            Time.timeScale = 1f;
            if (GameManager.Instance != null)
            {
                GameManager.Instance.ResetGame();
            }
        }

        /// <summary>
        /// Reloads the current active scene. Useful for a restart button on the
        /// game over panel.
        /// </summary>
        public void RestartGame()
        {
            Scene current = SceneManager.GetActiveScene();
            SceneManager.LoadScene(current.name);
            Time.timeScale = 1f;
            if (GameManager.Instance != null)
            {
                GameManager.Instance.ResetGame();
            }
        }

        public void QuitGame()
        {
            #if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying = false;
            #else
            Application.Quit();
            #endif
        }
    }
}

