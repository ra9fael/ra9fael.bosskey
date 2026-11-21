"""Run an installed windowed preview through the real strict plugin runner.

Creates a private runner instance, not the user's resident plugin. The runner
applies its existing restricted token, job limits and image-load mitigations.
"""
import argparse
import json
import os
import queue
import statistics
import struct
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path

import psutil
import win32file
import win32pipe
import win32event
import pywintypes

import desktop
import shot


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[round((len(ordered) - 1) * fraction)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("task", choices=["guard", "sky", "performance", "crash", "disconnect", "autohide"])
    parser.add_argument("output", type=Path)
    parser.add_argument("--exe", type=Path, default=Path(os.environ["LOCALAPPDATA"]) /
                        "WinShun/plugins/ra9fael.bosskey/bosskey.exe")
    parser.add_argument("--host", type=Path, default=Path(r"C:\Program Files\WinShun"))
    parser.add_argument("--target-python", type=Path)
    parser.add_argument("--enhanced", action="store_true", help="request one real UAC consent through the helper")
    parser.add_argument("--simulate-guard-error", type=int,
                        help="inject a helper failure packet instead of requesting UAC (e.g. 1223 or 2)")
    parser.add_argument("--focused-qml", action="store_true", help="disable native hooks and test QML exit alone")
    parser.add_argument("--expect-declined", action="store_true", help="expect a real UAC cancellation and ordinary fallback")
    args = parser.parse_args()
    desktop.require_input_desktop()
    assert not shot.u.FindWindowExW(None, None, None, "BossKeyFocusTarget"), "old test target remains"
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    name = "bosskey-acceptance-" + str(uuid.uuid4())
    pipe = win32pipe.CreateNamedPipe(r"\\.\pipe" + "\\" + name,
                                    win32pipe.PIPE_ACCESS_DUPLEX | win32file.FILE_FLAG_OVERLAPPED,
                                    win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_WAIT, 1, 65536, 65536, 0, None)
    messages = queue.Queue()
    events = []
    runner = None
    child_rc = None
    helpers = []

    def send(message):
        operation = pywintypes.OVERLAPPED()
        operation.hEvent = win32event.CreateEvent(None, True, False, None)
        try:
            win32file.WriteFile(pipe, ("_ " + json.dumps(message) + "\n").encode("utf-8"), operation)
            if win32event.WaitForSingleObject(operation.hEvent, 10000) != win32event.WAIT_OBJECT_0:
                win32file.CancelIoEx(pipe, operation)
                raise TimeoutError("runner control write")
            win32file.GetOverlappedResult(pipe, operation, False)
        finally:
            win32file.CloseHandle(operation.hEvent)

    def read_messages():
        pending = b""
        operation = pywintypes.OVERLAPPED()
        operation.hEvent = win32event.CreateEvent(None, True, False, None)
        try:
            while True:
                win32event.ResetEvent(operation.hEvent)
                _, data = win32file.ReadFile(pipe, 65536, operation)
                count = win32file.GetOverlappedResult(pipe, operation, True)
                pending += bytes(data[:count])
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    if line.startswith(b"_ "):
                        message = json.loads(line[2:])
                        events.append(message)
                        messages.put(message)
        except pywintypes.error:
            pass
        finally:
            win32file.CloseHandle(operation.hEvent)

    def wait_for(op, timeout=15):
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            event = messages.get(timeout=max(0.01, until - time.monotonic()))
            if event["op"] == "failed":
                raise RuntimeError(event)
            if event["op"] == op:
                return event
        raise TimeoutError(op)

    def wait_for_log(fragment, timeout):
        until = time.monotonic() + timeout
        while True:
            for event in events:
                if event.get("op") == "stderr" and fragment in event.get("text", ""):
                    return event["text"]
            if time.monotonic() >= until:
                raise TimeoutError(fragment)
            message = messages.get(timeout=max(0.01, until - time.monotonic()))
            if message.get("op") in ("failed", "exited"):
                raise RuntimeError(message)

    try:
        runner = subprocess.Popen([str(args.host / "WinShunPluginHost.exe"), "--pipe", name, "--key", name])
        connection = pywintypes.OVERLAPPED()
        connection.hEvent = win32event.CreateEvent(None, True, False, None)
        try:
            win32pipe.ConnectNamedPipe(pipe, connection)
            if win32event.WaitForSingleObject(connection.hEvent, 10000) != win32event.WAIT_OBJECT_0:
                win32file.CancelIoEx(pipe, connection)
                raise TimeoutError("runner pipe connection")
        finally:
            win32file.CloseHandle(connection.hEvent)
        threading.Thread(target=read_messages, daemon=True).start()
        wait_for("hello")
        duration = 38 if args.task == "performance" else (10 if args.task == "autohide" else 25)
        command = ["--cover", "blackhole", "--windowed", "--screen", "0", "--autohide", str(duration),
                   "--app-dir", str(args.host)]
        enhanced = args.enhanced or args.simulate_guard_error is not None
        if args.focused_qml and enhanced:
            raise ValueError("native and focused QML acceptance must run separately")
        if enhanced:
            command += ["--enhanced"]
        if args.focused_qml:
            command += ["--qml-only"]
        if args.task == "performance":
            command += ["--metrics", str(args.output / "frames.json")]
        send({"op": "start", "id": 1, "exe": str(args.exe.resolve()), "args": command,
              "cwd": str(args.exe.resolve().parent), "strict": True, "ui": True,
              "memoryMB": 1024, "processes": False, "stdin": False, "encoding": "utf-8",
              "qtDir": str(args.host), "env": [["WS_APP_DIR", str(args.host)]]})
        started = wait_for("started")
        if enhanced:
            line = wait_for_log("guard-launch=", 10)
            launch = json.loads(line.split("guard-launch=", 1)[1])
            launch_args = launch["args"].split()
            session = launch_args[launch_args.index("--session") + 1]
            if args.simulate_guard_error is not None:
                client = win32file.CreateFile(r"\\.\pipe\WinShun.BossKey.Input." + session,
                                             win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                                             0, None, win32file.OPEN_EXISTING, 0, None)
                try:
                    win32file.WriteFile(client, struct.pack("<II", 3, args.simulate_guard_error))
                    win32file.ReadFile(client, 8)  # keep the status available until the server acknowledges it
                finally:
                    win32file.CloseHandle(client)
            else:
                helpers.append(psutil.Popen([launch["path"], *launch_args]))
            wait_for_log("cover shown,", 130)
            for process in psutil.process_iter(["name", "cmdline"]):
                try:
                    if process.info["name"] == "bosskey-guard.exe" and session in (process.info["cmdline"] or []):
                        if process.pid not in [helper.pid for helper in helpers]:
                            helpers.append(process)
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    pass
        cover = shot.find_visible_cover()
        assert cover and desktop.window_pid(cover) == started["pid"], "runner preview not visible"
        facts = {"runner": str(args.host / "WinShunPluginHost.exe"), "strict": True,
                 "runner_integrity": desktop.integrity(runner.pid),
                 "cover_integrity": desktop.integrity(started["pid"]),
                 "cover_token": desktop.token_facts(started["pid"]),
                  "cover_rect": desktop.require_windowed(cover), "exe": str(args.exe.resolve())}
        if enhanced:
            facts["guard_error_simulated"] = args.simulate_guard_error
            facts["helpers"] = [{"pid": process.pid, "integrity": desktop.integrity(process.pid)}
                                for process in helpers if process.is_running()]
            facts["enhanced_mode_seen"] = any("protection=enhanced" in event.get("text", "") for event in events)
            if args.simulate_guard_error is None and not args.expect_declined:
                assert facts["enhanced_mode_seen"], "UAC was declined or enhanced startup failed"
                assert any(helper["integrity"] >= 12288 for helper in facts["helpers"]), facts
            else:
                assert not facts["enhanced_mode_seen"], "failed startup cannot enable enhanced mode"
                expected_error = 1223 if args.expect_declined else args.simulate_guard_error
                assert any(f"enhanced input unavailable ({expected_error})" in event.get("text", "")
                           for event in events), "fallback did not report the expected helper failure"
        shot.capture(cover, str(args.output / "cover.png"))
        assert facts["cover_integrity"] == 4096, facts
        assert facts["cover_token"]["restricted"] and not facts["cover_token"]["ui_access"], facts
        if args.task == "disconnect":
            assert enhanced and args.simulate_guard_error is None, "disconnect requires a real enhanced helper"
            for helper in helpers:
                if helper.is_running():
                    helper.terminate()
            wait_for_log("enhanced input disconnected; using ordinary mode", 5)
            facts["ordinary_fallback_after_disconnect"] = True
        if args.task in ("guard", "disconnect"):
            command = [sys.executable, str(Path(__file__).with_name("guard.py")),
                       "--output", str(args.output / "guard.json")]
            if args.target_python:
                command += ["--target-python", str(args.target_python.resolve())]
            if args.focused_qml or args.simulate_guard_error is not None or args.expect_declined or args.task == "disconnect":
                command += ["--focus-only", "--focused-exit"]
            checked = subprocess.run(command, check=False, capture_output=True, text=True, encoding="utf-8")
            child_rc = checked.returncode
            (args.output / "input-check.log").write_text(checked.stdout + checked.stderr, encoding="utf-8")
            print(checked.stdout + checked.stderr, end="")
        elif args.task == "sky":
            child_rc = subprocess.run([sys.executable, str(Path(__file__).with_name("sky_motion.py")),
                                       str(args.output)], check=False).returncode
        elif args.task == "performance":
            process = psutil.Process(started["pid"])
            time.sleep(12)
            process.cpu_percent(None)
            cpu, memory, private = [], [], []
            for _ in range(20):
                desktop.require_input_desktop()
                assert desktop.require_windowed(cover) == facts["cover_rect"]
                cpu.append(process.cpu_percent(interval=1))
                info = process.memory_info()
                memory.append(info.rss / 1048576)
                private.append(info.private / 1048576)
            facts["performance"] = {
                "sample_seconds": 20, "cpu_percent_one_logical_core": statistics.mean(cpu),
                "cpu_percent_machine": statistics.mean(cpu) / psutil.cpu_count(),
                "logical_cores": psutil.cpu_count(), "rss_mib_mean": statistics.mean(memory),
                "rss_mib_max": max(memory), "private_mib_mean": statistics.mean(private)}
            child_rc = 0
        else:
            if args.task == "crash":
                send({"op": "stop", "id": 1})
            child_rc = 0
        exited = wait_for("exited", timeout=duration + 10)
        facts["exit_code"] = exited["code"]
        facts["stderr"] = [event["text"] for event in events if event["op"] == "stderr"]
        facts["native_exit_seen"] = any("exit source=native" in line for line in facts["stderr"])
        facts["qml_exit_seen"] = any("exit source=qml" in line for line in facts["stderr"])
        if enhanced:
            facts["guard_launch_requests"] = sum("guard-launch=" in line for line in facts["stderr"])
            assert facts["guard_launch_requests"] == 1, "each cover start may request UAC only once"
            _, alive = psutil.wait_procs(helpers, timeout=5)
            facts["helpers_exited_after_hide"] = not alive
            assert not alive, "input helper remained after cover exit"
        if args.task in ("crash", "autohide"):
            import guard as input_check
            assert not input_check.exit_check.find_visible_cover(), "cover survived process exit"
            before = input_check.visible_dialogs()
            input_check.chord(0x5B, 0x52)
            time.sleep(0.5)
            dialogs = input_check.visible_dialogs() - before
            facts["Win+R_restored_after_hide"] = input_check.u.GetForegroundWindow() in dialogs
            if facts["Win+R_restored_after_hide"]:
                input_check.chord(0x1B)
            assert facts["Win+R_restored_after_hide"], "shortcut remained suppressed after termination"
        if args.focused_qml:
            assert facts["qml_exit_seen"] and not facts["native_exit_seen"], "focused QML exit was not isolated"
        if args.task == "guard" and (args.output / "guard.json").exists():
            guard = json.loads((args.output / "guard.json").read_text(encoding="utf-8"))
            facts["native_unfocused_exit_verified"] = bool(guard.get("exit_without_focus")
                                                           and guard.get("received_characters") == 0
                                                           and facts["native_exit_seen"])
        if args.task == "performance":
            frames = json.loads((args.output / "frames.json").read_text(encoding="utf-8"))["frame_swapped_intervals_ms"]
            assert len(frames) > 100, "insufficient steady-state frame events"
            facts["performance"].update({
                "steady_frame_events": len(frames), "interval_ms_median": statistics.median(frames),
                "interval_ms_p95": percentile(frames, 0.95), "interval_ms_p99": percentile(frames, 0.99),
                "interval_ms_max": max(frames), "mean_fps": 1000 / statistics.mean(frames),
                "intervals_over_66_7_ms": sum(interval > 66.7 for interval in frames)})
        (args.output / "runner.json").write_text(json.dumps(facts, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(facts, indent=2))
        expected_exit = 1 if args.task == "crash" else 0
        return child_rc or (0 if exited["code"] == expected_exit else 1)
    finally:
        if runner is not None and runner.poll() is None:
            send({"op": "shutdown"})
            runner.wait(timeout=10)
        win32file.CloseHandle(pipe)
        (args.output / "runner-events.json").write_text(json.dumps(events, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
