<#
.SYNOPSIS
    Build and deploy firmware to the mesh server for OTA distribution.

.DESCRIPTION
    Compiles firmware using Arduino CLI, uploads to the server, and optionally
    creates an OTA update job to push to nodes.

.PARAMETER NodeType
    The type of node to build: pir, led, button, button2, dht, watcher, gateway

.PARAMETER Version
    Version string for the firmware (e.g., 1.2.0). Required.

.PARAMETER ServerUrl
    URL of the mesh server API. Defaults to MESH_SERVER_URL env var or http://localhost:8000

.PARAMETER Notes
    Release notes for this firmware version.

.PARAMETER Deploy
    If specified, also creates an OTA update job after uploading.

.PARAMETER Force
    Force update even if nodes already have this firmware (by MD5).

.EXAMPLE
    .\deploy.ps1 button -Version 1.2.0
    .\deploy.ps1 button -Version 1.2.0 -Deploy -Notes "Bug fixes"
    .\deploy.ps1 pir -Version 2.0.0 -Deploy -Force
#>

param(
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet("pir", "led", "button", "button2", "dht", "watcher", "gateway")]
    [string]$NodeType,

    [Parameter(Mandatory=$true)]
    [string]$Version,

    [string]$ServerUrl = $null,

    [string]$Notes = "",

    [switch]$Deploy,

    [switch]$Force
)

$ErrorActionPreference = "Stop"

# Get server URL from parameter, env var, or default
if (-not $ServerUrl) {
    $ServerUrl = $env:MESH_SERVER_URL
}
if (-not $ServerUrl) {
    $ServerUrl = "http://localhost:8000"
}

$BUILD_DIR = "build"

# Step 1: Build the firmware
Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  Deploy: $NodeType v$Version" -ForegroundColor White
Write-Host "  Server: $ServerUrl" -ForegroundColor Gray
Write-Host "==========================================" -ForegroundColor Cyan

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

Push-Location $projectDir

try {
    # Run build script
    Write-Host ""
    Write-Host "Step 1: Building firmware..." -ForegroundColor Yellow
    & "$scriptDir\build.ps1" -NodeType $NodeType -Version $Version

    # Check for binary
    $binFile = Join-Path $BUILD_DIR "$NodeType\${NodeType}_${Version}.bin"
    if (-not (Test-Path $binFile)) {
        Write-Error "Build failed - binary not found: $binFile"
        exit 1
    }

    $fileSize = (Get-Item $binFile).Length
    $fileSizeKB = [math]::Round($fileSize / 1024, 1)

    # Step 2: Upload to server
    Write-Host ""
    Write-Host "Step 2: Uploading to server..." -ForegroundColor Yellow

    $uploadUrl = "$ServerUrl/api/v1/firmware/upload"

    # Use curl.exe for multipart upload (not the PowerShell alias)
    try {
        $uploadJson = & curl.exe -s -X POST $uploadUrl `
            -F "file=@$binFile" `
            -F "node_type=$NodeType" `
            -F "version=$Version" `
            -F "hardware=ESP32" `
            -F "release_notes=$Notes" `
            -F "is_stable=true"

        $uploadResponse = $uploadJson | ConvertFrom-Json

        if (-not $uploadResponse.id) {
            throw "No firmware ID in response: $uploadJson"
        }

        $firmwareId = $uploadResponse.id
        Write-Host "  Uploaded firmware ID: $firmwareId" -ForegroundColor Green
    }
    catch {
        Write-Host "  Upload FAILED!" -ForegroundColor Red
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }

    # Step 3: Create OTA update (if requested)
    if ($Deploy) {
        Write-Host ""
        Write-Host "Step 3: Creating OTA update job..." -ForegroundColor Yellow

        $otaUrl = "$ServerUrl/api/v1/ota/updates"
        $otaBody = @{
            firmware_id = $firmwareId
            force_update = $Force.IsPresent
        } | ConvertTo-Json

        try {
            $otaResponse = Invoke-RestMethod -Uri $otaUrl -Method Post -ContentType "application/json" -Body $otaBody
            $updateId = $otaResponse.id

            Write-Host "  Created OTA update ID: $updateId" -ForegroundColor Green
        }
        catch {
            Write-Host "  OTA job creation FAILED!" -ForegroundColor Red
            Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
            Write-Host "  (Firmware was uploaded successfully)" -ForegroundColor Yellow
        }
    }

    # Done
    Write-Host ""
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "  Deployment Complete!" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Firmware: $NodeType v$Version ($fileSizeKB KB)" -ForegroundColor White
    Write-Host "  Server:   $ServerUrl" -ForegroundColor Gray
    if ($Deploy) {
        Write-Host "  OTA:      Update job created" -ForegroundColor Gray
    } else {
        Write-Host "  OTA:      Use -Deploy to create update job" -ForegroundColor Gray
    }
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host ""

}
finally {
    Pop-Location
}
