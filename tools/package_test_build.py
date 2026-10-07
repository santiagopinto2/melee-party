"""Packages a test build of the Source Port: a folder that runs on a clean Windows PC after unzipping.

Made by tools/test_build.sh from the Linux cross build (docs/agent-testing.md) so a change can be
played on Windows before it ships. The zip is the build output (melee_source.exe, melee_game.dll and
its snapshot exclusions) plus the data files a release has beside the game (Sys, ui_sources,
licenses), a PlayTest.bat and a README. No game data: the player points it at their own ISO.

    python tools/package_test_build.py --name party-fix --exe-dir build-clangcl/port \
        --dll-dir build-sourceport-gcc --out ~/services/test-builds/www/builds \
        [--branch NAME --commit SHA --description TEXT]
"""
import argparse
import datetime
import html
import json
import re
import shutil
import zipfile
from pathlib import Path

import package_release

ROOT = Path(__file__).resolve().parents[1]

BAT = r"""@echo off
rem Melee Party test build. Drag your Melee NTSC 1.02 ISO onto this file once: it remembers the
rem path in iso-path.txt. Or put the ISO next to this file named melee.iso. playtest.log says what
rem it tried. No parenthesized blocks around paths: an ISO named "Melee (USA) (v1.02).iso" or a
rem folder with & in its name must not break the script.
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"
set "HERE=%~dp0"
set "LOG=%~dp0playtest.log"
set "SAVED=%~dp0iso-path.txt"
set "ISO="
> "%LOG%" echo PlayTest.bat started
set "MSG=folder: %HERE%"
call :log
if "%~1"=="" goto :remembered
set "ISO=%~f1"
if exist "%ISO%" goto :dropped
rem Explorer quotes a dropped path only when it has a space, so cmd splits one like
rem C:\Games\Melee,v1.02.iso (a comma or semicolon, no space) into several arguments: take the
rem whole argument line back, as it was dropped.
set "ISO=%*"
set "ISO=%ISO:"=%"
for %%I in ("%ISO%") do set "ISO=%%~fI"

:dropped
set "MSG=dropped: %ISO%"
call :log
if not exist "%ISO%" goto :missing
setlocal EnableDelayedExpansion
> "!SAVED!" echo !ISO!
endlocal
goto :play

:remembered
if not exist "%SAVED%" goto :beside
set /p "ISO=" < "%SAVED%"
set "MSG=remembered: %ISO%"
call :log
if exist "%ISO%" goto :play
set "MSG=the remembered ISO is gone; forgetting it"
call :log
del "%SAVED%" 2>nul
set "ISO="

:beside
if exist "%HERE%melee.iso" set "ISO=%HERE%melee.iso"
if not defined ISO goto :noiso
set "MSG=next to it: %ISO%"
call :log

:play
set "MSG=starting melee_source.exe"
call :log
"%HERE%melee_source.exe" --iso "%ISO%" --threaded-renderer --settings-path "%HERE%port-settings.ini" --sys-dir "%HERE%Sys" --user-dir "%HERE%User\Slippi" --replay-dir "%HERE%Replays" --card-dir "%HERE%User\GC\CardA"
set "CODE=%ERRORLEVEL%"
set "MSG=melee_source.exe exited with code %CODE%"
call :log
if "%CODE%"=="0" exit /b 0
echo.
echo The game stopped with code %CODE%. Send melee_port.log and playtest.log from this folder.
pause
exit /b 1

:missing
echo Cannot find the file that was dropped. Drop the ISO onto PlayTest.bat again.
set "MSG=not found"
call :log
pause
exit /b 1

:noiso
echo Drag your Melee NTSC 1.02 ISO onto PlayTest.bat, or put it next to it named melee.iso.
set "MSG=no ISO dropped, remembered or next to it"
call :log
pause
exit /b 1

:log
setlocal EnableDelayedExpansion
>> "!LOG!" echo !MSG!
endlocal
exit /b 0
"""

README = """Melee Party test build: {name}
{rule}

{description}

Branch {branch}, commit {commit}, built {date}.
This is a test build, not a release: try it, then say "ship it" or what to change.

How to play
-----------
1. Unzip this folder anywhere, separate from your normal Melee Party folder (that one is not
   touched; this folder keeps its own settings, saves and replays).
2. Drag your Melee NTSC 1.02 ISO onto PlayTest.bat. It remembers the ISO, so after that just
   double-click PlayTest.bat. (Or put the ISO next to it named melee.iso.)
3. Melee Party is under VS. Mode > Melee Party.

Notes
-----
- Built on Linux for any CPU with AVX (it runs on every PC that runs the release). Online play
  only works against someone on this same test build.
- Your saves from another folder: copy GALE01-*.gci into User\\GC\\CardA.
- If it crashes, send melee_port.log (and melee_port_crash.txt if there is one) from this folder.
- Nothing from the game is included; the ISO stays wherever you keep it.
"""

# Never in a test zip: disc images and anything extracted from one.
FORBIDDEN = re.compile(r"\.(iso|gcm|ciso|rvz|wia|gcz|nkit|dol|dat|usd|thp|hps|ssm|sem)$", re.I)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--name", required=True, help="short build name (letters, digits, - and _)")
    ap.add_argument("--exe-dir", type=Path, default=ROOT / "build-clangcl/port")
    ap.add_argument("--dll-dir", type=Path, default=ROOT / "build-sourceport-gcc")
    ap.add_argument("--out", type=Path, required=True, help="folder the zip and its .json go into")
    ap.add_argument("--branch", default="")
    ap.add_argument("--commit", default="")
    ap.add_argument("--description", default="")
    ap.add_argument("--keep", type=int, default=5, help="test builds kept in --out (oldest are deleted)")
    ap.add_argument("--index", type=Path, help="write an HTML page listing the builds here")
    ap.add_argument("--url-prefix", default="builds/", help="link to each zip from the index page")
    args = ap.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", args.name):
        raise SystemExit("--name: letters, digits, '.', '-' and '_' only")
    files = {"melee_source.exe": args.exe_dir / "melee_source.exe",
             "melee_game.dll": args.dll_dir / "melee_game.dll",
             "melee_game.snapexcl": args.dll_dir / "melee_game.snapexcl"}
    for path in files.values():
        if not path.is_file():
            raise SystemExit(f"missing {path} (build it first)")

    now = datetime.datetime.now().astimezone()
    stamp = now.strftime("%Y%m%d-%H%M")
    top = f"MeleeParty-test-{args.name}"
    staging = args.out / f".staging-{args.name}-{stamp}"
    folder = staging / top
    shutil.rmtree(staging, ignore_errors=True)
    folder.mkdir(parents=True)
    for name, src in files.items():
        shutil.copy2(src, folder / name)
    package_release.copy_sys(folder)
    package_release.copy_licenses_and_assets(folder, mod_tools=False)   # mod imports only; not in the repo
    (folder / "User/Slippi").mkdir(parents=True)
    (folder / "User/GC/CardA").mkdir(parents=True)
    (folder / "Replays").mkdir()
    (folder / "PlayTest.bat").write_bytes(BAT.replace("\n", "\r\n").encode("utf-8"))
    title = f"Melee Party test build: {args.name}"
    readme = README.format(name=args.name, rule="=" * len(title), date=now.strftime("%Y-%m-%d %H:%M %Z"),
                           description=args.description or "(no description)",
                           branch=args.branch or "-", commit=args.commit or "-")
    (folder / "README.txt").write_bytes(readme.replace("\n", "\r\n").encode("utf-8"))

    # Sys/ is Slippi's own files, copied verbatim from port/slippi_sys in the repository.
    bad = [p for p in folder.rglob("*") if p.is_file() and p.stat().st_size and
           ((FORBIDDEN.search(p.name) and p.relative_to(folder).parts[0] != "Sys") or p.stat().st_size > 64 << 20)]
    if bad:
        shutil.rmtree(staging)
        raise SystemExit("refusing to package possible game data: " + ", ".join(str(p) for p in bad))

    zip_name = f"MeleeParty-test-{args.name}-{stamp}.zip"
    zip_path = args.out / zip_name
    with zipfile.ZipFile(zip_path.with_suffix(".part"), "w", zipfile.ZIP_DEFLATED) as z:
        for path in sorted(folder.rglob("*")):
            z.write(path, path.relative_to(staging))
    zip_path.with_suffix(".part").rename(zip_path)
    shutil.rmtree(staging)
    meta = {"zip": zip_name, "name": args.name, "branch": args.branch, "commit": args.commit,
            "description": args.description, "date": now.isoformat(timespec="seconds"),
            "bytes": zip_path.stat().st_size}
    zip_path.with_suffix(".json").write_text(json.dumps(meta, indent=1), encoding="utf-8")
    builds = prune(args.out, args.keep)
    if args.index:
        write_index(args.index, builds, args.url_prefix)
    print(zip_path)


def prune(out, keep):
    """Keeps the newest `keep` builds in `out`; returns their metadata, newest first."""
    builds = []
    for meta_path in out.glob("MeleeParty-test-*.json"):
        try:
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        if (out / meta.get("zip", "")).is_file():
            builds.append(meta)
    builds.sort(key=lambda m: m["date"], reverse=True)
    for old in builds[keep:]:
        (out / old["zip"]).unlink(missing_ok=True)
        (out / old["zip"]).with_suffix(".json").unlink(missing_ok=True)
    return builds[:keep]


def write_index(path, builds, url_prefix):
    rows = []
    for b in builds:
        rows.append("<tr><td><a href=\"%s\">%s</a></td><td>%s</td><td>%s</td><td>%s</td><td>%.1f MB</td></tr>" % (
            html.escape(url_prefix + b["zip"]), html.escape(b["name"]), html.escape(b["date"][:16].replace("T", " ")),
            html.escape(b["branch"] + (" @ " + b["commit"] if b["commit"] else "")), html.escape(b["description"]),
            b["bytes"] / 1e6))
    page = """<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Melee Party test builds</title>
<style>body{font-family:system-ui,sans-serif;margin:1.5em;max-width:60em}table{border-collapse:collapse;width:100%%}
td,th{text-align:left;padding:.4em .6em;border-bottom:1px solid #ddd;vertical-align:top}</style></head>
<body><h1>Melee Party test builds</h1>
<p>Newest first. Unzip into a new folder, drag your ISO onto <b>PlayTest.bat</b>, play.
Your normal install is not touched. Only reachable over your tailnet.</p>
<table><tr><th>Build</th><th>Date</th><th>Branch</th><th>What changed</th><th>Size</th></tr>
%s
</table></body></html>
""" % ("\n".join(rows) or "<tr><td colspan=5>No builds yet.</td></tr>")
    tmp = path.with_suffix(".tmp")
    tmp.write_text(page, encoding="utf-8")
    tmp.replace(path)


if __name__ == "__main__":
    main()
