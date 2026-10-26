param(
	[switch]$SkipBuild
)

# 双配置验证：strict（嵌套 def 前端拒绝）与 nested（仅放开前端表示能力）都必须构建成功并全绿。
# 依赖安装与配置由 CMakePresets（strict/nested）驱动：toolchain、triplet、安装目录全部来自 preset，
# 本脚本不再手动传 vcpkg/LLVM 路径。

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

$cmakeCandidates = @(
	"C:\Program Files\CMake\bin\cmake.exe",
	"C:\PROGRA~1\CMake\bin\cmake.exe"
)
$cmake = $cmakeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $cmake) {
	$cmake = (Get-Command cmake.exe -ErrorAction SilentlyContinue).Source
}
if (-not $cmake) {
	throw "cmake.exe was not found."
}

$configurations = @(
	@{ Name = "strict"; BuildDir = "build-strict"; Switch = "OFF" },
	@{ Name = "nested"; BuildDir = "build-nested"; Switch = "ON" }
)

$summary = @()

foreach ($configuration in $configurations)
{
	$buildDir = Join-Path $repoRoot $configuration.BuildDir
	$label = $configuration.Name

	if (-not (Test-Path -LiteralPath $buildDir))
	{
		$configureLog = Join-Path $repoRoot "cfg-$label.log"
		& cmd /c "`"$cmake`" --preset $label > `"$configureLog`" 2>&1"
		if ($LASTEXITCODE -ne 0)
		{
			throw "Configure failed for $label (see $configureLog)."
		}
	}

	if (-not $SkipBuild)
	{
		$buildLog = Join-Path $repoRoot "bld-$label.log"
		& cmd /c "`"$cmake`" --build --preset $label > `"$buildLog`" 2>&1"
		if ($LASTEXITCODE -ne 0)
		{
			throw "Build failed for $label (see $buildLog)."
		}
	}

	$testExecutable = Join-Path $buildDir "Calculater\Debug\FunctionsUnitTest.exe"
	if (-not (Test-Path -LiteralPath $testExecutable))
	{
		throw "Test executable was not found: $testExecutable"
	}

	$stdoutLog = Join-Path $repoRoot "run-$label.log"
	$stderrLog = Join-Path $repoRoot "err-$label.log"
	$process = Start-Process -FilePath $testExecutable -NoNewWindow -PassThru `
		-RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
	if (-not $process.WaitForExit(300000))
	{
		$process.Kill()
		throw "Test run for $label timed out."
	}
	if ($process.ExitCode -ne 0)
	{
		throw "Test run for $label failed with exit code $($process.ExitCode) (see $stdoutLog)."
	}

	$passed = 0
	$failed = 0
	foreach ($line in Get-Content -LiteralPath $stdoutLog)
	{
		if ($line -match '^Passed:\s+(\d+)') { $passed = [int]$Matches[1] }
		elseif ($line -match '^Failed:\s+(\d+)') { $failed = [int]$Matches[1] }
	}
	if ($failed -ne 0)
	{
		throw "Test suite for $label reported $failed failures (see $stdoutLog)."
	}

	$summary += [pscustomobject]@{
		Configuration = $label
		Switch = $configuration.Switch
		Passed = $passed
		Stdout = Split-Path -Leaf $stdoutLog
	}
}

Write-Host ""
Write-Host "Dual-configuration verification passed:"
foreach ($entry in $summary)
{
	Write-Host ("  FC_ENABLE_NESTED_FUNCTIONS={0}  {1} -> {2} assertions passed" -f
		$entry.Switch, $entry.Configuration, $entry.Passed)
}
if ($summary[0].Passed -ge $summary[1].Passed)
{
	throw "The enabled configuration is expected to add assertions over the disabled one."
}

exit 0
