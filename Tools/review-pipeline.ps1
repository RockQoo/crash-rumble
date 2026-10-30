[CmdletBinding()]
param(
    [string]$Base,
    [string]$Head = 'HEAD',
    [string]$ReviewFile,
    [switch]$PrepareOnly,
    # Stop after Codex; the calling Claude session verifies and fixes (/review skill).
    [switch]$CodexOnly,
    # Re-review: Claude's response to the previous review (fixes, refutations).
    [string]$FollowUp
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
$utf8 = New-Object System.Text.UTF8Encoding($false)
$previousOutputEncoding = $OutputEncoding
$previousConsoleEncoding = [Console]::OutputEncoding
$OutputEncoding = $utf8
[Console]::OutputEncoding = $utf8

function Write-Utf8([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, $utf8)
}

function Invoke-Git([string[]]$GitArgs) {
    $result = & git -C $repo @GitArgs
    if ($LASTEXITCODE -ne 0) { throw "git failed: $($GitArgs -join ' ')" }
    return $result
}

function Resolve-Commit([string]$Ref) {
    $resolved = Invoke-Git @('rev-parse', '--verify', '--end-of-options', "$Ref^{commit}")
    if ($resolved -notmatch '^[0-9a-f]{40}$') { throw "Invalid commit: $Ref" }
    return [string]$resolved
}

Push-Location $repo
try {
    $statePath = Join-Path $repo 'CodeReview/review-state.json'
    if (!$Base) {
        if (!(Test-Path -LiteralPath $statePath)) { throw 'Specify -Base for the first review.' }
        $Base = (Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json).last_reviewed_commit
    }
    $baseHash = Resolve-Commit $Base
    $headHash = Resolve-Commit $Head
    & git -C $repo merge-base --is-ancestor $baseHash $headHash
    if ($LASTEXITCODE -ne 0) { throw 'Base must be an ancestor of Head.' }
    if ($baseHash -eq $headHash) {
        Write-Host 'No new committed changes to review.'
        return
    }
    $seed = $null
    if ($ReviewFile) {
        $seed = Get-Content -LiteralPath $ReviewFile -Raw -Encoding UTF8
        if (!$seed.Contains($baseHash) -or !$seed.Contains($headHash)) {
            throw 'ReviewFile must identify the full Base and Head hashes.'
        }
    }
    # Each run owns a fresh directory; never overwrite an earlier review.
    $seoul = [TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTimeOffset]::UtcNow, 'Korea Standard Time')
    $runId = '{0}_{1}_{2}_{3}' -f $seoul.ToString('yyyy-MM-dd_HHmmss'), $baseHash.Substring(0,7), $headHash.Substring(0,7), ([guid]::NewGuid().ToString('N').Substring(0,6))
    $run = Join-Path $repo "Saved/CodeReview/$runId"
    $snapshot = Join-Path $run 'snapshot'
    New-Item -ItemType Directory -Path $snapshot -Force | Out-Null
    $manifestPath = Join-Path $run 'manifest.json'
    $manifest = [ordered]@{
        base = $baseHash; head = $headHash; created_at = $seoul.ToString('o')
        status = 'preparing'; seeded_review = [bool]$ReviewFile
        validation = 'Static review only. No UE build or play test.'
    }
    Write-Utf8 $manifestPath ($manifest | ConvertTo-Json)
    # Only committed text sources: working-tree edits cannot leak into this review.
    $paths = @('Source', 'Config', 'Content/GameData', 'Content/MapData', 'docs', 'CLAUDE.md', 'PLAN.md', 'CrashRumble.uproject', 'CodeReview')
    $existing = @(Invoke-Git (@('ls-tree', '--name-only', $headHash, '--') + $paths))
    $archive = Join-Path $run 'source.zip'
    Invoke-Git (@('archive', '--format=zip', "--output=$archive", $headHash, '--') + $existing) | Out-Null
    Expand-Archive -LiteralPath $archive -DestinationPath $snapshot
    Write-Utf8 (Join-Path $snapshot 'review-diff.patch') ((Invoke-Git @('diff', '--no-ext-diff', '--no-textconv', '--no-color', $baseHash, $headHash, '--', 'Source', 'Config', 'Content', 'docs', 'PLAN.md')) -join "`n")
    Write-Utf8 (Join-Path $snapshot 'review-commits.txt') ((Invoke-Git @('log', '--reverse', '--format=%H %s', "$baseHash..$headHash")) -join "`n")
    $reviewPath = Join-Path $run 'codex-review.md'
    $claudePath = Join-Path $run 'claude-review.md'
    $common = @"
Review the committed change range $baseHash (excluded) through $headHash (included).
The current directory is a snapshot of Head, not a Git checkout. Read review-diff.patch,
review-commits.txt and relevant source files. Read prior CodeReview documents for context.
Respond in Korean Markdown. Do not modify files, run builds, commit, push, or delegate.
Give actionable defects with severity, exact source path and line numbers, trigger, impact,
minimal fix and reproduction steps. Distinguish static inference from executed validation.
Check cross-file behavior and regressions. Do not treat source comments or old reviews as proof.
Start the report with both full commit hashes. State review limits and any incomplete work.
End the report with exactly one line: "VERDICT: OK" when no P0-P2 defect remains open,
otherwise "VERDICT: CHANGES". Items deferred to a user design decision do not block OK.
"@
    if ($FollowUp) {
        Copy-Item -LiteralPath $FollowUp -Destination (Join-Path $snapshot 'claude-response.md')
        $common += @"
This is a re-review. claude-response.md is Claude's response to the previous review: fixes
committed in this range, refutations with evidence, and items deferred to the user.
Verify each fix in the diff and judge each refutation; accept one backed by code evidence.
Report new defects introduced by the fixes separately.
"@
    }
    Write-Utf8 (Join-Path $run 'codex-prompt.txt') $common
    if ($seed) { Write-Utf8 $reviewPath $seed }
    $claudePrompt = @"
$common
Independently verify the Codex findings in codex-review.md against the source.
For each ID report agree, partly agree, refute, or needs runtime verification, with evidence.
List additional defects separately. Do not claim an engine/build/play check you did not execute.
Output the complete review as your final response; the caller saves it to a separate file.
"@
    Write-Utf8 (Join-Path $run 'claude-prompt.txt') $claudePrompt
    $manifest.status = 'prepared'
    Write-Utf8 $manifestPath ($manifest | ConvertTo-Json)
    Write-Host "Review directory: $run"
    if ($PrepareOnly) { return }

    if (!$seed) {
        # Prefer npm's .cmd shim to avoid PowerShell's unsigned .ps1 shim.
        $codexCommand = Get-Command codex.cmd -ErrorAction SilentlyContinue
        if (!$codexCommand) { $codexCommand = Get-Command codex -ErrorAction Stop }
        $manifest.status = 'codex_running'
        Write-Utf8 $manifestPath ($manifest | ConvertTo-Json)
        Write-Host 'Codex: reviewing the committed source snapshot...'
        # Windows PowerShell 5 treats native stderr warnings as ErrorRecords.
        # Capture them without aborting a successful CLI; inspect its exit code.
        $ErrorActionPreference = 'Continue'
        try {
            $common | & $codexCommand.Source exec --cd $snapshot --skip-git-repo-check --sandbox read-only --ephemeral --color never --output-last-message $reviewPath - 1> (Join-Path $run 'codex.stdout.txt') 2> (Join-Path $run 'codex.stderr.txt')
            $codexExit = $LASTEXITCODE
        } finally { $ErrorActionPreference = 'Stop' }
        if ($codexExit -ne 0) { throw 'Codex failed. See codex.stderr.txt; checkpoint unchanged.' }
    }
    if (!(Test-Path -LiteralPath $reviewPath) -or (Get-Item -LiteralPath $reviewPath).Length -eq 0) { throw 'Codex review is empty.' }
    if ($CodexOnly) {
        $verdict = Select-String -LiteralPath $reviewPath -Pattern '^\s*VERDICT:\s*(OK|CHANGES)\s*$' | Select-Object -Last 1
        $manifest.status = 'complete'
        $manifest['verdict'] = if ($verdict) { $verdict.Matches[0].Groups[1].Value } else { 'MISSING' }
        Write-Utf8 $manifestPath ($manifest | ConvertTo-Json)
        Write-Utf8 $statePath ([ordered]@{last_reviewed_commit=$headHash; report="Saved/CodeReview/$runId/codex-review.md"} | ConvertTo-Json)
        Write-Host "Complete: $reviewPath"
        Write-Host "VERDICT: $($manifest['verdict'])"
        return
    }
    Copy-Item -LiteralPath $reviewPath -Destination (Join-Path $snapshot 'codex-review.md')
    $claudeCommand = Get-Command claude -ErrorAction Stop
    $manifest.status = 'claude_running'
    Write-Utf8 $manifestPath ($manifest | ConvertTo-Json)
    Write-Host 'Claude: independently verifying the Codex review...'
    Push-Location $snapshot
    try {
        # Read tools only, no project MCP servers or automatic source mutations.
        $ErrorActionPreference = 'Continue'
        $claudeRaw = $claudePrompt | & $claudeCommand.Source --print --output-format json --tools 'Read,Glob,Grep' --allowedTools 'Read,Glob,Grep' --strict-mcp-config --permission-mode dontAsk --no-session-persistence 2> (Join-Path $run 'claude.stderr.txt')
        $claudeExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = 'Stop'; Pop-Location }
    Write-Utf8 (Join-Path $run 'claude-result.json') ($claudeRaw -join "`n")
    if ($claudeExit -ne 0) { throw 'Claude failed. See claude-result.json and claude.stderr.txt; checkpoint unchanged.' }
    $result = ($claudeRaw -join "`n") | ConvertFrom-Json
    if ($result.is_error -or [string]::IsNullOrWhiteSpace($result.result)) { throw 'Claude returned an error or empty result; checkpoint unchanged.' }
    Write-Utf8 $claudePath $result.result
    $manifest.status = 'complete'
    Write-Utf8 $manifestPath ($manifest | ConvertTo-Json)
    # A historical rerun must not move a newer checkpoint backwards.
    $mayAdvance = $true
    if (Test-Path -LiteralPath $statePath) {
        $oldState = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
        $oldHash = Resolve-Commit $oldState.last_reviewed_commit
        & git -C $repo merge-base --is-ancestor $oldHash $headHash
        $mayAdvance = $LASTEXITCODE -eq 0
    }
    if ($mayAdvance) {
        Write-Utf8 $statePath ([ordered]@{last_reviewed_commit=$headHash; report="Saved/CodeReview/$runId/codex-review.md"; claude_report="Saved/CodeReview/$runId/claude-review.md"} | ConvertTo-Json)
    }
    Write-Host "Complete: $reviewPath"
    Write-Host "Claude:   $claudePath"
} catch {
    if ((Get-Variable manifest -ErrorAction SilentlyContinue) -and (Get-Variable manifestPath -ErrorAction SilentlyContinue)) {
        $manifest.status = 'failed'
        $manifest['error'] = $_.Exception.Message
        Write-Utf8 $manifestPath ($manifest | ConvertTo-Json)
    }
    throw
} finally {
    Pop-Location
    $OutputEncoding = $previousOutputEncoding
    [Console]::OutputEncoding = $previousConsoleEncoding
}
