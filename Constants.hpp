#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

namespace CandyCrush {

// Window Dimensions
constexpr unsigned int WINDOW_WIDTH = 1400;
constexpr unsigned int WINDOW_HEIGHT = 900;
const std::string WINDOW_TITLE = "Candy Crush Saga - macOS Premium";

// Grid Configurations
constexpr int GRID_ROWS = 9;
constexpr int GRID_COLS = 9;
constexpr float CELL_SIZE = 70.0f;
constexpr float CANDY_SIZE = 50.0f;
constexpr float GRID_MID_X = 400.0f;
constexpr float GRID_MID_Y = 150.0f;

// Textures configurations
constexpr int TOTAL_CANDY_TYPES = 6;
constexpr int SPECIAL_CANDY_TYPES = 2;

// Match criteria
constexpr int MIN_MATCH_COUNT = 3;

// Game states
enum class GameState {
    MainMenu,
    Gameplay,
    PauseMenu,
    LoadScreen,
    HighScores,
    GameOver
};

// Level structure
struct LevelConfig {
    int targetScore;
    int movesAllowed;
    float timeLimit; // in seconds
    std::string bgPath;
};

// Level presets (5 levels based on background files in project)
const std::vector<LevelConfig> LEVEL_PRESETS = {
    { 500,  30, 60.0f, "images/background/background.png" },    // Level 1
    { 1200, 25, 50.0f, "images/background/background1.png" },   // Level 2
    { 2200, 20, 40.0f, "images/background/background5.png" },   // Level 3
    { 3500, 15, 30.0f, "images/background/background9.png" },   // Level 4
    { 5000, 10, 25.0f, "images/background/background11.png" }   // Level 5 (Endless / Victory Challenge)
};

// High score persistence file
const std::string HIGH_SCORE_FILE = "highscore.txt";

// Save slot files
const std::string SAVE_SLOTS[3] = {
    "save_slot1.txt",
    "save_slot2.txt",
    "save_slot3.txt"
};
const std::string AUTO_SAVE_FILE = "autosave.txt";

// Color Palette
const sf::Color COLOR_TEXT_DARK(50, 50, 50);
const sf::Color COLOR_TEXT_LIGHT(245, 245, 245);
const sf::Color COLOR_PRIMARY(232, 62, 140);     // Pinkish red
const sf::Color COLOR_SECONDARY(111, 66, 193);   // Purple
const sf::Color COLOR_ACCENT(40, 167, 69);       // Green
const sf::Color COLOR_BG_GLASS(255, 255, 255, 180); // Semi-transparent white
const sf::Color COLOR_BG_GLASS_DARK(20, 20, 20, 200);

// Buttons and UI coordinates
struct ButtonConfig {
    sf::Vector2f position;
    sf::Vector2f size;
    std::string text;
    sf::Color baseColor;
    sf::Color hoverColor;
};

} // namespace CandyCrush
