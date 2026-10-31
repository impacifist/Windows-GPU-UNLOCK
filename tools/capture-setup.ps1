# GPL-2.0-only. Host discovery and input validation; no staging in these helpers.
function Get-CaptureProgress([string]$Drive) {
 $dir=$Drive+':\RTX3090-UNLOCK'
 $need=(Get-Content -LiteralPath (Join-Path $PSScriptRoot '../research/profile/proven.json') -Raw | ConvertFrom-Json).min_captures
 $logs=@(Get-ChildItem -LiteralPath $dir -Filter 'DIAG??.TXT' -File -ErrorAction SilentlyContinue | Where-Object {
  $text=[IO.File]::ReadAllText($_.FullName)
  $text.Contains('probe-pci-handles=') -and $text.Contains("capture-complete=1`r`n")
 })
 return [pscustomobject]@{Count=$logs.Count;Required=$need;Directory=$dir;Complete=($logs.Count -ge $need)}
}

function Get-CaptureWindowsPartition {
 $parts=@(Get-Partition | Where-Object IsSystem)
 if($parts.Count -ne 1){throw 'Windows boot partition is ambiguous. Setup stopped; do not guess a GUID. Ask a maintainer for help.'}
 $part=$parts[0];$disk=Get-Disk -Number $part.DiskNumber
 $guid=[guid]::Empty
 if($disk.BusType -eq 'USB' -or $disk.PartitionStyle -ne 'GPT' -or
    ![guid]::TryParse([string]$part.Guid,[ref]$guid) -or $guid -eq [guid]::Empty){
  throw 'This guided setup needs a GPT Windows system EFI partition on an internal disk. Your setup needs maintainer review; no files were staged.'
 }
 return [pscustomobject]@{Guid=$guid;DiskNumber=$part.DiskNumber;PartitionNumber=$part.PartitionNumber}
}

function Get-CaptureUsbChoices {
 $choices=@()
 foreach($disk in @(Get-Disk | Where-Object {$_.BusType -eq 'USB'})) {
  foreach($part in @(Get-Partition -DiskNumber $disk.Number)) {
   if(!$part.DriveLetter){continue}
   $vol=Get-Volume -DriveLetter $part.DriveLetter
   $reason=if($disk.IsBoot -or $disk.IsSystem -or $part.IsSystem -or $part.IsBoot){'Windows boot/system disk'}
    elseif($disk.IsOffline -or $disk.IsReadOnly){'disk is offline or read-only'}
    elseif([string]$part.DriveLetter -notmatch '^[D-Zd-z]$'){'drive letter is reserved by the existing staging tool'}
    elseif(!$disk.UniqueId){'disk identity is unavailable; ask a maintainer'}
    elseif($vol.FileSystem -ne 'FAT32'){'not FAT32; this tool will not format it'}else{''}
   $choices+=[pscustomobject]@{Drive=[string]$part.DriveLetter;DiskNumber=$disk.Number;PartitionNumber=$part.PartitionNumber
    Label=[string]$vol.FileSystemLabel;Model=[string]$disk.FriendlyName;SizeGiB=[math]::Round($vol.Size/1GB,1)
    FileSystem=[string]$vol.FileSystem;Eligible=(!$reason);Reason=$reason
    Identity=[string]$disk.UniqueId;Offset=$part.Offset;SizeBytes=$part.Size}
  }
 }
 return $choices
}

function Select-CaptureUsb($Choices,[string]$Answer) {
 $n=0
 if(![int]::TryParse($Answer,[ref]$n) -or $n -lt 1 -or $n -gt @($Choices).Count){throw 'No USB selected. Setup cancelled without staging.'}
 $choice=@($Choices)[$n-1]
 if(!$choice.Eligible){throw ('That USB cannot be used: '+$choice.Reason)}
 return $choice
}

function Assert-CaptureUsbUnchanged($Selected) {
 $current=@(Get-CaptureUsbChoices | Where-Object {$_.Drive -eq $Selected.Drive})
 if($current.Count -ne 1 -or !$current[0].Eligible -or $current[0].DiskNumber -ne $Selected.DiskNumber -or
    $current[0].PartitionNumber -ne $Selected.PartitionNumber -or $current[0].Model -ne $Selected.Model -or
    $current[0].Label -ne $Selected.Label -or $current[0].Identity -ne $Selected.Identity -or
    $current[0].Offset -ne $Selected.Offset -or $current[0].SizeBytes -ne $Selected.SizeBytes){
  throw 'The USB selection changed. Setup stopped before staging; start again and check the selected drive.'
 }
}

function Find-CaptureTool([string]$Name) {
 $command=Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
 if($command){return $command.Source}
 if($Name -in @('clang','lld-link')) {
  $candidate=Join-Path $env:ProgramFiles "LLVM\bin\$Name.exe"
  if(Test-Path -LiteralPath $candidate -PathType Leaf){return $candidate}
 }
 return $null
}
