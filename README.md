# PES 6 x GTA San Andreas — Mode FUN

Un mod pour **Pro Evolution Soccer 6 (PC)** qui ajoute un **Mode FUN** façon GTA : ton joueur sort un FAMAS, dégage les adversaires et va marquer tranquille. Des embrouilles entre joueurs sont aussi prévues, en cinématique.

Le jeu reste intact : saison, match amical et Ligue des Masters fonctionnent comme d'habitude. Quand le Mode FUN est désactivé, le mod n'affiche rien et ne touche à rien.

## L'inspiration

Ce fameux mème : [« Le mec qui a collé GTA et PES 2008 où Samuel Eto'o s'est fait fumer sur le terrain »](https://www.youtube.com/watch?v=39DJs8MwJU8).
On a voulu en faire un vrai mod jouable.

## Où on en est

| Étape | État |
|---|---|
| DLL chargée par PES 6, overlay en jeu (Direct3D 8) | ✅ |
| Bouton Mode FUN (F9), réglage sauvegardé | ✅ |
| Test d'affichage « WASTED » (F10) | ✅ |
| Lire la position des joueurs et du ballon en mémoire | 🔜 |
| FAMAS : tirer sur un adversaire et le neutraliser | 🔜 |
| Embrouilles : dialogues et cinématiques | 🔜 |

C'est un projet en cours. Pour l'instant, le Mode FUN ne fait qu'afficher l'overlay : il ne change encore rien au match.

## Installation (joueurs)

1. Il faut **PES 6 sur PC**. Le mod a été développé sur `PES6.exe` version de fichier 1.0.0.1.
2. Copie `dinput8.dll` dans le dossier du jeu, à côté de `PES6.exe`.
3. Lance le jeu normalement.

Pour désinstaller, supprime `dinput8.dll`, plus `pes6fun.ini` et `pes6fun.log` s'ils sont présents.

### Touches

| Touche | Action |
|---|---|
| F9 | Activer / désactiver le Mode FUN |
| F10 | Test : affiche « WASTED » (Mode FUN activé) |
| F11 | Debug : copie la mémoire du jeu dans `pes6fun_dumps\` (pour le développement) |
| F12 | Debug : panneau avec la position du ballon et du joueur actif |

Les touches se changent dans `pes6fun.ini`, créé à côté de `PES6.exe` :

```ini
[General]
ModeFun=0
[Keys]
ToggleModeFun=120   ; F9 (code de touche virtuelle Windows)
Test=121            ; F10
```

## Compiler soi-même

Sous Windows, avec Git Bash :

```sh
git clone --recursive <url-du-dépôt>
cd Pes6xGTASA
./setup.sh   # sous-modules + compilateur llvm-mingw portable dans tools/ (~150 Mo à télécharger)
./build.sh   # -> build/dinput8.dll
```

## Comment ça marche

- `dinput8.dll` est une DLL proxy. PES 6 la charge à la place de celle de Windows, et elle transmet `DirectInput8Create` au vrai `dinput8.dll`. `PES6.exe` n'est jamais modifié.
- PES 6 dessine avec **Direct3D 8**. Le mod redirige `Direct3DCreate8` dans la table d'imports du jeu, puis intercepte `CreateDevice`, `Present` et `Reset` avec [MinHook](https://github.com/TsudaKageyu/minhook).
- L'overlay utilise [Dear ImGui](https://github.com/ocornut/imgui), avec un backend Direct3D 8 écrit pour ce projet (`src/imgui_impl_dx8.cpp`), puisqu'ImGui n'en fournit pas.

Plus de détails techniques dans [`MODDING_PLAN.md`](MODDING_PLAN.md).

## Règles

- **Aucun fichier des jeux n'est inclus.** Ni PES 6, ni GTA San Andreas : il faut avoir ses propres copies.
- Mod **hors ligne et solo** uniquement.
- PES et Pro Evolution Soccer sont des marques de Konami. GTA et Grand Theft Auto sont des marques de Rockstar Games. Ce projet n'est affilié à aucun des deux.

## Licence

Code sous licence [MIT](LICENSE). Elle couvre uniquement le code du mod, pas les jeux.

## Crédits

- [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause), Tsuda Kageyu
- [Dear ImGui](https://github.com/ocornut/imgui) (MIT), Omar Cornut
- [llvm-mingw](https://github.com/mstorsjo/llvm-mingw), Martin Storsjö
- Adresses mémoire du ballon et du joueur actif : [pes6_game_physics_mod](https://github.com/angelballay/pes6_game_physics_mod) (MIT), Angel Ballay
- Code écrit avec l'aide de Claude Code (IA d'Anthropic).
