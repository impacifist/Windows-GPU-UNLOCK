# Create-new copy; caller must qualify the destination directory.
function Copy-SharedSecondPayload([string]$Source,[string]$Target,[string]$ExpectedHash){
 if((Get-Item -LiteralPath $Source).Length -gt 2MB){throw 'Unexpected payload size'}
 if((Get-FileHash -LiteralPath $Source).Hash -ine $ExpectedHash){throw 'Payload source hash differs'}
 $data=[IO.File]::ReadAllBytes($Source)
 $sha=[Security.Cryptography.SHA256]::Create()
 try {$hash=([BitConverter]::ToString($sha.ComputeHash($data))).Replace('-','')}finally{$sha.Dispose()}
 if($hash -ine $ExpectedHash){throw 'Payload changed before copy'}
 $stream=$null
 try {$stream=[IO.File]::Open($Target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$stream.Write($data,0,$data.Length);$stream.Flush($true)}finally{if($stream){$stream.Dispose()}}
 if((Get-FileHash -LiteralPath $Target).Hash -ine $ExpectedHash){throw 'Payload readback differs; preserve partial stage'}
}
