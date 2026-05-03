$path = "c:\Users\nasty\Desktop\voicezettel\Caddyfile"
$content = Get-Content -Path $path -Raw
$logBlock = @"
{
    log {
        output file c:\Users\nasty\Desktop\caddy.log
        level DEBUG
    }
}
"@
$newContent = $logBlock + "`n" + $content
Set-Content -Path $path -Value $newContent -Encoding UTF8
