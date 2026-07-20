# =================================================================
# InventEngine2 - 遊戲專案自動生成器 (工業級強固版)
# =================================================================

param (
    [Parameter(Mandatory=$false, HelpMessage="請輸入要建立的遊戲項目名稱（例如: MyCoolGame)")]
    [string]$ProjectName,

    [Parameter(Mandatory=$false, HelpMessage="請輸入遊戲項目要存放的硬碟路徑（例如: D:\InventEngineGames)")]
    [string]$TargetDir
)

$OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::InputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# 1. 互動式防錯：如果調用者沒有傳入參數，則在命令列主動彈出提示要求輸入
if ([string]::IsNullOrEmpty($ProjectName)) {
    $ProjectName = Read-Host "請輸入【遊戲項目名稱】"
}
if ([string]::IsNullOrEmpty($TargetDir)) {
    $TargetDir = Read-Host "請輸入【項目存放的父目錄路徑】"
}

# 2. 自動過濾與格式化項目名稱（移除空格，防止 CMake 報錯）
$ProjectName = $ProjectName -replace '\s+', ''
if ([string]::IsNullOrEmpty($ProjectName)) {
    Write-Error "錯誤：項目名稱不能為空！"
    exit 1
}

# 3. 計算出最終遊戲項目的實體硬碟絕對路徑
# 使用 Resolve-Path 確保路徑格式在 Windows 下完全合法
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$TemplateSrcFolder = Join-Path $ScriptDir "GameProject_Template"
$FinalGameFolder = Join-Path $TargetDir $ProjectName

Write-Host "`n[1/3] 正在檢查引擎模板環境..." -ForegroundColor Cyan
if (!(Test-Path $TemplateSrcFolder)) {
    Write-Error "錯誤：找不到模板資料夾 [$TemplateSrcFolder]！請確認腳本位置是否正確。"
    exit 1
}

# 4. 建立目標資料夾並複製模板檔案
Write-Host "[2/3] 正在為新遊戲建立實體目錄結構..." -ForegroundColor Cyan
if (!(Test-Path $FinalGameFolder)) {
    New-Item -ItemType Directory -Path $FinalGameFolder -Force | Out-Null
}

# 遞迴複製所有原始碼與 CMakeLists.txt 到新的遊戲路徑
Copy-Item -Path "$TemplateSrcFolder\*" -Destination $FinalGameFolder -Recurse -Force

# 5. 自動化文本替換：將新遊戲 CMake 中的模板佔位符改為真正的項目名
Write-Host "[3/3] 正在自動化配置新遊戲的 CMakeLists.txt..." -ForegroundColor Cyan
$NewGameCmakePath = Join-Path $FinalGameFolder "CMakeLists.txt"

if (Test-Path $NewGameCmakePath) {
    $Content = [System.IO.File]::ReadAllText($NewGameCmakePath)
    $NewContent = $Content -replace "GAME_TEMPLATE_NAME", $ProjectName
    [System.IO.File]::WriteAllText($NewGameCmakePath, $NewContent, [System.Text.Encoding]::UTF8)
} else {
    Write-Warning "警告：在複製後的目錄中找不到 CMakeLists.txt, 跳過自動重命名。"
}

# 6. 大功告成提示
Write-Host "`n=================================================================" -ForegroundColor Green
Write-Host " 恭喜！獨立遊戲項目 [$ProjectName] 已完美生成！" -ForegroundColor Green
Write-Host " 實體硬碟路徑: $FinalGameFolder" -ForegroundColor Green
Write-Host " 下一步指引:" -ForegroundColor Yellow
Write-Host "   1. 開啟終端機進入該目錄: cd `"$FinalGameFolder`"" -ForegroundColor Yellow
Write-Host "   2. 執行 CMake 進行專案註冊與 SLN 生成:" -ForegroundColor Yellow
Write-Host "      mkdir build && cd build && cmake .." -ForegroundColor Yellow
Write-Host "=================================================================`n" -ForegroundColor Green
