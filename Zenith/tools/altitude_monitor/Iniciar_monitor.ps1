$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
& python monitor.py --auto-connect
