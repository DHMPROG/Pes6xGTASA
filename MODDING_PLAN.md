# PES 6 x GTA San Andreas — plan de modding (« Mode FUN »)

Repérage du 2026-10-05. Rien n'a encore été modifié dans les dossiers des jeux.

## Jeu hôte : Pro Evolution Soccer 6 (PC)
- Installation : `D:\Games\Pro Evolution Soccer 6` (installé le 2026-10-05)
- `PES6.exe` : version de fichier 1.0.0.1, daté du 26/10/2006, 21,9 Mo, MD5 `BF39628CEF60DF46D0C8E1B4E220FFE6`
  - À confirmer : 1.00 ou 1.10, et exe d'origine ou modifié (les adresses mémoire en dépendent)
- Moteur : natif, x86 (32 bits), **Direct3D 8** (`d3d8.dll`), entrées via `DINPUT8.dll`
  - `um scan` indique « .NET » à tort : c'est `MLStart.exe` (outil de Ligue des Masters) qui est en .NET, pas le jeu
- Autres exécutables : `pes6zoom.exe` (lanceur avec réglage de caméra, 2011), `settings.exe`, `MLStart.exe`
- Anti-triche / en ligne : aucun. Les serveurs officiels sont fermés. Le mod reste hors ligne.
- Sauvegardes : `%USERPROFILE%\Documents\KONAMI\Pro Evolution Soccer 6\save` + `settings.dat`
- Données : `dat\*.afs` (textes et sons par langue, `opmov`)
- Kitserver 6 (fichiers datés de 2009) est présent dans `kitserver\` avec 12 modules (afs2fs, kserv, bserv, fserv, lodmixer, dxtools, stadium, bootserv, clock, psc…)
  - **Il n'est pas branché sur `PES6.exe`** : l'exe ne contient aucune référence à `kload.dll`. Il faudrait relancer `kitserver\setup.exe` pour l'activer.
  - `dxtools.dll` intercepte lui aussi D3D8 : à tester ensemble si on active Kitserver.

## Source d'assets : GTA San Andreas
- Installation : `D:\Games\Grand Theft Auto San Andreas`, `version.txt` = 1.0.0.22 (édition récente avec Rockstar Games Launcher : `title.rgl`, `MTLX.dll`)
- Modèles : `models\gta3.img` (format VER2), d'où on extraira `m4.dff` et `m4.txd`
- Sons : `audio\sfx\GENRL` (banques au format Rockstar, à extraire avec un outil communautaire comme SAAT)
- GTA n'est jamais lancé ni modifié. On y lit seulement des fichiers, en local, et aucun fichier extrait n'est redistribué.

## Route choisie
Le mod est une **DLL proxy `dinput8.dll`** (C++ 32 bits, MinHook, Dear ImGui avec un backend DX8 maison) placée à côté de `PES6.exe`.
- `PES6.exe` n'est pas modifié.
- On choisit `dinput8` plutôt que `d3d9` pour ne pas entrer en conflit avec `dxtools` de Kitserver.
- `Direct3DCreate8` est redirigé dans la table d'imports de `PES6.exe`, puis `CreateDevice`, `Present` (15) et `Reset` (14) sont interceptés avec MinHook.
- Si le Mode FUN est désactivé, la DLL ne touche à rien.

Pourquoi : il n'existe pas d'API de modding pour la logique de match. Kitserver ne gère que les assets (maillots, ballons, stades). Il faut donc des hooks natifs et de la lecture/écriture mémoire.

## Fonctionnalités
1. **Bouton Mode FUN** : overlay ImGui + touche F9, réglage enregistré dans `pes6fun.ini`. Actif dans tous les types de match, sans toucher aux menus de PES.
2. **FAMAS** : modèle M4 de GTA (en attendant un FAMAS de la communauté), dessiné près du joueur contrôlé et orienté comme lui. Flash du canon, trait de la balle, son de tir.
3. **Tir** : trajectoire calculée à partir des positions lues en mémoire. L'adversaire touché est figé au sol ou déplacé hors du terrain, puis remis en jeu au prochain arrêt. **Aucun joueur n'est jamais supprimé.** Texte « WASTED ».
4. **Embrouilles** : match en pause, bandes noires, sous-titres avec le nom des joueurs. Répliques dans `dialogues.json`. Texte seulement pour l'instant, sans voix ni génération d'assets.

## Environnement de test
- Sauvegarder `Documents\KONAMI\Pro Evolution Soccer 6` avant de commencer.
- Garder une copie de `PES6.exe` et de `kitserver\` (`um backup` ou une copie manuelle).
- Jouer en fenêtré via `settings.exe` pour faciliter les captures d'écran et le débogage.

## Inconnues à résoudre d'abord
- [ ] Version exacte de l'exe (1.00 ou 1.10) et éventuelle modification de l'exe
- [x] Ballon : `[0xBCCE94]` -> structure du ballon (fixe à `0x010105E0`), position float x/y/z à +0x20 (y = -28 au sol), état à +0x84 (0 conduite, 1 passe, 3 centre, 4 tir, 5 arrêté). Source : pes6_game_physics_mod (Angel Ballay, MIT), vérifiée sur notre exe.
- [x] Porteur du ballon : octet à `0x3BE09CC`, c'est un index dans le tableau des acteurs.
- [x] Tableau des acteurs (trouvé avec les copies F11) : 23 fiches de `0x240` octets, position float x/y/z de l'acteur 0 à `0x03BDCA60`. 0 = arbitre, 1..11 = équipe A (1 = gardien), 12..22 = équipe B (12 = gardien). Terrain : buts vers x = ±13200, touches vers z = ±8500.
- [ ] Index du joueur contrôlé : filtre en direct dans le panneau F12 (`re/` contient les scripts d'analyse des copies)
- [ ] État du match (en jeu / pause / menu)
- [ ] Matrices vue/projection (apitrace, RenderDoc ne gère pas D3D8) : hook de `SetTransform` / `SetVertexShaderConstant`
- [ ] Façon sûre de « neutraliser » un adversaire sans faire planter le moteur de match
- [ ] Mettre le match en pause depuis le code (pour les cinématiques)
- [ ] Compatibilité avec `pes6zoom.exe` et Kitserver si on les active

## Étape 1 : faite le 2026-10-05
La DLL se charge, l'overlay s'affiche, F9 active ou désactive le Mode FUN, F10 affiche « WASTED » (validé en jeu).
Attention : un PES 6 lancé par Claude n'obtient pas le focus. En plein écran, le device D3D8 est alors perdu en boucle tant qu'on ne clique pas sur la fenêtre.

## Première version jouable (à valider avant le reste)
En match amical avec le Mode FUN activé, F10 trace une ligne rouge depuis le joueur contrôlé, et le premier adversaire sur la ligne est déplacé hors du terrain. Le match continue sans planter.
