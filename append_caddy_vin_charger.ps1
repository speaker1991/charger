$config = @"

vin-charger.ru, www.vin-charger.ru {
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
Write-Host "Domain vin-charger.ru config appended successfully."
