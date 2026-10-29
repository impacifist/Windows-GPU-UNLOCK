@rem ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md.
@echo off
cd /d "%~dp0"
echo PyTorch GPU benchmark. Close other GPU workloads first.
echo.
set /p benchmark_label=Boot state (unlocked / locked / unknown): 
if "%benchmark_label%"=="" set "benchmark_label=unknown"
".venv\Scripts\python.exe" "pytorch-benchmark.py" --label "%benchmark_label%"
pause
