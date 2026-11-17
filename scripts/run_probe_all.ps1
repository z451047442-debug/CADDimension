param(
    [ValidateSet('Release','Debug','Hybrid')]
    [string]$Config = 'Release',
    [string]$Batch = 'D:\Program Files\3ds Max 2027\3ds Max 2027\3dsmaxbatch.exe',
    [string]$Ini = '',
    [switch]$AllowInfo,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Get-GateExitCode {
    param(
        [bool]$ResultExists,
        [int]$Pass,
        [int]$Fail,
        [int]$Fatal,
        [int]$DoneCount,
        [int]$ExpectedPass,
        [int]$UnexpectedInfo,
        [bool]$HashMatch
    )
    if (-not $ResultExists) { return 1 }
    if ($Fail -gt 0 -or $Fatal -gt 0) { return 1 }
    if ($DoneCount -lt 1) { return 1 }
    if ($ExpectedPass -gt 0 -and $Pass -lt $ExpectedPass) { return 1 }
    if ($UnexpectedInfo -gt 0) { return 1 }
    if (-not $HashMatch) { return 1 }
    return 0
}

if ($SelfTest) {
    $cases = @(
        @{ Name = 'missing result'; Params = @{ ResultExists = $false; Pass = 0; Fail = 0; Fatal = 0; DoneCount = 0; ExpectedPass = 1; UnexpectedInfo = 0; HashMatch = $true } },
        @{ Name = 'PASS below baseline'; Params = @{ ResultExists = $true; Pass = 28; Fail = 0; Fatal = 0; DoneCount = 1; ExpectedPass = 29; UnexpectedInfo = 0; HashMatch = $true } },
        @{ Name = 'missing DONE'; Params = @{ ResultExists = $true; Pass = 29; Fail = 0; Fatal = 0; DoneCount = 0; ExpectedPass = 29; UnexpectedInfo = 0; HashMatch = $true } },
        @{ Name = 'unexpected INFO'; Params = @{ ResultExists = $true; Pass = 29; Fail = 0; Fatal = 0; DoneCount = 1; ExpectedPass = 29; UnexpectedInfo = 1; HashMatch = $true } },
        @{ Name = 'hash mismatch'; Params = @{ ResultExists = $true; Pass = 29; Fail = 0; Fatal = 0; DoneCount = 1; ExpectedPass = 29; UnexpectedInfo = 0; HashMatch = $false } }
    )
    $bad = 0
    foreach ($c in $cases) {
        $p = $c.Params
        $rc = Get-GateExitCode -ResultExists $p.ResultExists -Pass $p.Pass -Fail $p.Fail -Fatal $p.Fatal -DoneCount $p.DoneCount -ExpectedPass $p.ExpectedPass -UnexpectedInfo $p.UnexpectedInfo -HashMatch $p.HashMatch
        if ($rc -eq 0) { "SELFTEST FAIL: $($c.Name)"; $bad++ } else { "SELFTEST PASS: $($c.Name)" }
    }
    if ($bad -gt 0) { exit 1 }
    exit 0
}

if (-not $Ini) {
    switch ($Config) {
        'Release' { $Ini = Join-Path $PSScriptRoot 'probe_plugin.ini' }
        'Debug'   { $Ini = Join-Path $PSScriptRoot 'probe_plugin_debug.ini' }
        'Hybrid'  { $Ini = Join-Path $PSScriptRoot 'probe_plugin_hybrid.ini' }
    }
}

$expected = @{
    dim = 29; text = 7; leader = 11; table = 20; b1 = 16; compat = 8
}
$expectedHashes = @{
    Release = @{ DLO = '92B62A52FA288DEAFC0405DF6D1DDA4DEE1756AC784382359C52969ADFFB963A'; MUI = '5343DABFDCBFAD95C19FBC4980E3B0688AB256D130F478B2BC8C83B30E987986' }
    Debug   = @{ DLO = 'A1792D29F15CE2083330A3761FEC1993841351C3D1A5C38EE3932D9191DE0294'; MUI = '4D77932AF0025D2E7B87FA7596B3F5895A5472BB619AF508B3599636CB4C6A41' }
    Hybrid  = @{ DLO = 'B5FD5DA5EF7C8053EEA85FED1C907AFA613777EDB67450ECB44AD34D6B985FF5'; MUI = '5343DABFDCBFAD95C19FBC4980E3B0688AB256D130F478B2BC8C83B30E987986' }
}

$infoWhitelist = @(
    '[PROBE][INFO] G7 legacy B1 scene loaded without crashing',
    '[PROBE][INFO] CADDimension class resolved: CADDimension',
    '[PROBE][INFO] G3 undo: ProbeBox not restored (batch undo may be disabled)',
    '[PROBE][INFO] after undo, ProbeDim.targetNode = undefined',
    '[PROBE][INFO] G5 target-move redraw / ObjectValidity is not verifiable in batch; interactive check required.'
)

$probes = @(
    @{ Name='dim';    Script='probe_dim_three_point.ms'; Result='scripts/fixtures/dim_three_point_result.log'; Tag='[DIM]' },
    @{ Name='text';   Script='probe_cadtext.ms';         Result='scripts/fixtures/cadtext_result.log';         Tag='[TEXT]' },
    @{ Name='leader'; Script='probe_cadleader.ms';       Result='scripts/fixtures/cadleader_result.log';       Tag='[LEADER]' },
    @{ Name='table';  Script='probe_cadtable.ms';        Result='scripts/fixtures/cadtable_result.log';        Tag='[TABLE]' },
    @{ Name='b1';     Script='probe_b1.ms';              Result='scripts/probe_b1_result.log';                 Tag='[PROBE]' },
    @{ Name='compat'; Script='probe_compat_fixtures.ms'; Result='scripts/fixtures/compat_fixtures_result.log'; Tag='[COMPAT]' }
)

$summary = Join-Path $PSScriptRoot ("probe_all_summary_" + $Config + ".log")
"PROBE_SUITE config=$Config ini=$Ini batch=$Batch" | Set-Content -Path $summary -Encoding UTF8
"timestamp=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" | Add-Content -Path $summary -Encoding UTF8

$totalPass = 0
$totalFail = 0
$totalInfo = 0
$unexpectedInfo = 0
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

    $resultExists = Test-Path $resultPath
    $text = ''
    if ($resultExists) { $text = Get-Content $resultPath -Raw }

    $pass  = ([regex]::Matches($text, [regex]::Escape($p.Tag + '[PASS]'))).Count
    $fail  = ([regex]::Matches($text, [regex]::Escape($p.Tag + '[FAIL]'))).Count
    $info  = ([regex]::Matches($text, [regex]::Escape($p.Tag + '[INFO]'))).Count
    $done  = ([regex]::Matches($text, [regex]::Escape($p.Tag + '[DONE]'))).Count
    $fatal = ([regex]::Matches($text, [regex]::Escape($p.Tag) + '\[[^\]]*FATAL[^\]]*\]')).Count

    $localUnexpected = 0
    $infoMatches = [regex]::Matches($text, '(?m)^.*' + [regex]::Escape($p.Tag + '[INFO]') + '.*$')
    foreach ($m in $infoMatches) {
        $line = $m.Value.Trim()
        if ($infoWhitelist -notcontains $line) {
            $localUnexpected++
            "UNEXPECTED_INFO: $line" | Add-Content $summary -Encoding UTF8
        }
    }

    $totalPass += $pass
    $totalFail += $fail
    $totalInfo += $info
    $unexpectedInfo += $localUnexpected

    $expectedPass = 0
    if ($expected.ContainsKey($p.Name)) { $expectedPass = [int]$expected[$p.Name] }

    $rc = Get-GateExitCode -ResultExists $resultExists -Pass $pass -Fail $fail -Fatal $fatal -DoneCount $done -ExpectedPass $expectedPass -UnexpectedInfo $localUnexpected -HashMatch $true
    if ($procExit -ne 0 -or $rc -ne 0) { $exitCode = 1 }

    $line = "{0,-7} exit={1} PASS={2} FAIL={3} INFO={4} DONE={5} FATAL={6} EXPECT={7}" -f $p.Name, $procExit, $pass, $fail, $info, $done, $fatal, $expectedPass
    $line | Add-Content $summary -Encoding UTF8
}

$dlo = Join-Path $root ("x64\" + $Config + "\Plugins\CADDimension.dlo")
$mui = Join-Path $root ("x64\" + $Config + "\Plugins\en-US\CADDimension.dlo.mui")
$hashMatch = $true
if (Test-Path $dlo) {
    $h = (Get-FileHash $dlo -Algorithm SHA256).Hash
    "DLO_SHA256=$h" | Add-Content $summary -Encoding UTF8
    if ($expectedHashes.ContainsKey($Config) -and $expectedHashes[$Config].DLO -ne $h) {
        "DLO_HASH_MISMATCH expected=$($expectedHashes[$Config].DLO)" | Add-Content $summary -Encoding UTF8
        $hashMatch = $false
    }
}
else {
    "DLO_MISSING=$dlo" | Add-Content $summary -Encoding UTF8
    $hashMatch = $false
}
if (Test-Path $mui) {
    $h = (Get-FileHash $mui -Algorithm SHA256).Hash
    "MUI_SHA256=$h" | Add-Content $summary -Encoding UTF8
    if ($expectedHashes.ContainsKey($Config) -and $expectedHashes[$Config].MUI -ne $h) {
        "MUI_HASH_MISMATCH expected=$($expectedHashes[$Config].MUI)" | Add-Content $summary -Encoding UTF8
        $hashMatch = $false
    }
}
else {
    "MUI_MISSING=$mui" | Add-Content $summary -Encoding UTF8
    $hashMatch = $false
}

$hashRc = Get-GateExitCode -ResultExists $true -Pass 1 -Fail 0 -Fatal 0 -DoneCount 1 -ExpectedPass 0 -UnexpectedInfo 0 -HashMatch $hashMatch
if ($hashRc -ne 0) { $exitCode = 1 }

"TOTAL PASS=$totalPass FAIL=$totalFail INFO=$totalInfo UNEXPECTED_INFO=$unexpectedInfo" | Add-Content $summary -Encoding UTF8

if ($exitCode -eq 0) { "INFO_POLICY=ok (whitelist enforced)" | Add-Content $summary -Encoding UTF8 }
else { "INFO_POLICY=blocked (see FAIL/DONE/FATAL/INFO/HASH above)" | Add-Content $summary -Encoding UTF8 }

Get-Content $summary
exit $exitCode
