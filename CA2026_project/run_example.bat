@echo off
set "TEST_DIR=examples/example_061225_win"
set "OUT_DIR=build/outputs"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

build\CA2026_test.exe "%TEST_DIR%/imem0.txt" "%TEST_DIR%/imem1.txt" "%TEST_DIR%/imem2.txt" "%TEST_DIR%/imem3.txt" "%TEST_DIR%/memin.txt" "%OUT_DIR%/memout.txt" "%OUT_DIR%/regout0.txt" "%OUT_DIR%/regout1.txt" "%OUT_DIR%/regout2.txt" "%OUT_DIR%/regout3.txt" "%OUT_DIR%/core0trace.txt" "%OUT_DIR%/core1trace.txt" "%OUT_DIR%/core2trace.txt" "%OUT_DIR%/core3trace.txt" "%OUT_DIR%/bustrace.txt" "%OUT_DIR%/dsram0.txt" "%OUT_DIR%/dsram1.txt" "%OUT_DIR%/dsram2.txt" "%OUT_DIR%/dsram3.txt" "%OUT_DIR%/tsram0.txt" "%OUT_DIR%/tsram1.txt" "%OUT_DIR%/tsram2.txt" "%OUT_DIR%/tsram3.txt" "%OUT_DIR%/stats0.txt" "%OUT_DIR%/stats1.txt" "%OUT_DIR%/stats2.txt" "%OUT_DIR%/stats3.txt"
