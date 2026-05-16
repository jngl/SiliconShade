#include <retro3d/core/Engine.h>
#include <retro3d/core/Basic.h>
#include <retro3d/render/Draw.h>
#include <iostream>

/**
 * @brief The Crypt Crawler - Démo technique du moteur Retro-3D
 */
class CryptCrawler : public Engine {
public:
    CryptCrawler(Arena& arena) : Engine(arena) {
        // Initialisation prévue ici (chargement des assets, configuration ECS)
    }

protected:
    void on_init() override {
        std::cout << "Crypt Crawler Initialized!" << std::endl;
    }

    void on_update(float dt) override {
        // Logique de jeu (entrées utilisateur, mouvement caméra)
        if (m_input.is_key_down(Input::K_ESCAPE)) {
            // Quitter ou menu
        }
    }

    void on_render() override {
        // Effacement de l'écran (Gris très foncé)
        draw_clear_buffer(m_app.color_buffer, 0xFF101010);
        draw_clear_depth_buffer(m_app.depth_buffer, 0);

        // TODO: Rendu du monde et des entités
    }

private:
    // Variables de jeu (caméra, statistiques, etc.)
};

int main() {
    // Allocation d'une zone mémoire de 1 Go pour l'application
    SystemMemory app_memory(GIGABYTES(1));
    Arena app_arena(app_memory);

    try {
        CryptCrawler game(app_arena);
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
