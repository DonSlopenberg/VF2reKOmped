# Virtua Fighter 2 — etap 16:9 i rozdzielczość renderowania

Kopia stanu przed zmianami: `backups/before-16x9-20261014-115451`.

## Potwierdzone w testach

- Oryginalny viewport walki: x=263, y=0, 1395×1080 w obrazie 1920×1080. Proporcje odpowiadają logicznemu ekranowi Model 2 496×384.
- Poszerzenie samego viewportu rozciągało modele. Zmiana macierzy ortograficznej dodatkowo ściskała warstwy 2D; tę zmianę cofnięto.
- Builder perspektywy `sub_821538A8` tworzy macierz 3D i transponuje ją przed zwrotem. Dla kamery perspektywicznej 496×384 pierwszy wiersz macierzy jest mnożony przez 0.7265625. Docelowy viewport walki ma 16:9, a pionowa skala i współrzędne HUD pozostają oryginalne.
- Użytkownik potwierdził poprawne proporcje postaci 3D oraz HUD po tej korekcie. Tło nadal kończyło się na dawnym obszarze obrazu.

## Aktualnie przygotowana implementacja

- Tylna bitmapa (wywołanie `sub_82144340` z adresu powrotu 0x82144590) jest powtarzana poziomo w swoim widocznym okresie. Warstwa HUD, wywoływana z 0x821445F8, nie jest powtarzana. Powtarzanie jest ograniczone do bitmap zajmujących pełną wysokość ekranu, aby nie powtarzać ekranu tytułowego.
- Przesunięcia bitmapy tylnej w `sub_821456B0` otrzymują filtr wygładzający o stałej 18 ms z obsługą zawijania 512 pikseli. Smoothing nie zmienia wejścia padów ani logiki rozgrywki. Cvar `vf2_smooth_background` można wyłączyć w F6. Efekt wymaga potwierdzenia wizualnego.
- F4 dopasowuje obszar klienta okna do dokładnego 16:9 i mieści zewnętrzną ramkę w 75% obszaru roboczego monitora.
- Gra rozdziela fizyczny ekran i logiczny, autorski canvas. Hook `sub_821067A0` aktualizuje tylko fizyczny rekord pod 0x824A0498; logiczny rekord pod offsetem +32 zachowuje swoje oryginalne wymiary.

## UHD — stan weryfikacji

Bezpośrednie wymuszenie bufora konsoli 3840×2160 dało uszkodzony obraz. Log nadal wskazywał viewport 1920×1080, a zrzut pokazał powielone i przemieszczone fragmenty. Ten wariant nie jest poprawnym UHD.

Kolejny wariant wykorzystuje skalowanie rysowania GPU, przed utworzeniem jego cache render-targetów:

| Wybór | Bufor gościa | Skala rysowania GPU | Docelowe renderowanie |
|---|---|---|---|
| 1280×720 | 1280×720 | 1×1 | 1280×720 |
| 1920×1080 | 1920×1080 | 1×1 | 1920×1080 |
| 2560×1440 | 1280×720 | 2×2 | 2560×1440 |
| 3840×2160 | 1920×1080 | 2×2 | 3840×2160 |

Moduł GPU jest ładowany przed ustawieniem `resolution_scale` i `draw_resolution_scale_x/y`, aby te ustawienia były już zarejestrowane. Loguje się wybrany tryb, bufor gościa, wartości skalowania GPU i rzeczywiste parametry viewportu walki. Zmiana dotyczy rasteryzacji przez GPU, a nie wyłącznie powiększenia gotowego obrazu przez okno.

**Bieżący status (14 października, 16:37):** wariant GPU 2× jest zainstalowany i uruchomiony na rzeczywistym pulpicie w fullscreen. Ekran tytułowy renderuje się poprawnie, bez uszkodzeń bezpośredniego bufora 4K. Log `runtime-video-uhd-scaled.log` potwierdza wybrany tryb 3840×2160, bufor gościa 1920×1080 oraz zarejestrowane ustawienia GPU 2×2. Sama walka, kafelkowanie i wygładzanie tła nie są jeszcze potwierdzone w tym wariancie. Nie należy oznaczać całego testu UHD jako zakończonego.

Tymczasowo włączony tryb klawiatury `mnk_mode` został wyłączony w ustawieniach bez zapisu konfiguracji. Próba klawiaturą nie otworzyła menu. Pozostaje wejść do Arcade padem i porównać viewport walki z wybraną skalą GPU.

F4 przetestowane w tej samej uruchomionej wersji UHD: fullscreen → okno z klientem 2640×1485 (dokładne 16:9) → fullscreen. Log potwierdza zewnętrzną ramkę 2666×1556 mieszczącą się w 75% monitora i oba przełączenia. Zainstalowany `Windows/vf2.exe` odpowiada artefaktowi kompilacji; SHA256: `6C32634E516965DFB50478C3654EFF7374E120371C137D2F4C89C0C92A7371EA`.

Logi: `runtime-video-probe.log`, `runtime-video-wide-trial.log`, `runtime-video-perspective.log`, `runtime-video-uhd-tiled.log`. Kompilacja następnego wariantu: `build-video-uhd-smooth.log`.

## Korekta przypisania warstw — 14 października, 17:04

Użytkownik potwierdził stabilność uruchomionego wariantu UHD, ale zauważył powielanie wyboru postaci i HUD walki. Zrzut runtime również pokazał dodatkowe paski życia i nazwy przy bokach. Wcześniejsze przypisanie caller 0x82144590 do samego tła było błędnym wnioskiem.

W nowej kompilacji caller 0x82144590 jest całkowicie wyłączony z dodatkowych kopii. Próba lustrzanego poszerzenia dotyczy wyłącznie caller 0x821445F8, przy tym samym warunku pełnej wysokości. Nie ma jeszcze potwierdzenia wizualnego zawartości tej drugiej warstwy. Zmiana nie modyfikuje ustawień UHD ani macierzy perspektywy.

Kopia wersji stabilnej UHD i źródła przed tą korektą: `backups/before-background-plane-fix-20261014-1703`. Nowy artefakt zbudowano pomyślnie (`build-background-plane-fix.log`). Podmiana pliku w Windows czeka na zamknięcie uruchomionej gry przez użytkownika.

## Podmiana po potwierdzonym zamknięciu gry — 17:13

Użytkownik zamknął grę. Nowy plik `Windows/vf2.exe` został podmieniony i jego SHA256 zgadza się z artefaktem kompilacji: `7F040DFEE4B76516C97FBF5E6CF0F176AA6C876A3899D2F256E1E9788E17BC96`.

Wersja z korektą warstw uruchomiona w 3840×2160, fullscreen, XInput. Log `runtime-background-plane-fix.log` potwierdza bufor 1920×1080 i GPU draw scale 2×2. Obserwacja runtime pokazała poprawne główne menu. Weryfikacja wyboru postaci, HUD i lustrzanego tła aren w tej wersji jest nadal w toku.

## Nieudana próba i wycofanie — 17:16

Świeży zrzut walki oraz odpowiedź użytkownika potwierdziły, że caller 0x821445F8 także zawiera tekst i HUD: zamiana caller nie oddzieliła tła. Błędne wcześniejsze twierdzenia o jednoznacznym przypisaniu warstw należy odrzucić. Powielanie finalnych bitmap nie jest właściwą metodą izolowania samego tła aren.

Ze źródła usunięto całe hooki `sub_82144340` (dodatkowe kopie) i `sub_821456B0` (eksperymentalny filtr). Zachowano działające ustawienia UHD, fizyczny rekord wyjścia, korektę perspektywy i poszerzenie viewportu. Kompilacja wycofania przeszła pomyślnie, `build-background-rollback.log`. Czeka na zamknięcie gry i instalację; tło po wycofaniu wróci do ograniczonej szerokości oryginalnej bitmapy.

## Wycofanie zainstalowane — 17:22

Po potwierdzeniu zamknięcia gry zainstalowano wersję bez hooków dodatkowych kopii bitmap i filtrowania scrolla. SHA256 `Windows/vf2.exe` i artefaktu kompilacji: `AE9507632352EDC46AC161DBAE9FD87D9A701F5280C1066DB55078B24CB6BA2D`. Uruchomiono ją na rzeczywistym pulpicie w fullscreen, UHD, XInput. Log `runtime-background-rollback.log` potwierdza wybór 3840×2160, bufor gościa 1920×1080, GPU 2×2. Poszerzenie samego bitmapowego tła aren pozostaje nierozwiązane.

Użytkownik upoważnił agenta do samodzielnego zamykania i testowania gry. Dostępne narzędzie UI wysyła klawiaturę i mysz, nie przyciski fizycznego pada; ta zgoda nie stanowi dowodu przeprowadzenia testu padem.

Kontrola runtime po aktywowaniu okna pokazała intro Shuna z pojedynczymi napisami PUSH START BUTTON / FREE PLAY / ©SEGA 1994, bez dodatkowych kopii bitmap. Widoczne są dawne granice bitmapowego tła, zgodnie z wycofaniem jego poszerzenia. Weryfikacja intro nie stanowi jeszcze testu wyboru postaci ani lokalnej walki w tej konkretnej wersji.
