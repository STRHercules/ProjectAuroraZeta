using UnityEngine;

namespace ProjectAuroraZeta
{
    /// <summary>
    /// Simple camera controller for panning and zooming in a top down view.
    /// Attach this to the main camera.
    /// </summary>
    [RequireComponent(typeof(Camera))]
    public class CameraController : MonoBehaviour
    {
        public float moveSpeed = 10f;
        public float zoomSpeed = 5f;
        public float minZoom = 2f;
        public float maxZoom = 20f;
        private Camera _camera;

        void Awake()
        {
            _camera = GetComponent<Camera>();
        }

        void Update()
        {
            float h = Input.GetAxis("Horizontal");
            float v = Input.GetAxis("Vertical");
            Vector3 move = new Vector3(h, v, 0f) * moveSpeed * Time.deltaTime;
            transform.position += move;

            float scroll = Input.GetAxis("Mouse ScrollWheel");
            if (Mathf.Abs(scroll) > 0.01f)
            {
                float size = _camera.orthographicSize - scroll * zoomSpeed;
                _camera.orthographicSize = Mathf.Clamp(size, minZoom, maxZoom);
            }
        }
    }
}
