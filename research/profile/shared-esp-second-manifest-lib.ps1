function Get-SharedSecondManifestChecks($Manifest,[guid]$ExpectedGuid,[string]$ChildHash,[string]$LauncherHash){
 $guidOk=$false
 try {$guidOk=([guid]$Manifest.WindowsEspGuid -eq $ExpectedGuid)}catch{}
 [ordered]@{
  second_launcher_mode=($Manifest.Mode -ceq 'shared-esp-second-launcher-development')
  second_child_name=($Manifest.AuditFileName -ceq 'EVGA-SHARED-UNLOCK02.EFI')
  esp_guid_matches=$guidOk
  staged_child_matches=($ChildHash -match '^[0-9a-fA-F]{64}$' -and $ChildHash -ieq $Manifest.AuditEfiSha256)
  staged_launcher_matches=($LauncherHash -match '^[0-9a-fA-F]{64}$' -and $LauncherHash -ieq $Manifest.EfiSha256)
 }
}
