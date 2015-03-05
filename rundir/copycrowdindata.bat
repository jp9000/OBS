@echo off
setlocal enabledelayedexpansion
for /D /r %%d in (crowdin\*) do (
	set longlocale=%%~nd
	set shortlocale=!longlocale:~0,2!
	if !longlocale!==en-PT (
		set localename=pe
	) else ( if !longlocale!==pt-BR (
		set localename=br
	) else ( if !longlocale!==pt-PT (
		set localename=pt
	) else ( if !longlocale!==es-ES (
		set localename=es
	) else ( if !longlocale!==sr-CS (
		set localename=cs
	) else ( if !longlocale!==sv-SE (
		set localename=sv
	) else ( if !longlocale!==zh-CN (
		set localename=zh
	) else ( if !longlocale!==zh-TW (
		set localename=tw
	) else ( if !longlocale!==ur-PK (
		set localename=ur
	) else (
		set localename=!longlocale:~-2!
	) ) ) ) ) ) ) ) )
	for /f %%f in ("crowdin\!longlocale!\Main\!shortlocale!.ini") do set size=%%~zf
	if !size! gtr 0 (
		echo Found locale !longlocale!, copying to !localename!
		copy crowdin\!longlocale!\Main\!shortlocale!.ini locale\!localename!.txt
		copy crowdin\!longlocale!\plugins\DShowPlugin\!shortlocale!.ini plugins\DShowPlugin\locale\!localename!.txt
		copy crowdin\!longlocale!\plugins\PSVPlugin\!shortlocale!.ini plugins\PSVPlugin\locale\!localename!.txt
		copy crowdin\!longlocale!\plugins\scenesw\!shortlocale!.ini plugins\scenesw\locale\!localename!.txt
	)
)