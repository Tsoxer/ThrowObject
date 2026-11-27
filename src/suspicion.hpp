#pragma once

namespace RE { class Actor; }

namespace to
{
void loadSuspicionVoices();
bool playSuspicion(RE::Actor& target, bool allowConcurrent = false);
void updateSuspicion(float dt);
bool suspicionActive();
void stopSuspicion();
}
