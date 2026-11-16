param(
    [ValidateSet('Release','Debug','Hybrid')]
    [string]$Config = 'Release',
    [string]$Batch = 'D:\Program Files\3ds Max 2027\3ds Max 2027\3dsmaxbatch.exe',
    [string]$Ini = '',
    [switch]$AllowInfo
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if (-not $Ini) {
    switch ($Config) {
        'Release' { $Ini = Join-Path $PSScriptRoot 'probe_plugin.ini' }
        'Debug'   { $Ini = Join-Path $PSScriptRoot 'probe_plugin_debug.ini' }
        'Hybrid'  { $Ini = Join-Path $PSScriptRoot 'probe_plugin_hybrid.ini' }
    }
}

$probes = @(
    @{ Name='dim';    Script='probe_dim_three_point.ms'; Result='scripts/fixtures/dim_three_point_result.log'; Tag='[DIM]' },
    @{ Name='text';   Script='probe_cadtext.ms';         Result='scripts/fixtures/cadtext_result.log';         Tag='[TEXT]' },
    @{ Name='leader'; Script='probe_cadleader.ms';       Result='scripts/fixtures/cadleader_result.log';       Tag='[LEADER]' },
    @{ Name='table';  Script='probe_cadtable.ms';        Result='scripts/fixtures/cadtable_result.log';        Tag='[TABLE]' },
    @{ Name='b1';     Script='probe_b1.ms';              Result='scripts/probe_b1_result.log';                 Tag='[PROBE]' },
    @{ Name='compat'; Script='probe_compat_fixtures.ms'; Result='scripts/fixtures/compat_fixtures_result.log'; Tag='[COMPAT]' }
)

$summary = Join-Path $PSScriptRoot 'probe_all_summary.log'
"PROBE_SUITE config=$Config ini=$Ini batch=$Batch" | Set-Content -Path $summary -Encoding UTF8
"timestamp=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" | Add-Content -Path $summary -Encoding UTF8

$totalPass = 0
$totalFail = 0
$totalInfo = 0
$exitCode = 0

foreach ($p in $probes) {
    $scriptPath = Join-Path $PSScriptRoot $p.Script
    $resultPath = Join-Path $root $p.Result
    $logPath = Join-Path $PSScriptRoot ("gate_" + $p.Name + "_" + $Config + ".log")

    if (-not (Test-Path $scriptPath)) {
        "[$($p.Name)] MISSING script: $scriptPath" | Add-Content $summary -Encoding UTF8
        $exitCode = 1
        continue
    }
    if (Test-Path $resultPath) { Remove-Item $resultPath -Force }

    & $Batch $scriptPath -p $Ini -dm on -log $logPath -v 4 | Out-Null
    $procExit = $LASTEXITCODE

    $text = ''
    if (Test-Path $resultPath) { $text = Get-Content $resultPath -Raw }
    $pass = ([regex]::Matches($text, [regex]::Escape($p.Tag + '[PASS]'))).Count
    $fail = ([regex]::Matches($text, [regex]::Escape($p.Tag + '[FAIL]'))).Count
    $info = ([regex]::Matches($text, [regex]::Escape($p.Tag + '[INFO]'))).Count

    $totalPass += $pass
    $totalFail += $fail
    $totalInfo += $info

    $line = "{0,-7} exit={1} PASS={2} FAIL={3} INFO={4}" -f $p.Name, $procExit, $pass, $fail, $info
    $line | Add-Content $summary -Encoding UTF8

    if ($procExit -ne 0 -or $fail -gt 0) { $exitCode = 1 }
}

$dlo = Join-Path $root ("x64\" + $Config + "\Plugins\CADDimension.dlo")
$mui = Join-Path $root ("x64\" + $Config + "\Plugins\en-US\CADDimension.dlo.mui")
if (Test-Path $dlo) {
    $h = (Get-FileHash $dlo -Algorithm SHA256).Hash
    "DLO_SHA256=$h" | Add-Content $summary -Encoding UTF8
}
else {
    "DLO_MISSING=$dlo" | Add-Content $summary -Encoding UTF8
    $exitCode = 1
}
if (Test-Path $mui) {
    $h = (Get-FileHash $mui -Algorithm SHA256).Hash
    "MUI_SHA256=$h" | Add-Content $summary -Encoding UTF8
}
else {
    "MUI_MISSING=$mui" | Add-Content $summary -Encoding UTF8
    $exitCode = 1
}

"TOTAL PASS=$totalPass FAIL=$totalFail INFO=$totalInfo" | Add-Content $summary -Encoding UTF8

if ($exitCode -eq 0 -and $totalInfo -gt 0 -and -not $AllowInfo) {
    "INFO_POLICY=blocked (info>0; pass -AllowInfo only with a documented whitelist)" | Add-Content $summary -Encoding UTF8
    $exitCode = 2
}
elseif ($exitCode -eq 0) {
    "INFO_POLICY=ok" | Add-Content $summary -Encoding UTF8
}

Get-Content $summary
exit $exitCode
