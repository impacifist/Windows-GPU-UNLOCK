# Read-only firmware comparison; defaults to the current SULK02 recovery backup.
param([string]$StateDirectory='E:\UNLOCK\ESP-SECOND02')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
$backup=Join-Path ([IO.Path]::GetFullPath($StateDirectory)) 'firmware-backup.json'
$s=Get-Content -LiteralPath $backup -Raw | ConvertFrom-Json
if($s.Schema -notin @('shared-esp-audit-once-v1','shared-esp-second-register-v1') -or $s.OptionName -cnotmatch '^Boot7E[0-9A-F]{2}$' -or $s.OptionName -cne ('Boot{0:X4}' -f [int]$s.OptionNumber)){throw 'Unexpected backup schema or option'}
if($s.Schema -eq 'shared-esp-second-register-v1'){
 $decoded=Read-SharedEspLoadOption $s.Option
 if($decoded.Path -cne '\EFI\EVGA-SHARED-LAUNCH02.EFI' -or $decoded.PartitionGuid -ne [guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'){throw 'Unexpected SULK02 recovery option'}
}
if(-not ('SharedEspFirmware.Native' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'shared-esp-firmware-native.cs')}
function DescribeOption($v){
 if($null -eq $v){return $null}
 [byte[]]$data=[Convert]::FromBase64String($v.DataBase64)
 if($data.Length -lt 8){return '[malformed]'}
 $i=6;while($i+1 -lt $data.Length -and ($data[$i] -ne 0 -or $data[$i+1] -ne 0)){$i+=2}
 if($i+1 -ge $data.Length){return '[malformed]'}
 [Text.Encoding]::Unicode.GetString($data,6,$i-6)
}
function DisplayOrder($v){
 if($null -eq $v){return $null};[byte[]]$data=[Convert]::FromBase64String($v.DataBase64)
 if($data.Length -eq 0 -or $data.Length%2){throw 'Malformed BootOrder'}
 @((0..(($data.Length/2)-1)) | ForEach-Object {'Boot{0:X4}' -f [BitConverter]::ToUInt16($data,$_ * 2)})
}
$scope=[SharedEspFirmware.Native]::EnablePrivilege()
try {
 $native=[SharedEspFirmware.Native]::Read('BootOrder');if($null -eq $native){throw 'BootOrder is missing'}
 $current=ConvertTo-FirmwareValue $native.Data $native.Attributes
 $next=[SharedEspFirmware.Native]::Read('BootNext');$test=[SharedEspFirmware.Native]::Read($s.OptionName)
 $testValue=if($test){ConvertTo-FirmwareValue $test.Data $test.Attributes}else{$null}
 $entries=@(foreach($entry in $s.BootEntries){$v=[SharedEspFirmware.Native]::Read($entry.Name);$value=if($v){ConvertTo-FirmwareValue $v.Data $v.Attributes}else{$null};[pscustomobject]@{name=$entry.Name;original_description=(DescribeOption $entry.Value);current_present=($null -ne $value);current_description=(DescribeOption $value);matches_backup=(Test-FirmwareValueEqual $entry.Value $value)}})
 $oldNames=@(DisplayOrder $s.BootOrder)
 $added=@(foreach($name in @(DisplayOrder $current)){if($name -notin $oldNames){$v=[SharedEspFirmware.Native]::Read($name);$value=if($v){ConvertTo-FirmwareValue $v.Data $v.Attributes}else{$null};[pscustomobject]@{name=$name;present=($null -ne $value);description=(DescribeOption $value)}}})
 $nextName=$null;if($next){if($next.Data.Length -eq 2){$nextName='Boot{0:X4}' -f [BitConverter]::ToUInt16($next.Data,0)}else{$nextName='malformed'}}
 [ordered]@{read_only=$true;before_order=@(DisplayOrder $s.BootOrder);current_order=@(DisplayOrder $current);order_attributes_before=$s.BootOrder.Attributes;order_attributes_current=$current.Attributes;existing_entries=$entries;added_entries=$added;temporary_option=$s.OptionName;temporary_option_present=($null -ne $test);temporary_option_matches_backup=(Test-FirmwareValueEqual $s.Option $testValue);boot_next=$nextName} | ConvertTo-Json -Depth 6
}finally{$scope.Dispose()}
