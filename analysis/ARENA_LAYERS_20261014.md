# Etap tła aren i pomijania logotypów — 14 października, 17:27

Limit etapu: 30 minut. Kopia wersji potwierdzonej bez błędów UI: `backups/before-arena-layer-fix-20261014-1727`.

Pomiar pojedynczej warstwy sprzętowej (`VF2_LAYER_PROBE=1`) pokazał chmury/horyzont w intrze bez napisów; wybór postaci zawiera w tej samej warstwie grafikę PLAYER SELECT, więc numer warstwy nie wystarcza do ograniczenia odbicia. Funkcja `sub_82145A90` renderuje obie warstwy sprzętowe do obu końcowych bitmap zależnie od kolejności przed/po 3D. Dawne przypisanie końcowych tekstur do samego HUD lub tła było niepoprawne.

Wariant testowy (`VF2_ARENA_TRIAL=1`): obie bitmapy końcowe mają bufor 684×384 i logiczną projekcję -93.333…589.333. HUD pozostaje w swoim oryginalnym obszarze 0…496. Lustrzane fragmenty są tworzone przed kompozycją dla sprzętowej warstwy 1 tylko w trybie przewijanym; każdy fragment przycinany jest do 0…496 przed odbiciem. Całe końcowe bitmapy nie są powielane. Smoothing jest przygotowany wyłącznie dla przesunięcia warstwy 1 w oryginalnych builderach scrolla, z zachowaniem ułamkowego przesunięcia przed emisją quadów.

Aktualnie shipping `Windows/vf2.exe` nadal pozostaje wersją bez eksperymentalnych zmian. Warianty pomiarowe uruchamiane jako `Windows/vf2_probe.exe`. Parametry UHD: gość1920×1080 / GPU2×2.

## Osobne render-targety horyzontu — etap od 18:04

Nowa hipoteza po zgłoszeniu nieruchomego środka i wsuwania boków: odbicie ma dotyczyć gotowego obrazu horyzontu, po wykonaniu oryginalnego shadera, a przed kompozycją z HUD. Nie jest to jeszcze potwierdzona przyczyna dawnego błędu.

Implementacja próbna tworzy dwa dodatkowe render-targety 496×384, zachowując oryginalną projekcję 0…496. W `sub_82144050` przekierowuje sprzętową warstwę1 w trybie przewijanym1/2 do oddzielnego targetu dla właściwej kolejności przed/po3D. Przed renderowaniem warstwy0 przywraca docelowy target HUD i czyści go z alpha0. Po zakończeniu `sub_82145A90` deskryptory są zawsze przywracane. Podczas kompozycji `sub_82144548` / `sub_821445B0` rysuje sam horyzont wraz z lustrzanymi bokami, następnie pojedynczy HUD.

Smoothing dotyczy wyłącznie przesunięć źródła1 w builderach scrolla. Okres zawijania odpowiada trybowi: w trybie1 pion1024/poziom512, w trybie2 pion512/poziom1024. Poprzedni wariant używał512 dla obu osi.

Kompilacja: `build-isolated-horizon.log`; uruchomienie próby UHD: `runtime-isolated-horizon.log`. Główny `Windows/vf2.exe` nadal nie jest podmieniony; próba działa jako `vf2_probe.exe` z `VF2_ARENA_TRIAL=1`.

## Wycofanie nieudanych eksperymentów

Użytkownik potwierdził, że wariant osobnego targetu również nie naprawił tła w intrze. Poprawki horyzontu i smoothscrollingu pozostają NIEUKOŃCZONE. Główna wersja nie została zastąpiona eksperymentem. Zamknięto procesy pomiarowe i uruchomiono zachowany Windows/vf2.exe (SHA256 AE9507632352EDC46AC161DBAE9FD87D9A701F5280C1066DB55078B24CB6BA2D). CMake ponownie buduje tylko main.cpp i vf2_video.cpp. Pliki prób pozostają do analizy, ale nie wchodzą do kompilacji głównej. Pomijanie logotypów nie jest wdrożone ani zweryfikowane.
