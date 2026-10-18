# Virtua Fighter 2 — ocena pierwszego etapu

Data: 14 października 2026. Cel: aplikacja Windows 11 w C++, lokalna gra na dwóch padach, wybór rozdzielczości do UHD 3840 × 2160. Pierwszy etap miał limit 30 minut i nie obejmował instalacji narzędzi ani długiej kompilacji.

## Wynik

Pakiet został wydobyty i nadaje się do dalszej analizy. Nie powstała jeszcze rekompilacja, plik EXE dla Windows ani działająca gra. Wykonalność pełnego portu pozostaje niepotwierdzona. Najbardziej obiecujący kandydat do próby to ReXGlue, łączący generowanie C++ z warstwą funkcji Xboxa. Sam XenonRecomp nie dostarcza runtime.

## Dowody lokalne

Poniższe obserwacje pochodzą z własnych skryptów Python i bajtów dostarczonej kopii. Nie są dowodami REA. Narzędzie REA `open_binary` odrzuciło kontener LIVE/STFS i wydobyty XEX jako nieobsługiwane formaty; sesja analizy nie została otwarta.

- Źródło: `5841129C/000D0000/65D8FAC10B21FA185AAB93607663594CB568C78658`.
- Format LIVE/STFS, 66 928 640 bajtów, nazwa w metadanych: Virtua Fighter 2, Title ID `5841129C`.
- SHA-256 źródła: `2e4700ca9bc3ea747379a1d3648b59bcdff6c528d1aefe241a51b47757c74099`.
- Sprawdzono SHA-1 nadrzędnej tablicy, 96 tablic niższego poziomu i wszystkich 16 232 bloków danych. Wszystkie zgodne. To sprawdzenie spójności względem metadanych, nie uwierzytelnienie podpisu Microsoftu.
- Wydobyto 37 plików i 8 katalogów, łącznie 66 417 270 bajtów plików. Zweryfikowano łańcuchy, długości i liczbę bloków.
- `default.xex`: 2 277 376 bajtów; SHA-256 `00677891100a73636c9b1937c996c320b437eb61f11ce86848ddf6ee8a8e289f`.
- XEX2: oryginalna nazwa `vf2.exe`, adres ładowania `0x82000000`, punkt wejścia `0x821AA770`, deklarowany rozmiar obrazu 21 692 416 bajtów.
- Szyfrowanie XEX typu 1, kompresja typu 2 (normal/LZX), okno LZX 32 768 bajtów. Obraz nie został odszyfrowany ani zdekompresowany w tym etapie.
- Nagłówki importów: `xam.xex` — 166 referencji; `xboxkrnl.exe` — 332 referencje. Referencje nie oznaczają liczby unikalnych funkcji. Ordynały wymagają dostępu do rozpakowanego obrazu.
- Metadane bibliotek statycznych obejmują D3D9, D3DX9, XGRAPHC, XAUDIO2, XNET, XONLINE i biblioteki wykonawcze. Nie należy instalować oryginalnego Xbox XDK; potrzebujemy zgodnej implementacji użytych funkcji.
- Zasoby: archiwa FARC, dźwięk AWB/ACB/ADX, katalog `media/rom/vf2_rom` z plikami `rom_data.bin`, `rom_pol.bin`, `rom_tex.bin`, `ic12_13.bin` i `ic12_15.bin`.

Katalog ROM sugeruje dodatkową warstwę odtwarzania gry automatowej wewnątrz portu Xbox 360. To hipoteza na podstawie organizacji danych, nie potwierdzenie emulowanego procesora ani sposobu renderowania. Część zasobów ma niejawny format; same rozszerzenia nie wystarczą do ich interpretacji.

## Wymagania i granice ustaleń

| Obszar | Stan | Następna weryfikacja |
| --- | --- | --- |
| Wydobycie kopii | Wykonane | Zachować manifest i SHA-256 |
| Natywne C++ na Windows 11 | Kandydat techniczny, zgodność nieznana | ReXGlue: załadowanie XEX, analiza przepływu, generowanie C++ |
| Dwa pady, lokalne versus | Wymaganie zapisane, niewdrożone | Dwa niezależne sloty wejścia, dołączanie gracza 2, menu i walka; ustalić modele padów |
| Rozdzielczość do UHD | Wymaganie zapisane, niewdrożone | Rozdzielić rozmiar okna/wyjścia, renderowanie sceny i skalowanie UI |
| Dźwięk | Zasoby obecne, działanie nieznane | Obsługa middleware i rzeczywiste odtwarzanie |
| Tempo rozgrywki | Ryzyko z raportu zewnętrznego | Sprawdzić prawidłową szybkość przy zmianie rozdzielczości i synchronizacji |

Raport zgodności Xenia dla tego Title ID opisuje działającą rozgrywkę, obsługę 1080p i zależność szybkości gry od FPS. To obserwacje innego testera na określonej kompilacji Xenia, nie test naszej kopii ani potwierdzenie działania ReXGlue. Źródło: [raport gry](https://github.com/xenia-project/game-compatibility/issues/2202).

## Narzędzia

W PATH dostępne są Git i Python 3.14. Nie znaleziono clang, clang-cl, CMake, Ninja, rexglue ani XenonRecomp. Sprawdzone typowe ścieżki LLVM, CMake, instalatora Visual Studio i nagłówków Windows SDK też nie istnieją. Nie jest to pełna inwentaryzacja wszystkich dysków. Wynik PATH zapisano w `toolchain_probe.json`.

Do pierwszej próby proponuję pobranie gotowego wydania ReXGlue dla Windows x64 do lokalnego katalogu narzędzi, po zgodzie użytkownika. Strona wydania wskazuje obecnie [v0.10.0](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0). Należy sprawdzić faktyczne pliki wydania i działanie CLI przed przyjęciem, że pakiet pozwala na samodzielną analizę.

Do późniejszej kompilacji dokumentacja wskazuje Visual Studio 2022 z komponentami C++, Clang 20 lub nowszy, obsługę clang-cl/MSBuild, CMake 3.25+ i Ninja. Windows SDK oraz biblioteki C++ powinny pochodzić z instalatora Visual Studio. Źródło: [Getting Started](https://github.com/rexglue/rexglue-sdk/wiki/Getting-Started). Zakres i rozmiar instalacji trzeba sprawdzić przed jej rozpoczęciem. Na obecnym etapie nie stwierdzono potrzeby dostarczenia BIOS-u.

Żadnych narzędzi nie zainstalowano ani nie pobrano. Próba pobrania publicznych nagłówków źródłowych przez Python zakończyła się brakiem dostępu do sieci w środowisku poleceń; dokumentację odczytano przez narzędzie WWW.

## Proponowany drugi etap — wymaga zgody

Limit 30 minut na pobranie i sprawdzenie gotowego ReXGlue, załadowanie XEX oraz próbę wygenerowania C++. Zapisać logi, liczbę odkrytych funkcji, nierozwiązane wywołania i brakujące importy. Jeśli CLI wymaga kompilacji SDK, dodatkowej instalacji lub zadanie przekracza limit, zatrzymać się i przedstawić konkretny zakres. Ten etap nie gwarantuje uruchomienia gry. Pełna adaptacja grafiki, dźwięku, dwóch padów i UHD będzie osobnym etapem po ocenie wyników.

## Odtwarzalność

`python tools/inspect_package.py` odtwarza wydobycie oraz `analysis/package_inventory.json`. Skrypt odmawia nadpisania pliku o innej zawartości i odrzuca ścieżki wychodzące poza katalog wynikowy.

`python tools/inspect_xex.py` odtwarza `analysis/xex_headers.json`. Oba skrypty używają wyłącznie standardowej biblioteki Pythona. Program kończy się błędem przy niezgodnych granicach lub nieobsługiwanym układzie. Obsługa STFS jest celowo ograniczona do układu tego pakietu.

Formaty odczytano na podstawie [stfs_xbox.h](https://github.com/xenia-project/xenia/blob/master/src/xenia/vfs/devices/stfs_xbox.h), [stfs_container_device.cc](https://github.com/xenia-project/xenia/blob/master/src/xenia/vfs/devices/stfs_container_device.cc) i [xex2_info.h](https://github.com/xenia-project/xenia/blob/master/src/xenia/kernel/util/xex2_info.h). Informacje o rekompilacji: [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), [ReXGlue](https://github.com/rexglue/rexglue-sdk) i [CLI](https://github.com/rexglue/rexglue-sdk/wiki/rexglue-CLI-Commands).
