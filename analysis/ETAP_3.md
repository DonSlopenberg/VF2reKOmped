# Etap 3 — pierwsza aplikacja Windows

14 października 2026. Zgoda użytkownika obejmowała instalację narzędzi i pierwszą
kompilację w limicie 60 minut. Użytkownik doprecyzował: cały interfejs po angielsku.

## Dostarczony wynik

W folderze `Windows` jest natywna aplikacja startowa `Virtua Fighter 2.exe`,
silnik `vf2.exe` i trzy biblioteki runtime. Launcher ma angielski interfejs,
wybór 720p, 1080p, 1440p i UHD 3840 × 2160, opcję Fullscreen i licznik
kontrolerów Xbox przez Windows XInput. W trybie okienkowym rozmiar okna jest
dopasowany do pulpitu; rozdzielczość zgłaszana grze jest przekazywana osobno.
Okno gry ma tytuł „Virtua Fighter 2”. Zasoby pozostają w sąsiednim `extracted`.

To wersja testowa, nie w pełni zweryfikowane wydanie. Oryginalny pakiet pozostaje
niezmieniony. Java ani BIOS nie były potrzebne do kompilacji i uruchomienia.

## Instalacja i kompilacja

- Visual Studio Build Tools 2022 17.14.42: narzędzia C++ x64/x86, CMake/Ninja
  i Windows SDK 22621. Instalacja zakończona, `vswhere` raportował kompletną
  instalację bez konieczności restartu. Log instalatora wskazał około 7,18 GB
  zmiany zajętości dysku dla operacji Build Tools.
- LLVM/Clang 20.1.8. SHA-256 pobranego instalatora:
  `3197846a2b19063687dd56e93e34cd941e3548d907f23a6131571321bdf9fe7b`, zgodny
  z metadanymi oficjalnego wydania. Instalator Microsoftu miał ważny podpis.
- ReXGlue 0.10.0, kod aplikacji C++23, konfiguracja RelWithDebInfo, Direct3D 12.
- Ostatnia pełna kompilacja i linkowanie zakończyły się kodem 0.
  Polecenia: `native/build.cmd`, osobno GUI: `native/build.cmd launcher`.

## Analiza funkcji

Po pierwszym uruchomieniu runtime wskazał dodatkowe niezarejestrowane cele
wywołań pośrednich. Zapisano je w `native/vf2_analysis.toml` z odsyłaczami
do odpowiednich logów. Dodano moduł Xenos do konfiguracji CMake i runtime.

Diagnostyczny zrzut `decoded-image.bin` wykonano przed uruchomieniem kodu gościa
przez opcjonalny hook `VF2_DUMP_IMAGE`. Nie zmienia on obrazu gry. W sekcjach
.rdata i początkowym .data znaleziono 199 dodatkowych kandydatów na wejścia.
Pierwsza szeroka lista ujawniła problem: 14 pozycji było importami systemowymi,
a 139 wskazywało wnętrza już przetłumaczonych funkcji. Usunięto je z listy
samodzielnych wejść na podstawie metadanych importów i porównania z generowaniem
bazowym. Zachowano 46 kandydatów w `vf2_pointer_entries.toml`.

Końcowy wynik: **13 249 definicji funkcji**, zero `REX_FATAL` i zero tekstu
`unresolved` we wszystkich `vf2_recomp.*.cpp`. To kontrola wygenerowanych źródeł
i walidacji narzędzia, nie dowód poprawności wszystkich ścieżek wykonania.
Wcześniejsze pośrednie pliki z błędami nie są wynikiem końcowym.

## Testy i zgłoszenia użytkownika

| Obszar | Dowód / stan |
| --- | --- |
| Uruchomienie natywnego EXE | Potwierdzone procesem i logiem |
| Grafika | Xenos uruchomiony na NVIDIA RTX PRO 6000 przez Direct3D 12 |
| Ekran tytułowy | Potwierdzony wizualnie przez computer-use |
| Dwa pady Xbox | Angielski launcher pokazał „2 connected” |
| Lokalny Versus | Użytkownik potwierdził poprawne działanie dwóch graczy w wcześniejszym teście |
| Lokalny zapis | Użytkownik zobaczył dialog tworzenia zapisu runtime |
| Arcade | Użytkownik zgłosił crash/restart; test poprawionej wersji bez podmian w toku |
| Osiągnięcie | Log `runtime-final.log` potwierdza odblokowanie ID 6 |
| Rozdzielczości | Wybór GUI wdrożony; końcowy launcher przekazuje jawnie video_mode_width/height |
| Natywne renderowanie sceny UHD | Nie zmierzone |
| Dźwięk, rumble, dłuższa stabilność | Brak pełnej weryfikacji |

Restart kilka sekund po osiągnięciu czasowo pokrył się z moją podmianą EXE
i zakończeniem procesów testowych. Nie ustalono, że osiągnięcie spowodowało
crash. Użytkownik został o tym poinformowany. Poproszono o nieprzerywany retest
Arcade na poprawionej kompilacji; od tego momentu nie zastępowano EXE gry.

W logach pozostają komunikaty o niezaimplementowanym
`NtQueryInformationFile(XFileXctdCompressionInformation)`. Dotychczas nie
uniemożliwiły startu ani potwierdzonego Versus. Nie uzyskano raportu Windows,
który jednoznacznie wyjaśniałby zgłoszony restart Arcade.

## Ustawienia i ograniczenia

ReXGlue domyślnie ma `license_mask=0`. Dla dostarczonej przez użytkownika kopii
launcher przekazuje `license_mask=1`; wcześniejsze uruchomienie z zerem wywołało
stub sklepu Xbox. Język jest jawnie ustawiony na angielski (`user_language=1`).

Kontrola SDK wykazała, że przy braku jawnych parametrów video_mode rozmiar
okna może mieć pierwszeństwo przed presetem `resolution`. Końcowy launcher
przekazuje jawnie oba parametry video_mode, żeby wybrana rozdzielczość była
niezależna od dopasowania okna do monitora. Ta konfiguracja wymaga jeszcze
weryfikacji rzeczywistego rozmiaru bufora renderowania sceny gry.

Bieżący proces diagnostyczny miał włączoną pomocniczą klawiaturę przez
`--mnk_mode`. Zwykły launcher wybiera XInput bez włączania tej opcji.

## Aktualizacja po nieprzerywanym teście użytkownika

Użytkownik ukończył dwie walki Arcade w poprawionej kompilacji. Zgłosił natomiast
blokadę po wybraniu Achievements z menu głównego. Log o 11:22:00 czasu lokalnego
pokazuje `__imp__XamShowAchievementsUI STUB`: backend SDK nie ma tej funkcji UI.
To osobny, potwierdzony brak implementacji. Stan odblokowań istnieje w
`Windows/userdata/achievements/5841129C.toml`; nowe logi potwierdzają ID 1 i 2,
wcześniejszy log potwierdza ID 6.

Nowy zakres użytkownika: zastąpić blokujące otwieranie Achievements listą
lokalnych odblokowań oraz dopasować skalowanie dialogów runtime i nawigację
padem. Ponieważ jest to większa praca UI, wymaga kolejnego uzgodnionego etapu
czasowego. Nie rozpoczęto tej implementacji bez zapytania o zakres czasowy.

## Ślady do kontynuacji

- `stage3_summary.json`: tożsamości plików pakietu i stan testów.
- `final_codegen_check.json`: kontrola końcowego generowania.
- `build-final-title.log`: udana końcowa kompilacja EXE gry.
- `runtime-final-filtered.log`: nieprzerywany bieżący test poprawionej wersji.
- `pointer_candidates.json`, `interior_pointer_exclusions.json`: pochodzenie
  i wykluczenia kandydatów.
- `baseline_generated`: kontrolna translacja bez szerokich kandydatów.
- `Windows/logs`, `Windows/userdata`: logi i dane użytkownika zwykłego launchera.

Źródła implementacyjne: [ReXGlue](https://github.com/rexglue/rexglue-sdk/tree/v0.10.0),
[XInput backend](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/input/input_system.cpp),
[video mode](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/kernel/xboxkrnl/xboxkrnl_video.cpp),
[licence-mask runtime](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/kernel/xam/xam_content.cpp).

REA nie obsługuje źródłowego STFS ani XEX w tej sesji. Wyniki analizy kodu
pochodzą z ReXGlue i skryptów lokalnych; obserwacje UI z computer-use oraz
bezpośrednich odpowiedzi użytkownika. Nie nadano im fikcyjnych Evidence IDs REA.
