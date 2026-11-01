# vcpkg asset-caching download script (X_VCPKG_ASSET_SOURCES=x-script,...): downloads through
# .NET, which honours the system proxy where vcpkg's own WinHTTP downloader fails.
param([Parameter(Mandatory)][string]$Url, [Parameter(Mandatory)][string]$Dst)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$tmp = "$Dst.part"
for ($try = 1; $try -le 3; ++$try) {
    try {
        Invoke-WebRequest -Uri $Url -OutFile $tmp -UseBasicParsing -TimeoutSec 600
        Move-Item -Force $tmp $Dst
        exit 0
    } catch {
        Write-Host "vcpkg-fetch: attempt $try failed for ${Url}: $($_.Exception.Message)"
        Start-Sleep -Seconds (3 * $try)
    }
}
exit 1
