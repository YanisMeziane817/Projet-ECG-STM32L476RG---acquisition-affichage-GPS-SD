# Cardiographe Embarqué Portatif — STM32L476RG

> Acquisition ECG temps réel, affichage sur écran couleur, sauvegarde CSV sur carte SD avec géolocalisation GPS.

---

## 📋 Table des matières

- [Vue d'ensemble](#vue-densemble)
- [Matériel requis](#matériel-requis)
- [Brochage complet](#brochage-complet)
- [Architecture logicielle](#architecture-logicielle)
- [Format des données CSV](#format-des-données-csv)
- [Installation et compilation](#installation-et-compilation)
- [Utilisation pas à pas](#utilisation-pas-à-pas)
- [Analyse sur PC](#analyse-sur-pc)
- [Structure du projet](#structure-du-projet)
- [Auteurs](#auteurs)

---

## Vue d'ensemble

Ce projet réalise un **électrocardiographe embarqué portatif** sur carte Nucleo-L476RG. Il est capable de :

| Fonctionnalité | Détails |
|---|---|
| 📡 Acquisition ECG | 1000 Hz, résolution 12 bits |
| 🖥️ Affichage temps réel | Écran ILI9341 240×320 px, défilement continu, vert sur noir |
| 💾 Sauvegarde SD | Fichiers CSV auto-nommés (FatFS) |
| 🛰️ Géolocalisation | GPS SparkFun Logger GPS |
| 🔘 Contrôle | Bouton poussoir — déclenche un enregistrement de 500 échantillons |
| 🐍 Analyse PC | Script Python — visualisation |

### Schéma général

```
┌──────────────────────────────────────────────────────────┐
│                   STM32L476RG (Nucleo)                    │
│                                                          │
│  PC3  ◄────  AD8232 (amplificateur ECG)                  │
│  PA5/6/7 ◄── SPI1 ──► Carte SD (FatFS)                  │
│  PB6  ──────────────► CS carte SD                        │
│  PA2/3 ◄──── USART2 ─► GPS SparkFun     │
│  D0-D7 ──────────────► ILI9341 240×320                    │
│  PA11 ◄─────────────  Bouton poussoir                    │
└──────────────────────────────────────────────────────────┘
```

---

## Matériel requis

| Composant | Référence | Rôle |
|---|---|---|
| Microcontrôleur | STM32L476RG (Nucleo-L476RG) | MCU principal |
| Amplificateur ECG | AD8232 | Acquisition et filtreage du signal cardiaque |
| Écran couleur | ILI9341 2.8" 240×320 | Affichage temps réel |
| Carte SD | Toute carte SD/microSD | Stockage CSV |
| Module GPS | SparkFun Logger GPS | Géolocalisation |
| Bouton poussoir | Bouton momentané NO | Déclenchement enregistrement |
| Câbles / breadboard | — | Connexions |
| Électrodes ECG | Électrodes adhésives 3 fils | Captation signal cardiaque |

> ⚠️ Le jumper **VBAT/3V3** du module GPS SparkFun doit être **soudé** pour une alimentation en 3.3V depuis le Nucleo.

---

## Brochage complet

### AD8232 → STM32L476RG

| Signal CubeMX | Broche STM32 | Broche AD8232 | Fonction |
|---|---|---|---|
| Analog_input_PC3 | PC3 | OUTPUT | Signal ECG analogique → ADC1_IN4 |
| — | PC5 | LO− | Détection décrochage électrode − |
| GPIO_Input | PC6 | LO+ | Détection décrochage électrode + |
| SDN | PC8 | SDN | Shutdown ampli (actif bas) |
| — | 3.3V | 3.3V | Alimentation |
| — | GND | GND | Masse |

### Carte SD → SPI1

| Signal CubeMX | Broche STM32 | Fonction |
|---|---|---|
| SPI1_SCK | PA5 | Horloge SPI |
| SPI1_MISO | PA6 | Données SD → MCU |
| SPI1_MOSI | PA7 | Données MCU → SD |
| SD_CS | PB6 | Chip Select (actif bas) |
| — | 3.3V | Alimentation |
| — | GND | Masse |

### GPS SparkFun → USART2

| Signal CubeMX | Broche STM32 | Fonction |
|---|---|---|
| USART_RX | PA3 | Trames NMEA GPS → MCU |
| USART_TX | PA2 | Debug MCU → HTerm PC |
| — | 3.3V | Alimentation |
| — | GND | Masse |
| Baudrate | — | 9600 bps |

### Écran ILI9341 — Bus parallèle D0-D7

| Signal CubeMX | Broche STM32 | Fonction |
|---|---|---|
| LCD_RD | PA0 | Read strobe |
| LCD_WR | PA1 | Write strobe |
| LCD_RST | PC1 | Reset écran |
| LCD_CS | PB0 | Chip Select écran |
| LCD_D0 | PA9 | Data bus bit 0 |
| LCD_D1 | PC7 | Data bus bit 1 |
| LCD_D2 | PA10 | Data bus bit 2 |
| LCD_D3 | PB3 | Data bus bit 3 |
| LCD_D4 | PB5 | Data bus bit 4 |
| LCD_D5 | PB4 | Data bus bit 5 |
| LCD_D6 | PB1 | Data bus bit 6 |
| LCD_D7 | PA8 | Data bus bit 7 |

### Bouton poussoir

| Signal CubeMX | Broche STM32 | Config |
|---|---|---|
| Pousoir | PA11 | Pull-up interne, actif bas, EXTI |

---

## Architecture logicielle

### Cadence d'acquisition — TIM2

```
fclk = 80 MHz
TIM2 : PSC = 79, ARR = 999
→ fech = 80 000 000 / (79+1) / (999+1) = 1000 Hz exactement
→ Interruption TIM2 toutes les 1 ms → lancement ADC1
```

### Pipeline de traitement

```
Interruption TIM2 @ 1000 Hz
        │
        ▼
ADC1 IN4 (PC3) — lecture 12 bits (0–4095)
        │
        ▼
ecg_filter_ma() — filtre moyenne glissante (MA_SIZE configurable)
        │
        ├──► Affichage pixel vert sur ILI9341 (défilement)
        │
        └──► Buffer circulaire 500 échantillons (raw + filtered)
                    │
               [Appui bouton PA11]
                    │
                    ▼
        Lecture dernière trame GPS ($GPRMC)
                    │
                    ▼
        Écriture ECG_NNN.CSV sur carte SD (FatFS)
```

### Gestion GPS par interruption

Le GPS envoie des trames NMEA en continu à 9600 baud. La réception se fait caractère par caractère via `HAL_UART_Receive_IT` :

- `USART2_IRQHandler` dans `stm32l4xx_it.c` accumule les caractères
- Quand un `\n` est détecté, la trame est parsée
- Si c'est une trame `$GPRMC` avec fix valide (`A`), latitude et longitude sont mis à jour
- La LED bleue du module GPS s'allume en fixe quand le fix est acquis (~1 min en extérieur)


## Format des données CSV

Chaque appui sur le bouton crée un nouveau fichier sur la carte SD.

```
index;raw;filtered;latitude;longitude
0;2048;2051;48.573400;7.752600
1;2055;2052;48.573400;7.752600
2;2071;2058;48.573400;7.752600
...
499;2041;2049;48.573400;7.752600
```

| Colonne | Type | Description |
|---|---|---|
| `index` | entier | Numéro de l'échantillon (0 à 499) |
| `raw` | entier 12 bits | Valeur ADC brute (0–4095) |
| `filtered` | entier | Valeur après filtre MA firmware |
| `latitude` | flottant | Latitude WGS84 en degrés décimaux |
| `longitude` | flottant | Longitude WGS84 en degrés décimaux |

500 échantillons à 1000 Hz = **0.5 seconde** de signal par enregistrement.  
Si le GPS n'a pas de fix, latitude et longitude sont à `0.000000`.

---

## Installation et compilation

### Prérequis logiciels

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) version ≥ 1.13
- Pilote ST-Link (installé automatiquement avec CubeIDE)
- Python 3.8+ avec pip (pour l'analyse PC uniquement)

### Cloner et ouvrir le projet

```bash
git clone https://github.com/YanisMeziane817/Projet-ECG-STM32L476RG---acquisition-affichage-GPS-SD.git
```

Dans STM32CubeIDE :
1. `File` → `Import` → `Existing Projects into Workspace`
2. Sélectionner le dossier cloné
3. Cocher le projet `test` → `Finish`

### Compiler et flasher

```
Project → Build Project    (Ctrl+B)
Run → Debug As → STM32 Cortex-M C/C++ Application
```

Le firmware est automatiquement flashé et l'exécution démarre.

### Dépendances incluses dans le repo

Toutes les bibliothèques sont incluses, aucun téléchargement supplémentaire n'est nécessaire.

| Bibliothèque | Source | Emplacement |
|---|---|---|
| STM32L4xx HAL | ST Microelectronics (CubeMX) | `Drivers/STM32L4xx_HAL_Driver/` |
| FatFS | ChaN (R0.15) | `Middlewares/FatFs/` + `FATFS/` |
| Driver ILI9341 | Custom | `Core/Src/ili9341.c` |
| Filtre ECG | Custom | `Core/Src/ecg_filter.c` |

---

## Utilisation pas à pas

1. Assembler le circuit selon le brochage ci-dessus
2. Insérer une carte SD formatée FAT32
3. Brancher le Nucleo en USB → flasher le firmware
4. Placer les 3 électrodes ECG (RA bras droit, LA bras gauche, RL jambe droite)
5. Appuyer sur le bouton poussoir
6. L'écran s'allume : le signal ECG défile en vert en temps réel
7. Aller en extérieur et attendre le fix GPS : la LED bleue du module devient fixe (~1 min)
8. Appuyer sur le bouton → 500 échantillons sont enregistrés dans `ECG_001.CSV`
9. Chaque appui suivant crée `ECG_002.CSV`, `ECG_003.CSV`, etc.
10. Retirer la SD et analyser sur PC avec le script Python

---

## Analyse sur PC

```bash
cd scripts/
pip install -r requirements.txt
python ecg_analysis.py ../data/ECG_001.CSV
```

Options disponibles :

```bash
python ecg_analysis.py ECG_001.CSV          # Analyse + graphe
python ecg_analysis.py ECG_001.CSV --map    # + carte GPS interactive (folium)
python ecg_analysis.py ECG_001.CSV --no-plot  # Sans affichage graphique
```

Le script produit :
- Tracé signal brut ADC vs signal filtré firmware
- Détection automatique des pics R (Pan-Tompkins simplifié)
- Calcul fréquence cardiaque moyenne (BPM) et variabilité (HRV/SDNN)
- Graphe intervalles RR
- Carte GPS interactive HTML (si `--map` et GPS fix disponible)

---

## Structure du projet

```
Projet-ECG/
│
├── Core/
│   ├── Inc/
│   │   ├── main.h                  # Defines globaux, prototypes
│   │   ├── ecg_filter.h            # Interface filtre MA (MA_SIZE configurable)
│   │   ├── ili9341.h               # Interface driver écran
│   │   ├── ili9341_porsche.h       # Mapping broches écran parallèle 8080
│   │   ├── font.h                  # Polices d'affichage
│   │   ├── stm32l4xx_hal_conf.h    # Config HAL (CubeMX)
│   │   └── stm32l4xx_it.h          # Déclarations IRQ handlers
│   │
│   └── Src/
│       ├── main.c                  # Init, boucle principale, ISR bouton
│       ├── ecg_filter.c            # Implémentation filtre moyenne glissante
│       ├── ili9341.c               # Driver ILI9341 (commandes, pixels, scroll)
│       ├── ili9341_porsche.c       # Abstraction hardware bus 8080
│       ├── font.c                  # Rendu texte à l'écran
│       ├── stm32l4xx_it.c          # TIM2_IRQ, USART2_IRQ, EXTI_PA11_IRQ
│       ├── stm32l4xx_hal_msp.c     # Init bas niveau périphériques (CubeMX)
│       ├── syscalls.c              # Stubs newlib
│       ├── sysmem.c                # Gestion heap
│       └── system_stm32l4xx.c      # Config horloge système (CubeMX)
│
├── Drivers/
│   ├── STM32L4xx_HAL_Driver/       # HAL complet ST (généré CubeMX)
│   │   └── (stm32l4xx_hal_adc.c, _gpio.c, _spi.c, _tim.c, _uart.c ...)
│   └── FATFS/
│       ├── App/fatfs.c / fatfs.h   # Point d'entrée FatFS applicatif
│       └── Target/
│           ├── ffconf.h            # Config FatFS (taille secteur, LFN...)
│           ├── user_diskio.c       # Glue FatFS ↔ SPI SD
│           └── user_diskio.h
│
├── Middlewares/
│   └── FatFs/                      # Sources FatFS ChaN R0.15
│       ├── ff.c / ff.h             # Cœur FatFS
│       ├── diskio.c                # Interface disque abstraite
│       ├── ff_gen_drv.c            # Driver générique
│       └── ccsbcs.c                # Tables de caractères
│
├── scripts/
│   ├── ecg_analysis.py             # Analyse Python : BPM, HRV, graphes, GPS
│   └── requirements.txt            # pandas, numpy, matplotlib, scipy, folium
│
├── docs/
│   ├── rapport_technique.md        # Rapport technique complet
│   └── index.html                  # Page de présentation GitHub Pages
│
├── test.ioc                        # Fichier config STM32CubeMX (re-générable)
├── STM32L476RGTX_FLASH.ld          # Linker script Flash
├── STM32L476RGTX_RAM.ld            # Linker script RAM
├── .gitignore
└── README.md
```

---

## Auteurs

**Yanis Meziane** — [@YanisMeziane817](https://github.com/YanisMeziane817)

**Evan Legland** — [@EvanLegd](https://github.com/EvanLegd)

Projet réalisé dans le cadre de la fin de seconde année d'école d'ingénieur (ENSISA).

---

*Licence MIT — Libre de réutilisation avec attribution.*
