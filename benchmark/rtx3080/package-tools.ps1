# SPDX-License-Identifier: GPL-2.0-only
# CPU-only helpers shared by setup.ps1, build.ps1 and synthetic offline tests.
Set-StrictMode -Version Latest

function Get-BaselineFileSha256 {
    param([Parameter(Mandatory=$true)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-BaselineStreamSha256 {
    param([Parameter(Mandatory=$true)][IO.Stream]$Stream)
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($Stream))).Replace('-','').ToLowerInvariant() }
    finally { $algorithm.Dispose() }
}

function Read-BaselineDependencyLock {
    param([Parameter(Mandatory=$true)][string]$Path)
    $lock = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $expectedNames = @('cuda_nvcc','cuda_cudart','cuda_cccl','libcublas')
    if ($lock.schema_version -ne 1 -or $lock.cuda_toolkit -cne '12.6.0' -or
        $lock.manifest_url -cne 'https://developer.download.nvidia.com/compute/cuda/redist/redistrib_12.6.0.json' -or
        $lock.manifest_sha256 -cnotmatch '^[a-f0-9]{64}$' -or
        @($lock.components).Count -ne 4 -or
        @(Compare-Object $expectedNames @($lock.components | ForEach-Object { $_.name }) -CaseSensitive).Count) {
        throw 'Unexpected CUDA dependency lock.'
    }
    foreach ($item in $lock.components) {
        if ($item.version -cnotmatch '^[0-9]+(\.[0-9]+){2,3}$' -or
            $item.sha256 -cnotmatch '^[a-f0-9]{64}$' -or
            ($item.bytes -isnot [int] -and $item.bytes -isnot [long]) -or $item.bytes -le 0) {
            throw 'Malformed CUDA component lock.'
        }
        $expected = $item.name + '/windows-x86_64/' + $item.name + '-windows-x86_64-' + $item.version + '-archive.zip'
        if ($item.relative_path -cne $expected) { throw 'Unexpected CUDA archive location.' }
    }
    return $lock
}

function Assert-BaselineArchive {
    param([Parameter(Mandatory=$true)][string]$ArchivePath,
          [Parameter(Mandatory=$true)]$Component)
    $item = Get-Item -LiteralPath $ArchivePath -Force
    if ($item -isnot [IO.FileInfo] -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
        $item.Length -ne $Component.bytes -or
        (Get-BaselineFileSha256 $ArchivePath) -cne $Component.sha256) {
        throw 'Pinned CUDA archive size or SHA-256 verification failed.'
    }
}

function Get-BaselineArchiveIndex {
    param([Parameter(Mandatory=$true)][string]$ArchivePath,
          [Parameter(Mandatory=$true)][string]$ArchiveRoot)
    if ($ArchiveRoot -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]+$') {
        throw 'Invalid expected archive root.'
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $files = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
    $directories = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $entryNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $zip = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('\','/')
            $isDirectory = $name.EndsWith('/')
            $relative = if ($isDirectory) { $name.Substring(0,$name.Length-1) } else { $name }
            $parts = $relative.Split('/')
            if ($parts.Count -eq 0 -or $parts[0] -cne $ArchiveRoot -or -not $entryNames.Add($relative)) {
                throw 'Unexpected or duplicate archive member.'
            }
            foreach ($part in $parts) {
                if (-not $part -or $part -eq '.' -or $part -eq '..' -or
                    $part -match '[<>:"|?*\x00-\x1f]' -or $part.EndsWith(' ') -or $part.EndsWith('.') -or
                    $part -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)') {
                    throw 'Archive contains an unsupported Windows member path.'
                }
            }
            if (-not $isDirectory -and $parts.Count -lt 2) {
                throw 'Archive payload must be below its expected root directory.'
            }
            for ($i=1; $i -lt $parts.Count; $i++) {
                [void]$directories.Add(($parts[0..($i-1)] -join '/'))
            }
            if ($isDirectory) {
                [void]$directories.Add($relative)
            } else {
                $stream = $entry.Open()
                try { $digest = Get-BaselineStreamSha256 $stream }
                finally { $stream.Dispose() }
                $files.Add($relative,[pscustomobject]@{path=$relative;bytes=[long]$entry.Length;sha256=$digest})
            }
        }
    } finally { $zip.Dispose() }
    if ($files.Count -eq 0) { throw 'Archive contains no files.' }
    $windowsNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($name in @($files.Keys) + @($directories)) {
        if (-not $windowsNames.Add($name)) { throw 'Archive paths alias on Windows.' }
    }
    [string[]]$fileNames = @($files.Keys)
    [string[]]$directoryNames = @($directories)
    [Array]::Sort($fileNames,[StringComparer]::Ordinal)
    [Array]::Sort($directoryNames,[StringComparer]::Ordinal)
    $lines = [Collections.Generic.List[string]]::new()
    foreach ($name in $directoryNames) { $lines.Add('D ' + $name) }
    foreach ($name in $fileNames) {
        $record = $files[$name]
        $lines.Add('F ' + $record.sha256 + ' ' + $record.bytes.ToString([Globalization.CultureInfo]::InvariantCulture) + ' ' + $name)
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($lines -join [char]10) + [char]10)
    $stream = [IO.MemoryStream]::new($bytes,$false)
    try { $treeDigest = Get-BaselineStreamSha256 $stream }
    finally { $stream.Dispose() }
    return [pscustomobject]@{
        archive_root=$ArchiveRoot
        files=@($fileNames | ForEach-Object { $files[$_] })
        directories=@($directoryNames)
        tree_sha256=$treeDigest
    }
}

function Assert-BaselineExtractedTree {
    param([Parameter(Mandatory=$true)][string]$Directory,
          [Parameter(Mandatory=$true)]$Index)
    $root = [IO.Path]::GetFullPath($Directory).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $rootItem = Get-Item -LiteralPath $root -Force
    if ($rootItem -isnot [IO.DirectoryInfo] -or ($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Extracted component root is not an ordinary directory.'
    }
    $expectedFiles = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
    foreach ($record in $Index.files) { $expectedFiles.Add($record.path,$record) }
    $seenFiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $seenDirectories = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $queue = [Collections.Generic.Queue[string]]::new()
    $queue.Enqueue($root)
    while ($queue.Count) {
        $current = $queue.Dequeue()
        foreach ($item in @(Get-ChildItem -LiteralPath $current -Force)) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw 'Extracted component contains a reparse path.'
            }
            $full = [IO.Path]::GetFullPath($item.FullName)
            if (-not $full.StartsWith($root + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
                throw 'Extracted component entry is outside its root.'
            }
            $relative = $full.Substring($root.Length+1).Replace('\','/')
            if ($item -is [IO.DirectoryInfo]) {
                [void]$seenDirectories.Add($relative)
                $queue.Enqueue($full)
            } elseif ($item -is [IO.FileInfo]) {
                if (-not $expectedFiles.ContainsKey($relative) -or -not $seenFiles.Add($relative)) {
                    throw 'Extracted component contains an unexpected file.'
                }
                $expected = $expectedFiles[$relative]
                if ($item.Length -ne $expected.bytes -or (Get-BaselineFileSha256 $full) -cne $expected.sha256) {
                    throw 'Extracted component content differs from the pinned archive.'
                }
            } else { throw 'Unexpected extracted component entry type.' }
        }
    }
    if ($seenFiles.Count -ne $expectedFiles.Count -or $seenDirectories.Count -ne @($Index.directories).Count) {
        throw 'Extracted component file or directory set is incomplete or changed.'
    }
    foreach ($directoryName in $Index.directories) {
        if (-not $seenDirectories.Contains($directoryName)) { throw 'Extracted component directory set differs.' }
    }
}

function Install-BaselineVerifiedArchive {
    param([Parameter(Mandatory=$true)][string]$ArchivePath,
          [Parameter(Mandatory=$true)][string]$Destination,
          [Parameter(Mandatory=$true)]$Index)
    if (Test-Path -LiteralPath $Destination) {
        Assert-BaselineExtractedTree -Directory $Destination -Index $Index
        return $false
    }
    $parent = Split-Path -Parent ([IO.Path]::GetFullPath($Destination))
    $parentItem = Get-Item -LiteralPath $parent -Force
    if ($parentItem -isnot [IO.DirectoryInfo] -or ($parentItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Component parent must be an ordinary directory.'
    }
    $stage = Join-Path $parent ('.staging-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stage | Out-Null
    # A failed extraction remains isolated here. A later call uses a new stage.
    Expand-Archive -LiteralPath $ArchivePath -DestinationPath $stage
    Assert-BaselineExtractedTree -Directory $stage -Index $Index
    [IO.Directory]::Move($stage,[IO.Path]::GetFullPath($Destination))
    return $true
}

function Write-BaselineAtomicJson {
    param([Parameter(Mandatory=$true)][string]$Path,
          [Parameter(Mandatory=$true)]$Value)
    $temporary = $Path + '.pending-' + [guid]::NewGuid().ToString('N')
    $bytes = [Text.Encoding]::UTF8.GetBytes(($Value | ConvertTo-Json -Depth 12))
    $stream = [IO.File]::Open($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $stream.Write($bytes,0,$bytes.Length); $stream.Flush($true) }
    finally { $stream.Dispose() }
    if (Test-Path -LiteralPath $Path) {
        $existing = Get-Item -LiteralPath $Path -Force
        if ($existing -isnot [IO.FileInfo] -or ($existing.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Receipt destination is not an ordinary file.'
        }
        [IO.File]::Replace($temporary,$Path,[NullString]::Value)
    } else { [IO.File]::Move($temporary,$Path) }
}

function Assert-BaselineCompilerVersion {
    param([Parameter(Mandatory=$true)][string]$ToolsetVersion,
          [Parameter(Mandatory=$true)][string]$CompilerVersion)
    $toolset = [regex]::Match($ToolsetVersion,'^14\.(2[0-9]|3[0-9])\.[0-9]+(\.[0-9]+)?$')
    $compiler = [regex]::Match($CompilerVersion,'^19\.(2[0-9]|3[0-9])\.[0-9]+\.[0-9]+$')
    if (-not $toolset.Success -or -not $compiler.Success -or $toolset.Groups[1].Value -cne $compiler.Groups[1].Value) {
        throw 'Resolved compiler version is outside the reviewed MSVC family or differs from the selected toolset.'
    }
}

function Get-BaselineCompilerDescriptor {
    param([Parameter(Mandatory=$true)][string]$CompilerPath,
          [Parameter(Mandatory=$true)][string]$ExpectedPath,
          [Parameter(Mandatory=$true)][string]$ToolsetVersion)
    $resolved = [IO.Path]::GetFullPath($CompilerPath)
    $expected = [IO.Path]::GetFullPath($ExpectedPath)
    if (-not [string]::Equals($resolved,$expected,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Resolved cl.exe differs from the selected x64 compiler.'
    }
    $item = Get-Item -LiteralPath $resolved -Force
    if ($item -isnot [IO.FileInfo] -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Resolved compiler is not an ordinary file.'
    }
    $info = $item.VersionInfo
    $version = '{0}.{1}.{2}.{3}' -f $info.FileMajorPart,$info.FileMinorPart,$info.FileBuildPart,$info.FilePrivatePart
    Assert-BaselineCompilerVersion -ToolsetVersion $ToolsetVersion -CompilerVersion $version
    return [pscustomobject]@{
        msvc_compiler_version=$version
        msvc_compiler_sha256=(Get-BaselineFileSha256 $resolved)
        compiler_directory=$item.DirectoryName
    }
}

function Get-BaselineCompilerOverrides {
    $names = @('CL','_CL_','NVCC_PREPEND_FLAGS','NVCC_APPEND_FLAGS')
    return @($names | Where-Object { [Environment]::GetEnvironmentVariable($_,'Process') })
}

function Assert-BaselineCleanCompilerEnvironment {
    if (@(Get-BaselineCompilerOverrides).Count) {
        throw 'Ambient compiler option overrides must be absent or empty; values were not recorded.'
    }
}

function Get-BaselineSourceRecords {
    param([Parameter(Mandatory=$true)][string]$RepositoryRoot,
          [Parameter(Mandatory=$true)][string[]]$Paths)
    foreach ($relative in $Paths) {
        [pscustomobject]@{path=$relative;sha256=(Get-BaselineFileSha256 (Join-Path $RepositoryRoot $relative))}
    }
}

function Assert-BaselineSourceSnapshot {
    param([Parameter(Mandatory=$true)][string]$RepositoryRoot,
          [Parameter(Mandatory=$true)][object[]]$Records,
          [Parameter(Mandatory=$true)][string]$ExpectedCommit,
          [Parameter(Mandatory=$true)][string]$CurrentCommit)
    if ($ExpectedCommit -cnotmatch '^[a-f0-9]{40}$' -or $CurrentCommit -cne $ExpectedCommit) {
        throw 'Source snapshot changed; no artifacts or manifest published.'
    }
    try {
        foreach ($record in $Records) {
            if ((Get-BaselineFileSha256 (Join-Path $RepositoryRoot $record.path)) -cne $record.sha256) {
                throw 'Source content differs.'
            }
        }
    } catch { throw 'Source snapshot changed or became unreadable; no artifacts or manifest published.' }
}
