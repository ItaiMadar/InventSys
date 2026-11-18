param([string]$Empp)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$emccTemp = Join-Path $PSScriptRoot ".emcc-temp"
New-Item -ItemType Directory -Force -Path $emccTemp | Out-Null
$env:EMCC_TEMP_DIR = $emccTemp
$exports = '["_place_create","_place_state","_place_scan_card","_place_dismiss_cooldown_notice","_place_set_preview","_place_change_color","_place_confirm","_place_finish_commit","_place_tick","_place_preview_x","_place_preview_y","_place_selected_color","_place_preview_display_color","_place_board_size","_place_board_color","_place_cooldown_remaining"]'

if (-not $Empp) {
  $command = Get-Command "em++" -ErrorAction SilentlyContinue
  $cachedEmpp = Join-Path $env:USERPROFILE ".cache\emsdk\upstream\emscripten\em++.exe"
  if ($command) { $Empp = $command.Source }
  elseif (Test-Path -LiteralPath $cachedEmpp) { $Empp = $cachedEmpp }
  else { throw "Emscripten was not found. Install emsdk or pass -Empp with the path to em++." }
}

& $Empp `
  (Join-Path $projectRoot "src\PlaceStateMachine.cpp") `
  (Join-Path $PSScriptRoot "PlaceStateMachineWasm.cpp") `
  ("-I" + (Join-Path $projectRoot "src")) `
  -std=c++11 `
  -O2 `
  -sMODULARIZE=1 `
  -sEXPORT_NAME=createPlaceCore `
  -sENVIRONMENT=web `
  -sSINGLE_FILE=1 `
  ("-sEXPORTED_FUNCTIONS=" + $exports) `
  '-sEXPORTED_RUNTIME_METHODS=["cwrap"]' `
  -o (Join-Path $PSScriptRoot "place-core.js")

if ($LASTEXITCODE -ne 0) { throw "Simulator build failed." }
Write-Host "Built simulator\place-core.js from the current C++ sources."
