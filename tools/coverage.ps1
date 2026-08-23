param(
	[string]$BuildDir = "build-debug",
	[string]$Configuration = "Debug",
	[double]$MinimumCoverage = 95.0
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot ".." )).Path
$buildPath = if ([IO.Path]::IsPathRooted($BuildDir)) {
	$BuildDir
} else {
	Join-Path $repoRoot $BuildDir
}
$buildRoot = (Resolve-Path $buildPath).Path
$testExecutable = Join-Path $buildRoot "Calculater\$Configuration\FunctionsUnitTest.exe"
$coverageXml = Join-Path $buildRoot "FunctionsUnitTest.coverage.xml"

if (-not (Test-Path -LiteralPath $testExecutable)) {
	throw "Test executable was not found: $testExecutable"
}

$coverageCandidates = @(
	"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\Extensions\Microsoft\CodeCoverage.Console\Microsoft.CodeCoverage.Console.exe",
	"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\Extensions\Microsoft\CodeCoverage.Console\Microsoft.CodeCoverage.Console.exe",
	"C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\Extensions\Microsoft\CodeCoverage.Console\Microsoft.CodeCoverage.Console.exe",
	"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\Extensions\Microsoft\CodeCoverage.Console\Microsoft.CodeCoverage.Console.exe"
)
$coverageTool = $coverageCandidates |
	Where-Object { Test-Path -LiteralPath $_ } |
	Select-Object -First 1

if (-not $coverageTool) {
	$coverageTool = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Filter Microsoft.CodeCoverage.Console.exe -Recurse -ErrorAction SilentlyContinue |
		Select-Object -First 1 -ExpandProperty FullName
}

if (-not $coverageTool) {
	throw "Microsoft.CodeCoverage.Console.exe was not found. Install the Visual Studio code coverage tools."
}

& $coverageTool collect `
	--output $coverageXml `
	--output-format xml `
	--include-files (Join-Path $repoRoot "Calculater\*.cc") `
	-- $testExecutable

if ($LASTEXITCODE -ne 0) {
	throw "The covered test process failed with exit code $LASTEXITCODE."
}

[xml]$report = Get-Content -LiteralPath $coverageXml -Raw
$sourcePaths = @{}
foreach ($source in $report.results.modules.module.source_files.source_file) {
	$sourcePaths[[string]$source.id] = [string]$source.path
}

# Only production implementation files are measured. Test drivers and main.cc
# are excluded so the threshold describes the library implementation itself.
$lineStates = @{}
foreach ($module in $report.results.modules.module) {
	foreach ($function in $module.functions.function) {
	foreach ($range in $function.ranges.range) {
		$sourceId = [string]$range.source_id
		if (-not $sourcePaths.ContainsKey($sourceId)) { continue }
		$sourcePath = $sourcePaths[$sourceId]
		if ([IO.Path]::GetExtension($sourcePath) -ne ".cc") { continue }
		if ($sourcePath -match "\\(unit_test|call_ast_test|main)\.cc$") { continue }

		$key = "$sourcePath|$([string]$range.start_line)"
		$state = switch ([string]$range.covered) {
			"yes" { 2 }
			"partial" { 1 }
			default { 0 }
		}
		if (-not $lineStates.ContainsKey($key) -or $state -gt $lineStates[$key]) {
			$lineStates[$key] = $state
		}
	}
	}
}

$files = @{}
foreach ($entry in $lineStates.GetEnumerator()) {
	$separator = $entry.Key.LastIndexOf("|")
	$path = $entry.Key.Substring(0, $separator)
	if (-not $files.ContainsKey($path)) {
		$files[$path] = @{ covered = 0; total = 0 }
	}
	++$files[$path].total
	if ($entry.Value -gt 0) { ++$files[$path].covered }
}

$totalCovered = 0
$totalLines = 0
foreach ($path in ($files.Keys | Sort-Object)) {
	$covered = $files[$path].covered
	$total = $files[$path].total
	$totalCovered += $covered
	$totalLines += $total
	$percentage = if ($total -eq 0) { 100.0 } else { 100.0 * $covered / $total }
	Write-Host ("{0}: {1:N2}% ({2}/{3})" -f $path, $percentage, $covered, $total)
}

$overall = if ($totalLines -eq 0) { 0.0 } else { 100.0 * $totalCovered / $totalLines }
Write-Host ("Overall production line coverage: {0:N2}% ({1}/{2})" -f $overall, $totalCovered, $totalLines)

if ($overall -lt $MinimumCoverage) {
	throw ("Coverage {0:N2}% is below the required {1:N2}% threshold." -f $overall, $MinimumCoverage)
}

exit 0
