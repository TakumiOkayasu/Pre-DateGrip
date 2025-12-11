"""Lint commands for Pre-DateGrip."""

import shutil
import subprocess

from . import utils


def lint_frontend(fix: bool = False) -> bool:
    """Lint frontend code with Biome."""
    project_root = utils.get_project_root()
    frontend_dir = project_root / "frontend"

    print(f"\n{'#'*60}")
    print("#  Linting Frontend")
    if fix:
        print("#  Mode: Auto-fix")
    print(f"{'#'*60}")

    # Find package manager
    pkg_info = utils.find_package_manager()
    if not pkg_info:
        print("\nERROR: No package manager found")
        return False

    pkg_manager, pkg_path = pkg_info

    # Run lint
    lint_cmd = [str(pkg_path), "run", "lint"]
    if fix:
        lint_cmd.append("--")
        lint_cmd.append("--write")

    success, _ = utils.run_command(
        lint_cmd,
        "Biome lint",
        cwd=frontend_dir
    )

    # Run type check
    print("\n[Type checking...]")
    success2, _ = utils.run_command(
        [str(pkg_path), "run", "typecheck"],
        "TypeScript check",
        cwd=frontend_dir
    )

    if success and success2:
        print("\n[OK] Lint passed!")
        return True
    else:
        print("\n[FAIL] Lint failed")
        return False


def lint_cpp(fix: bool = False) -> bool:
    """Lint C++ code with clang-format."""
    project_root = utils.get_project_root()
    src_dir = project_root / "src"

    print(f"\n{'#'*60}")
    print("#  Linting C++")
    if fix:
        print("#  Mode: Auto-fix")
    print(f"{'#'*60}")

    # Check for clang-format
    clang_format = shutil.which("clang-format")
    if not clang_format:
        print("\nERROR: clang-format not found")
        print("Install: winget install LLVM.LLVM")
        return False

    # Get version
    try:
        result = subprocess.run(
            [clang_format, "--version"],
            capture_output=True,
            text=True
        )
        print(f"\n{result.stdout.strip()}")
    except Exception:
        pass

    # Find all C++ files
    cpp_files = []
    for ext in ['*.cpp', '*.h']:
        cpp_files.extend(src_dir.rglob(ext))

    if not cpp_files:
        print("\nERROR: No C++ files found")
        return False

    print(f"\nFound {len(cpp_files)} C++ files")

    # Format files
    errors = 0
    for file in cpp_files:
        if fix:
            # Auto-fix
            result = subprocess.run(
                [clang_format, "-i", "-style=file", str(file)],
                capture_output=True
            )
            if result.returncode != 0:
                print(f"  [FAIL] {file.relative_to(project_root)}")
                errors += 1
            else:
                print(f"  [OK] {file.relative_to(project_root)}")
        else:
            # Check only
            result = subprocess.run(
                [clang_format, "--style=file", "--dry-run", "--Werror", str(file)],
                capture_output=True
            )
            if result.returncode != 0:
                print(f"  [FAIL] {file.relative_to(project_root)}")
                errors += 1

    if errors > 0:
        print(f"\n[FAIL] {errors} file(s) need formatting")
        if not fix:
            print("Run with --fix to auto-format")
        return False
    else:
        print("\n[OK] All files properly formatted!")
        return True


def lint_all(fix: bool = False) -> bool:
    """Lint both frontend and C++ code."""
    print(f"\n{'='*60}")
    print("  Linting All (Frontend + C++)")
    print(f"{'='*60}")

    success1 = lint_frontend(fix=fix)
    success2 = lint_cpp(fix=fix)

    if success1 and success2:
        print(f"\n{'='*60}")
        print("  ALL LINTS PASSED")
        print(f"{'='*60}")
        return True
    else:
        print(f"\n{'='*60}")
        print("  SOME LINTS FAILED")
        print(f"{'='*60}")
        return False
