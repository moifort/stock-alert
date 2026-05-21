# Stock Alert

Un petit capteur Apple Home qui te prévient quand un bocal, une boîte ou un
réservoir se vide. Tu ranges le bidule au-dessus d'un contenant — quand le
niveau passe sous un seuil que tu choisis, ton iPhone te notifie comme si une
porte venait de s'ouvrir.

<!-- TODO photo : le device monté au-dessus d'un bocal -->
<!-- TODO photo : la notification Apple Home -->

> 🚧 **Phase 1 (état actuel) :** le capteur de distance n'est pas encore câblé,
> un bouton physique sur la carte simule les changements d'état pour valider
> toute la chaîne Apple Home. La Phase 2 branchera le vrai capteur ToF.

## Ce que ça fait, concrètement

- Apparaît dans l'app **Maison** d'iOS comme un **détecteur de contact**
  (l'équivalent virtuel d'un capteur de porte).
- Deux états : **`Fermé`** = "stock OK", **`Ouvert`** = "stock bas".
- Tu peux créer des **automatisations** dessus : recevoir une notification,
  allumer une LED, déclencher un scénario, envoyer un message — tout ce
  qu'Apple Home permet sur un capteur de contact.
- Communique en **Matter over Wi-Fi** (pas de hub propriétaire, pas d'app
  tierce). Un HomePod, HomePod mini ou Apple TV 4K récent fait office de
  concentrateur Matter.

## Le matériel

| Composant                                   | Pourquoi                                          | ~Prix    |
| ------------------------------------------- | ------------------------------------------------- | -------- |
| Seeed Studio **XIAO ESP32-S3**              | MCU compact avec Wi-Fi + Bluetooth + USB-C       | ~10 €    |
| **Antenne U.FL / IPEX** 2.4 GHz             | ⚠️ Quasi-obligatoire (voir Troubleshooting)       | ~3 €     |
| Capteur ToF **VL53L1X**                     | Mesure la distance jusqu'au contenu (Phase 2)     | ~7 €     |
| 4 fils Dupont F-F                            | Câblage capteur ↔ carte                            | ~1 €     |
| Câble USB-C → USB-A ou USB-C                | Pour flasher le firmware                          | déjà ✓   |

### Câblage du capteur (Phase 2)

| VL53L1X | XIAO ESP32-S3       |
| ------- | ------------------- |
| `VCC`   | `3V3`               |
| `GND`   | `GND`               |
| `SDA`   | `D4` (GPIO 5)       |
| `SCL`   | `D5` (GPIO 6)       |

> ⚠️ Ne JAMAIS alimenter le VL53L1X en 5 V. Les GPIO du XIAO sont 3.3 V only.

<!-- TODO photo : câblage propre du VL53L1X sur le XIAO -->

## Installation depuis un Mac

### 0. Pré-requis

```bash
# Si pas déjà installés
xcode-select --install                  # outils Apple
brew install git python@3.13 cmake ninja dfu-util ccache
```

Python 3.10 ou supérieur. Pas besoin d'Arduino ni de PlatformIO.

### 1. Cloner

```bash
git clone https://github.com/moifort/stock-alert.git
cd stock-alert
```

### 2. Installer la toolchain (~30 min, ~3 Go)

```bash
./scripts/setup.sh
```

Le script télécharge et configure ESP-IDF v5.5 + ESP-Matter v1.4 dans
`./toolchain/` (ignoré de git). C'est long mais c'est une fois pour toutes.

### 3. Charger l'environnement (à chaque nouvelle session shell)

```bash
source ./scripts/activate.sh
```

Tu devrais voir :

```
✅ ESP-IDF: ESP-IDF v5.5.3
✅ ESP-Matter at .../toolchain/esp-matter
✅ gn: .../pigweed/gn
```

### 4. Compiler

```bash
idf.py set-target esp32s3
idf.py build
```

La première compilation prend 5–10 min (Matter est lourd). Les suivantes
quelques secondes grâce au cache incrémental.

### 5. Brancher le XIAO en USB-C, puis flasher

```bash
# Le port peut être /dev/cu.usbmodem2101, /dev/cu.usbmodem1101, etc.
ls /dev/cu.usbmodem*
idf.py -p /dev/cu.usbmodem2101 erase-flash flash monitor
```

Le serial monitor affiche le log de boot et un **QR code Matter**. Garde-le
ouvert pour la suite.

> Pour quitter le monitor : `Ctrl+]`.

## Pairing avec Apple Home

<!-- TODO photo : QR code Matter scanné depuis Maison -->

1. Sur ton iPhone, ouvre l'app **Maison** → **+** → **Ajouter un accessoire**.
2. **Scanne le QR code** affiché dans le serial monitor (ou tape "Plus
   d'options" et entre le code à 11 chiffres affiché juste après le QR).
3. Apple Home affichera **"Accessoire Matter non certifié"** — c'est normal
   et inoffensif (on utilise un Vendor ID de test). Tape **Ajouter quand même**.
4. Apple Home demande à quel Wi-Fi le rattacher et pousse les credentials
   au device via Bluetooth.
5. Au bout de 1 à 2 minutes, le device apparaît comme **Stock Sensor** avec
   l'icône d'un détecteur de contact.

> **Important :** garde l'app Maison **au premier plan** pendant toute la durée
> du pairing. Ne verrouille pas l'iPhone, ne bascule pas vers une autre app.
> Apple Home enchaîne deux fabrics Matter (un pour ton iPhone, un pour ton
> HomePod) et le 2ᵉ peut échouer si tu lâches l'attention.

## Utilisation au quotidien (Phase 1)

Une fois pairé, deux gestes physiques sur la carte XIAO :

| Geste                          | Effet                                                |
| ------------------------------ | ---------------------------------------------------- |
| Appui court (< 3 s) sur **B**  | Bascule l'état Fermé ↔ Ouvert dans Maison           |
| Appui long (≥ 3 s) sur **B**   | Réinitialisation complète (efface le pairing Matter) |
| Appui court sur **R**          | Reboot logiciel (préserve le pairing)                |

Les boutons sont **microscopiques** (composants SMD ~1.5 mm × 1.5 mm) à
gauche du module Seeed, près du connecteur USB-C. Lettres `B` et `R`
sérigraphiées en blanc à côté. Presser avec un ongle ou la pointe d'un stylo.

Le changement d'état est visible dans Maison **après 1 à 5 secondes** (latence
normale Apple Home + iCloud Sync, identique aux produits Aqara / Eve).

## Feuille de route

- ✅ **Phase 1** — accessoire Matter + bouton physique simulant un capteur
- 🚧 **Phase 2** — câblage du VL53L1X, lecture périodique, déclenchement
  automatique avec hystérésis (`seuil_low` / `seuil_ok` configurables via NVS)
- 🔮 **Phase 3** — boîtier imprimé 3D, fixation magnétique au-dessus du
  contenant, autonomie sur batterie + USB-C charging

## Troubleshooting

### Apple Home affiche "Accessoire sans réponse"

Le device est commissioné mais ton hub Apple ne peut pas lui parler en local.
Cause la plus fréquente : **AP isolation** activée sur ton réseau Wi-Fi
(souvent par défaut sur les SSID "IoT" séparés). À désactiver dans l'interface
admin de ta box. Le device et le HomePod doivent pouvoir se parler en
client-à-client sur le même SSID, sinon Matter ne fonctionne pas.

### Le pairing échoue, le device ne se voit dans aucun scan Wi-Fi

Quasi-toujours un **problème d'antenne**. Le XIAO ESP32-S3 est livré configuré
pour utiliser une antenne externe via le connecteur U.FL — sans antenne
branchée, le signal radio est ~30 dB plus faible que la normale (RSSI -90+ dBm
au lieu de -50). Solutions :

1. **Brancher une antenne U.FL/IPEX 2.4 GHz** (~3 € sur Amazon, plug & play)
2. **Déplacer la résistance 0 Ω** sur les pads `PCB` (sur la face arrière du
   board) pour utiliser l'antenne céramique intégrée au module — moins de
   portée mais zéro accessoire. Nécessite un fer à souder fin et une loupe.

### `pairing failed` après plusieurs tentatives

1. **Vérifier qu'un Home hub est actif** : Maison → Réglages du domicile →
   Concentrateurs et passerelles → au moins un HomePod / Apple TV en
   **Connecté**. Sans hub, Matter ne s'installera pas.
2. **Supprimer toute trace fantôme** : si une précédente tentative a laissé
   un "Accessoire Matter" résiduel, appui long → Supprimer, puis retente.
3. **Effacer le NVS du device** : `idf.py -p <port> erase-flash flash` pour
   repartir d'un état complètement vierge.

### Le bouton **B** ne réagit pas

Ce sont des composants SMD très plats. Presser bien au centre, pas sur le
côté. Le `mock_button` log dans le serial monitor confirme la détection.

## Sous le capot (pour les curieux)

- Firmware en **C++17** sur **ESP-IDF v5.5** + **ESP-Matter v1.4** (SDK
  Espressif officiel, code-first — pas de fichier ZAP à compiler à la main).
- Un seul endpoint Matter : **Contact Sensor** (cluster `BooleanState` 0x0045).
- Commissioning en BLE + Wi-Fi (multi-admin Apple Home, deux fabrics).
- Hystérésis Phase 2 : `THRESHOLD_LOW_MM` / `THRESHOLD_OK_MM` séparés pour
  éviter le flapping autour d'un seuil unique.
- Constantes hardware centralisées dans `main/include/stock_alert_config.h`.
- Conventions et pièges spécifiques documentés dans `CLAUDE.md`.

## Licence

À définir.
