#pragma once

// Keeps the display awake while the live view is up (SPEC 4.2). Qt has no API for this, so on
// Windows it calls SetThreadExecutionState; elsewhere it is a no-op that only tracks state.
// The Windows setting is per thread: acquire and release must run on the GUI thread, the one
// that lives for the whole session. acquire/release are idempotent; the destructor releases.
class DisplaySleepInhibitor
{
public:
    DisplaySleepInhibitor() = default;
    ~DisplaySleepInhibitor();
    DisplaySleepInhibitor(const DisplaySleepInhibitor &) = delete;
    DisplaySleepInhibitor &operator=(const DisplaySleepInhibitor &) = delete;

    void acquire();
    void release();
    bool isActive() const { return m_active; }

private:
    bool m_active = false;
};
