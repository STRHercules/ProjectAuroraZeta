// Shared SDL_mixer backend with simple ref-counted lifetime.
#pragma once

namespace Engine::Audio {

// Returns true if SDL2_mixer backend is available and initialized.
bool acquireBackend();

// Releases one reference; when it reaches zero the backend is torn down.
void releaseBackend();

bool backendAvailable();

}  // namespace Engine::Audio

