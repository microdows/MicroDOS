# Changelog

Toutes les versions notables de MicroDOS.
Format basé sur [SemVer](https://semver.org/).

## [v0.6.0] - 2026-10-04

### Ajouté
- API DOS (INT 21h) : AH=09h, 02h, 4Ch, 00h, 30h, 0Bh, 01h, 08h, 2Ch, 0Fh
- Retour propre au shell après exécution d'un .COM
- Programme de test TEST21.COM

### Corrigé
- Manque `iret` à la fin de `.done` dans `int21_handler`
- `push dx` manquant (pile déséquilibrée)

## [v0.5.0] - 2026-10-03

### Ajouté
- Exécution de fichiers .COM
- Programme HELLO.COM (test)
- Édition spéciale : commande `reset`

## [v0.4.0] - 2026-10-03

### Ajouté
- Commande `type` pour afficher un fichier
- FAT12 standard (1 secteur réservé au lieu de 5)
- Compatible Windows et Linux

### Modifié
- `stage2.bin` est maintenant un fichier normal sur la disquette

## [v0.3.0] - 2026-10-03

### Ajouté
- Commande `dir` (liste les fichiers)
- Commande `rebootdos` (redémarrage de MicroDOS)
- BPB (BIOS Parameter Block) pour FAT12

### Modifié
- `reboot` redémarre maintenant complètement le PC

## [v0.2.0] - 2026-10-03

### Ajouté
- Shell interactif avec prompt `>`
- Commandes : `help`, `cls`, `ver`, `reboot`
- Lecture du clavier (INT 0x16)
- Retour arrière (Backspace)

## [v0.1.0] - 2026-10-02

### Ajouté
- Bootloader qui affiche un message
- Signature de boot 0xAA55
- Chargement du stage 2 depuis la disquette

[v0.6.0]: https://github.com/microdows/microdos/releases/tag/v0.6.0
[v0.5.0]: https://github.com/microdows/microdos/releases/tag/v0.5.0
[v0.4.0]: https://github.com/microdows/microdos/releases/tag/v0.4.0
[v0.3.0]: https://github.com/microdows/microdos/releases/tag/v0.3.0
[v0.2.0]: https://github.com/microdows/microdos/releases/tag/v0.2.0
[v0.1.0]: https://github.com/microdows/microdos/releases/tag/v0.1.0
