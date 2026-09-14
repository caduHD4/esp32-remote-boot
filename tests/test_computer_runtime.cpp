#include <cassert>
#include <cstdio>

#include "../firmware/include/computer_runtime.hpp"

static void addSystem(rb::ComputerRuntime& runtime, uint16_t id, const char* name) {
    rb::Entry entry{id, "", false, false};
    std::snprintf(entry.name, sizeof entry.name, "%s", name);
    assert(runtime.boot.append(entry));
}

int main() {
    rb::ComputerRuntime desktop;
    rb::ComputerRuntime server;
    desktop.reset("desktop");
    server.reset("server");

    addSystem(desktop, 1, "Windows");
    addSystem(desktop, 2, "Linux");
    addSystem(server, 10, "Server OS");
    desktop.boot.defaultTarget = 1;
    server.boot.defaultTarget = 10;

    assert(desktop.boot.request(2, 100) == 202);
    assert(desktop.boot.pending == 2);
    assert(server.boot.pending == rb::None);
    assert(server.boot.selected(101) == 10);

    desktop.wol.configure(9, 3, 100);
    server.wol.configure(7, 1, 250);
    assert(desktop.wol.enqueue(100));
    assert(desktop.wol.due(100));
    desktop.wol.recordAttempt(100);
    assert(desktop.wol.remaining == 2);
    assert(!desktop.wol.due(199));
    assert(desktop.wol.due(200));
    assert(server.wol.remaining == 0);
    assert(!desktop.wol.enqueue(200));
    desktop.wol.cancel();
    assert(!desktop.wol.enqueue(3099));
    assert(desktop.wol.enqueue(3100));

    desktop.heartbeat(1000, 2, "desktop-host", "Windows", "desktop-session", true, true);
    assert(desktop.boot.online(1001));
    assert(!server.boot.online(1001));
    assert(desktop.hostname == "desktop-host");
    assert(server.hostname.empty());

    assert(desktop.power.enqueue("cmd-a", "reboot", "0001", "desktop-session", 1100));
    assert(server.power.enqueue("cmd-b", "shutdown", "", "server-session", 1100));
    assert(desktop.power.id == "cmd-a");
    assert(server.power.id == "cmd-b");
    assert(desktop.power.heartbeat("desktop-session", "cmd-a", true, 1101));
    assert(desktop.power.id.empty());
    assert(server.power.id == "cmd-b");

    assert(desktop.discoveryGeneration == 1);
    assert(server.discoveryGeneration == 1);
    assert(desktop.requestDiscovery() == 2);
    assert(server.discoveryGeneration == 1);

    desktop.disconnectAgent();
    assert(!desktop.boot.heartbeatSeen);
    assert(desktop.agentSession.empty());
    assert(!desktop.rebootEnabled);
    assert(server.power.id == "cmd-b");
    assert(server.boot.defaultTarget == 10);

    desktop.reset("replacement");
    assert(desktop.id == "replacement");
    assert(desktop.boot.count == 0);
    assert(desktop.boot.pending == rb::None);
    assert(desktop.wol.remaining == 0);
    assert(desktop.discoveryGeneration == 1);

    puts("PASS: isolated boot, WoL, presence, power command and discovery runtimes");
}
