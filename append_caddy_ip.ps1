$config = @"

# Временный доступ по IP пока DNS не заработает
http://31.186.145.255:80 {
    handle_path /mqtt* {
        reverse_proxy 127.0.0.1:9001
    }
    handle_path /api* {
        reverse_proxy 127.0.0.1:8000
    }
    handle {
        reverse_proxy 127.0.0.1:3001
    }
}
"@

Add-Content -Path "c:\Users\nasty\Desktop\voicezettel\Caddyfile" -Value $config -Encoding UTF8
Write-Host "IP route appended successfully."
