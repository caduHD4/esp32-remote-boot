"""Opt-in ESP integration tests with simulated PCs; never operates OS power/EFI.

Requires an empty schema-3 ESP and an explicitly supplied private JSON file with
admin_token. Pass --prepare only to set that password on first access.
Uses reserved locally administered MACs; removes only PCs it creates.
"""
import argparse
import json
import secrets
import time
import urllib.error
import urllib.request
from pathlib import Path
from urllib.parse import urlparse
from websockets.sync.client import connect

parser = argparse.ArgumentParser()
parser.add_argument("--url", required=True)
parser.add_argument("--credential-file", type=Path, required=True)
parser.add_argument("--prepare", action="store_true")
parser.add_argument("--soak-seconds", type=int, default=120)
parser.add_argument("--report", type=Path, default=Path("build/live-multi-pc-report.json"))
args = parser.parse_args()
private = json.loads(args.credential_file.read_text(encoding="utf-8"))
password = private["admin_token"]
checks = []
created = []
sockets = []
bindings = []
latencies = []


def api(path, method="GET", payload=None, token=password, session=None, expected=200):
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    if session:
        headers["X-Agent-Session"] = session
    request = urllib.request.Request(args.url.rstrip("/") + path, data=None if payload is None else json.dumps(payload).encode(), headers=headers, method=method)
    started=time.monotonic()
    try:
        response = urllib.request.urlopen(request, timeout=12)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        data = response.read().decode()
        latencies.append(time.monotonic()-started)
        assert response.status == expected, f"{method} {path}: expected {expected}, got {response.status}"
        return json.loads(data)


def check(name, condition=True):
    assert condition, name
    checks.append(name)
    print("PASS:", name, flush=True)


def wait_online():
    for attempt in range(45):
        time.sleep(1)
        try:
            return api("/api/v2/bootstrap")
        except (OSError, AssertionError):
            pass
    raise AssertionError("ESP did not return after reboot")


def start_pair():
    return api("/api/v2/pairing/start", "POST", {"hostname": "simulated-PC", "os": "FakeHost"}, token=None, expected=201)


def approve_pair(pair, pc):
    found = api("/api/v2/pairing/lookup", "POST", {"code": pair["user_code"]})
    assert found["pairing_id"] == pair["pairing_id"]
    api(f"/api/v2/pairing/{pair['pairing_id']}/approve", "POST", {"pc_id": pc, "installation_name": "Hardware test FakeHost"})
    poll = f"/api/v2/pairing/{pair['pairing_id']}/poll"
    approved = api(poll, "POST", {}, token=pair["device_secret"])
    repeated = api(poll, "POST", {}, token=pair["device_secret"])
    assert approved == repeated and approved["pc_id"] == pc and len(approved["token"]) == 64
    approved["pair"] = pair
    return approved


def confirm(binding):
    pair = binding["pair"]
    api(f"/api/v2/pairing/{pair['pairing_id']}/confirm", "POST", {}, token=pair["device_secret"])


def connect_agent(binding):
    host = urlparse(args.url).hostname
    socket = connect(f"ws://{host}:81/agent/v2", subprotocols=["arduino"], open_timeout=8, close_timeout=1, legacy=True)
    session = secrets.token_hex(16)
    socket.send(json.dumps({"type": "hello", "protocol": 2, "agent_id": binding["agent_id"], "token": binding["token"], "session_id": session, "hostname": "FakeHost", "os": "Simulated", "boot_id": "0001", "permissions": {"reboot": True, "shutdown": True}}))
    ready = json.loads(socket.recv(timeout=6))
    assert ready["type"] == "ready" and ready["pc_id"] == binding["pc_id"] and ready["session_id"] == session
    binding["session"] = session
    return socket


def sync(binding, systems, extra=None, expected=200):
    return api("/api/v2/agent/systems/sync", "POST", {"systems": systems, **(extra or {})}, token=binding["token"], session=binding["session"], expected=expected)


try:
    setup = api("/api/v2/setup", token=None)
    if setup["required"]:
        assert args.prepare, "Fresh ESP requires --prepare"
        api("/api/v2/setup", "POST", {"password": password, "repeat_password": "mismatch"}, token=None, expected=400)
        api("/api/v2/setup", "POST", {"password": password, "repeat_password": password}, token=None)
        time.sleep(2)
        wait_online()
    initial = api("/api/v2/bootstrap")
    assert initial["pcs"] == [] and initial["agents"] == [], "Refusing to test a nonempty ESP"
    check("first-access setup and zero-PC login")
    api("/api/v2/setup", "POST", {"password": password, "repeat_password": password}, token=None, expected=409)
    api("/api/v2/bootstrap", token="incorrect-password", expected=403)
    api("/api/v1/heartbeat", "POST", {}, expected=410)
    check("setup closure, auth rejection and legacy rejection")
    for i in range(4):
        pc = api("/api/v2/pcs", "POST", {"name": f"Simulated PC {i+1}", "mac": f"02:00:00:FE:00:{i+1:02X}"}, expected=201)["pc"]
        created.append(pc["pc_id"])
    api("/api/v2/pcs", "POST", {"name": "Overflow", "mac": "02:00:00:FE:00:05"}, expected=409)
    api(f"/api/v2/pcs/{created[1]}", "PUT", {"mac": "02:00:00:fe:00:01"}, expected=400)
    check("4-PC limit and canonical duplicate MAC rejection")
    api("/api/v2/pairing/window", "POST", {})
    pending = [start_pair() for _ in range(3)]
    api("/api/v2/pairing/start", "POST", {"hostname": "Overflow", "os": "FakeHost"}, token=None, expected=429)
    check("3 pending pairing capacity")
    for i, pair in enumerate(pending):
        bindings.append(approve_pair(pair, created[i]))
        sockets.append(connect_agent(bindings[-1]))
        catalog = [{"id": f"{n+1:04X}", "name": f"PC{i+1} system {n+1} " + "x"*40, "hidden": False, "blocked": False} for n in range(24)]
        sync(bindings[-1], catalog)
        confirm(bindings[-1])
        check(f"PC {i+1} catalog synchronized with 24 entries")
    fourth = start_pair()
    confirm(bindings[0])  # lost confirmation response after reuse of its pending slot
    bindings.append(approve_pair(fourth, created[3]))
    sockets.append(connect_agent(bindings[-1]))
    sync(bindings[-1], [{"id": f"{n+1:04X}", "name": "PC4 system " + str(n+1) + "x"*40} for n in range(24)])
    confirm(bindings[-1])
    check("four simultaneous authenticated agents, 24 entries each and idempotent pairing")
    snapshot = api("/api/v2/bootstrap")
    assert all(pc["status"]["online"] for pc in snapshot["pcs"])
    assert len(snapshot["agents"]) == 4
    public = json.dumps(snapshot)
    assert password not in public and all(binding["token"] not in public for binding in bindings)
    check("bootstrap presence and secret redaction")
    for pc in created:
        assert "systems" not in api(f"/api/v2/pcs/{pc}")
        page = api(f"/api/v2/pcs/{pc}/systems?offset=8&limit=8")
        assert page["total"] == 24 and len(page["systems"]) == 8 and page["systems"][0]["id"] == "0009"
    api(f"/api/v2/pcs/{created[0]}/systems?offset=0&limit=24", expected=400)
    check("versioned pagination and no full-catalog bypass")
    catalog=[]
    for offset in range(0,24,8):
        catalog.extend(api(f"/api/v2/pcs/{created[0]}/systems?offset={offset}&limit=8")["systems"])
    catalog[0]["hidden"]=True
    api(f"/api/v2/pcs/{created[0]}/systems","PUT",{"systems":catalog})
    before=api(f"/api/v2/pcs/{created[0]}/systems")
    incoming=[{**entry,"hidden":False,"ignored":"x"*50} for entry in reversed(catalog)]
    sync(bindings[0],incoming)
    assert api(f"/api/v2/pcs/{created[0]}/systems")==before
    sync(bindings[0],[{"id":"00af","name":"A"},{"id":"00AF","name":"B"}],expected=400)
    check("catalog order/hidden preserved, unchanged generation and canonical duplicate IDs")
    prior = api(f"/api/v2/pcs/{created[1]}/systems")
    sync(bindings[0], [{"id": "0001", "name": "Only PC one"}], {"pc_id": created[1]})
    assert api(f"/api/v2/pcs/{created[1]}/systems") == prior
    sync(bindings[0], [{"id": "0001", "name": "A"}, {"id": "0001", "name": "B"}], expected=400)
    api("/api/v2/agent/systems/sync", "POST", {"systems": []}, token=bindings[0]["token"], session=bindings[1]["session"], expected=409)
    check("token-derived PC scope, stale sessions and duplicate catalog rejection")
    api(f"/api/v2/pcs/{created[1]}/discovery/request", "POST", {}, expected=202)
    discovery = json.loads(sockets[1].recv(timeout=6))
    assert discovery["type"] == "discover" and discovery["pc_id"] == created[1]
    check("targeted discovery route and identity")
    binding=bindings[1]
    api(f"/api/v2/pcs/{created[1]}/shutdown", "POST", {"confirm": "SHUTDOWN"}, expected=202)
    command=json.loads(sockets[1].recv(timeout=6));assert command["type"]=="command" and command["pc_id"]==created[1]
    sockets[1].send(json.dumps({"type":"ack","id":command["id"],"pc_id":binding["pc_id"],"agent_id":binding["agent_id"],"session_id":binding["session"]}))
    accepted=json.loads(sockets[1].recv(timeout=6));assert accepted["accepted"]
    sockets[1].send(json.dumps({"type":"result","id":command["id"],"requested":False,"pc_id":binding["pc_id"],"agent_id":binding["agent_id"],"session_id":binding["session"]}))
    check("simulated shutdown ACK/result without OS actions")
    # Restore full capacity before the soak and persistent restart test.
    sync(bindings[0], [{"id": f"{n+1:04X}", "name": "PC1 capacity " + str(n+1) + "x"*40} for n in range(24)])
    before=api(f"/api/v2/pcs/{created[0]}/systems")
    api(f"/api/v2/pcs/{created[0]}","PUT",{"default_target":"FFFF"},expected=400)
    saved_name=api(f"/api/v2/pcs/{created[0]}")["name"]
    api(f"/api/v2/pcs/{created[0]}","PUT",{"name":"Must roll back","unknown_field":True},expected=400)
    assert api(f"/api/v2/pcs/{created[0]}")["name"]==saved_name
    assert api(f"/api/v2/pcs/{created[0]}/systems")==before
    assert all(pc["status"]["online"] for pc in api("/api/v2/bootstrap")["pcs"])
    check("invalid changes roll back durable configuration without disconnecting agents")
    minimum_heap=2**32;minimum_largest=2**32;tailscale_samples=0;tailscale_expected=initial["status"]["tailscale"]["configured"]
    end=time.monotonic()+args.soak_seconds;last_uptime=0
    print("SOAK START: 4 PCs, 96 entries, 4 sockets",flush=True)
    while time.monotonic()<end:
        status=api("/api/v2/status")
        assert status["uptime"]>=last_uptime,"Unexpected ESP restart during soak"
        last_uptime=status["uptime"]
        minimum_heap=min(minimum_heap,status["heap"])
        minimum_largest=min(minimum_largest,status["tailscale"]["largest_block"])
        if status["tailscale"]["control_online"] and status["tailscale"]["derp_online"]:
            tailscale_samples+=1
        assert all(pc["status"]["online"] for pc in api("/api/v2/bootstrap")["pcs"])
        time.sleep(2)
    check(f"capacity soak {args.soak_seconds}s with 4 sockets and 96 entries")
    if tailscale_expected:
        assert tailscale_samples>=10,"Tailscale control and DERP must remain connected under capacity load"
        check("Tailscale control and DERP online during full-capacity soak")
    assert args.soak_seconds>=61, "Rate-limit reset requires at least 61 seconds soak"
    for i,pc in enumerate(created):
        extra=approve_pair(start_pair(),pc)
        # A second installation cannot take over the active PC session.
        try:
            duplicate=connect_agent(extra)
        except Exception:
            pass
        else:
            duplicate.close()
            raise AssertionError("Second active session was accepted")
        sockets[i].close();time.sleep(.2)
        replacement=connect_agent(extra)
        confirm(extra)
        replacement.close();time.sleep(.2)
        sockets[i]=connect_agent(bindings[i])
        bindings.append(extra)
    check("8 agent bindings, with Windows/Linux-style alternate installations and one active session per PC")
    overflow=start_pair()
    api(f"/api/v2/pairing/{overflow['pairing_id']}/approve","POST",{"pc_id":created[0],"installation_name":"Overflow"},expected=409)
    api(f"/api/v2/pairing/{overflow['pairing_id']}","DELETE")
    check("8-binding capacity rejection")
    # Revoke a binding and prove its HTTP access is gone immediately.
    api(f"/api/v2/agents/{bindings[0]['agent_id']}","DELETE")
    sync(bindings[0], [], expected=403)
    check("immediate agent revocation")
    for socket in sockets:socket.close()
    sockets.clear()
    api("/api/v2/system/reboot","POST",{},expected=202)
    time.sleep(2);restored=wait_online()
    assert len(restored["pcs"])==4 and len(restored["agents"])==7
    assert api(f"/api/v2/pcs/{created[3]}/systems")["total"]==24
    check("NVS durable reload of full per-PC configuration")
    api("/api/v2/integrations/sinric","PUT",{"sinric_pc_id":created[0],"sinric_enabled":False,"sinric_slots":[]})
    time.sleep(2);wait_online()
    slot={"device_id":"0123456789abcdef01234567","boot_id":"0001"}
    api("/api/v2/integrations/sinric","PUT",{"sinric_pc_id":created[0],"sinric_enabled":False,"sinric_slots":[slot]})
    time.sleep(2);wait_online()
    api("/api/v2/integrations/sinric","PUT",{"sinric_pc_id":created[1],"sinric_enabled":False,"sinric_slots":[slot]})
    time.sleep(2);restored=wait_online()
    assert restored["config"]["sinric_slots"]==[]
    api(f"/api/v2/pcs/{created[1]}","DELETE",{"confirm":"DELETE_PC"})
    created.remove(created[1]);restored=api("/api/v2/bootstrap")
    assert restored["config"]["sinric_pc_id"]=="" and not restored["config"]["sinric_enabled"]
    check("Sinric selection clears stale mappings; removal disables selected integration")
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps({"checks":checks,"minimum_heap":minimum_heap,"minimum_largest_block":minimum_largest,"maximum_http_latency_s":round(max(latencies),3),"tailscale_online_samples":tailscale_samples,"soak_seconds":args.soak_seconds,"real_os_power_actions":False},indent=2)+"\n")
finally:
    for socket in sockets:
        try:socket.close()
        except OSError:pass
    for pc in created:
        try:api(f"/api/v2/pcs/{pc}","DELETE",{"confirm":"DELETE_PC"})
        except (OSError,AssertionError):print("Cleanup pending for a simulated PC",flush=True)
