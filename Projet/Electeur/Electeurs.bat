REM juste besoin de changer le dernier argument dans les parenth�ses du IN pour augmenter le nombre d'�lecteurs.

FOR /L %%A IN (1,1,10) DO (
	start %~dp0\Debug\Electeur.exe
	timeout 2
)