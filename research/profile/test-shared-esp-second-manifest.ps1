$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-second-manifest-lib.ps1')
$g=[guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
$m=@{Mode='shared-esp-second-launcher-development';AuditFileName='EVGA-SHARED-UNLOCK02.EFI';WindowsEspGuid=$g.ToString();AuditEfiSha256=('A'*64);EfiSha256=('B'*64)}
$c=Get-SharedSecondManifestChecks $m $g ('a'*64) ('b'*64)
if(@($c.Values | Where-Object {!$_}).Count){throw 'Matching manifest refused'}
foreach($field in @('Mode','AuditFileName','WindowsEspGuid','AuditEfiSha256','EfiSha256')){
 $bad=$m.Clone();$bad[$field]='wrong';$c=Get-SharedSecondManifestChecks $bad $g ('A'*64) ('B'*64)
 if(@($c.Values | Where-Object {!$_}).Count -ne 1){throw "Did not isolate $field"}
}
'PASS 6 manifest comparison cases; no hardware or files accessed'
