"""Local conversion paths; generated/downloaded files stay outside tracked tools."""
from pathlib import Path
import os
REPO = Path(__file__).resolve().parents[2]
ROOT = Path(os.environ.get("SKYSCAPE_PORT_ROOT", REPO / "external/skyscape-port")).resolve()
GAME = Path(os.environ.get("SKYSCAPE_GAME", r"D:\SteamLibrary\steamapps\common\Skyrim Special Edition")).resolve()
LOCAL = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData/Local")) / "Skyrim Special Edition"
# Override for redirected Documents (for example OneDrive).
DOCUMENTS = Path(os.environ.get("SKYSCAPE_DOCUMENTS", Path.home() / "Documents")) / "My Games/Skyrim Special Edition"
BACKUPS = REPO / "backups"
SKSE_SOURCE = Path(os.environ.get("SKYSCAPE_SKSE_SOURCE", REPO / "external/skse64/scripts/modified"))
TEXCONV = Path(os.environ.get("SKYSCAPE_TEXCONV", REPO / "external/asset-tools/texconv.exe"))
def require_game_closed():
    import subprocess
    result = subprocess.run(["powershell", "-NoProfile", "-Command", "if(Get-Process SkyrimSE -ErrorAction SilentlyContinue){exit 1}"], check=False)
    if result.returncode != 0:
        raise RuntimeError("Exit Skyrim before installing or rolling back")
