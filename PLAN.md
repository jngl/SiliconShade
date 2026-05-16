# Plan d'Action : Moteur "Retro-HL" (ECS & Pipeline Fixe)

Ce document définit la trajectoire pour transformer le moteur en un framework 3D de type Half-Life, utilisant une architecture ECS et un pipeline de rendu optimisé et simplifié.

## Phase 1 : Fondations & Infrastructure
*Objectif : Établir une base solide pour les transformations 3D et la gestion des ressources.*

1. **Refonte du Module Math**
   - [ ] Implémenter une bibliothèque mathématique complète : `Vec3f`, `Mat4`, `Quat`.
   - [ ] Ajouter les primitives : `Plane`, `Ray`, `Sphere`.
   - [ ] Fonctions de projection : `perspective`, `look_at`.
2. **Infrastructure Mémoire Avancée**
   - [ ] Système de **Markers** pour l'Arena (allocations temporaires).
   - [ ] **Pool Allocator** pour les composants ECS.
   - [ ] **Thread-Local Arenas** pour le rendu parallèle.
3. **Resource Manager**
   - [ ] Système de cache pour éviter de charger plusieurs fois la même texture ou le même modèle.
   - [ ] Support des formats **OBJ** (statique) et **MD2** (animé).
   - [ ] Gestion centralisée des textures (PNG/TGA).
4. **Caméra & Transform**
   - [ ] Classe `Camera` (LookAt, Perspective Correct).
   - [ ] Classe `Transform` (Position, Rotation, Scale) avec hiérarchie simple.

## Phase 2 : Le Pipeline de Rendu & ECS
*Objectif : Fournir une API de haut niveau et structurer le monde.*

1. **Le "Renderer" (Pipeline Fixe Software)**
   - [ ] Support de la **Résolution Dynamique** (redimensionnement du buffer).
   - [ ] Encapsuler le rastériseur SIMD en True Color (32-bit).
   - [ ] Implémenter le **Clipping Frustum Complet** (6 plans).
2. **Noyau ECS (Entity Component System)**
   - [ ] Implémenter `World`, `Entity` et les systèmes de base.
   - [ ] Système de rendu ECS automatique (parcours des entités MeshRenderer).
3. **Infrastructure UI (2D Software)**
   - [ ] Primitives de dessin 2D : Rectangles (remplis/bordures), Lignes.
   - [ ] Rendu de **Bitmap Fonts** (Affichage de texte via atlas de textures).
   - [ ] Support des **Sprites 2D** avec transparence (Alpha Blending).
4. **Debug Overlay & Diagnostics**
   - [ ] Affichage des FPS, nombre de triangles et consommation mémoire.

## Phase 3 : Environnement Type Half-Life (BSP)
*Objectif : Charger et afficher des niveaux complexes.*

1. **Parser BSP (Quake/HL)**
   - [ ] Chargement des géométries, textures et Lightmaps.
2. **Visibilité & Culling**
   - [ ] Implémenter le PVS (Potentially Visible Set).
   - [ ] Frustum Culling (AABB vs Frustum).

## Phase 4 : Physique & Gameplay (C++ Pur)
*Objectif : Interactions et mouvements fluides.*

1. **Collisions BSP**
   - [ ] Glissement contre les murs (Slide Move) et détection de sol.
2. **Entités Dynamiques**
   - [ ] Support complet des modèles **MD2** avec interpolation.
3. **Systèmes de Gameplay ECS**
   - [ ] Implémentation des comportements (triggers, portes) en C++.

## Phase 5 : HUD & Interface de Jeu
*Objectif : Interaction utilisateur et feedback visuel.*

1. **HUD de Jeu**
   - [ ] Affichage dynamique des PV, munitions et inventaire.
   - [ ] Réticule de visée (Crosshair) et marqueurs d'impact.
2. **Système de Menus**
   - [ ] Menu principal et écran de pause.
   - [ ] Gestion des états de jeu (Start, Pause, Game Over).

## Phase 6 : Audio & Polissage
1. **Sons 3D** (Miniaudio) : Spatialisation et formats OGG/WAV.
2. **Esthétique Rétro** : Post-process de **Dithering** logiciel.
3. **Optimisations SIMD Finales**.
