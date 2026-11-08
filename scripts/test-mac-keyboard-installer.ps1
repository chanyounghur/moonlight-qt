# Run only on a disposable CI machine: this installs/uninstalls Moonlight.
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $repoRoot 'build/installer-tests'
New-Item -ItemType Directory -Path $testDir -Force | Out-Null
$setup = Join-Path $repoRoot 'build/installer-x64-release/Moonlight-MacKeyboard-v5-x64-Setup.exe'
$official = Join-Path $testDir 'MoonlightSetup-official-6.2.0.exe'
$installDir = Join-Path $env:ProgramFiles 'Moonlight Game Streaming'
$settingsKey = 'HKCU:\Software\Moonlight Game Streaming Project\Moonlight'

function Invoke-Package([string]$path, [string]$verb, [string]$stage) {
    $log = Join-Path $testDir ($stage + '.log')
    $process = Start-Process -FilePath $path -ArgumentList @($verb, '/quiet', '/norestart', '/log', ('"' + $log + '"')) -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -notin @(0, 3010)) { throw "$stage failed with exit code $($process.ExitCode); see $log" }
}

function Verify-Installed([string]$stage) {
    $profile = Get-Content (Join-Path $installDir 'mac-keyboard.json') -Raw | ConvertFrom-Json
    if (!$profile.enabled -or !$profile.rawKeyboard -or $profile.capsLock -ne 'ctrl-space' -or $profile.PSObject.Properties.Name -contains 'normalizeKoreanModifiers') {
        throw "${stage}: wrong installed keyboard profile"
    }
    if (Test-Path (Join-Path $installDir 'portable.dat')) { throw "${stage}: portable settings unexpectedly enabled" }
    foreach ($name in @('Moonlight.exe','source-commit.txt','mac-keyboard.json','START-HERE.txt')) {
        $expected = Get-FileHash (Join-Path $repoRoot "build/deploy-x64-release/$name")
        $actual = Get-FileHash (Join-Path $installDir $name)
        if ($expected.Hash -ne $actual.Hash) { throw "${stage}: installed $name differs from build" }
    }
    $products = @(Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*' -ErrorAction SilentlyContinue |
        Where-Object { $_.DisplayName -like 'Moonlight*' -and $_.WindowsInstaller -eq 1 })
    if ($products.Count -ne 1 -or $products[0].DisplayVersion -ne "6.2.0.$env:MAC_KEYBOARD_BUILD") {
        throw "${stage}: expected one upgraded MSI product"
    }
    $smoke = Start-Process -FilePath (Join-Path $installDir 'Moonlight.exe') -ArgumentList '--version' -WorkingDirectory $installDir -WindowStyle Hidden -Wait -PassThru
    if ($smoke.ExitCode -ne 0) { throw "${stage}: installed application cannot start" }
}

Invoke-WebRequest 'https://github.com/moonlight-stream/moonlight-qt/releases/download/v6.2.0/MoonlightSetup-6.2.0.exe' -OutFile $official
if ((Get-FileHash $official).Hash -ne 'EE2A1B56EC83010F4F8A275BB71892A7DC7A3BC874DFAEBE706B74D5D10831BD') { throw 'Official fixture checksum mismatch' }
Invoke-Package $official '/install' 'official-install'
New-Item -Path $settingsKey -Force | Out-Null
$sentinel = [guid]::NewGuid().ToString()
New-ItemProperty -Path $settingsKey -Name 'installer-test-pairing-sentinel' -Value $sentinel -PropertyType String -Force | Out-Null
Invoke-Package $setup '/install' 'upgrade-to-mac-keyboard'
Verify-Installed 'Upgrade'
if ((Get-ItemPropertyValue -Path $settingsKey -Name 'installer-test-pairing-sentinel') -ne $sentinel) {
    throw 'Existing user settings were lost during upgrade'
}
Write-Output 'Installer upgrade test passed: official 6.2.0 replaced, existing settings preserved, profile included, installed executable starts.'
Invoke-Package $setup '/uninstall' 'uninstall-custom'
if (Test-Path (Join-Path $installDir 'Moonlight.exe')) { throw 'Custom uninstall left the executable installed' }
Invoke-Package $setup '/install' 'clean-install-custom'
Verify-Installed 'Clean install'
Write-Output 'Installer clean-install test passed: profile included, runtime dependencies available, installed executable starts.'
