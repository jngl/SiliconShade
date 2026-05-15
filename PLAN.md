# Plan d'Action : Moteur FPS Rétro (Quality-First Approach)

Ce document détaille les étapes pour transformer le prototype en un moteur de jeu professionnel, en mettant l'accent sur la stabilité et la qualité dès le départ.

## Phase 1 : Fondation & Infrastructure de Qualité
*Objectif : Mettre en place les outils de diagnostic et la structure de base.*

1. **Framework de Test & Sanitizers**
   - [x] Migrer tous les tests vers `doctest` (Math, Memory, Threads, Engine).
   - [x] Activer les Sanitizers (`ASan`, `TSan`, `UBSan`) dans le build de développement.
2. **Refactoring Core**
    - Extraire la logique de `main.cpp` vers une classe `Engine`.
    - Créer un `Input Manager` (clavier/souris) robuste.
3. **Boucle de Jeu (Game Loop)**
   - Implémenter la gestion du temps (Delta Time) avec *fixed time step* pour la physique.

## Phase 2 : Rendu, Caméra & Validation Visuelle
*Objectif : Assurer la fidélité du rendu et la fluidité des déplacements.*

1. **Système de Caméra 3D**
   - Implémenter les matrices Vue/Projection et les contrôles FPS.
2. **Validation SIMD & Parité**
   - Créer des tests comparant les résultats scalaires vs AVX2 pour garantir la précision des optimisations.
3. **Tests de Régression Visuelle (Snapshots)**
   - Système de capture et comparaison de frames ("Golden Images") pour éviter les régressions graphiques lors des optimisations futures.

## Phase 3 : Monde, Collisions & Robustesse
*Objectif : Charger des environnements complexes de manière sûre.*

1. **Intégration BSP (Binary Space Partitioning)**
   - Parser Quake .bsp et implémenter le PVS.
2. **Fuzzing des Parsers**
   - Tester le chargement d'assets (.obj, .bsp) avec des fichiers corrompus pour garantir la robustesse du moteur.
3. **Collisions & Physique**
   - Utiliser les plans du BSP pour les collisions et le glissement contre les murs.

## Phase 4 : Entités & Gameplay
*Objectif : Rendre le monde interactif.*

1. **Entity System**
   - Système d'acteurs dynamiques (Ennemis, Projectiles).
2. **Animation MD2**
   - Intégrer le format Quake 2 pour les modèles animés.
3. **View Model (Armes)**
   - Rendu de l'arme du joueur au premier plan.

## Phase 5 : Audio & Finitions
*Objectif : Immersion et esthétique.*

1. **Système Audio (Miniaudio)**
   - Gestion des sons 3D et d'ambiance.
2. **Stress Tests**
   - Pousser le `ThreadPool` et le nombre d'entités à leurs limites pour identifier les goulots d'étranglement.
3. **Esthétique Rétro**
   - Palette 256 couleurs et dithering optionnel.
