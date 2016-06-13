@echo off
rem xcopy /s/i/y x64\Release\Debander.ofx.bundle "C:\Program Files\Common Files\OFX\Plugins\Debander.ofx.bundle"
xcopy /s/i/y x64\Debug\Debander.ofx.bundle "C:\Program Files\Common Files\OFX\Plugins\Debander.ofx.bundle"

"C:\Program Files\INRIA\Natron-2.0.3\bin\Natron.exe" "Debander Test Project.ntp"
