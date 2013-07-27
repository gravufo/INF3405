REM juste besoin de changer le dernier argument dans les parenthèses du IN pour augmenter le nombre d'électeurs.

FOR /L %%A IN (1,1,10) DO (
	start %~dp0\Debug\Electeur.exe
	timeout 2
)