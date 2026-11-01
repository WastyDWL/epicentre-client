# vcpkg asset-caching download script (X_VCPKG_ASSET_SOURCES=x-script,...): downloads through
# .NET, which honours the system proxy where vcpkg's own WinHTTP downloader fails.
param([Parameter(Mandatory)][string]$Url, [Parameter(Mandatory)][string]$Dst)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# mirror.msys2.org redirects to a random mirror, and some of them answer 403; try fixed ones too.
$urls = @($Url)
if ($Url -match '^https?://(mirror|repo)\.msys2\.org/(.+)$') {
    $path = $Matches[2]
    $urls += "https://ftp2.osuosl.org/pub/msys2/$path", "https://repo.msys2.org/$path",
             "https://mirrors.dotsrc.org/msys2/$path"
}

$tmp = "$Dst.part"
foreach ($u in $urls) {
    for ($try = 1; $try -le 2; ++$try) {
        try {
            # Some mirrors answer 403 to PowerShell's default user agent.
            Invoke-WebRequest -Uri $u -OutFile $tmp -UseBasicParsing -TimeoutSec 600 -UserAgent 'curl/8.9.1'
            Move-Item -Force $tmp $Dst
            exit 0
        } catch {
            Write-Host "vcpkg-fetch: attempt $try failed for ${u}: $($_.Exception.Message)"
            Start-Sleep -Seconds (2 * $try)
        }
    }
}
exit 1
