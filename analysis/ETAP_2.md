# Etap 2 — generowanie kodu C++

14 października 2026. Użytkownik zatwierdził pobranie ReXGlue i próbę generowania
w limicie 30 minut oraz wskazał dwa pady Xbox z natywną obsługą Windows.

## Wynik

ReXGlue v0.10.0 dla Windows x64 pobrano do `tools/rexglue-sdk-0.10.0`.
SHA-256 ZIP zgodny z metadanymi wydania GitHub:
`ad4485da853fbe932e9f5dbb643f9b5fe85dc73775ebbad4df6879fe6df0c11b`.
Nie wykonano instalacji systemowej.

CLI poprawnie odszyfrował i zdekompresował XEX na potrzeby analizy oraz
utworzył projekt `native` i źródła. To obserwacja z pomyślnego odczytu
i translacji instrukcji, nie test wykonania gry.

Pierwsza analiza zgłosiła sześć nierozwiązanych skoków. Jednorazowe
diagnostyczne `--force` pozwoliło obejrzeć źródła, ale wstawiło sześć
`REX_FATAL`; taki wynik nie był uznany za poprawny port.

Dodano sześć adresów wejścia w `native/vf2_analysis.toml`, z automatycznym
wykrywaniem granic (size=0). Nie zmieniono bajtów XEX ani nie pominięto instrukcji.
Po tym generowanie bez `--force` zakończyło się kodem 0 i przeszło walidację.

Wynik: **13 198 definicji funkcji**, 61 plików translacji C++, 63 813 119 bajtów
tych plików. Bounded search we wszystkich `vf2_recomp.*.cpp` nie znalazł
`REX_FATAL` ani tekstu `unresolved`. To nie jest dowód poprawności wszystkich
instrukcji, kompletności wywołań pośrednich ani działania runtime.

Sprawdzone nowo wykryte wejścia:

| Adres | Obserwacja z wygenerowanych instrukcji |
| --- | --- |
| 0x82130C30 | 60 instrukcji, zakończenia blr |
| 0x82444F38 | 6 instrukcji, pośredni skok bctr |
| 0x82444F50 | 6 instrukcji, pośredni skok bctr |
| 0x82444F68 | 6 instrukcji, pośredni skok bctr |
| 0x82444F80 | 6 instrukcji, pośredni skok bctr |
| 0x82444F98 | 6 instrukcji, pośredni skok bctr |

Ostatnie pięć to krótkie wejścia wywołujące funkcje przez tablicę. Nie
stwierdzono ich semantycznych nazw. Podsumowanie i listy instrukcji:
`codegen_summary.json`. Log pomyślnej analizy:
`rexglue-codegen-seeded-console.log`. Nazwy importów z wygenerowanego kodu
obejmują `XamInputGetCapabilities`, `XamInputGetState`, `XamInputSetState`.
Znaleziono 237 różnych nazw __imp__; zgodność ich implementacji w runtime
pozostaje do sprawdzenia podczas linkowania i uruchamiania.

## Pady i UHD

Lokalna pomoc CLI potwierdza backend `xinput` oraz presety rozdzielczości
720p/1080p/1440p/4k. Kod ReXGlue v0.10.0 tworzy sterownik XInput dla tego
ustawienia i domyślnie stosuje `SlotAssignment`, przypisujące osobne urządzenia
do osobnych użytkowników gry. Źródło: [input_system.cpp](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/input/input_system.cpp).

Dodano domyślne `xinput` w hooku `OnPreSetup` aplikacji, z zachowaniem
ustawienia użytkownika z konfiguracji lub CLI. Ten niewielki kod nie został
jeszcze skompilowany. Pady nie były sprawdzane sprzętowo. Nie powstał jeszcze
interfejs wyboru rozdzielczości ani potwierdzenie renderowania UHD.

## Dalsze wymagania

Brak kompilatora i Windows SDK blokuje pierwszą kompilację. Przygotowano
`native/build-tools.vsconfig` dla komponentów C++ x64/x86, CMake/Ninja i
Windows SDK 22621. Plan: Visual Studio Build Tools 2022 plus osobny LLVM/Clang
20 lub nowszy. Microsoft podaje Clang 19.1.5 w komponentach VS 2022; stąd
sam wybór Clanga w tym instalatorze może nie spełnić wymagań SDK.
Źródła: [Microsoft](https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools?view=vs-2022),
[ReXGlue](https://github.com/rexglue/rexglue-sdk/wiki/Getting-Started).

Instalacja tych narzędzi i pierwsza kompilacja wymagają osobnej zgody.
Rzeczywisty rozmiar instalacji należy odczytać z instalatora. Proponowany
limit kolejnego etapu: 60 minut, z przerwą na ocenę wyników, jeśli pojawią
się większe problemy. Nie ma gwarancji uruchomienia rozgrywki w tym czasie.
Do odczytu i generowania kodu nie był potrzebny BIOS.

Oryginalny pakiet i wydobyty XEX zostały ponownie sprawdzone SHA-256 po analizie.
Nie zmieniono ich zawartości. REA nie prowadził sesji natywnej; logi tego
etapu pochodzą z ReXGlue i lokalnych skryptów.
