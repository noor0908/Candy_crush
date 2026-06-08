#include "Game.hpp"
#include "Constants.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <ctime>

namespace CandyCrush {

// Helper to check if file exists
bool fileExists(const std::string& filename) {
    std::ifstream f(filename.c_str());
    return f.good();
}

Game::Game()
    : m_window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), WINDOW_TITLE, sf::Style::Close)
    , m_isMuted(false)
    , m_state(GameState::MainMenu)
    , m_score(0)
    , m_movesLeft(30)
    , m_currentLevel(0)
    , m_levelTimer(60.0f)
    , m_highScore(0)
    , m_comboMultiplier(1)
    , m_movesSinceAutosave(0)
    , m_selectedTile(-1, -1)
    , m_suggestedMoveStart(-1, -1)
    , m_suggestedMoveEnd(-1, -1)
    , m_hasHintActive(false)
    , m_undoScore(0)
    , m_undoMovesLeft(30)
    , m_undoLevel(0)
    , m_undoTimer(60.0f)
    , m_undoAvailable(false)
    , m_undoUsedThisLevel(false)
    , m_boardSettling(false)
    , m_settlingDelayTimer(0.0f)
{
    m_window.setFramerateLimit(60);
    
    // Set initial grid elements to -1 (empty)
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            m_grid[r][c] = -1;
            m_visualPositions[r][c] = sf::Vector2f(GRID_MID_X + c * CELL_SIZE, GRID_MID_Y + r * CELL_SIZE);
            m_assignedToBurst[r][c] = false;
        }
    }

    loadAssets();
    initAudio();
    loadHighScore();
}

Game::~Game() {
    saveHighScores();
}

void Game::run() {
    m_clock.restart();
    while (m_window.isOpen()) {
        float dt = m_clock.restart().asSeconds();
        
        // Cap dt to prevent massive jumps during lags
        if (dt > 0.1f) dt = 0.1f;

        processEvents();
        update(dt);
        render();
    }
}

void Game::processEvents() {
    sf::Event event;
    while (m_window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            m_window.close();
        } else {
            handleInput(event);
        }
    }
}

void Game::loadAssets() {
    // Load font
    if (!m_font.loadFromFile("images/text/Lato-Black.ttf")) {
        std::cerr << "CRITICAL ERROR: Failed to load Lato-Black.ttf\n";
    }

    // Load background textures (one for each level)
    std::string bgPaths[5] = {
        "images/background/background.png",
        "images/background/background1.png",
        "images/background/background5.png",
        "images/background/background9.png",
        "images/background/background11.png"
    };

    m_levelBgTextures.resize(5);
    for (int i = 0; i < 5; ++i) {
        if (!m_levelBgTextures[i].loadFromFile(bgPaths[i])) {
            std::cerr << "WARNING: Failed to load background: " << bgPaths[i] << ", using generic\n";
        }
    }

    // Load standard candies textures
    for (int i = 0; i < TOTAL_CANDY_TYPES; ++i) {
        std::string path = "images/candies/" + std::to_string(i + 1) + ".png";
        if (!m_candyTextures[i].loadFromFile(path)) {
            std::cerr << "CRITICAL ERROR: Failed to load candy texture: " << path << "\n";
        }
    }

    // Load special candies textures
    for (int i = 0; i < SPECIAL_CANDY_TYPES; ++i) {
        std::string path = "images/Special1/" + std::to_string(i + 1) + ".png";
        if (!m_specialTextures[i].loadFromFile(path)) {
            std::cerr << "CRITICAL ERROR: Failed to load special texture: " << path << "\n";
        }
    }
}

void Game::initAudio() {
    // 1. Procedural Match Sound (C5: 523.25Hz and E5: 659.25Hz)
    {
        unsigned int sampleRate = 44100;
        float duration = 0.18f;
        std::uint64_t sampleCount = static_cast<std::uint64_t>(sampleRate * duration);
        std::vector<std::int16_t> samples(sampleCount);
        for (std::uint64_t i = 0; i < sampleCount; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double envelope = 1.0 - (static_cast<double>(i) / sampleCount); // Linear decay
            double wave = 0.5 * (std::sin(2.0 * M_PI * 523.25 * t) + std::sin(2.0 * M_PI * 659.25 * t));
            samples[i] = static_cast<std::int16_t>(wave * 32767.0 * envelope * 0.3);
        }
        m_matchSoundBuffer.loadFromSamples(samples.data(), samples.size(), 1, sampleRate);
    }

    // 2. Procedural Special Activation (Sweep pitch up from 400Hz to 1300Hz)
    {
        unsigned int sampleRate = 44100;
        float duration = 0.35f;
        std::uint64_t sampleCount = static_cast<std::uint64_t>(sampleRate * duration);
        std::vector<std::int16_t> samples(sampleCount);
        for (std::uint64_t i = 0; i < sampleCount; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double envelope = 1.0 - (static_cast<double>(i) / sampleCount);
            double freq = 400.0 + (1300.0 - 400.0) * (t / duration);
            double wave = std::sin(2.0 * M_PI * freq * t);
            samples[i] = static_cast<std::int16_t>(wave * 32767.0 * envelope * 0.3);
        }
        m_specialSoundBuffer.loadFromSamples(samples.data(), samples.size(), 1, sampleRate);
    }

    // 3. Procedural Game Over (Sweep pitch down from 600Hz to 150Hz)
    {
        unsigned int sampleRate = 44100;
        float duration = 0.7f;
        std::uint64_t sampleCount = static_cast<std::uint64_t>(sampleRate * duration);
        std::vector<std::int16_t> samples(sampleCount);
        for (std::uint64_t i = 0; i < sampleCount; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double envelope = 1.0 - (static_cast<double>(i) / sampleCount);
            double freq = 600.0 + (150.0 - 600.0) * (t / duration);
            double wave = std::sin(2.0 * M_PI * freq * t);
            samples[i] = static_cast<std::int16_t>(wave * 32767.0 * envelope * 0.45);
        }
        m_gameOverSoundBuffer.loadFromSamples(samples.data(), samples.size(), 1, sampleRate);
    }

    // Load and play background music
    if (m_bgMusic.openFromFile("candycrush.ogg")) {
        m_bgMusic.setLoop(true);
        m_bgMusic.setVolume(35.0f);
        m_bgMusic.play();
    } else {
        std::cerr << "WARNING: Failed to load candycrush.ogg, running in silent mode\n";
    }
}

void Game::initGrid() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    // Keep regenerating grid until a valid configuration with possible moves is found
    do {
        for (int r = 0; r < GRID_ROWS; ++r) {
            for (int c = 0; c < GRID_COLS; ++c) {
                do {
                    m_grid[r][c] = std::rand() % TOTAL_CANDY_TYPES;
                } while (
                    (c >= 2 && m_grid[r][c] == m_grid[r][c - 1] && m_grid[r][c] == m_grid[r][c - 2]) ||
                    (r >= 2 && m_grid[r][c] == m_grid[r - 1][c] && m_grid[r][c] == m_grid[r - 2][c])
                );
                
                // Position visual candy instantly for startup
                m_visualPositions[r][c] = sf::Vector2f(
                    GRID_MID_X + c * CELL_SIZE,
                    GRID_MID_Y + r * CELL_SIZE
                );
                m_assignedToBurst[r][c] = false;
            }
        }
    } while (!isAnyMovePossible());

    m_boardSettling = false;
}

void Game::resetGame() {
    m_score = 0;
    m_currentLevel = 0;
    m_movesLeft = LEVEL_PRESETS[0].movesAllowed;
    m_levelTimer = LEVEL_PRESETS[0].timeLimit;
    m_comboMultiplier = 1;
    m_movesSinceAutosave = 0;
    m_selectedTile = sf::Vector2i(-1, -1);
    m_hasHintActive = false;
    m_undoAvailable = false;
    m_undoUsedThisLevel = false;
    
    initGrid();
}

void Game::update(float dt) {
    // 1. Update visual assets
    updateVisualPositions(dt);

    // 2. Update active effects/texts
    for (auto it = m_particles.begin(); it != m_particles.end();) {
        it->update(dt);
        if (it->lifetime <= 0) it = m_particles.erase(it);
        else ++it;
    }

    for (auto it = m_floatingTexts.begin(); it != m_floatingTexts.end();) {
        it->update(dt);
        if (it->lifetime <= 0) it = m_floatingTexts.erase(it);
        else ++it;
    }

    // 3. Update gameplay parameters
    if (m_state == GameState::Gameplay) {
        // Decrease timer unless board is actively cascading/settling
        if (!m_boardSettling && !isAnimating()) {
            m_levelTimer -= dt;
            if (m_levelTimer <= 0.0f) {
                m_levelTimer = 0.0f;
                m_state = GameState::GameOver;
                updateHighScoreList();
                if (!m_isMuted) {
                    m_soundEffect.setBuffer(m_gameOverSoundBuffer);
                    m_soundEffect.play();
                }
            }
        }

        // Hint timer expiry
        if (m_hasHintActive && m_hintTimer.getElapsedTime().asSeconds() > 3.0f) {
            m_hasHintActive = false;
        }

        // Gameplay state machine: Board Cascade & Refilling
        if (!isAnimating()) {
            if (m_boardSettling) {
                m_settlingDelayTimer += dt;
                if (m_settlingDelayTimer > 0.15f) { // Slight pause between clears for clarity
                    m_settlingDelayTimer = 0.0f;
                    
                    bool hasMatches = checkMatches();
                    if (hasMatches) {
                        applyMatchClearance();
                        refillGrid();
                        m_comboMultiplier++; // Cascade multiplier
                    } else {
                        // All matches processed and board settled
                        m_boardSettling = false;
                        m_comboMultiplier = 1;

                        // Check if board has valid options, else auto-shuffle
                        if (!isAnyMovePossible()) {
                            spawnFloatingText("NO MOVES LEFT! SHUFFLING...", sf::Vector2f(GRID_MID_X + 100, GRID_MID_Y + 280), sf::Color::Yellow);
                            initGrid();
                        }
                    }
                }
            }
        }
    }
}

void Game::updateVisualPositions(float dt) {
    constexpr float LERP_SPEED = 12.0f;
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            if (m_grid[r][c] != -1) {
                float targetX = GRID_MID_X + c * CELL_SIZE;
                float targetY = GRID_MID_Y + r * CELL_SIZE;

                float dx = targetX - m_visualPositions[r][c].x;
                float dy = targetY - m_visualPositions[r][c].y;

                if (std::abs(dx) > 0.5f) {
                    m_visualPositions[r][c].x += dx * LERP_SPEED * dt;
                } else {
                    m_visualPositions[r][c].x = targetX;
                }

                if (std::abs(dy) > 0.5f) {
                    m_visualPositions[r][c].y += dy * LERP_SPEED * dt;
                } else {
                    m_visualPositions[r][c].y = targetY;
                }
            }
        }
    }
}

bool Game::isAnimating() const {
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            if (m_grid[r][c] != -1) {
                float targetX = GRID_MID_X + c * CELL_SIZE;
                float targetY = GRID_MID_Y + r * CELL_SIZE;
                float dx = targetX - m_visualPositions[r][c].x;
                float dy = targetY - m_visualPositions[r][c].y;
                if (std::sqrt(dx*dx + dy*dy) > 1.5f) {
                    return true;
                }
            }
        }
    }
    return false;
}

void Game::handleInput(const sf::Event& event) {
    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
            if (m_state == GameState::Gameplay) {
                m_state = GameState::PauseMenu;
            } else if (m_state == GameState::PauseMenu) {
                m_state = GameState::Gameplay;
            }
        }
    }

    if (event.type == sf::Event::MouseButtonPressed) {
        if (event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2f mousePos(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
            
            switch (m_state) {
                case GameState::MainMenu:
                    handleMainMenuClicks(mousePos);
                    break;
                case GameState::PauseMenu:
                    handlePauseMenuClicks(mousePos);
                    break;
                case GameState::LoadScreen:
                    handleSaveLoadClicks(mousePos, false); // Load screen slot clicks
                    break;
                case GameState::HighScores:
                    handleHighScoreClicks(mousePos);
                    break;
                case GameState::GameOver:
                    handleGameOverClicks(mousePos);
                    break;
                case GameState::Gameplay:
                    handleGameplayClicks(mousePos);
                    break;
            }
        }
    }
}

void Game::handleMainMenuClicks(sf::Vector2f mousePos) {
    // Play Button
    if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 150, 300), sf::Vector2f(300, 80))) {
        resetGame();
        m_state = GameState::Gameplay;
    }
    // Load Game Button
    else if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 150, 420), sf::Vector2f(300, 80))) {
        m_state = GameState::LoadScreen;
    }
    // High Scores Button
    else if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 150, 540), sf::Vector2f(300, 80))) {
        m_state = GameState::HighScores;
    }
    // Quit Button
    else if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 150, 660), sf::Vector2f(300, 80))) {
        m_window.close();
    }
}

void Game::handlePauseMenuClicks(sf::Vector2f mousePos) {
    // Resume
    if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 150, 300), sf::Vector2f(300, 80))) {
        m_state = GameState::Gameplay;
    }
    // Save Game
    else if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 150, 420), sf::Vector2f(300, 80))) {
        // Switch load screen to Save Mode (we reuse the load screen and handle slot saves)
        m_state = GameState::LoadScreen; 
    }
    // Quit to Main Menu
    else if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 150, 540), sf::Vector2f(300, 80))) {
        m_state = GameState::MainMenu;
    }
}

void Game::handleSaveLoadClicks(sf::Vector2f mousePos, bool) {
    // Back Button
    if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 200, 680), sf::Vector2f(400, 70))) {
        // If we came from pause menu (saving), return to pause. Otherwise return to main menu
        if (m_movesLeft > 0 && m_levelTimer > 0.0f && m_score > 0) {
            m_state = GameState::PauseMenu;
        } else {
            m_state = GameState::MainMenu;
        }
        return;
    }

    // We check if we are saving (came from gameplay/pause) or loading (came from main menu)
    bool isSavingMode = (m_movesLeft > 0 && m_levelTimer > 0.0f && m_score > 0 && m_state == GameState::LoadScreen && m_grid[0][0] != -1);

    // Slot 1
    if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 200, 250), sf::Vector2f(400, 70))) {
        if (isSavingMode) {
            saveGame(SAVE_SLOTS[0]);
            spawnFloatingText("SLOT 1 SAVED!", sf::Vector2f(WINDOW_WIDTH/2 - 100, 200), sf::Color::Green);
            m_state = GameState::PauseMenu;
        } else {
            if (fileExists(SAVE_SLOTS[0])) {
                loadGame(SAVE_SLOTS[0]);
                m_state = GameState::Gameplay;
            }
        }
    }
    // Slot 2
    else if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 200, 350), sf::Vector2f(400, 70))) {
        if (isSavingMode) {
            saveGame(SAVE_SLOTS[1]);
            spawnFloatingText("SLOT 2 SAVED!", sf::Vector2f(WINDOW_WIDTH/2 - 100, 200), sf::Color::Green);
            m_state = GameState::PauseMenu;
        } else {
            if (fileExists(SAVE_SLOTS[1])) {
                loadGame(SAVE_SLOTS[1]);
                m_state = GameState::Gameplay;
            }
        }
    }
    // Slot 3
    else if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 200, 450), sf::Vector2f(400, 70))) {
        if (isSavingMode) {
            saveGame(SAVE_SLOTS[2]);
            spawnFloatingText("SLOT 3 SAVED!", sf::Vector2f(WINDOW_WIDTH/2 - 100, 200), sf::Color::Green);
            m_state = GameState::PauseMenu;
        } else {
            if (fileExists(SAVE_SLOTS[2])) {
                loadGame(SAVE_SLOTS[2]);
                m_state = GameState::Gameplay;
            }
        }
    }
    // Auto-Save Slot (Read-only loading from Menu)
    else if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 200, 550), sf::Vector2f(400, 70))) {
        if (!isSavingMode && fileExists(AUTO_SAVE_FILE)) {
            loadGame(AUTO_SAVE_FILE);
            m_state = GameState::Gameplay;
        }
    }
}

void Game::handleHighScoreClicks(sf::Vector2f mousePos) {
    if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 150, 680), sf::Vector2f(300, 80))) {
        m_state = GameState::MainMenu;
    }
}

void Game::handleGameOverClicks(sf::Vector2f mousePos) {
    if (isMouseOverButton(mousePos, sf::Vector2f(WINDOW_WIDTH/2 - 200, 500), sf::Vector2f(400, 80))) {
        m_state = GameState::MainMenu;
    }
}

void Game::handleGameplayClicks(sf::Vector2f mousePos) {
    // 1. Check UI Side Buttons
    
    // Pause button
    if (isMouseOverButton(mousePos, sf::Vector2f(30, 30), sf::Vector2f(100, 50))) {
        m_state = GameState::PauseMenu;
        return;
    }

    // Hint button
    if (isMouseOverButton(mousePos, sf::Vector2f(1230, 30), sf::Vector2f(140, 50))) {
        runHintSystem();
        return;
    }

    // Undo button
    if (isMouseOverButton(mousePos, sf::Vector2f(1230, 100), sf::Vector2f(140, 50))) {
        restoreUndoState();
        return;
    }

    // Mute/Unmute button
    if (isMouseOverButton(mousePos, sf::Vector2f(1230, 170), sf::Vector2f(140, 50))) {
        m_isMuted = !m_isMuted;
        if (m_isMuted) {
            m_bgMusic.setVolume(0.0f);
        } else {
            m_bgMusic.setVolume(35.0f);
        }
        return;
    }

    // Save Game Button
    if (isMouseOverButton(mousePos, sf::Vector2f(1230, 240), sf::Vector2f(140, 50))) {
        m_state = GameState::LoadScreen; // Slots selection screen
        return;
    }

    // 2. Check Grid Clicks
    if (mousePos.x >= GRID_MID_X && mousePos.x < GRID_MID_X + GRID_COLS * CELL_SIZE &&
        mousePos.y >= GRID_MID_Y && mousePos.y < GRID_MID_Y + GRID_ROWS * CELL_SIZE)
    {
        int col = static_cast<int>((mousePos.x - GRID_MID_X) / CELL_SIZE);
        int row = static_cast<int>((mousePos.y - GRID_MID_Y) / CELL_SIZE);

        if (row >= 0 && row < GRID_ROWS && col >= 0 && col < GRID_COLS) {
            selectGridTile(sf::Vector2i(row, col));
        }
    }
}

void Game::selectGridTile(sf::Vector2i gridPos) {
    if (m_boardSettling || isAnimating()) return;

    int r = gridPos.x;
    int c = gridPos.y;

    // Direct click activation of special candies (values 6 or 7)
    if (m_grid[r][c] >= 6) {
        saveUndoState();
        m_hasHintActive = false;
        
        // Trigger activation
        int specialType = m_grid[r][c];
        m_grid[r][c] = -1; // Clear activator spot
        
        if (!m_isMuted) {
            m_soundEffect.setBuffer(m_specialSoundBuffer);
            m_soundEffect.play();
        }
        
        triggerSpecialCandy(r, c, specialType);
        applyMatchClearance();
        refillGrid();
        
        m_movesLeft--;
        m_movesSinceAutosave++;
        triggerAutosave();

        m_boardSettling = true;
        m_selectedTile = sf::Vector2i(-1, -1);
        return;
    }

    // Standard Match-3 swapping logic
    if (m_selectedTile == sf::Vector2i(-1, -1)) {
        m_selectedTile = gridPos;
    } else {
        // If clicking adjacent tile, perform swap check
        int dr = std::abs(m_selectedTile.x - r);
        int dc = std::abs(m_selectedTile.y - c);
        
        if ((dr == 1 && dc == 0) || (dr == 0 && dc == 1)) {
            // Valid neighbor click, proceed to swap check
            saveUndoState();
            
            // Swap in matrix
            std::swap(m_grid[r][c], m_grid[m_selectedTile.x][m_selectedTile.y]);
            
            bool matchCreated = checkMatches();
            if (matchCreated) {
                // Keep the swap and process clearances
                if (!m_isMuted) {
                    m_soundEffect.setBuffer(m_matchSoundBuffer);
                    m_soundEffect.play();
                }
                
                applyMatchClearance();
                refillGrid();
                
                m_movesLeft--;
                m_movesSinceAutosave++;
                triggerAutosave();
                
                m_boardSettling = true;
                m_selectedTile = sf::Vector2i(-1, -1);
                m_hasHintActive = false;
            } else {
                // No match, swap back instantly
                std::swap(m_grid[r][c], m_grid[m_selectedTile.x][m_selectedTile.y]);
                
                // Select newly clicked tile instead of keeping the old selection
                m_selectedTile = gridPos;
            }
        } else {
            // The clicked candy is not adjacent (diagonal or further away).
            // Reject the swap, and treat this second click as a new first selection.
            m_selectedTile = gridPos;
        }
    }
}

void Game::triggerSpecialCandy(int r, int c, int specialType) {
    m_assignedToBurst[r][c] = true;
    
    if (specialType == 6) { // Clear entire row and column
        for (int i = 0; i < GRID_COLS; ++i) {
            if (m_grid[r][i] != -1 && !m_assignedToBurst[r][i]) {
                m_assignedToBurst[r][i] = true;
                if (m_grid[r][i] >= 6) {
                    triggerSpecialCandy(r, i, m_grid[r][i]); // Chain reaction
                }
            }
        }
        for (int i = 0; i < GRID_ROWS; ++i) {
            if (m_grid[i][c] != -1 && !m_assignedToBurst[i][c]) {
                m_assignedToBurst[i][c] = true;
                if (m_grid[i][c] >= 6) {
                    triggerSpecialCandy(i, c, m_grid[i][c]); // Chain reaction
                }
            }
        }
    } else if (specialType == 7) { // 3x3 Explode
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                int nr = r + dr;
                int nc = c + dc;
                if (nr >= 0 && nr < GRID_ROWS && nc >= 0 && nc < GRID_COLS) {
                    if (m_grid[nr][nc] != -1 && !m_assignedToBurst[nr][nc]) {
                        m_assignedToBurst[nr][nc] = true;
                        if (m_grid[nr][nc] >= 6) {
                            triggerSpecialCandy(nr, nc, m_grid[nr][nc]); // Chain reaction
                        }
                    }
                }
            }
        }
    }
}

bool Game::checkMatches() {
    bool matchFound = false;

    // Reset burst flags
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            m_assignedToBurst[r][c] = false;
        }
    }

    // 1. Scan Horizontal Matches
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS - 2; ++c) {
            int val = m_grid[r][c];
            if (val != -1 && val == m_grid[r][c + 1] && val == m_grid[r][c + 2]) {
                m_assignedToBurst[r][c] = true;
                m_assignedToBurst[r][c + 1] = true;
                m_assignedToBurst[r][c + 2] = true;
                matchFound = true;
            }
        }
    }

    // 2. Scan Vertical Matches
    for (int r = 0; r < GRID_ROWS - 2; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            int val = m_grid[r][c];
            if (val != -1 && val == m_grid[r + 1][c] && val == m_grid[r + 2][c]) {
                m_assignedToBurst[r][c] = true;
                m_assignedToBurst[r + 1][c] = true;
                m_assignedToBurst[r + 2][c] = true;
                matchFound = true;
            }
        }
    }

    // (Diagonal matches are intentionally disabled to match Candy Crush mechanics)

    return matchFound;
}

bool Game::checkMatchesExist() const {
    // Scan horizontal
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS - 2; ++c) {
            int val = m_grid[r][c];
            if (val != -1 && val == m_grid[r][c + 1] && val == m_grid[r][c + 2]) return true;
        }
    }
    // Scan vertical
    for (int r = 0; r < GRID_ROWS - 2; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            int val = m_grid[r][c];
            if (val != -1 && val == m_grid[r + 1][c] && val == m_grid[r + 2][c]) return true;
        }
    }
    // (Diagonal match simulation check is disabled)
    return false;
}

void Game::applyMatchClearance() {
    // Identify 4-match or 5-match to place special candies
    // Scan horizontal groups
    for (int r = 0; r < GRID_ROWS; ++r) {
        int matchLen = 0;
        int startCol = -1;
        int type = -1;
        for (int c = 0; c < GRID_COLS; ++c) {
            if (m_assignedToBurst[r][c] && m_grid[r][c] < 6) {
                if (matchLen == 0) {
                    matchLen = 1;
                    startCol = c;
                    type = m_grid[r][c];
                } else if (m_grid[r][c] == type) {
                    matchLen++;
                } else {
                    if (matchLen >= 4) {
                        int midC = startCol + matchLen / 2;
                        m_grid[r][midC] = (matchLen == 4) ? 6 : 7;
                        m_assignedToBurst[r][midC] = false; // Spare from burst to place special
                    }
                    matchLen = 1;
                    startCol = c;
                    type = m_grid[r][c];
                }
            } else {
                if (matchLen >= 4) {
                    int midC = startCol + matchLen / 2;
                    m_grid[r][midC] = (matchLen == 4) ? 6 : 7;
                    m_assignedToBurst[r][midC] = false;
                }
                matchLen = 0;
            }
        }
        if (matchLen >= 4) {
            int midC = startCol + matchLen / 2;
            m_grid[r][midC] = (matchLen == 4) ? 6 : 7;
            m_assignedToBurst[r][midC] = false;
        }
    }

    // Scan vertical groups
    for (int c = 0; c < GRID_COLS; ++c) {
        int matchLen = 0;
        int startRow = -1;
        int type = -1;
        for (int r = 0; r < GRID_ROWS; ++r) {
            if (m_assignedToBurst[r][c] && m_grid[r][c] < 6) {
                if (matchLen == 0) {
                    matchLen = 1;
                    startRow = r;
                    type = m_grid[r][c];
                } else if (m_grid[r][c] == type) {
                    matchLen++;
                } else {
                    if (matchLen >= 4) {
                        int midR = startRow + matchLen / 2;
                        m_grid[midR][c] = (matchLen == 4) ? 6 : 7;
                        m_assignedToBurst[midR][c] = false;
                    }
                    matchLen = 1;
                    startRow = r;
                    type = m_grid[r][c];
                }
            } else {
                if (matchLen >= 4) {
                    int midR = startRow + matchLen / 2;
                    m_grid[midR][c] = (matchLen == 4) ? 6 : 7;
                    m_assignedToBurst[midR][c] = false;
                }
                matchLen = 0;
            }
        }
        if (matchLen >= 4) {
            int midR = startRow + matchLen / 2;
            m_grid[midR][c] = (matchLen == 4) ? 6 : 7;
            m_assignedToBurst[midR][c] = false;
        }
    }

    // Trigger chains on flagged specials
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            if (m_assignedToBurst[r][c] && m_grid[r][c] >= 6) {
                triggerSpecialCandy(r, c, m_grid[r][c]);
            }
        }
    }

    // Perform burst, spawn score floating text, and spawn particles
    int clearedCount = 0;
    sf::Vector2f matchCenter(0.f, 0.f);

    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            if (m_assignedToBurst[r][c]) {
                int type = m_grid[r][c];
                
                sf::Vector2f pos = m_visualPositions[r][c] + sf::Vector2f(CELL_SIZE / 2.f, CELL_SIZE / 2.f);
                spawnParticleBurst(pos, type);
                
                matchCenter += pos;
                m_grid[r][c] = -1;
                clearedCount++;
            }
        }
    }

    if (clearedCount > 0) {
        matchCenter /= static_cast<float>(clearedCount);
        
        int scoreGained = clearedCount * 10 * m_comboMultiplier;
        m_score += scoreGained;

        // Check and update high score immediately
        if (m_score > m_highScore) {
            m_highScore = m_score;
        }

        // Float score texts
        std::string scoreText = "+" + std::to_string(scoreGained);
        if (m_comboMultiplier > 1) {
            scoreText += " (Combo " + std::to_string(m_comboMultiplier) + "x!)";
        }
        spawnFloatingText(scoreText, matchCenter - sf::Vector2f(50.f, 20.f), sf::Color::Cyan);

        // Level Up check
        checkLevelProgression();
    }
}

void Game::checkLevelProgression() {
    if (m_currentLevel < 4) { // Up to Level 5 (index 4)
        if (m_score >= LEVEL_PRESETS[m_currentLevel].targetScore) {
            m_currentLevel++;
            
            // Apply level values
            m_movesLeft = LEVEL_PRESETS[m_currentLevel].movesAllowed;
            m_levelTimer = LEVEL_PRESETS[m_currentLevel].timeLimit;
            m_undoUsedThisLevel = false;
            m_undoAvailable = false;

            spawnFloatingText("LEVEL UP! LEVEL " + std::to_string(m_currentLevel + 1), 
                              sf::Vector2f(GRID_MID_X + 80.f, GRID_MID_Y + 200.f), sf::Color::Magenta);
            
            // Re-shuffle to clear initial matches
            initGrid();
        }
    }
}

bool Game::refillGrid() {
    bool shifted = false;
    for (int c = 0; c < GRID_COLS; ++c) {
        for (int r = GRID_ROWS - 1; r >= 0; --r) {
            if (m_grid[r][c] == -1) {
                // Find candy above
                int k = r - 1;
                while (k >= 0 && m_grid[k][c] == -1) {
                    k--;
                }
                
                if (k >= 0) {
                    m_grid[r][c] = m_grid[k][c];
                    m_visualPositions[r][c] = m_visualPositions[k][c]; // Interpolate from old spot
                    m_grid[k][c] = -1;
                    shifted = true;
                } else {
                    // Spawn new candy
                    m_grid[r][c] = std::rand() % TOTAL_CANDY_TYPES;
                    // Position Y above screen for fall-in effect
                    m_visualPositions[r][c] = sf::Vector2f(
                        GRID_MID_X + c * CELL_SIZE,
                        GRID_MID_Y - CELL_SIZE * (GRID_ROWS - r)
                    );
                    shifted = true;
                }
            }
        }
    }
    return shifted;
}

bool Game::isAnyMovePossible() {
    // Simulates adjacent swaps to find if any combination can produce a match of 3
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            // Swap right
            if (c < GRID_COLS - 1) {
                if (checkMatchSimulation(r, c, r, c + 1)) return true;
            }
            // Swap down
            if (r < GRID_ROWS - 1) {
                if (checkMatchSimulation(r, c, r + 1, c)) return true;
            }
        }
    }
    return false;
}

bool Game::checkMatchSimulation(int r1, int c1, int r2, int c2) {
    // Temporary swap
    std::swap(m_grid[r1][c1], m_grid[r2][c2]);
    bool matchExists = checkMatchesExist();
    // Swap back
    std::swap(m_grid[r1][c1], m_grid[r2][c2]);
    return matchExists;
}

void Game::runHintSystem() {
    if (m_boardSettling || isAnimating()) return;

    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            // Check swap right
            if (c < GRID_COLS - 1) {
                if (checkMatchSimulation(r, c, r, c + 1)) {
                    m_suggestedMoveStart = sf::Vector2i(r, c);
                    m_suggestedMoveEnd = sf::Vector2i(r, c + 1);
                    m_hasHintActive = true;
                    m_hintTimer.restart();
                    return;
                }
            }
            // Check swap down
            if (r < GRID_ROWS - 1) {
                if (checkMatchSimulation(r, c, r + 1, c)) {
                    m_suggestedMoveStart = sf::Vector2i(r, c);
                    m_suggestedMoveEnd = sf::Vector2i(r + 1, c);
                    m_hasHintActive = true;
                    m_hintTimer.restart();
                    return;
                }
            }
        }
    }

    // If no moves exist
    spawnFloatingText("NO MOVES DETECTED!", sf::Vector2f(GRID_MID_X + 100.f, GRID_MID_Y + 280.f), sf::Color::Red);
    initGrid();
}

void Game::saveUndoState() {
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            m_undoGrid[r][c] = m_grid[r][c];
        }
    }
    m_undoScore = m_score;
    m_undoMovesLeft = m_movesLeft;
    m_undoLevel = m_currentLevel;
    m_undoTimer = m_levelTimer;
    m_undoAvailable = true;
}

void Game::restoreUndoState() {
    if (!m_undoAvailable) {
        spawnFloatingText("NO SWAP TO UNDO!", sf::Vector2f(1000.f, 120.f), sf::Color::Red);
        return;
    }
    if (m_undoUsedThisLevel) {
        spawnFloatingText("UNDO ALREADY USED!", sf::Vector2f(1000.f, 120.f), sf::Color::Red);
        return;
    }

    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            m_grid[r][c] = m_undoGrid[r][c];
            // Snap visual positions to the restored layout
            m_visualPositions[r][c] = sf::Vector2f(GRID_MID_X + c * CELL_SIZE, GRID_MID_Y + r * CELL_SIZE);
        }
    }
    m_score = m_undoScore;
    m_movesLeft = m_undoMovesLeft;
    m_currentLevel = m_undoLevel;
    m_levelTimer = m_undoTimer;
    
    m_undoAvailable = false;
    m_undoUsedThisLevel = true;
    m_selectedTile = sf::Vector2i(-1, -1);
    m_hasHintActive = false;

    spawnFloatingText("MOVE UNDONE!", sf::Vector2f(1200.f, 120.f), sf::Color::Yellow);
}

void Game::triggerAutosave() {
    if (m_movesSinceAutosave >= 10) {
        saveGame(AUTO_SAVE_FILE);
        m_movesSinceAutosave = 0;
        spawnFloatingText("GAME AUTO-SAVED!", sf::Vector2f(GRID_MID_X + 150.f, GRID_MID_Y - 30.f), sf::Color::Green);
    }
}

void Game::loadGame(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (file.is_open()) {
        file >> m_currentLevel >> m_score >> m_movesLeft >> m_levelTimer;
        for (int r = 0; r < GRID_ROWS; ++r) {
            for (int c = 0; c < GRID_COLS; ++c) {
                file >> m_grid[r][c];
                // Snap visual positions
                m_visualPositions[r][c] = sf::Vector2f(
                    GRID_MID_X + c * CELL_SIZE,
                    GRID_MID_Y + r * CELL_SIZE
                );
            }
        }
        file.close();
        
        m_boardSettling = false;
        m_selectedTile = sf::Vector2i(-1, -1);
        m_hasHintActive = false;
        m_undoAvailable = false;
        m_undoUsedThisLevel = false;

        spawnFloatingText("GAME LOADED!", sf::Vector2f(GRID_MID_X + 180.f, GRID_MID_Y - 30.f), sf::Color::Cyan);
    } else {
        std::cerr << "ERROR: Failed to load save file: " << filename << "\n";
    }
}

void Game::saveGame(const std::string& filename) const {
    std::ofstream file(filename.c_str());
    if (file.is_open()) {
        file << m_currentLevel << "\n" << m_score << "\n" << m_movesLeft << "\n" << m_levelTimer << "\n";
        for (int r = 0; r < GRID_ROWS; ++r) {
            for (int c = 0; c < GRID_COLS; ++c) {
                file << m_grid[r][c] << " ";
            }
            file << "\n";
        }
        file.close();
    } else {
        std::cerr << "ERROR: Failed to write save file: " << filename << "\n";
    }
}

void Game::loadHighScore() {
    m_highScoresList.clear();
    std::ifstream file(HIGH_SCORE_FILE.c_str());
    if (file.is_open()) {
        int val;
        while (file >> val) {
            m_highScoresList.push_back(val);
        }
        file.close();
    }
    
    // Pad to 5 scores if empty/incomplete
    while (m_highScoresList.size() < 5) {
        m_highScoresList.push_back(0);
    }

    std::sort(m_highScoresList.rbegin(), m_highScoresList.rend());
    m_highScore = m_highScoresList[0];
}

void Game::updateHighScoreList() {
    m_highScoresList.push_back(m_score);
    std::sort(m_highScoresList.rbegin(), m_highScoresList.rend());
    
    // Keep top 5
    if (m_highScoresList.size() > 5) {
        m_highScoresList.resize(5);
    }

    m_highScore = m_highScoresList[0];
    saveHighScores();
}

void Game::saveHighScores() {
    std::ofstream file(HIGH_SCORE_FILE.c_str());
    if (file.is_open()) {
        for (int score : m_highScoresList) {
            file << score << "\n";
        }
        file.close();
    }
}

void Game::spawnParticleBurst(sf::Vector2f pos, int candyType) {
    // Choose particle color depending on candy type
    sf::Color pColor = sf::Color::White;
    switch (candyType) {
        case 0: pColor = sf::Color(255, 75, 75); break;   // Red
        case 1: pColor = sf::Color(255, 165, 0); break;   // Orange
        case 2: pColor = sf::Color(255, 235, 50); break;  // Yellow
        case 3: pColor = sf::Color(50, 220, 50); break;   // Green
        case 4: pColor = sf::Color(50, 150, 255); break;  // Blue
        case 5: pColor = sf::Color(185, 80, 255); break;  // Purple
        case 6: pColor = sf::Color::Cyan; break;          // Spec 1
        case 7: pColor = sf::Color::Magenta; break;       // Spec 2
    }

    for (int i = 0; i < 15; ++i) {
        Particle p;
        p.position = pos;
        float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.f;
        float speed = static_cast<float>((std::rand() % 150) + 70);
        p.velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
        p.color = pColor;
        p.size = static_cast<float>((std::rand() % 6) + 4);
        p.lifetime = 0.45f + (std::rand() % 10) * 0.04f;
        p.maxLifetime = p.lifetime;
        
        m_particles.push_back(p);
    }
}

void Game::spawnFloatingText(const std::string& text, sf::Vector2f position, sf::Color color) {
    FloatingText ft;
    ft.text = text;
    ft.position = position;
    ft.color = color;
    ft.lifetime = 1.0f;
    ft.maxLifetime = 1.0f;
    ft.characterSize = 24;
    m_floatingTexts.push_back(ft);
}

bool Game::isMouseOverButton(sf::Vector2f mousePos, sf::Vector2f pos, sf::Vector2f size) const {
    return (mousePos.x >= pos.x && mousePos.x < pos.x + size.x &&
            mousePos.y >= pos.y && mousePos.y < pos.y + size.y);
}

void Game::drawGlassPanel(sf::Vector2f pos, sf::Vector2f size, sf::Color outlineColor) {
    sf::RectangleShape panel(size);
    panel.setPosition(pos);
    panel.setFillColor(COLOR_BG_GLASS);
    panel.setOutlineThickness(3.0f);
    panel.setOutlineColor(outlineColor);
    m_window.draw(panel);
}

void Game::drawButton(sf::Vector2f pos, sf::Vector2f size, const std::string& text, sf::Color baseColor, sf::Vector2f mousePos) {
    sf::RectangleShape btn(size);
    btn.setPosition(pos);
    
    // Check hover
    if (isMouseOverButton(mousePos, pos, size)) {
        sf::Color hoverColor = baseColor;
        hoverColor.r = std::min(hoverColor.r + 30, 255);
        hoverColor.g = std::min(hoverColor.g + 30, 255);
        hoverColor.b = std::min(hoverColor.b + 30, 255);
        btn.setFillColor(hoverColor);
        btn.setOutlineColor(sf::Color::White);
    } else {
        btn.setFillColor(baseColor);
        btn.setOutlineColor(sf::Color(255, 255, 255, 100));
    }
    
    btn.setOutlineThickness(2.0f);
    m_window.draw(btn);

    // Text centering
    sf::Text btnText;
    btnText.setFont(m_font);
    btnText.setString(text);
    btnText.setCharacterSize(22);
    btnText.setFillColor(COLOR_TEXT_LIGHT);
    
    sf::FloatRect textRect = btnText.getLocalBounds();
    btnText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    btnText.setPosition(pos.x + size.x / 2.0f, pos.y + size.y / 2.0f);
    m_window.draw(btnText);
}

void Game::render() {
    m_window.clear();
    
    // Draw Level Background (or default background for menus)
    int bgIndex = std::min(m_currentLevel, static_cast<int>(m_levelBgTextures.size()) - 1);
    m_bgSprite.setTexture(m_levelBgTextures[bgIndex]);
    
    // Resize background to fit window
    sf::Vector2u bgSize = m_levelBgTextures[bgIndex].getSize();
    m_bgSprite.setScale(
        static_cast<float>(WINDOW_WIDTH) / bgSize.x,
        static_cast<float>(WINDOW_HEIGHT) / bgSize.y
    );
    m_window.draw(m_bgSprite);

    sf::Vector2f mousePos = static_cast<sf::Vector2f>(sf::Mouse::getPosition(m_window));

    if (m_state == GameState::MainMenu) {
        // Dim overlay
        sf::RectangleShape dim(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
        dim.setFillColor(sf::Color(0, 0, 0, 150));
        m_window.draw(dim);

        // Title text
        sf::Text title;
        title.setFont(m_font);
        title.setString("CANDY CRUSH SAGA");
        title.setCharacterSize(56);
        title.setFillColor(sf::Color(255, 200, 50));
        sf::FloatRect tr = title.getLocalBounds();
        title.setOrigin(tr.left + tr.width/2.f, tr.top + tr.height/2.f);
        title.setPosition(WINDOW_WIDTH/2.f, 120.f);
        
        m_window.draw(title);

        // Buttons
        drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 150, 300), sf::Vector2f(300, 80), "Play New Game", COLOR_PRIMARY, mousePos);
        drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 150, 420), sf::Vector2f(300, 80), "Load Game", COLOR_SECONDARY, mousePos);
        drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 150, 540), sf::Vector2f(300, 80), "High Scores", COLOR_ACCENT, mousePos);
        drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 150, 660), sf::Vector2f(300, 80), "Quit Game", sf::Color(100, 100, 100), mousePos);
    }
    
    else if (m_state == GameState::LoadScreen) {
        sf::RectangleShape dim(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
        dim.setFillColor(sf::Color(0, 0, 0, 170));
        m_window.draw(dim);

        sf::Text title;
        title.setFont(m_font);
        
        bool isSavingMode = (m_movesLeft > 0 && m_levelTimer > 0.0f && m_score > 0 && m_grid[0][0] != -1);
        title.setString(isSavingMode ? "SELECT SAVE SLOT" : "SELECT LOAD SLOT");
        title.setCharacterSize(44);
        title.setFillColor(sf::Color::White);
        sf::FloatRect tr = title.getLocalBounds();
        title.setOrigin(tr.left + tr.width/2.f, tr.top + tr.height/2.f);
        title.setPosition(WINDOW_WIDTH/2.f, 120.f);
        m_window.draw(title);

        // Dynamic labels for save slots
        std::string labels[4] = { "Slot 1 (Empty)", "Slot 2 (Empty)", "Slot 3 (Empty)", "Auto-Save (Empty)" };
        std::string files[4] = { SAVE_SLOTS[0], SAVE_SLOTS[1], SAVE_SLOTS[2], AUTO_SAVE_FILE };

        for (int i = 0; i < 4; ++i) {
            if (fileExists(files[i])) {
                std::ifstream f(files[i].c_str());
                int lv, sc;
                f >> lv >> sc;
                f.close();
                
                labels[i] = (i == 3 ? "Auto-Save" : "Slot " + std::to_string(i + 1)) + " (Level " + std::to_string(lv + 1) + " - Score " + std::to_string(sc) + ")";
            }
        }

        drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 200, 250), sf::Vector2f(400, 70), labels[0], COLOR_PRIMARY, mousePos);
        drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 200, 350), sf::Vector2f(400, 70), labels[1], COLOR_PRIMARY, mousePos);
        drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 200, 450), sf::Vector2f(400, 70), labels[2], COLOR_PRIMARY, mousePos);
        
        // Auto-save is load-only, disable in save mode
        if (!isSavingMode) {
            drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 200, 550), sf::Vector2f(400, 70), labels[3], COLOR_SECONDARY, mousePos);
        }

        drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 200, 680), sf::Vector2f(400, 70), "Go Back", sf::Color(120, 120, 120), mousePos);
    }
    
    else if (m_state == GameState::HighScores) {
        sf::RectangleShape dim(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
        dim.setFillColor(sf::Color(0, 0, 0, 180));
        m_window.draw(dim);

        sf::Text title;
        title.setFont(m_font);
        title.setString("TOP HIGH SCORES");
        title.setCharacterSize(44);
        title.setFillColor(sf::Color(255, 215, 0));
        sf::FloatRect tr = title.getLocalBounds();
        title.setOrigin(tr.left + tr.width/2.f, tr.top + tr.height/2.f);
        title.setPosition(WINDOW_WIDTH/2.f, 120.f);
        m_window.draw(title);

        // Draw top 5 scores
        for (int i = 0; i < 5; ++i) {
            sf::Text scoreTxt;
            scoreTxt.setFont(m_font);
            
            int scoreVal = (i < static_cast<int>(m_highScoresList.size())) ? m_highScoresList[i] : 0;
            scoreTxt.setString("Rank #" + std::to_string(i + 1) + " :   " + std::to_string(scoreVal) + " PTS");
            scoreTxt.setCharacterSize(28);
            scoreTxt.setFillColor(sf::Color::White);
            
            sf::FloatRect r = scoreTxt.getLocalBounds();
            scoreTxt.setOrigin(r.left + r.width/2.f, r.top + r.height/2.f);
            scoreTxt.setPosition(WINDOW_WIDTH/2.f, 240.f + i * 70.f);
            m_window.draw(scoreTxt);
        }

        drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 150, 680), sf::Vector2f(300, 80), "Main Menu", sf::Color(120, 120, 120), mousePos);
    }
    
    else if (m_state == GameState::GameOver) {
        sf::RectangleShape dim(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
        dim.setFillColor(sf::Color(50, 0, 0, 200));
        m_window.draw(dim);

        sf::Text title;
        title.setFont(m_font);
        title.setString("GAME OVER");
        title.setCharacterSize(68);
        title.setFillColor(sf::Color::Red);
        sf::FloatRect tr = title.getLocalBounds();
        title.setOrigin(tr.left + tr.width/2.f, tr.top + tr.height/2.f);
        title.setPosition(WINDOW_WIDTH/2.f, 200.f);
        m_window.draw(title);

        sf::Text scTxt;
        scTxt.setFont(m_font);
        scTxt.setString("FINAL SCORE: " + std::to_string(m_score) + " POINTS");
        scTxt.setCharacterSize(34);
        scTxt.setFillColor(sf::Color::White);
        tr = scTxt.getLocalBounds();
        scTxt.setOrigin(tr.left + tr.width/2.f, tr.top + tr.height/2.f);
        scTxt.setPosition(WINDOW_WIDTH/2.f, 320.f);
        m_window.draw(scTxt);

        drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 200, 500), sf::Vector2f(400, 80), "Back to Menu", COLOR_PRIMARY, mousePos);
    }
    
    else { // Gameplay or PauseMenu
        // 1. Draw Gameplay Panel Backgrounds (Left Side Panel)
        drawGlassPanel(sf::Vector2f(20, 150), sf::Vector2f(340, 630), COLOR_SECONDARY);

        // Draw Left UI details
        sf::Text lvTxt;
        lvTxt.setFont(m_font);
        lvTxt.setFillColor(COLOR_TEXT_DARK);
        
        lvTxt.setCharacterSize(36);
        lvTxt.setString("LEVEL " + std::to_string(m_currentLevel + 1));
        lvTxt.setPosition(40, 180);
        m_window.draw(lvTxt);

        lvTxt.setCharacterSize(22);
        lvTxt.setString("Target: " + std::to_string(LEVEL_PRESETS[m_currentLevel].targetScore) + " pts");
        lvTxt.setPosition(40, 230);
        m_window.draw(lvTxt);

        // Divider
        sf::RectangleShape divider(sf::Vector2f(300, 3));
        divider.setFillColor(COLOR_SECONDARY);
        divider.setPosition(40, 280);
        m_window.draw(divider);

        // Score display
        sf::Text scTxt;
        scTxt.setFont(m_font);
        scTxt.setCharacterSize(26);
        scTxt.setFillColor(COLOR_TEXT_DARK);
        scTxt.setString("Score: " + std::to_string(m_score));
        scTxt.setPosition(40, 320);
        m_window.draw(scTxt);

        scTxt.setString("High Score: " + std::to_string(m_highScore));
        scTxt.setCharacterSize(20);
        scTxt.setFillColor(sf::Color(100, 100, 100));
        scTxt.setPosition(40, 360);
        m_window.draw(scTxt);

        // Moves left
        sf::Text mvTxt;
        mvTxt.setFont(m_font);
        mvTxt.setCharacterSize(26);
        mvTxt.setFillColor(COLOR_TEXT_DARK);
        mvTxt.setString("Moves Left: " + std::to_string(m_movesLeft));
        mvTxt.setPosition(40, 420);
        m_window.draw(mvTxt);

        // Timer
        sf::Text timerTxt;
        timerTxt.setFont(m_font);
        timerTxt.setCharacterSize(26);
        // Highlight yellow/red if low time
        timerTxt.setFillColor(m_levelTimer < 10.0f ? sf::Color::Red : COLOR_TEXT_DARK);
        std::stringstream ss;
        ss << "Time Left: " << static_cast<int>(m_levelTimer) << "s";
        timerTxt.setString(ss.str());
        timerTxt.setPosition(40, 480);
        m_window.draw(timerTxt);

        // Cascade Multiplier indicator
        if (m_comboMultiplier > 1) {
            sf::Text comboTxt;
            comboTxt.setFont(m_font);
            comboTxt.setString("COMBO: " + std::to_string(m_comboMultiplier) + "x!");
            comboTxt.setCharacterSize(28);
            comboTxt.setFillColor(sf::Color(255, 69, 0));
            comboTxt.setPosition(40, 560);
            m_window.draw(comboTxt);
        }

        // 2. Draw Side Buttons (Right Panel)
        drawButton(sf::Vector2f(30, 30), sf::Vector2f(100, 50), "Pause", sf::Color(180, 180, 180), mousePos);
        drawButton(sf::Vector2f(1230, 30), sf::Vector2f(140, 50), "Hint", COLOR_PRIMARY, mousePos);
        drawButton(sf::Vector2f(1230, 100), sf::Vector2f(140, 50), "Undo", m_undoAvailable && !m_undoUsedThisLevel ? COLOR_SECONDARY : sf::Color(100, 100, 100), mousePos);
        drawButton(sf::Vector2f(1230, 170), sf::Vector2f(140, 50), m_isMuted ? "Unmute" : "Mute", COLOR_ACCENT, mousePos);
        drawButton(sf::Vector2f(1230, 240), sf::Vector2f(140, 50), "Save Game", sf::Color(50, 150, 200), mousePos);

        // 3. Draw Grid Tiles and Candies
        sf::Texture cellTex;
        cellTex.loadFromFile("images/background/cell.png"); // Use cell texture
        sf::Sprite cellSprite(cellTex);
        float cellScale = CELL_SIZE / cellTex.getSize().x;
        cellSprite.setScale(cellScale, cellScale);

        // Draw cells
        for (int r = 0; r < GRID_ROWS; ++r) {
            for (int c = 0; c < GRID_COLS; ++c) {
                cellSprite.setPosition(GRID_MID_X + c * CELL_SIZE, GRID_MID_Y + r * CELL_SIZE);
                m_window.draw(cellSprite);
            }
        }

        // Draw candies at visual positions
        for (int r = 0; r < GRID_ROWS; ++r) {
            for (int c = 0; c < GRID_COLS; ++c) {
                int type = m_grid[r][c];
                if (type != -1) {
                    sf::Sprite s;
                    if (type < 6) { // Normal candy
                        s.setTexture(m_candyTextures[type]);
                        float scale = CANDY_SIZE / m_candyTextures[type].getSize().x;
                        s.setScale(scale, scale);
                    } else { // Special candy
                        s.setTexture(m_specialTextures[type - 6]);
                        float scale = CANDY_SIZE / m_specialTextures[type - 6].getSize().x;
                        s.setScale(scale, scale);
                    }
                    
                    // Draw centered inside the cell
                    float posX = m_visualPositions[r][c].x + (CELL_SIZE - CANDY_SIZE) / 2.0f;
                    float posY = m_visualPositions[r][c].y + (CELL_SIZE - CANDY_SIZE) / 2.0f;
                    s.setPosition(posX, posY);
                    m_window.draw(s);
                }
            }
        }

        // Selection Highlight
        if (m_selectedTile != sf::Vector2i(-1, -1)) {
            sf::RectangleShape outline(sf::Vector2f(CELL_SIZE, CELL_SIZE));
            outline.setPosition(GRID_MID_X + m_selectedTile.y * CELL_SIZE, GRID_MID_Y + m_selectedTile.x * CELL_SIZE);
            outline.setFillColor(sf::Color::Transparent);
            outline.setOutlineThickness(4.f);
            outline.setOutlineColor(sf::Color::Yellow);
            m_window.draw(outline);
        }

        // Hint Highlight (Blinking Outline)
        if (m_hasHintActive) {
            sf::RectangleShape outline(sf::Vector2f(CELL_SIZE, CELL_SIZE));
            outline.setFillColor(sf::Color::Transparent);
            outline.setOutlineThickness(4.f);
            
            // Blinking effect
            float timeSec = m_hintTimer.getElapsedTime().asSeconds();
            sf::Color hintColor = (static_cast<int>(timeSec * 5) % 2 == 0) ? sf::Color::White : sf::Color::Magenta;
            outline.setOutlineColor(hintColor);

            // Highlight start
            outline.setPosition(GRID_MID_X + m_suggestedMoveStart.y * CELL_SIZE, GRID_MID_Y + m_suggestedMoveStart.x * CELL_SIZE);
            m_window.draw(outline);
            
            // Highlight end
            outline.setPosition(GRID_MID_X + m_suggestedMoveEnd.y * CELL_SIZE, GRID_MID_Y + m_suggestedMoveEnd.x * CELL_SIZE);
            m_window.draw(outline);
        }

        // 4. Draw Particles
        for (const auto& p : m_particles) {
            p.draw(m_window);
        }

        // 5. Draw Floating Texts
        for (const auto& ft : m_floatingTexts) {
            ft.draw(m_window, m_font);
        }

        // Pause Menu Dim Overlay
        if (m_state == GameState::PauseMenu) {
            sf::RectangleShape dim(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
            dim.setFillColor(sf::Color(0, 0, 0, 160));
            m_window.draw(dim);

            // Pause Menu Title
            sf::Text pauseTitle;
            pauseTitle.setFont(m_font);
            pauseTitle.setString("GAME PAUSED");
            pauseTitle.setCharacterSize(50);
            pauseTitle.setFillColor(sf::Color::White);
            sf::FloatRect tr = pauseTitle.getLocalBounds();
            pauseTitle.setOrigin(tr.left + tr.width/2.f, tr.top + tr.height/2.f);
            pauseTitle.setPosition(WINDOW_WIDTH/2.f, 150.f);
            m_window.draw(pauseTitle);

            // Pause buttons
            drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 150, 300), sf::Vector2f(300, 80), "Resume Play", COLOR_PRIMARY, mousePos);
            drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 150, 420), sf::Vector2f(300, 80), "Save Game", COLOR_SECONDARY, mousePos);
            drawButton(sf::Vector2f(WINDOW_WIDTH/2 - 150, 540), sf::Vector2f(300, 80), "Quit to Main Menu", sf::Color(120, 120, 120), mousePos);
        }
    }

    m_window.display();
}

} // namespace CandyCrush
