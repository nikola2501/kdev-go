# KDevelop + kdev-go na macOS-u (Apple Silicon)

Uputstvo za kompletan setup na M-seriji (testirano planiranje za M4, arm64):
build KDevelopa 6, ovog Go plugina (`qt6-port` grana) i sve što je potrebno
da IDE radi sa Go kodom. Stanje: avgust 2026.

## Zašto build iz sorsa

- Zvanični macOS binariji KDevelopa **ne postoje** — mac port je bez
  održavaoca. CI šabloni za `craft-macos-arm64-qt6` postoje u kdevelop repou,
  ali su zakomentarisani u `.gitlab-ci.yml`.
- **MacPorts otpada**: nudi kdevelop 4.7.4 (iz 2015!) i nema nijedan `kf6-*`
  port. **Homebrew otpada**: ne pakuje KDE Frameworks uopšte.
- Jedini realan put je **KDE Craft** — alat kojim KDE pravi zvanične Windows
  buildove KDevelopa sa istim Qt6 blueprintom. On builduje ceo lanac:
  Qt6 → KF6 → libclang → KDevelop.

## Šta ti treba

| | |
|---|---|
| Hardver | Apple Silicon (M1–M4), ~40 GB slobodnog prostora |
| OS | macOS 13+ |
| Alati | Xcode Command Line Tools, Python ≥ 3.9, Go (za sam rad, i da `go env` radi) |
| Vreme | sa Craft binarnim kešom: sati (download + build KDevelopa); bez keša: znatno duže |

```sh
xcode-select --install
python3 --version
brew install go        # ili sa go.dev; treba i pluginu (go env GOROOT/GOMODCACHE)
```

> **Važno:** builduj iz čistog shella. Ako je Homebrew/MacPorts Qt u `PATH`-u,
> CMake ume da pokupi pogrešan Qt.

## 1. Craft bootstrap

```sh
curl -O https://raw.githubusercontent.com/KDE/craft/master/setup/CraftBootstrap.py
python3 CraftBootstrap.py --prefix ~/CraftRoot
```

Bootstrap je interaktivan — biraj **Qt6** i native (arm64) kompajler.
Posle toga, u svakom novom terminalu:

```sh
source ~/CraftRoot/craft/craftenv.sh    # postavlja PATH, $KDEROOT itd.
```

## 2. KDevelop

```sh
craft kdevelop
```

**Ne paniči zbog obima:** Craft ima binarni keš (podrazumevano uključen,
`UseCache=True` u `CraftSettings.ini`) — Qt6, KF6 frameworke, LLVM i
QtWebEngine za macOS arm64 po pravilu **skida gotove** sa KDE servera,
ne kompajlira ih. Iz sorsa se builduje samo ono čega u kešu nema —
tipično sam KDevelop i pokoja sitnica. Realno: nekoliko GB downloada +
20-60 min kompajliranja; višesatni build je scenario praznog keša.
Pokretanje:

```sh
craft --run kdevelop
# ili: open ~/CraftRoot/Applications/KDE/kdevelop.app
```

Ako neki blueprint u lancu pukne (mac CI ne radi, pa se dešava —
istorijski: dbus, okteta, poppler):

- opcione zavisnosti preskoči: `craft --set ignored=1 <paket>`
- opcione delove KDevelopa isključi CMake flagom, npr.
  `-DCMAKE_DISABLE_FIND_PACKAGE_SubversionLibrary=ON`

Za deljivi `.dmg` (opciono): `craft --package kdevelop` → `~/CraftRoot/tmp/`.

## 3. kdev-go

Craft je već buildovao `kdevelop-pg-qt` (KF6 verzija; CMake paket se zove
`KDevelopPGQt`). U craft shellu:

```sh
git clone -b qt6-port https://github.com/nikola2501/kdev-go.git
cd kdev-go
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_PREFIX_PATH=$KDEROOT -DCMAKE_INSTALL_PREFIX=$KDEROOT
cmake --build build -j$(sysctl -n hw.ncpu)
cmake --install build
```

Plugin se instalira u `$KDEROOT/lib/plugins/kdevplatform/<N>/` — isti prefiks
iz kog Craft pokreće KDevelop, pa nikakve env promenljive nisu potrebne.
Verzija plugin API-ja (`<N>`) se automatski čita iz `KDevPlatformConfig.cmake`
i mora se poklapati sa buildovanim KDevelopom (poklapa se sama, jer buildaš
protiv istog prefiksa).

### builtins.go — macOS caka broj 1

Na macOS-u Qt **ne čita** `XDG_DATA_DIRS`; `QStandardPaths` gleda u
`~/Library/Application Support`. Instalacija stavlja `builtins.go` u
`$KDEROOT/share/kdev-go/`, što plugin pri pokretanju kroz `craft --run`
najverovatnije vidi — ali ako builtin tipovi (`len`, `make`, `append`,
`error`…) ostanu nerazrešeni, kopiraj ručno:

```sh
mkdir -p ~/Library/"Application Support"/kdev-go
cp builtins.go ~/Library/"Application Support"/kdev-go/
```

Plugin ima samoizlečenje keša: fajlovi parsirani dok builtins nije bio
dostupan automatski se reparsiraju kad postane.

### Testovi (opciono)

Craft-ov KDevelop builduje i `KDevPlatformTests` biblioteku, pa je dovoljno:

```sh
cmake -B build -DBUILD_TESTING=ON [...isti flagovi...]
cmake --build build && cd build && ctest --output-on-failure
```

Ako `KDev::Tests` target ne postoji (distro paketi ga često ne nose),
pogledaj `KDEVPLATFORM_TESTS_PREFIX` opciju u korenskom `CMakeLists.txt`.

## 4. Smoke test

1. Otvori `.go` fajl → semantičko bojenje odmah.
2. Kursor na simbol, drži **⌥ Option** (= Alt) → popup deklaracije.
   Napomena: dok je *Code Browser* tool view otvoren i otključan, KDevelop
   navigaciju prikazuje u njemu umesto u plutajućem popup-u — zatvori panel
   ili klikni katanac.
3. Completion posle tačke na struct promenljivoj.
4. Projekat sa `go.mod` + `vendor/`: simboli iz zavisnosti treba da imaju
   popup i completion (module podrška: go.mod, vendor/, module cache).
5. Prvo indeksiranje ume da traje par minuta (stdlib kaskada kroz importe) —
   jednokratno, keš preživljava restart.

## Šta na macOS-u ne radi (i šta umesto toga)

| Oblast | Stanje | Zamena |
|---|---|---|
| Konsole tool view | Ne postoji (nema KonsolePart) | Terminal.app / iTerm |
| gdb debugger | Mrtav na Apple Siliconu | KDevelop **lldb** plugin |
| Plasma/KRunner | CMake ih sam preskače | — |
| Gatekeeper | Blokira nepotpisan `.app` iz `.dmg` | `xattr -dr com.apple.quarantine kdevelop.app` |
| Sitni UI kvarovi | Port bez održavaoca | Zapisuj; kandidati za upstream |

## Poznata ograničenja plugina (sve platforme)

- Gramatika ne zna Go 1.18+ sintaksu (generici pre svega) — takvi fajlovi se
  preskaču pri parsiranju (~9% tipičnog vendor stabla).
- `gometalinter` plugin cilja odavno mrtav alat (zameniti golangci-lint-om).
- `gobuildsystem` je iz GOPATH ere.
