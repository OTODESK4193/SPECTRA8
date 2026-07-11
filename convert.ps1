$word = New-Object -ComObject Word.Application
$word.Visible = $false
$files = Get-ChildItem "D:\VST_Project\SPECTRA8\Docs\*.docx"
foreach ($f in $files) {
    $doc = $word.Documents.Open($f.FullName)
    $txtPath = $f.FullName -replace '\.docx$', '.txt'
    $doc.SaveAs([ref]$txtPath, [ref]2)
    $doc.Close()
    Write-Host "Converted: $($f.Name)"
}
$word.Quit()
Write-Host "Done"
