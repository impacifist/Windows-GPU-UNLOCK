# SPDX-License-Identifier: GPL-2.0-only
# Synthetic ZIP/compiler/source tests only. No downloads or CUDA execution.
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'package-tools.ps1')
$script:ChecksPassed = 0
function Assert-Test {
    param([bool]$Condition,[string]$Message)
    if (-not $Condition) { throw ('Assertion failed: ' + $Message) }
}
function Assert-Refused {
    param([scriptblock]$Action,[string]$Pattern='.')
    $refused = $false
    try { & $Action | Out-Null }
    catch {
        if ($_.Exception.Message -notmatch $Pattern) { throw }
        $refused = $true
    }
    Assert-Test $refused 'Expected refusal.'
}
function Check {
    param([string]$Name,[scriptblock]$Action)
    & $Action
    $script:ChecksPassed++
    Write-Output ('PASS ' + $Name)
}
$testParent = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'build'))
New-Item -ItemType Directory -Force -Path $testParent | Out-Null
$testRoot = Join-Path $testParent ('offline-package-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
try {
    Check 'reviewed lock structure' {
        $lock = Read-BaselineDependencyLock (Join-Path $PSScriptRoot 'packages-lock.json')
        Assert-Test (@($lock.components).Count -eq 4) 'Four components required.'
        $lock.manifest_url = 'https://example.invalid/private'
        $badLock = Join-Path $testRoot 'bad-lock.json'
        [IO.File]::WriteAllText($badLock,($lock | ConvertTo-Json -Depth 6))
        Assert-Refused { Read-BaselineDependencyLock $badLock } 'Unexpected CUDA dependency lock'
    }
    $source = Join-Path $testRoot 'source'
    $archiveRoot = 'component-archive'
    $sourceTree = Join-Path $source $archiveRoot
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceTree 'include'),(Join-Path $sourceTree 'bin'),(Join-Path $sourceTree 'empty') | Out-Null
    $originalHeader = 'synthetic-header'
    [IO.File]::WriteAllText((Join-Path $sourceTree 'include\test.h'),$originalHeader)
    [IO.File]::WriteAllText((Join-Path $sourceTree 'bin\payload.txt'),'synthetic-data')
    $archive = Join-Path $testRoot 'component.zip'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($source,$archive)
    $component = [pscustomobject]@{bytes=(Get-Item -LiteralPath $archive).Length;sha256=(Get-BaselineFileSha256 $archive)}
    Check 'archive size and SHA256' {
        Assert-BaselineArchive -ArchivePath $archive -Component $component
        $bad = [pscustomobject]@{bytes=$component.bytes;sha256=('0' * 64)}
        Assert-Refused { Assert-BaselineArchive -ArchivePath $archive -Component $bad } 'verification failed'
        $bad.bytes++
        Assert-Refused { Assert-BaselineArchive -ArchivePath $archive -Component $bad } 'verification failed'
    }
    $index = Get-BaselineArchiveIndex -ArchivePath $archive -ArchiveRoot $archiveRoot
    Check 'content index includes implicit and empty directories' {
        Assert-Test (@($index.files).Count -eq 2) 'Two fixture files expected.'
        Assert-Test (@($index.directories).Count -eq 4) 'Four fixture directories expected.'
        Assert-Test ($index.tree_sha256 -cmatch '^[a-f0-9]{64}$') 'Canonical tree hash required.'
        $again = Get-BaselineArchiveIndex -ArchivePath $archive -ArchiveRoot $archiveRoot
        Assert-Test ($again.tree_sha256 -ceq $index.tree_sha256) 'Tree hash must be stable.'
    }
    $packages = Join-Path $testRoot 'packages'
    New-Item -ItemType Directory -Path $packages | Out-Null
    $destination = Join-Path $packages 'example'
    $orphan = Join-Path $packages '.staging-interrupted-fixture'
    New-Item -ItemType Directory -Path $orphan | Out-Null
    [IO.File]::WriteAllText((Join-Path $orphan 'incomplete.txt'),'partial')
    Check 'interrupted staging permits a fresh verified publication' {
        $created = Install-BaselineVerifiedArchive -ArchivePath $archive -Destination $destination -Index $index
        Assert-Test $created 'New component should be published.'
        Assert-BaselineExtractedTree -Directory $destination -Index $index
        Assert-Test (Test-Path -LiteralPath (Join-Path $orphan 'incomplete.txt')) 'Prior incomplete staging is preserved.'
    }
    Check 'verified published component can resume without receipt' {
        $created = Install-BaselineVerifiedArchive -ArchivePath $archive -Destination $destination -Index $index
        Assert-Test (-not $created) 'Matching component should be reused.'
    }
    $header = Join-Path $destination ($archiveRoot + '\include\test.h')
    Check 'same-length extracted-file corruption is refused' {
        [IO.File]::WriteAllText($header,'synthetic-headeR')
        Assert-Refused { Install-BaselineVerifiedArchive -ArchivePath $archive -Destination $destination -Index $index } 'content differs'
        Assert-Test ([IO.File]::ReadAllText($header) -ceq 'synthetic-headeR') 'Unverified content is not overwritten.'
        [IO.File]::WriteAllText($header,$originalHeader)
    }
    Check 'missing files are refused' {
        Remove-Item -LiteralPath $header
        Assert-Refused { Assert-BaselineExtractedTree -Directory $destination -Index $index } 'incomplete or changed'
        [IO.File]::WriteAllText($header,$originalHeader)
    }
    Check 'additional files are refused' {
        $extra = Join-Path $destination ($archiveRoot + '\extra.txt')
        [IO.File]::WriteAllText($extra,'extra')
        Assert-Refused { Assert-BaselineExtractedTree -Directory $destination -Index $index } 'unexpected file'
        Remove-Item -LiteralPath $extra
    }
    Check 'additional empty directories are refused' {
        $extra = Join-Path $destination ($archiveRoot + '\unexpected')
        New-Item -ItemType Directory -Path $extra | Out-Null
        Assert-Refused { Assert-BaselineExtractedTree -Directory $destination -Index $index } 'incomplete or changed'
        Remove-Item -LiteralPath $extra
    }
    Check 'incomplete published directory is refused without overwrite' {
        $incomplete = Join-Path $packages 'incomplete'
        New-Item -ItemType Directory -Path $incomplete | Out-Null
        [IO.File]::WriteAllText((Join-Path $incomplete 'keep.txt'),'keep')
        Assert-Refused { Install-BaselineVerifiedArchive -ArchivePath $archive -Destination $incomplete -Index $index } 'unexpected file'
        Assert-Test ([IO.File]::ReadAllText((Join-Path $incomplete 'keep.txt')) -ceq 'keep') 'Existing files are retained.'
    }
    Check 'receipt creation and replacement recover from incomplete temporary data' {
        $receipt = Join-Path $testRoot 'receipt.json'
        [IO.File]::WriteAllText(($receipt + '.pending-interrupted'),'{')
        Write-BaselineAtomicJson -Path $receipt -Value ([pscustomobject]@{generation=1;tree_sha256=$index.tree_sha256})
        Assert-Test ((Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).generation -eq 1) 'Complete first receipt required.'
        Write-BaselineAtomicJson -Path $receipt -Value ([pscustomobject]@{generation=2;tree_sha256=$index.tree_sha256})
        Assert-Test ((Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).generation -eq 2) 'Complete replacement required.'
        Assert-Test ([IO.File]::ReadAllText(($receipt + '.pending-interrupted')) -ceq '{') 'Prior temporary data is preserved.'
    }
    Check 'actual compiler family matches selected documented toolset' {
        Assert-BaselineCompilerVersion '14.29.30133' '19.29.30159.0'
        Assert-BaselineCompilerVersion '14.38.33130' '19.38.33135.0'
        Assert-Refused { Assert-BaselineCompilerVersion '14.44.35207' '19.44.35229.0' } 'outside the reviewed MSVC family'
        Assert-Refused { Assert-BaselineCompilerVersion '14.38.33130' '19.39.33523.0' } 'differs from the selected toolset'
        Assert-Refused { Assert-BaselineCompilerVersion '14.38.33130' 'C:/private/example' } 'outside the reviewed MSVC family'
    }
    Check 'resolved compiler path and executable version are checked' {
        $fakeCompiler = Join-Path $testRoot 'fake-cl.exe'
        [IO.File]::WriteAllText($fakeCompiler,'synthetic non-executable fixture')
        Assert-Refused { Get-BaselineCompilerDescriptor -CompilerPath $fakeCompiler -ExpectedPath (Join-Path $testRoot 'different.exe') -ToolsetVersion '14.38.33130' } 'differs from the selected'
        Assert-Refused { Get-BaselineCompilerDescriptor -CompilerPath $fakeCompiler -ExpectedPath $fakeCompiler -ToolsetVersion '14.38.33130' } 'outside the reviewed MSVC family'
    }
    Check 'ambient compiler overrides are refused without recording values' {
        $saved = @{}
        $names = @('CL','_CL_','NVCC_PREPEND_FLAGS','NVCC_APPEND_FLAGS')
        foreach ($name in $names) {
            $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
            [Environment]::SetEnvironmentVariable($name,$null,'Process')
        }
        try {
            Assert-BaselineCleanCompilerEnvironment
            foreach ($name in $names) {
                [Environment]::SetEnvironmentVariable($name,'C:/private/fixture','Process')
                Assert-Refused { Assert-BaselineCleanCompilerEnvironment } '^Ambient compiler option overrides must be absent or empty; values were not recorded\.$'
                Assert-Test (@(Get-BaselineCompilerOverrides) -contains $name) 'Readiness reports variable names only.'
                [Environment]::SetEnvironmentVariable($name,$null,'Process')
            }
            Assert-BaselineCleanCompilerEnvironment
        } finally {
            foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
        }
    }
    Check 'source snapshot detects changed files and changed commit' {
        $snapshotFile = Join-Path $testRoot 'source-snapshot.txt'
        [IO.File]::WriteAllText($snapshotFile,'before')
        $records = @(Get-BaselineSourceRecords -RepositoryRoot $testRoot -Paths @('source-snapshot.txt'))
        $commit = 'a' * 40
        Assert-BaselineSourceSnapshot -RepositoryRoot $testRoot -Records $records -ExpectedCommit $commit -CurrentCommit $commit
        Assert-Refused { Assert-BaselineSourceSnapshot -RepositoryRoot $testRoot -Records $records -ExpectedCommit $commit -CurrentCommit ('b' * 40) } 'Source snapshot changed'
        [IO.File]::WriteAllText($snapshotFile,'after')
        Assert-Refused { Assert-BaselineSourceSnapshot -RepositoryRoot $testRoot -Records $records -ExpectedCommit $commit -CurrentCommit $commit } 'Source snapshot changed'
    }
    Write-Output ("Synthetic package/compiler/source checks passed: {0}; downloads=0; CUDA executions=0." -f $script:ChecksPassed)
} finally {
    $cleanup = [IO.Path]::GetFullPath($testRoot)
    if (-not $cleanup.StartsWith($testParent + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($cleanup) -cnotmatch '^offline-package-tests-[a-f0-9]{32}$') {
        throw 'Refusing test cleanup outside its owned directory.'
    }
    if (Test-Path -LiteralPath $cleanup) { Remove-Item -LiteralPath $cleanup -Recurse -Force }
}
