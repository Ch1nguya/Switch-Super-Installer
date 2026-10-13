#pragma once

// Keeps the console awake while SSI works (transfers, backups, album copies) and, when SSM asks for it,
// as long as the console is on a charger (docked): the screen may turn off but the console does not sleep.
namespace inst::power {
    // A job starts / ends. Safe from any thread; the system call itself is made by update() on the UI thread.
    void acquire();
    void release();
    // Option sent by SSM: stay awake while charging.
    void setKeepAwakeOnAc(bool enabled);
    // Apply the current policy. Call regularly from the UI thread (it is cheap).
    void update();
    void shutdown();

    struct Hold {
        Hold() { acquire(); update(); }
        ~Hold() { release(); update(); }
    };
}
