# Cible Fonctionnelle - Retro-3D-Engine (SiliconShade)

Ce document détaille la vision à long terme du moteur, son utilisation par les développeurs, et la structure prévue pour les scènes et les assets.

## 1. Création de Maps (Niveaux)
Le moteur s'inspire de l'architecture de **Half-Life** et **Quake**.

- **Format :** Utilisation du format **BSP (Binary Space Partitioning)** pour les environnements statiques.
- **Outils de Création :** Les niveaux seront créés à l'aide d'éditeurs tiers comme **TrenchBroom** ou **GtkRadiant**, exportant au format `.map` puis compilés en `.bsp`.
- **Fonctionnalités BSP :**
    - Géométrie optimisée avec pré-calcul de visibilité (PVS - Potentially Visible Set).
    - Gestion des **Lightmaps** pour un éclairage statique réaliste et performant.
    - Données de collision intégrées pour une physique de mouvement fluide (glissement contre les murs).

## 2. Importation des Assets (Modèles 3D & Textures)
L'importation des assets se fera via des formats standards et légers.

- **Modèles Statiques :** Format **OBJ** (via `tinyobjloader`). Utilisé pour les petits objets de décor et props.
- **Modèles Animés :** Format **MD2** (utilisé dans Quake 2). Ce format est privilégié pour sa simplicité (interpolation d'images clés sur le CPU) et son esthétique rétro.
- **Textures :** Formats **PNG** ou **TGA**. Le moteur gère le sampling bilinéaire et le mipmapping (à implémenter) pour éviter le scintillement.
- **Gestionnaire de Ressources :** Un `ResourceManager` centralisé assurera que chaque asset n'est chargé qu'une seule fois en mémoire (cache).

## 3. Structure de la Scène
Le moteur utilise une architecture **ECS (Entity Component System)** pour une flexibilité maximale et des performances optimales (data locality).

- **World :** Conteneur principal de toutes les entités et systèmes.
- **Entities :** Identifiants simples représentant les objets du jeu (joueur, ennemis, lumières, props).
- **Components :** Structures de données pures attachées aux entités :
    - `Transform` : Position, rotation, échelle.
    - `MeshRenderer` : Référence à un modèle et une texture.
    - `Camera` : Paramètres de vue.
    - `Physics` : Vélocité, boîte de collision.
- **Systems :** Logique qui traite les entités possédant certains composants :
    - `RenderSystem` : Parcourt les `MeshRenderer` et `Transform` pour dessiner la scène.
    - `PhysicsSystem` : Met à jour les positions en fonction des collisions avec le BSP.

## 4. Utilisation Type du Moteur
Le développeur interagira principalement avec la classe `Engine` et le `World`.

```cpp
class MyGame : public Engine {
    void on_init() override {
        // Chargement de la map
        m_world.load_bsp("maps/e1m1.bsp");

        // Création d'une entité dynamique
        Entity player = m_world.create_entity();
        m_world.add_component<Transform>(player, {0, 0, 0});
        m_world.add_component<Camera>(player, ...);
    }

    void on_update(float dt) override {
        // La logique est principalement gérée par les systèmes ECS
        // mais des comportements spécifiques peuvent être ajoutés ici.
    }
};
```

## 5. Pipeline de Matériaux & Esthétique Rétro
Le moteur ne cherche pas le réalisme moderne, mais une esthétique "Software Renderer" marquée.
- **Rendu des Couleurs :** Utilisation du **True Color (32-bit)**. Le look rétro est obtenu via des shaders logiciels de post-process (Dithering, Tone-mapping) pour simuler une profondeur de couleur réduite si nécessaire.
- **Texture Mapping :** **Correction de perspective complète** (Perspective Correct Mapping) pour assurer une stabilité visuelle moderne, tout en conservant des textures basse résolution.
- **Effets :** Support du **Dithering** (tremblement) pour simuler une profondeur de couleur réduite et du **Mipmapping** logiciel pour limiter le scintillement des textures lointaines.

## 6. Audio & Immersion
Intégration de la bibliothèque `miniaudio`.
- **Audio 3D :** Gestion de la spatialisation basique (panoramique et atténuation par la distance).
- **Formats :** Priorité au format WAV pour les sons courts et OGG pour les musiques d'ambiance.

## 7. Physique & Interactions
Le système de physique est couplé aux données du BSP.
- **Collisions Statiques :** Utilisation des "Clip Nodes" du BSP pour une détection ultra-rapide.
- **Collisions Dynamiques :** Boîtes AABB pour les objets simples et cylindres/hulls pour les personnages.
- **Réponse :** Algorithme de "Slide Move" pour permettre de glisser le long des murs sans rester bloqué.

## 8. Logique de Jeu & Scripting
La logique de jeu est entièrement intégrée au moteur pour des performances maximales.
- **Logic Systems :** Les systèmes ECS gèrent la logique globale (IA, animations).
- **Implémentation :** Tout le code de gameplay est écrit en **C++ natif**. L'architecture ECS permet une modularité suffisante sans nécessiter de langage de script externe.

## 9. Contraintes Techniques & Performance
- **Résolution :** **Dynamique**. Le tampon de rendu s'adapte à la taille de la fenêtre utilisateur, permettant un affichage net sur les écrans modernes tout en conservant un pipeline de rastérisation logiciel.
- **Budget :** Cible de ~10 000 à 20 000 triangles affichés simultanément à 60 FPS sur un processeur moderne (multi-threadé).
