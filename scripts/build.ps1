param(
  [string]$Board = "lckfb_sky_board_stm32f407",
  [string]$AppDir = "",
  [string]$BuildDir = "",
  [string]$Pristine = "auto",
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$WestArgs
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Resolve-Path (Join-Path $ScriptDir "..")

if ([string]::IsNullOrWhiteSpace($AppDir)) {
  $AppDir = $ProjectDir
}

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
  $BuildDir = Join-Path $ProjectDir "build/$Board"
}

$west = Get-Command west -ErrorAction SilentlyContinue
if (-not $west) {
  Write-Error "west not found in PATH. Activate your Zephyr venv first."
  exit 127
}

function Find-WestTopdir([string]$startDir) {
  $dir = Resolve-Path $startDir
  while ($true) {
    if (Test-Path (Join-Path $dir ".west") -PathType Container) {
      return $dir
    }
    $parent = Split-Path -Parent $dir
    if ($parent -eq $dir -or [string]::IsNullOrWhiteSpace($parent)) {
      return $null
    }
    $dir = $parent
  }
}

$westTopdir = Find-WestTopdir $ProjectDir
if (-not $westTopdir) {
  Write-Error "Could not find Zephyr workspace (.west) from $ProjectDir"
  exit 2
}

Push-Location $westTopdir
& west build -b $Board $AppDir -d $BuildDir -p $Pristine @WestArgs
$exitCode = $LASTEXITCODE
Pop-Location
exit $exitCode
