#!/usr/bin/env python3
# /// script
# requires-python = ">=3.14"
# ///
"""
Pre-DateGrip Frontend Build Script

Usage:
    uv run scripts/build_frontend.py [--clean]

Options:
    --clean    Force clean build (clear all caches)
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def run_command(cmd: list[str], description: str, cwd: Path | None = None) -> bool:
    """Run a command and return success status."""
    print(f"\n{'='*60}")
    print(f"  {description}")
    print(f"  Command: {' '.join(cmd)}")
    if cwd:
        print(f"  Working directory: {cwd}")
    print(f"{'='*60}\n")

    try:
        result = subprocess.run(cmd, cwd=cwd)
        return result.returncode == 0
    except FileNotFoundError as e:
        print(f"ERROR: Command not found: {cmd[0]}")
        print(f"  Details: {e}")
        return False
    except Exception as e:
        print(f"ERROR: Failed to execute command: {e}")
        return False


def find_package_manager() -> tuple[str, Path] | None:
    """Find available package manager (Bun or npm)."""
    # Try Bun first (preferred)
    bun_path = shutil.which("bun")
    if bun_path:
        try:
            result = subprocess.run(
                [bun_path, "--version"],
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                version = result.stdout.strip()
                print(f"Found Bun: {version}")
                return ("bun", Path(bun_path))
        except Exception:
            pass

    # Try npm as fallback
    npm_path = shutil.which("npm")
    if npm_path:
        try:
            result = subprocess.run(
                [npm_path, "--version"],
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                version = result.stdout.strip()
                print(f"Found npm: {version}")
                return ("npm", Path(npm_path))
        except Exception:
            pass

    return None


def check_node_modules(frontend_dir: Path, pkg_manager: str) -> bool:
    """Check if node_modules exists and is up to date."""
    node_modules = frontend_dir / "node_modules"
    package_json = frontend_dir / "package.json"

    if not node_modules.exists():
        return False

    # Check if package.json is newer than node_modules
    if package_json.exists():
        pkg_json_mtime = package_json.stat().st_mtime
        node_modules_mtime = node_modules.stat().st_mtime

        if pkg_json_mtime > node_modules_mtime:
            print("  package.json has been modified, dependencies need to be reinstalled")
            return False

    return True


def clear_build_caches(frontend_dir: Path, project_root: Path):
    """Clear all build caches (Vite, dist, WebView2)."""
    caches_to_clear = [
        frontend_dir / "dist",
        frontend_dir / "node_modules" / ".vite",
        frontend_dir / ".vite",
        project_root / "build" / "Debug" / "PreDateGrip.exe.WebView2",
        project_root / "build" / "Release" / "PreDateGrip.exe.WebView2",
    ]

    print("\n[Cache Clearing]")
    cleared_count = 0
    for cache_path in caches_to_clear:
        if cache_path.exists():
            try:
                shutil.rmtree(cache_path)
                print(f"  [OK] Cleared: {cache_path.relative_to(project_root)}")
                cleared_count += 1
            except Exception as e:
                print(f"  [FAIL] Failed to clear {cache_path}: {e}")
        else:
            print(f"  [SKIP] Not found: {cache_path.relative_to(project_root)}")

    if cleared_count > 0:
        print(f"\n  Cleared {cleared_count} cache(s)")
    else:
        print("  No caches to clear")


def main():
    # Parse arguments
    parser = argparse.ArgumentParser(description="Build Pre-DateGrip frontend")
    parser.add_argument("--clean", action="store_true", help="Force clean build (clear all caches)")
    args = parser.parse_args()

    print(f"\n{'#'*60}")
    print("#  Pre-DateGrip Frontend Build Script")
    if args.clean:
        print("#  Mode: Clean Build")
    print(f"{'#'*60}")

    # Get project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    frontend_dir = project_root / "frontend"

    if not frontend_dir.exists():
        print(f"\nERROR: Frontend directory not found: {frontend_dir}")
        sys.exit(1)

    print(f"\nProject root: {project_root}")
    print(f"Frontend directory: {frontend_dir}")

    # Clear caches if --clean flag is set
    if args.clean:
        clear_build_caches(frontend_dir, project_root)

    # Find package manager
    print("\n[1/3] Detecting package manager...")
    pkg_info = find_package_manager()

    if not pkg_info:
        print("\nERROR: No package manager found (Bun or npm)")
        print("\nInstall options:")
        print("  1. Bun (recommended):")
        print("     powershell -c \"irm bun.sh/install.ps1 | iex\"")
        print("  2. npm:")
        print("     winget install OpenJS.NodeJS")
        sys.exit(1)

    pkg_manager, pkg_path = pkg_info

    # Check and install dependencies
    print("\n[2/3] Checking dependencies...")
    needs_install = not check_node_modules(frontend_dir, pkg_manager)

    if needs_install:
        print("  Installing dependencies...")
        install_cmd = [str(pkg_path), "install"]
        if not run_command(install_cmd, f"{pkg_manager} install", frontend_dir):
            print("\nERROR: Failed to install dependencies!")
            sys.exit(1)
    else:
        print("  Dependencies are up to date")

    # Build
    print("\n[3/3] Building frontend...")
    build_cmd = [str(pkg_path), "run", "build"]

    if not run_command(build_cmd, f"{pkg_manager} run build", frontend_dir):
        print("\nERROR: Frontend build failed!")
        sys.exit(1)

    # Check output
    dist_dir = frontend_dir / "dist"
    if not dist_dir.exists():
        print("\nERROR: Build output directory not found!")
        print(f"  Expected: {dist_dir}")
        sys.exit(1)

    # Report success
    print(f"\n{'='*60}")
    print("  BUILD SUCCESSFUL!")
    print(f"{'='*60}")

    # Calculate dist size
    total_size = sum(f.stat().st_size for f in dist_dir.rglob('*') if f.is_file())
    print(f"\n  Output directory: {dist_dir}")
    print(f"  Total size: {total_size / 1024 / 1024:.2f} MB")

    # Count files
    file_count = sum(1 for _ in dist_dir.rglob('*') if _.is_file())
    print(f"  Files: {file_count}")

    # List main files
    print("\n  Main files:")
    for file in sorted(dist_dir.glob('*')):
        if file.is_file():
            size = file.stat().st_size / 1024
            print(f"    {file.name} ({size:.1f} KB)")

    # Clear WebView2 cache after successful build to ensure fresh load
    print("\n[Post-Build] Clearing WebView2 cache...")
    webview2_caches = [
        project_root / "build" / "Debug" / "PreDateGrip.exe.WebView2",
        project_root / "build" / "Release" / "PreDateGrip.exe.WebView2",
    ]

    cleared = False
    for cache_path in webview2_caches:
        if cache_path.exists():
            try:
                shutil.rmtree(cache_path)
                print(f"  [OK] Cleared: {cache_path.relative_to(project_root)}")
                cleared = True
            except Exception as e:
                print(f"  [FAIL] Failed to clear {cache_path}: {e}")

    if cleared:
        print("  WebView2 will load fresh frontend files on next startup")
    else:
        print("  No WebView2 cache to clear")

    print()


if __name__ == "__main__":
    main()
