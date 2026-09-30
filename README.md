# ToolBox

Petite app Windows pleine de fonctions utiles au quotidien : **moteur en C++** (léger) et **interface en HTML/CSS/JavaScript** affichée par WebView2 (déjà inclus dans Windows 10/11).

## Fonctions

Chaque fonction peut être **activée ou désactivée** depuis la barre latérale, sauf le Moniteur et le Convertisseur, qui sont toujours actifs.

| Fonction | Ce qu'elle fait |
|---|---|
| **Moniteur** *(toujours actif)* | Processeur, mémoire, disque, batterie et réseau en direct, avec des jauges rondes |
| **Convertisseur** *(toujours actif)* | Longueur, masse, température, volume, vitesse, aire, données, temps et devises |
| **Programmes** | Arrête les programmes inutiles (★ + « Mode léger ») et les relance d'un clic |
| **Garde Enter** | Efface la touche voisine d'Enter (`à`, `\`, `#`…) frappée par accident, même si Enter est déjà pressé |
| **Presse-papiers** | Historique de tout ce qui est copié + **fenêtre rapide** ouverte par un raccourci au choix (Ctrl + Alt + V par défaut) qui colle l'élément choisi ; les mots de passe des gestionnaires sont ignorés |
| **Capture d'écran** | Un raccourci fige l'écran et on sélectionne une zone : **image** (Ctrl + Alt + S) copiée et enregistrée en PNG dans Images\Captures ToolBox, ou **texte** (Ctrl + Alt + T) lu par **Tesseract** (OCR) et copié |
| **Statistiques** | Temps d'écran par app, processeur et mémoire sur 48 h, compteurs des fonctions ToolBox (tout reste sur le PC) |
| **Pipette** | Un raccourci (Ctrl + Alt + C) affiche une loupe : clic sur un pixel, sa couleur est copiée en HEX, RGB ou HSL |
| **Raccourcis d'apps** | Groupes d'apps, de fichiers ou de sites à lancer d'un seul clic |
| **Lancement par lieu** | Ouvre une ou plusieurs apps quand tu arrives quelque part (Wi-Fi et/ou GPS) |

L'accueil regroupe les jauges, un convertisseur rapide (« 10 km en mi ») et des raccourcis vers chaque fonction.

Performance : l'interface est libérée quand ToolBox reste caché 3 min, et la fenêtre du presse-papiers peut être préparée au démarrage (réglages dans Paramètres).

Autres fonctions : recherche des fonctions (`Ctrl+F`), démarrage avec Windows, icône près de l'horloge, **mises à jour automatiques**.

## Télécharger

Onglet **Releases** → `ToolBox.exe`. C'est un seul fichier, sans installation.

> Windows SmartScreen peut afficher un avertissement la première fois, parce que l'app n'est pas signée : **Informations complémentaires → Exécuter quand même**.

## Publier une nouvelle version

1. Onglet **Actions** → **Build** → **Run workflow**, puis entrer la version (ex. `0.2.0`).
   On peut aussi pousser un tag : `git tag v0.2.0 && git push origin v0.2.0`.
2. La GitHub Action compile et crée la Release avec `ToolBox.exe` et `ToolBox.exe.sha256`.
3. Les ToolBox installés la détectent (toutes les 6 h), vérifient l'empreinte SHA-256, puis se mettent à jour.

## Compiler soi-même (Windows)

Il faut Visual Studio 2022 (charge de travail « Développement Desktop en C++ »), CMake et [vcpkg](https://vcpkg.io) (pour Tesseract, listé dans `vcpkg.json`).

```bat
set VCPKG_ROOT=C:\chemin\vers\vcpkg
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

La première compilation de Tesseract prend un moment. Ensuite, vcpkg la garde en cache.

Pour voir l'interface **sans Windows**, il suffit d'ouvrir `ui/index.html` dans un navigateur. Des données d'exemple s'affichent.

## Organisation du code

```
src/
  main.cpp              démarrage, instance unique
  app.cpp               fenêtre, WebView2, barre des tâches, messages avec l'interface
  settings.cpp          réglages (%APPDATA%\ToolBox\settings.json)
  updater.cpp           mises à jour via les Releases GitHub
  geo.cpp               position (GPS / service de localisation de Windows)
  screen_select.cpp     écran figé + sélection d'une zone (capture, OCR)
  hotkey.cpp            raccourcis clavier globaux configurables
  http.cpp              téléchargements HTTPS (mises à jour, langues de l'OCR)
  modules/              une fonction = un module (Start / Stop / State / HandleAction)
ui/
  index.html, style.css, app.js, fonts.css   interface (fusionnée dans le .exe à la compilation)
```

**Ajouter une fonction :** créer une classe qui hérite de `Module` (`src/modules/module.h`), l'ajouter dans `App::CreateModules()`, puis ajouter sa page dans `PAGES` (`ui/app.js`).
