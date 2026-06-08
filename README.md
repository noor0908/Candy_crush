# Candy Crush Saga - macOS Port
A modernized, premium C++ clone of Candy Crush Saga built using **SFML 2.x** and compiled natively for macOS (Apple Silicon / Intel). This project features responsive controls, smooth animations, procedural sound synthesis, level progression, and multi-slot saves.
---
## 🎮 Game Features
### 1. Modernized Gameplay
* **Horizontal & Vertical Swaps Only:** Strict adjacency check prevents illegal diagonal moves or diagonal match clearing, matching authentic Match-3 rules.
* **Combo Multiplier:** Cascade matches multiply score gains (`1x`, `2x`, `3x`, etc.). The multiplier resets once the board settles.
* **Special Candies:**
  * **Striped Candy (Match of 4):** Clears the entire row and column when clicked or matched.
  * **Wrapped Candy (Match of 5):** Clears a 3x3 surrounding region.
  * *Supports chain-reaction explosions* (exploding one special candy triggers adjacent ones).
* **Undo Move:** Revert the board, score, moves, and timer to the state before the last swap. Available **once per level**.
* **Difficulty Progression (5 Levels):** Level up by meeting score thresholds. Each level decreases the moves allowed, speeds up the countdown timer, and changes the background image.
### 2. High-Quality Audio & Visuals
* **Procedurally Synthesized Sound Effects:** Direct sound synthesis generates harmonic chimes for normal matches, riser sweeps for special activations, and descending notes for game over—fully self-contained without needing external SFX assets.
* **Mute Toggle:** Quickly mute or unmute background music and sound effects on screen.
* **Smooth Animations:** Slide animations interpolate visual positions on refilling and cascading.
* **Particles & Floaters:** Colored particle bursts explode at match centers, with floating text popping up to show score gains.
### 3. Save & Load System
* **3 Separate Save Slots:** Save slot selections detail your level and score.
* **Auto-Save:** The game automatically saves your state to a backup slot every 10 moves.
* **Timer Persistence:** Save slots capture and restore the remaining countdown timer precisely.
---
## 🎹 Controls
* **Left Mouse Click:** Select candies to swap, activate special power-ups, or navigate menu buttons.
* **Escape (ESC):** Toggle the Pause Menu during gameplay to save or return to the main menu.
---
## 📁 Project Structure
```
Project14/
├── Project14/
│   ├── Constants.hpp      # Named constants, UI layouts, colors, and level presets
│   ├── Particle.hpp       # Effect classes for Particles and FloatingText popups
│   ├── Game.hpp           # Game class declaration encapsulating state and loops
│   ├── Game.cpp           # Game logic, rendering, event polling, and audio synthesis
│   ├── main.cpp           # Application entry point
│   ├── candycrush.ogg     # Background music track
│   └── images/            # Graphic assets (candies, backgrounds, fonts)
└── README.md              # Project documentation
```
---
## 🚀 Setup and Compilation
### Prerequisites
You need **Homebrew** and **SFML 2.x** (`sfml@2`) installed on your Mac.
1. **Install SFML 2.x via Homebrew:**
   ```bash
   brew install sfml@2
   ```
2. **Navigate to the Source Directory:**
   ```bash
   cd Project14/Project14
   ```
3. **Compile the Game:**
   Link against the Homebrew `sfml@2` Keg:
   ```bash
   clang++ -std=c++17 -I/opt/homebrew/opt/sfml@2/include main.cpp Game.cpp -o candycrush -L/opt/homebrew/opt/sfml@2/lib -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio
   ```
4. **Launch the Game:**
   *Note: Always run the executable from the source directory so it can find the `images/` folder and `candycrush.ogg` music file.*
   ```bash
   ./candycrush
   ```
