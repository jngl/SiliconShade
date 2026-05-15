# Instructions du Projet : Retro-3D-Engine

Ce document définit les règles de développement, les standards de qualité et les contraintes architecturales que l'agent Gemini CLI doit rigoureusement respecter.

## 1. Conventions de Codage
- **Style de Nommage :**
    - `PascalCase` pour les classes et les structs (ex: `Rasterizer`, `Mat4`).
    - `snake_case` pour les méthodes et fonctions (ex: `is_running()`, `transform_vertex()`).
- **Standard C++ :** C++20. Utilisation des `std::` modernes (containers, smart pointers, algorithms) encouragée, sauf si cela impacte négativement les performances dans les zones critiques.

## 2. Architecture & Performance
- **Boucle de Rendu (Hot Path) :**
    - **Zéro Allocation :** Aucune allocation dynamique (new/malloc) n'est autorisée à l'intérieur de la boucle de rendu ou de mise à jour. Utiliser des pools ou des buffers pré-alloués.
- **Calcul Intensif (SIMD) :**
    - Toute nouvelle fonction de calcul intensif (transformations, rasterisation, physique) doit inclure ou prévoir une implémentation optimisée en **AVX2**.
    - Maintenir une parité de résultat entre les versions scalaires et SIMD.

## 3. Définition de "Terminé" (Definition of Done)
Une tâche n'est considérée comme achevée que si les critères suivants sont remplis :
- **Tests Unitaires :** Un test unitaire doit être ajouté ou mis à jour avec une couverture significative.
- **Validation Mémoire (ASan) :** Le code doit passer le validateur `AddressSanitizer` sans aucune erreur.
- **Validation Performance :** Pour le code de rendu, une vérification rapide de l'impact sur les performances doit être effectuée.
- **Mise à jour du Plan :** Le fichier `PLAN.md` doit être mis à jour pour refléter l'avancement (cocher les tâches finies).

## 4. Workflow de l'Agent
- Avant toute implémentation majeure, valider la stratégie avec l'utilisateur.
- Utiliser systématiquement les sanitizers lors de l'exécution des tests.
- Prioriser la lisibilité du code tout en respectant les contraintes de performance bas-niveau.
