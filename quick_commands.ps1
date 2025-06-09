# ESP-IDF rm01-bsp Project Quick Commands
# Version: 1.0
# Usage: . .\quick_commands.ps1
# 
# Function: Provides simplified development command aliases for improved efficiency

# Project information display
function Show-ProjectInfo {
    Write-Host ""
    Write-Host "===========================================" -ForegroundColor Green
    Write-Host "ESP-IDF rm01-bsp Project Status" -ForegroundColor Green
    Write-Host "===========================================" -ForegroundColor Green
    Write-Host "Project Path: $(Get-Location)" -ForegroundColor Cyan
    Write-Host "IDF_PATH: $env:IDF_PATH" -ForegroundColor Cyan
    
    try {
        $version = & idf.py --version 2>$null
        Write-Host "IDF Version: $version" -ForegroundColor Cyan
    } catch {
        Write-Host "IDF Version: Not available" -ForegroundColor Red
    }
    
    if (Test-Path "build/rm01-bsp.bin") {
        $buildInfo = Get-Item "build/rm01-bsp.bin"
        $size = [math]::Round($buildInfo.Length / 1KB, 1)
        Write-Host "Last Build: $(Get-Date $buildInfo.LastWriteTime -Format 'yyyy-MM-dd HH:mm') (${size}KB)" -ForegroundColor Cyan
    } else {
        Write-Host "Last Build: No build found" -ForegroundColor Yellow
    }
    Write-Host ""
}

# Quick build
function Build-Quick {
    Write-Host "Building rm01-bsp..." -ForegroundColor Yellow
    idf.py build
}

# Quick flash
function Flash-Quick {
    Write-Host "Flashing rm01-bsp..." -ForegroundColor Yellow
    idf.py flash
}

# Quick monitor
function Monitor-Quick {
    Write-Host "Starting monitor (Ctrl+] to exit)..." -ForegroundColor Yellow
    idf.py monitor
}

# Quick flash and monitor
function Flash-Monitor-Quick {
    Write-Host "Flashing and monitoring rm01-bsp..." -ForegroundColor Yellow
    idf.py flash monitor
}

# Quick size analysis
function Size-Quick {
    Write-Host "Analyzing project size..." -ForegroundColor Yellow
    idf.py size
    Write-Host ""
    Write-Host "Component sizes:" -ForegroundColor Cyan
    idf.py size-components
}

# Quick clean
function Clean-Quick {
    Write-Host "Cleaning build..." -ForegroundColor Yellow
    idf.py clean
}

# Set aliases
Set-Alias -Name build -Value Build-Quick
Set-Alias -Name flash -Value Flash-Quick
Set-Alias -Name monitor -Value Monitor-Quick
Set-Alias -Name fm -Value Flash-Monitor-Quick
Set-Alias -Name size -Value Size-Quick
Set-Alias -Name clean -Value Clean-Quick
Set-Alias -Name info -Value Show-ProjectInfo

# Display welcome message
Write-Host ""
Write-Host "ESP-IDF rm01-bsp Quick Commands Loaded!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Available commands:" -ForegroundColor Cyan
Write-Host "  info     - Show project information" -ForegroundColor White
Write-Host "  build    - Build the project" -ForegroundColor White
Write-Host "  flash    - Flash firmware to device" -ForegroundColor White
Write-Host "  monitor  - Monitor serial output" -ForegroundColor White
Write-Host "  fm       - Flash and monitor" -ForegroundColor White
Write-Host "  size     - Show size analysis" -ForegroundColor White
Write-Host "  clean    - Clean build files" -ForegroundColor White
Write-Host ""
Write-Host "Standard idf.py commands also available:" -ForegroundColor Cyan
Write-Host "  idf.py menuconfig  - Configuration menu" -ForegroundColor Gray
Write-Host "  idf.py erase-flash - Erase device flash" -ForegroundColor Gray
Write-Host "  idf.py app-flash   - Flash app only" -ForegroundColor Gray
Write-Host ""

# Display current status
Show-ProjectInfo
