<#
.SYNOPSIS
    Build ESP32 mesh node firmware for OTA deployment.

.DESCRIPTION
    Compiles firmware using Arduino CLI and outputs a .bin file ready for OTA upload.

.PARAMETER NodeType
    The type of node to build: pir, led, button, button2, dht, watcher, gateway

.PARAMETER Version
    Version string for the firmware (e.g., 1.2.0). Defaults to "dev".

.PARAMETER All
    Build all node types.

.EXAMPLE
    .\build.ps1 button
    .\build.ps1 button -Version 1.2.0
    .\build.ps1 -All -Version 2.0.0
#>

param(
    [Parameter(Position=0)]
    [ValidateSet("pir", "led", "button", "button2", "dht", "watcher", "gateway")]
    [string]$NodeType,

    [string]$Version = "dev",

    [switch]$All
)

$ErrorActionPreference = "Stop"

# Configuration
$BOARD = "esp32:esp32:esp32"
$BUILD_DIR = "build"
$FIRMWARE_DIR = "firmware"

# Map node types to sketch directories
$SKETCHES = @{
    "pir"     = "mesh_shared_state_pir"
    "led"     = "mesh_shared_state_led"
    "button"  = "mesh_shared_state_button"
    "button2" = "mesh_shared_state_button2"
    "dht"     = "mesh_shared_state_dht11"
    "watcher" = "mesh_shared_state_watcher"
    "gateway" = "mesh_gateway"
}

# Build properties for OTA-compatible partition scheme
$BUILD_PROPS = "build.partitions=min_spiffs"

function Show-Usage {
    Write-Host ""
    Write-Host "Usage: .\build.ps1 <node_type> [-Version VERSION]" -ForegroundColor Cyan
    Write-Host "       .\build.ps1 -All [-Version VERSION]" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Node types:" -ForegroundColor Yellow
    Write-Host "  pir, led, button, button2, dht, watcher, gateway"
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Yellow
    Write-Host "  .\build.ps1 button"
    Write-Host "  .\build.ps1 button -Version 1.2.0"
    Write-Host "  .\build.ps1 -All -Version 2.0.0"
    Write-Host ""
    exit 1
}

function Build-Firmware {
    param(
        [string]$Type,
        [string]$Ver
    )

    $sketchName = $SKETCHES[$Type]
    if (-not $sketchName) {
        Write-Error "Unknown node type: $Type"
        return $false
    }

    $sketchPath = Join-Path $FIRMWARE_DIR "$sketchName\$sketchName.ino"
    $outputDir = Join-Path $BUILD_DIR $Type

    # Verify sketch exists
    if (-not (Test-Path $sketchPath)) {
        Write-Error "Sketch not found: $sketchPath"
        return $false
    }

    Write-Host ""
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "  Building: $Type" -ForegroundColor White
    Write-Host "  Sketch:   $sketchName" -ForegroundColor Gray
    Write-Host "  Version:  $Ver" -ForegroundColor Gray
    Write-Host "==========================================" -ForegroundColor Cyan

    # Create output directory
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

    # Compile with Arduino CLI
    Write-Host ""
    Write-Host "Compiling..." -ForegroundColor Yellow

    $compileArgs = @(
        "compile"
        "--fqbn", $BOARD
        "--build-property", $BUILD_PROPS
        "--output-dir", $outputDir
        "--warnings", "default"
        $sketchPath
    )

    # Run compilation
    # Note: arduino-cli may report non-fatal errors (partitions.csv copy on OneDrive)
    # We check for the binary file to determine actual success
    try {
        & arduino-cli @compileArgs 2>$null
    } catch {
        # Ignore - we'll check for the binary
    }

    # Find the compiled binary - this is the real test of success
    $binFile = Join-Path $outputDir "$sketchName.ino.bin"

    if (-not (Test-Path $binFile)) {
        Write-Host ""
        Write-Host "Compilation FAILED - no binary produced!" -ForegroundColor Red
        return $false
    }

    # Copy to versioned filename
    $versionedFile = Join-Path $outputDir "${Type}_${Ver}.bin"
    Copy-Item $binFile $versionedFile -Force

    # Calculate MD5 hash
    $md5 = (Get-FileHash -Algorithm MD5 $versionedFile).Hash.ToLower()
    $size = (Get-Item $versionedFile).Length
    $sizeKB = [math]::Round($size / 1024, 1)

    Write-Host ""
    Write-Host "Build SUCCESS!" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Output: $versionedFile" -ForegroundColor White
    Write-Host "  Size:   $sizeKB KB ($size bytes)" -ForegroundColor Gray
    Write-Host "  MD5:    $md5" -ForegroundColor Gray
    Write-Host ""

    return $true
}

# Main script
Push-Location (Split-Path $PSScriptRoot -Parent)

try {
    # Check arduino-cli is available
    $null = Get-Command arduino-cli -ErrorAction Stop
}
catch {
    Write-Error "arduino-cli not found. Please install it first."
    exit 1
}

# Determine what to build
if ($All) {
    Write-Host ""
    Write-Host "Building ALL node types with version: $Version" -ForegroundColor Cyan

    $success = 0
    $failed = 0

    foreach ($type in $SKETCHES.Keys) {
        if (Build-Firmware -Type $type -Ver $Version) {
            $success++
        } else {
            $failed++
        }
    }

    Write-Host ""
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "  Build Complete" -ForegroundColor White
    Write-Host "  Success: $success" -ForegroundColor Green
    if ($failed -gt 0) {
        Write-Host "  Failed:  $failed" -ForegroundColor Red
    }
    Write-Host "==========================================" -ForegroundColor Cyan
}
elseif ($NodeType) {
    Build-Firmware -Type $NodeType -Ver $Version | Out-Null
}
else {
    Show-Usage
}

Pop-Location
