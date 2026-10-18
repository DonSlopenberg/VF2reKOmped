# Krótka diagnostyka obrazu 16:9 — 2026-10-14

Zakres zatwierdzony przez użytkownika: kopia obecnego stanu i krótka diagnostyka, bez wdrażania zmian.

## Kopia przed zmianami

`backups/before-16x9-20261014-115451`

165 plików, 103 363 459 bajtów. Zawiera projekt `native` wraz z wygenerowanym kodem, pliki aplikacji i bibliotek z `Windows`, lokalne zapisy, osiągnięcia, cache i logi. Pominięto katalog pośrednich wyników kompilacji `native/out`; gotowe pliki wykonywalne są zachowane w kopii `Windows`. Oryginalne zasoby gry i zainstalowane narzędzia pozostają w istniejących katalogach.

Sumy wszystkich skopiowanych plików: `manifest.sha256.json` w katalogu kopii. SHA-256 obu plików wykonywalnych i trzech plików źródłowych aplikacji sprawdzono względem oryginałów po utworzeniu kopii i ponownie po diagnostyce; pozostały zgodne.

## Ustalenia

- Launcher przekazuje oddzielne `video_mode_width` i `video_mode_height`; wszystkie cztery dostępne rozdzielczości mają proporcje 16:9. Log ostatniego testu `runtime-stage4c.log` potwierdza tryb gry 1920×1080. Sam wybór rozdzielczości 4:3 w launcherze nie wyjaśnia zgłoszonego widoku.
- Wygenerowana funkcja `sub_82100D28` (`native/generated/default/vf2_recomp.43.cpp`) wywołuje `XGetVideoMode` i inicjalizuje ekran 1920×1080 lub 1280×720. To analiza kodu; nie jest to pomiar końcowego viewportu podczas walki.
- Runtime ReXGlue 0.10.0 domyślnie włącza `present_letterbox`. Presenter zachowuje deklarowane proporcje obrazu gościa. Wyłączenie tej opcji może rozciągnąć cały obraz, ale nie dostosowuje projekcji ani pozycji elementów HUD. Rozciągnięcie obrazu 4:3 do 16:9 zwiększyłoby szerokość obiektów względem wysokości o 33⅓%.
- Obecne F4 ustawia niezależnie 75% szerokości i wysokości obszaru roboczego monitora, łącznie z ramką okna. Nie gwarantuje dokładnych proporcji 16:9 obszaru klienta, zwłaszcza po odjęciu paska zadań i dekoracji okna.
- Sprite'y i animacje interfejsu mają własne archiwa `media/rom/sprite` i `media/rom/auth2d`. Nie ustalono jeszcze ich transformacji ani zakotwiczenia HUD.

## Niewyjaśnione w krótkim zakresie

Nie ustalono jeszcze, czy widok walki 4:3 jest zapisany wewnątrz renderowanego obrazu gry, czy powstaje podczas prezentacji tego obrazu. Nie znaleziono i nie przetestowano poprawki projekcji 3D ani układu HUD. Nie przeprowadzono nowego testu rozgrywki.

## Zakres przyszłej poprawki

1. Dopasować obszar klienta okna do dokładnego 16:9 w granicach 75% monitora.
2. Zmierzyć viewport i deklarowane proporcje podczas walki, aby rozdzielić projekcję gry od skalowania runtime.
3. Dostosować projekcję i układ HUD/sprite'ów stosownie do ustalonej przyczyny; sprawdzić proporcje postaci, tekstu, ikon i kadru w Arcade i Versus.

W tym etapie nie zmieniono źródeł, plików wykonywalnych ani ustawień gry.

Źródła upstream: [Presenter ReXGlue 0.10.0](https://raw.githubusercontent.com/rexglue/rexglue-sdk/v0.10.0/src/ui/presenter.cpp), [tryby wideo runtime](https://raw.githubusercontent.com/rexglue/rexglue-sdk/v0.10.0/src/kernel/xboxkrnl/xboxkrnl_video.cpp).
