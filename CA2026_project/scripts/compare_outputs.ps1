$projectRoot = Resolve-Path "$PSScriptRoot\.."
$outputDir = "$projectRoot\outputs"
$refDir = "$projectRoot\examples\example_061225_win\reference_outputs"

$files = @(
    "stats0.txt", "stats1.txt", "stats2.txt", "stats3.txt",
    "regout0.txt", "regout1.txt", "regout2.txt", "regout3.txt",
    "core0trace.txt", "core1trace.txt", "core2trace.txt", "core3trace.txt",
    "bustrace.txt",
    "dsram0.txt", "dsram1.txt", "dsram2.txt", "dsram3.txt",
    "tsram0.txt", "tsram1.txt", "tsram2.txt", "tsram3.txt",
    "memout.txt"
)

Write-Host "Comparing Generated Outputs vs Reference Outputs" -ForegroundColor Cyan
Write-Host "Generated: $outputDir"
Write-Host "Reference: $refDir"
Write-Host "---------------------------------------------------"

foreach ($file in $files) {
    $genFile = "$outputDir\$file"
    $refFile = "$refDir\$file"

    if (-not (Test-Path $genFile)) {
        Write-Host "MISSING: $file (Not found in outputs)" -ForegroundColor Red
        continue
    }

    if (-not (Test-Path $refFile)) {
        Write-Host "SKIP:    $file (No reference file found)" -ForegroundColor Gray
        continue
    }

    # Compare content using standard Windows FC (File Compare) for speed
    $process = Start-Process -FilePath "fc.exe" -ArgumentList "/w `"$genFile`" `"$refFile`"" -NoNewWindow -PassThru -Wait -RedirectStandardOutput "fc_out.tmp" -RedirectStandardError "fc_err.tmp"
    
    if ($process.ExitCode -eq 0) {
        Write-Host "MATCH:   $file" -ForegroundColor Green
    } else {
        Write-Host "DIFF:    $file" -ForegroundColor Red
    }
}
