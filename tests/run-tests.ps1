# In der Developer PowerShell fuer Visual Studio starten.
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
Push-Location $project
try {
    foreach ($config in 'Release','Debug') {
        & msbuild Programmstarter.vcxproj /nologo /v:minimal /p:Platform=x64 "/p:Configuration=$config"
        if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen: $config" }
    }
    & msbuild Programmstarter.vcxproj /nologo /v:minimal /p:Platform=x64 /p:Configuration=Release /p:StarterMode=Probe '/p:OutDir=bin\Test ä mit Leerzeichen\'
    if ($LASTEXITCODE -ne 0) { throw 'Probe-Build fehlgeschlagen' }
    & msbuild Programmstarter.vcxproj /nologo /v:minimal /p:Platform=x64 /p:Configuration=Release /p:StarterMode=Test '/p:OutDir=bin\Tests\'
    if ($LASTEXITCODE -ne 0) { throw 'Test-Build fehlgeschlagen' }
    Copy-Item programme.txt bin/Tests/programme.txt -Force
    $process = Start-Process -FilePath (Join-Path $project 'bin/Tests/Programmstarter.exe') -WorkingDirectory (Join-Path $project 'bin/Tests') -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(15000)) { throw 'Test-Zeitlimit erreicht' }
    Get-Content bin/Tests/Testergebnis.txt
    if ($process.ExitCode -ne 0) { throw 'Tests fehlgeschlagen' }
} finally { Pop-Location }
