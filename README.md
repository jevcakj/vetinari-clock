# Hodiny lorda Vetinariho

Hodiny inspirované hodinami z předpokoje kanceláře lorda Vetinariho z knižní série Zeměplocha autora Terryho Pratchetta.
V knize Zaslaná pošta jsou popsány následovně: "Hodiny v předpokoji kanceláře lorda Vetinariho netikaly správně. Někdy tikaly o chloupek pomaleji, jindy zase o
trošičku rychleji. Občas ani tak, ani tak. Mohli jste si toho všimnout jedině tehdy, když jste tam seděli alespoň pět
minut, protože za tu dobu se některé malé, ale velmi důležité části vašeho mozku zbláznily."

## Popis
Principem je vytvořit hodiny, které defaultně budou vteřinově tikat nepravidelně v náhodných intervalech, ale minutově budou přesné. Bude možné nastavit jeden nebo více časových úseků, ve kterých budou hodiny rychlejší/pomalejší. V tomto případě nemusí být hodiny přesné minutově ani hodinově(denně ano), ale vždy bude dána maximální odchylka, o kterou se budou od reálného času lišit, tzn. pokud bude odchylka 5 minut a na hodinách je 12:12, pak reálný čas je mezi 12:07 a 12:17.  
Hodiny se budou zobrazovat na webové stránce sdílené web serverem na mikrokontroleru ESP32. Na stránce bude také možné přenastavit čas, maximální odchylku od rálného času nebo nastavit několik časových úseků, ve kterých chceme, aby šly hodiny rychleji/pomaleji.
Pro synchronizaci reálného času bude použit ntp.

## Knihovny
- AsyncTCP
- ESPAsyncWebServer
- ArduinoJson

## Režimy
1. **Normální režim**: Hodiny tikají nepravidelně, ale dokončí přesně 60 tiků za minutu.
2. **Režim časových úseků**: Během uživatelem definovaných časových období tikají rychleji nebo pomaleji než reálný čas, výsledný časový rozdíl  je následně automaticky korigován

## Funkce uživatelského rozhraní

### Hlavní stránka (/)
- Zobrazení hodin v reálném čase
- Automatické znovupřipojení při odpojení WebSocket
- Sledování času na straně klienta (snižuje zátěž serveru)

### Stránka nastavení (/nastaveni.html)
- Dynamická správa časových úseků
- Přidání/odebrání úseků přes JavaScript
- Validace formuláře:
  - Čas začátku musí být před časem konce
  - Musí být vybrán směr rychlosti
- Tlačítko reset pro znovunačtení aktuální konfigurace
- Načte se s aktuální konfigurací

# Technické poznámky

## Webové rozhraní

#### `GET /`
Hlavní stránka zobrazující aktuální čas. Čas se aktualizuje přes WebSocket připojení každou sekundu.

#### `GET /nastaveni.html`
Stránka nastavení, kde uživatelé mohou:
- Nastavit počáteční čas hodin
- Přidat více časových úseků s:
  - Čas začátku (HH:MM)
  - Čas konce (HH:MM)
  - Maximální odchylka (minuty a sekundy)
  - Směr rychlosti (rychleji nebo pomaleji)

#### `POST /data`
Přijímá aktualizace konfigurace jako JSON:
```json
{
  "time": "2025-09-30T14:30",
  "ranges": [
    {
      "stime": "09:00",
      "etime": "11:00",
      "min": "5",
      "sec": "0",
      "speed": "1"
    }
  ]
}
```

#### `GET /config`
Vrací aktuální konfiguraci jako JSON pro vyplnění formuláře nastavení.

### WebSocket komunikace

- **Cesta**: `/ws`
- **Účel**: Aktualizace hodin v reálném čase
- **Protokol**: Server vysílá prázdnou zprávu při každém tiku; klient zvyšuje lokální čas

## Server

### Konfigurace

#### Nastavení WiFi
```cpp
const char *ssid = "wifi";
const char *password = "password";
```

#### Synchronizace času
- **NTP servery**: `0.cz.pool.ntp.org`, `0.europe.pool.ntp.org`, `time.google.com`
- **GMTOffset**: 3600 sekund (UTC+1)
- **Letní čas**: 3600 sekund

### Datové struktury

#### `time_range_t`
Definuje časový úsek s modifikací rychlosti:
- `stime`: Čas začátku v sekundách od půlnoci
- `etime`: Čas konce v sekundách od půlnoci
- `max_diff`: Maximální povolený časový rozdíl v sekundách
- `stimeS`: Čas začátku jako řetězec (formát HH:MM)
- `etimeS`: Čas konce jako řetězec (formát HH:MM)
- `min`: Maximální odchylka v minutách
- `sec`: Maximální odchylka v sekundách
- `speed`: Boolean příznak (true = rychleji, false = pomaleji)

#### `config_t`
Hlavní konfigurační struktura:
- `config_time`: Časový offset od reálného času
- `range_count`: Počet nakonfigurovaných časových úseků
- `ranges[10]`: Pole uživatelem definovaných časových úseků

### Globální stavové proměnné

- `config`: Aktuální konfigurace
- `compl_ranges[10]`: Automaticky generované korekční úseky
- `real_time`: Aktuální reálný čas
- `virt_time`: Aktuální virtuální (zobrazovaný) čas
- `range_end_millis`: Časová značka v milisekundách, kdy končí aktuální úsek/minuta
- `current_speed`: Aktuální rychlostní režim (rychleji/pomaleji)

### Základní algoritmy

#### Algoritmus normálního režimu (`handle_classic`)

Když není aktivní žádný časový úsek, hodiny tikají nepravidelně, ale udržují přesnost po minutách:

1. Vypočítá zbývající milisekundy do konce aktuální minuty
2. Vypočítá zbývající potřebné tiky (60 - aktuální_sekunda)
3. Spočítá průměrné milisekundy na tik
4. Přidá náhodnou odchylku (±500ms nebo ±100ms v závislosti na zbývajícím čase)
5. Zajistí, že 60. tik skončí přesně na hranici minuty

**Příklad**: Pokud zbývá 30 sekund a je potřeba 30 tiků, průměr = 1000ms/tik, ale skutečná zpoždění se náhodně liší okolo této hodnoty.

#### Algoritmus režimu úseku (`handle_range`)

Během uživatelem definovaných časových úseků:

1. **Řízení rychlosti**: 
   - Rychlejší režim: Náhodný interval tiku 500-1100ms
   - Pomalejší režim: Náhodný interval tiku 900-1500ms

2. **Monitorování odchylky**:
   - Sleduje rozdíl mezi virtuálním a reálným časem
   - Pokud odchylka přesáhne max_diff, vynutí obrácení rychlosti
   - V rámci 90% max_diff se vrací k uživatelem zadané rychlosti

3. **Generování automatického korekčního úseku**:
   - Po každém uživatelském úseku vytvoří kompenzační úsek
   - Opačná rychlost než uživatelský úsek
   - Délka vypočítána tak, aby eliminovala akumulovaný rozdíl v rozumném časovém úseku

**Příklad**: 
- Uživatelský úsek: 09:00-11:00, odchylka max o 5 minut
- Pokud hodiny jdou o 5 minut napřed do 11:00
- Kompenzační úsek: 11:00-11:10, pomaleji, pro synchronizaci zpět

### Inicializační sekvence

1. Inicializace sériové komunikace (115200 baud)
2. Připojení k WiFi
3. Synchronizace s NTP servery
4. Inicializace WebSocket handleru
5. Konfigurace HTTP routes
6. Start webového serveru
7. Inicializace časových proměnných

### Hlavní smyčka

Funkce `loop()` se provádí nepřetržitě:

1. Zaznamená čas začátku
2. Vysílá WebSocket notifikaci tiku
3. Zvýší real_time a virt_time
4. Zkontroluje aktivní časový úsek
5. Vypočítá vhodné zpoždění tiku:
   - Použije `handle_range()` pokud je v nakonfigurovaném úseku
   - Jinak použije `handle_classic()`
6. Čeká do dalšího tiku s ohledem na čas zpracování

## Poznámky k implementaci

### Implementace nepravidelného tikání

Nepravidelnost je dosažena pomocí randomizace při zachování matematické přesnosti:
- Každá délka tiku se náhodně liší
- Celková doba na minutu je přísně kontrolována
- Vytváří psychologický efekt časové nejistoty

### Generování korekčního úseku

Automaticky generováno ve funkci `handle_data()`:
```cpp
compl_ranges[i].stime = c_range->etime;
ulong compl_range_len = c_range->speed == 1 ? c_range->max_diff * 2 : c_range->max_diff / 2 ;
compl_ranges[i].etime = c_range->etime + compl_range_len;
compl_ranges[i].speed = c_range->speed == 0;
```

To zajišťuje, že akumulované časové rozdíly jsou eliminovány, čímž se udržuje dlouhodobá přesnost.

### Reprezentace času

- **Reálný čas**: Synchronizován s NTP, reprezentuje skutečný nástěnný čas
- **Virtuální čas**: Zobrazovaný čas, může se odchylovat během úseků, ale samo-koriguje
- **Config Time**: Offset aplikovaný pro zobrazení vlastního počátečního času
