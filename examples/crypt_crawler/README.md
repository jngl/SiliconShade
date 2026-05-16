# The Crypt Crawler (Demo)

**The Crypt Crawler** est une démonstration technique conçue pour valider les fonctionnalités du moteur **Retro-3D-Engine** en conditions réelles. Ce projet sert de banc d'essai pour l'API, les performances et l'architecture ECS.

## 🎯 Objectifs de la Démo
- **Validation du Pipeline :** Tester le chargement de modèles (OBJ) et de textures avec le rastériseur SIMD.
- **Mouvement FPS :** Implémenter une caméra à la première personne avec gestion des entrées clavier/souris.
- **Test de l'ECS :** Gérer les objets interactifs (coffres) et les éléments de décor comme des entités.
- **Rendu 2D :** Afficher un HUD minimaliste (croix de visée, compteur d'objets).

## 🕹️ Gameplay (Concept)
Le joueur est plongé dans une crypte sombre. Son objectif est de retrouver les **3 Coffres Sacrés** dispersés dans le niveau. 
- **Z, Q, S, D :** Déplacement.
- **Souris :** Regarder autour de soi.
- **Espace :** Action / Interaction.

## 🏗️ Structure du Projet
- `main.cpp` : Point d'entrée et boucle de jeu.
- `GameWorld.cpp/h` : Gestion de l'état du jeu et intégration ECS.
- `assets/` : Ressources spécifiques à la démo (textures, modèles).

## 📈 Évolution avec le Moteur
| Phase Moteur | Ajout dans Crypt Crawler |
| :--- | :--- |
| **Phase 1 (Maths)** | Caméra fluide et déplacements de base. |
| **Phase 2 (ECS/2D)** | Système d'entités pour les coffres et HUD 2D. |
| **Phase 3 (BSP)** | Premier vrai niveau avec murs et collisions. |
| **Phase 4 (Physique)** | Collisions glissantes contre les murs. |
| **Phase 5 (HUD)** | Inventaire et feedback visuel complet. |
