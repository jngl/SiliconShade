# Retro-3D-Engine

Un moteur de jeu pour FPS rétro (style années 90) développé en C++20, utilisant un rastériseur logiciel (software renderer) haute performance avec optimisations SIMD.

## 🚀 Caractéristiques Techniques

- **Langage :** C++20.
- **Rendu :** Software Renderer (pas de GPU requis).
- **Optimisations :** 
    - Rasterisation multithreadée.
    - Calculs intensifs optimisés avec **AVX2 (SIMD)**.
- **Dépendances :** 
    - **SDL2** pour la fenêtre, les événements et l'affichage du framebuffer.
    - **stb_image** pour le chargement des textures.
    - **tinyobjloader** pour le chargement des modèles 3D.
- **Architecture :** Conçu pour le "Zéro Allocation" dans la boucle de rendu.

## 🛠️ Installation & Build

### Prérequis

- Un compilateur supportant le C++20 (GCC, Clang ou MSVC).
- Un processeur supportant les instructions AVX2.
- **SDL2** installé sur votre système.

### Compilation

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## 🧪 Qualité & Tests

Le projet suit une approche **"Quality-First"**. 

- **Sanitizers :** Utilisation systématique de `ASan`, `TSan` et `UBSan`.
- **Tests :** Tests unitaires pour les mathématiques, la mémoire et le threadpool.
- **Validation SIMD :** Tests de parité entre les versions scalaires et AVX2.

Pour lancer les tests :
```bash
# Dans le dossier build
./MathTests
./MemoryTests
./ThreadTests
```

## 🗺️ Roadmap

Le développement est structuré en 5 phases majeures (voir `PLAN.md` pour les détails) :
1. **Phase 1 :** Fondation & Infrastructure de Qualité (En cours).
2. **Phase 2 :** Rendu, Caméra & Validation Visuelle.
3. **Phase 3 :** Monde (BSP), Collisions & Robustesse.
4. **Phase 4 :** Entités & Gameplay.
5. **Phase 5 :** Audio & Finitions.

## 📜 Licence

Ce projet est sous licence MIT. Voir le fichier `LICENSE` (à venir) pour plus de détails.
