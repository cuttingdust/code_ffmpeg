param(
    [switch]$PullVcpkg,
    [switch]$DefaultFromRemote
)

# 只更新注册表基线，不安装依赖；需要 Git 和指向 vcpkg 仓库的 VCPKG_ROOT。
# 默认使用本地 HEAD；PullVcpkg 先快进拉取，DefaultFromRemote 查询默认远端。
# 两个开关可以组合；外部 Git 退出码仍需显式检查。
$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
$ConfigPath = Join-Path $Root "vcpkg-configuration.json"

function Write-Section {
    param([string]$Message)
    Write-Host ""
    Write-Host $Message
}

function Write-Detail {
    param([string]$Name, [string]$Value)
    Write-Host ("      {0,-18}: {1}" -f $Name, $Value)
}

# 临时切换目录执行 Git，返回第一行；finally 确保失败时也恢复原目录。
function Invoke-GitText {
    param(
        [string]$WorkingDirectory,
        [string[]]$Arguments
    )

    $oldLocation = Get-Location
    try {
        Set-Location $WorkingDirectory
        $output = & git @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "git $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
        }
        return ($output | Select-Object -First 1)
    } finally {
        Set-Location $oldLocation
    }
}

function Get-RemoteHead {
	param([string]$Repository)

	$line = & git ls-remote $Repository HEAD
	if ($LASTEXITCODE -ne 0 -or -not $line) {
		throw "Failed to query remote HEAD: $Repository"
	}

	return (($line | Select-Object -First 1) -split "\s+")[0]
}

# 网络失败保留已有基线并告警；若旧基线也为空则抛错终止。
function Get-RemoteHeadOrCurrent {
	param(
		[string]$Repository,
		[string]$CurrentBaseline
	)

	try {
		return Get-RemoteHead -Repository $Repository
	} catch {
		if ([string]::IsNullOrWhiteSpace($CurrentBaseline)) {
			throw
		}

		Write-Host ("      warning       : {0}" -f $_.Exception.Message) -ForegroundColor Yellow
		Write-Host ("      action        : keep current baseline {0}" -f $CurrentBaseline) -ForegroundColor Yellow
		return $CurrentBaseline
	}
}

function Write-JsonUtf8NoBom {
    param(
        [string]$Path,
        $Value
    )

    $json = $Value | ConvertTo-Json -Depth 50
    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, $encoding)
}

function Quote-Json {
    param([string]$Value)
    return ($Value | ConvertTo-Json -Compress)
}

# 按项目现有结构重建 JSON，保留 overlay 与注册表配置，输出 UTF-8 无 BOM。
# 新增其他配置字段时需同步扩展此写入器，它不是任意 JSON 字段的无损编辑器。
function Write-VcpkgConfiguration {
    param(
        [string]$Path,
        $Config
    )

	$lines = New-Object System.Collections.Generic.List[string]
	$lines.Add("{")

	# Overlay 配置属于项目构建的一部分，更新 baseline 时必须原样保留。
	foreach ($propertyName in @("overlay-ports", "overlay-triplets")) {
		$property = $Config.PSObject.Properties[$propertyName]
		if (-not $property) {
			continue
		}

		$values = @($property.Value)
		$lines.Add("`t`"$propertyName`": [")
		for ($i = 0; $i -lt $values.Count; $i++) {
			$comma = if ($i -lt $values.Count - 1) { "," } else { "" }
			$lines.Add("`t`t$(Quote-Json ([string]$values[$i]))$comma")
		}
		$lines.Add("`t],")
	}

	$lines.Add("`t""default-registry"": {")
	$defaultKind = [string]$Config.'default-registry'.kind
	$defaultRepository = [string]$Config.'default-registry'.repository
	$lines.Add("`t`t""kind"": $(Quote-Json $defaultKind),")
	if (-not [string]::IsNullOrWhiteSpace($defaultRepository)) {
		$lines.Add("`t`t""repository"": $(Quote-Json $defaultRepository),")
	}
	$lines.Add("`t`t""baseline"": $(Quote-Json ([string]$Config.'default-registry'.baseline))")
    $lines.Add("`t},")
    $lines.Add("`t""registries"": [")

    $registries = @($Config.registries)
    for ($i = 0; $i -lt $registries.Count; $i++) {
        $registry = $registries[$i]
        $comma = if ($i -lt $registries.Count - 1) { "," } else { "" }
        $packages = @($registry.packages)

        $lines.Add("`t`t{")
        $lines.Add("`t`t`t""kind"": $(Quote-Json ([string]$registry.kind)),")
        $lines.Add("`t`t`t""repository"": $(Quote-Json ([string]$registry.repository)),")
        $lines.Add("`t`t`t""baseline"": $(Quote-Json ([string]$registry.baseline)),")
        $lines.Add("`t`t`t""packages"": [")

        for ($j = 0; $j -lt $packages.Count; $j++) {
            $packageComma = if ($j -lt $packages.Count - 1) { "," } else { "" }
            $lines.Add("`t`t`t`t$(Quote-Json ([string]$packages[$j]))$packageComma")
        }

        $lines.Add("`t`t`t]")
        $lines.Add("`t`t}$comma")
    }

    $lines.Add("`t]")
    $lines.Add("}")

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllLines($Path, $lines.ToArray(), $encoding)
}

Write-Host "============================================================"
Write-Host " update_vcpkg_configuration"
Write-Host "============================================================"
Write-Detail "Project root" $Root
Write-Detail "Config file" $ConfigPath
Write-Detail "Pull vcpkg" ([string]$PullVcpkg.IsPresent)
Write-Detail "Default remote" ([string]$DefaultFromRemote.IsPresent)
Write-Detail "Time" (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Write-Host "============================================================"

Write-Section "[1/6] Validating files"
if (-not (Test-Path $ConfigPath)) {
    throw "Missing vcpkg-configuration.json: $ConfigPath"
}

if (-not $env:VCPKG_ROOT) {
    throw "VCPKG_ROOT is not set"
}

if (-not (Test-Path (Join-Path $env:VCPKG_ROOT ".git"))) {
    throw "VCPKG_ROOT is not a git checkout: $env:VCPKG_ROOT"
}

Write-Detail "VCPKG_ROOT" $env:VCPKG_ROOT

Write-Section "[2/6] Loading current configuration"
$config = Get-Content -Raw $ConfigPath | ConvertFrom-Json
$oldDefault = [string]$config.'default-registry'.baseline
if (-not $config.'default-registry'.kind) {
	$config.'default-registry' | Add-Member -Force -MemberType NoteProperty -Name kind -Value "builtin"
}
Write-Detail "Current default" $oldDefault

if ($config.registries) {
    foreach ($registry in @($config.registries)) {
        Write-Detail "Current registry" ("{0} -> {1}" -f $registry.repository, $registry.baseline)
    }
}

Write-Section "[3/6] Resolving default vcpkg baseline"
if ($PullVcpkg) {
    Write-Detail "Command" "git -C $env:VCPKG_ROOT pull --ff-only"
    $pullOutput = Invoke-GitText -WorkingDirectory $env:VCPKG_ROOT -Arguments @("pull", "--ff-only")
    Write-Detail "Pull result" ([string]$pullOutput)
}

if ($DefaultFromRemote) {
	$defaultRepo = if ($config.'default-registry'.repository) { [string]$config.'default-registry'.repository } else { "https://github.com/microsoft/vcpkg.git" }
	$newDefault = Get-RemoteHeadOrCurrent -Repository $defaultRepo -CurrentBaseline $oldDefault
	Write-Detail "Source" "remote HEAD"
} else {
	$newDefault = Invoke-GitText -WorkingDirectory $env:VCPKG_ROOT -Arguments @("rev-parse", "HEAD")
	Write-Detail "Source" "local VCPKG_ROOT HEAD"
}

Write-Detail "New default" $newDefault
$config.'default-registry'.baseline = $newDefault
$config.'default-registry'.kind = "builtin"
if ($config.'default-registry'.PSObject.Properties["repository"] -and -not $DefaultFromRemote) {
	$config.'default-registry'.PSObject.Properties.Remove("repository")
}

Write-Section "[4/6] Resolving custom registry baselines"
if ($config.registries) {
    foreach ($registry in @($config.registries)) {
        if ($registry.kind -ne "git") {
            Write-Detail "Skip registry" ("{0} kind={1}" -f $registry.repository, $registry.kind)
            continue
        }

		$repo = [string]$registry.repository
		$old = [string]$registry.baseline
		$new = Get-RemoteHeadOrCurrent -Repository $repo -CurrentBaseline $old
		$registry.baseline = $new
		Write-Detail "Registry" $repo
        Write-Detail "Old baseline" $old
        Write-Detail "New baseline" $new
    }
}

# 基线全部解析完再写回；此前失败不会覆盖配置。
Write-Section "[5/6] Writing vcpkg-configuration.json"
Write-VcpkgConfiguration -Path $ConfigPath -Config $config
Write-Detail "Written" $ConfigPath

# 展示差异供检查，不自动暂存、提交或推送。
Write-Section "[6/6] Final diff"
$repoRoot = Invoke-GitText -WorkingDirectory $Root -Arguments @("rev-parse", "--show-toplevel")
Set-Location $repoRoot
git diff -- src/vcpkg-configuration.json
Write-Host ""
git status --short -- src/vcpkg-configuration.json

Write-Host ""
Write-Host "============================================================"
Write-Host " Done."
Write-Host "============================================================"
Write-Host " Common usage:"
Write-Host "   .\update_vcpkg_configuration.bat"
Write-Host "   .\update_vcpkg_configuration.bat -PullVcpkg"
Write-Host "   .\update_vcpkg_configuration.bat -DefaultFromRemote"
Write-Host "============================================================"
