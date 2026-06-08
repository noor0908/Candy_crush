#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <array>
#include <string>
#include "Constants.hpp"
#include "Particle.hpp"

namespace CandyCrush {

class Game {
public:
    Game();
    ~Game();

    // Runs the main game loop
    void run();

private:
    // Core Game Loops
    void processEvents();
    void update(float dt);
    void render();

    // Initialization methods
    void loadAssets();
    void initAudio(); // Synthesizes match, special match, and game over sounds programmatically
    void initGrid();
    void resetGame();

    // Event & Input handlers
    void handleInput(const sf::Event& event);
    void handleMainMenuClicks(sf::Vector2f mousePos);
    void handlePauseMenuClicks(sf::Vector2f mousePos);
    void handleSaveLoadClicks(sf::Vector2f mousePos, bool isSaving);
    void handleHighScoreClicks(sf::Vector2f mousePos);
    void handleGameOverClicks(sf::Vector2f mousePos);
    void handleGameplayClicks(sf::Vector2f mousePos);
    void selectGridTile(sf::Vector2i gridPos);

    // Board mechanics
    bool checkMatches(); // Scans grid for matches and flags them
    void applyMatchClearance(); // Resolves flagged matches, updates score/combos, triggers sounds/particles
    bool refillGrid(); // Slides candies down, spawns new ones, returns true if anything shifted
    bool isAnyMovePossible(); // Check if there are possible matches on the board
    bool checkMatchSimulation(int r1, int c1, int r2, int c2); // Simulate swap
    void triggerSpecialCandy(int r, int c, int specialType);
    bool checkMatchesExist() const; // Check if there are active matches without applying them

    // Hints & AI
    void runHintSystem();

    // Undo System
    void saveUndoState();
    void restoreUndoState();

    // Save and Load System
    void loadGame(const std::string& filename);
    void saveGame(const std::string& filename) const;
    void triggerAutosave();

    // High Scores System
    void loadHighScore();
    void updateHighScoreList();
    void saveHighScores();

    void checkLevelProgression();
    void updateVisualPositions(float dt);
    void spawnParticleBurst(sf::Vector2f pos, int candyType);
    void spawnFloatingText(const std::string& text, sf::Vector2f position, sf::Color color);
    void drawGlassPanel(sf::Vector2f pos, sf::Vector2f size, sf::Color outlineColor = sf::Color::White);
    void drawButton(sf::Vector2f pos, sf::Vector2f size, const std::string& text, sf::Color baseColor, sf::Vector2f mousePos);
    bool isMouseOverButton(sf::Vector2f mousePos, sf::Vector2f pos, sf::Vector2f size) const;
    bool isAnimating() const; // Check if any candies are currently sliding

    // SFML Core elements
    sf::RenderWindow m_window;
    sf::Font m_font;
    std::vector<sf::Texture> m_levelBgTextures;
    std::array<sf::Texture, TOTAL_CANDY_TYPES> m_candyTextures;
    std::array<sf::Texture, SPECIAL_CANDY_TYPES> m_specialTextures;

    // Sprite instances
    sf::Sprite m_bgSprite;
    sf::Sprite m_candySprite;

    // Audio elements
    sf::Music m_bgMusic;
    sf::SoundBuffer m_matchSoundBuffer;
    sf::SoundBuffer m_specialSoundBuffer;
    sf::SoundBuffer m_gameOverSoundBuffer;
    sf::Sound m_soundEffect;
    bool m_isMuted;

    // Grid states
    std::array<std::array<int, GRID_COLS>, GRID_ROWS> m_grid;
    std::array<std::array<sf::Vector2f, GRID_COLS>, GRID_ROWS> m_visualPositions;
    std::array<std::array<bool, GRID_COLS>, GRID_ROWS> m_assignedToBurst;

    // Game variables
    GameState m_state;
    int m_score;
    int m_movesLeft;
    int m_currentLevel;
    float m_levelTimer; // Countdown timer
    int m_highScore;
    std::vector<int> m_highScoresList; // Store top scores
    int m_comboMultiplier;
    int m_movesSinceAutosave;

    // Selection indicators
    sf::Vector2i m_selectedTile; // (-1, -1) if nothing selected
    sf::Vector2i m_suggestedMoveStart; // For hint highlight
    sf::Vector2i m_suggestedMoveEnd;
    bool m_hasHintActive;
    sf::Clock m_hintTimer;

    // Undo Buffer
    std::array<std::array<int, GRID_COLS>, GRID_ROWS> m_undoGrid;
    int m_undoScore;
    int m_undoMovesLeft;
    int m_undoLevel;
    float m_undoTimer;
    bool m_undoAvailable;
    bool m_undoUsedThisLevel;

    // Visual lists
    std::vector<Particle> m_particles;
    std::vector<FloatingText> m_floatingTexts;

    // Timer & frame helpers
    sf::Clock m_clock;
    bool m_boardSettling; // True if matches are cascade processing
    float m_settlingDelayTimer;
};

} // namespace CandyCrush
