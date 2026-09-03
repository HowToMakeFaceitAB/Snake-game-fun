#include <SFML/Graphics.hpp>
#include <vector>
#include <deque>
#include <random>
#include <string>
#include <iostream>
#include <optional>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <functional>
#include <fstream>
#include <cmath>

enum Direction { UP, DOWN, LEFT, RIGHT };
enum class GameState { PLAYING, PAUSED, GAME_OVER, OPTIONS };
enum class WindowMode { WINDOWED, FULLSCREEN, BORDERLESS };
enum class MovementStyle { SMOOTH, RETRO };

struct Point {
    int x, y;
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

class SnakeGame {
private:
    sf::RenderWindow window;
    sf::Font font;

    // Game States
    GameState state;
    GameState previousState;

    // Snake & Gameplay Data
    std::vector<Point> snake;
    std::vector<Point> prevSnake;
    std::vector<Point> foods;
    Direction dir;
    std::deque<Direction> inputQueue;
    const size_t MAX_QUEUE_SIZE = 2;

    int score;
    int highScore;

    // Movement & Graphics Options
    MovementStyle movementStyle;
    bool vsyncEnabled;
    int maxFpsIndex;
    std::vector<int> fpsOptions;
    int customFpsLimit;

    // Grid Settings
    int gridSizeIndex;
    std::vector<int> gridSizes;
    std::vector<std::string> gridLabels;
    int customGridSize;

    // Speed Settings
    int speedIndex;
    std::vector<float> speedIntervals;
    std::vector<std::string> speedLabels;
    float customSpeedInterval;

    // Fruit Settings
    int fruitQuantityIndex;
    std::vector<int> fruitQuantities;
    int customFruitQuantity;

    // Video Settings
    WindowMode currentWindowMode;
    std::vector<sf::VideoMode> availableModes;
    int currentResIndex;

    // Menu Scale Settings (Independent of Grid Size)
    int menuScaleIndex;
    std::vector<float> menuScaleFactors;
    std::vector<std::string> menuScaleLabels;

    // Keybindings
    sf::Keyboard::Key restartKey;
    bool isRebindingKey;

    // Direct Text Input Box State (Locked to explicit target)
    bool isEditingText;
    int activeCustomTarget; // -1: none, 3: FPS, 5: Speed, 6: Fruit, 7: Grid
    std::string textInputBuffer;

    // Dropdown UI State
    int openDropdownIndex; // -1 if no dropdown is open
    int hoveredDropdownOption; // Option index currently hovered

    // Menu Navigation
    int selectedMenuIndex;

    // Timing & Sizing
    sf::Clock moveClock;
    float cellSize;
    float boardWidth;
    float boardHeight;
    float offsetX;
    float offsetY;

    std::mt19937 rng;

    const std::string CONFIG_FILENAME = "config.txt";

    float getMenuScale() const {
        return menuScaleFactors[menuScaleIndex];
    }

    void setGameState(GameState newState) {
        state = newState;
        window.setMouseCursorVisible(state != GameState::PLAYING);
        isEditingText = false;
        activeCustomTarget = -1;
        isRebindingKey = false;
        openDropdownIndex = -1;
        hoveredDropdownOption = -1;
    }

    int getActiveGridSize() const {
        if (gridSizeIndex == 6) return customGridSize;
        return gridSizes[gridSizeIndex];
    }

    float getActiveSpeedInterval() const {
        if (speedIndex == 4) return customSpeedInterval;
        return speedIntervals[speedIndex];
    }

    int getMaxAllowedFruits() const {
        int activeGridSize = getActiveGridSize();
        int maxByGrid = (activeGridSize * activeGridSize) / 3;
        return std::min(100, std::max(1, maxByGrid));
    }

    int getActiveFruitQuantity() const {
        if (fruitQuantityIndex == 4) return customFruitQuantity;
        return fruitQuantities[fruitQuantityIndex];
    }

    int getActiveFpsLimit() const {
        if (maxFpsIndex == 5) return customFpsLimit;
        return fpsOptions[maxFpsIndex];
    }

    std::string formatFloat(float val, int decimals) const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(decimals) << val;
        return ss.str();
    }

    std::string getKeyName(sf::Keyboard::Key key) const {
        if (key >= sf::Keyboard::Key::A && key <= sf::Keyboard::Key::Z) {
            return std::string(1, static_cast<char>('A' + (static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::A))));
        }
        if (key >= sf::Keyboard::Key::Num0 && key <= sf::Keyboard::Key::Num9) {
            return std::string(1, static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::Num0))));
        }
        switch (key) {
        case sf::Keyboard::Key::R: return "R";
        case sf::Keyboard::Key::Space: return "Space";
        case sf::Keyboard::Key::Enter: return "Enter";
        case sf::Keyboard::Key::Tab: return "Tab";
        case sf::Keyboard::Key::Backspace: return "Backspace";
        default: return "Key#" + std::to_string(static_cast<int>(key));
        }
    }

    void loadConfig() {
        std::ifstream inFile(CONFIG_FILENAME);
        if (!inFile.is_open()) return;

        std::string key;
        while (inFile >> key) {
            if (key == "windowMode") {
                int mode; if (inFile >> mode) currentWindowMode = static_cast<WindowMode>(std::clamp(mode, 0, 2));
            }
            else if (key == "resIndex") {
                inFile >> currentResIndex;
            }
            else if (key == "speedIndex") {
                int idx; if (inFile >> idx) speedIndex = std::clamp(idx, 0, 4);
            }
            else if (key == "customSpeed") {
                float spd; if (inFile >> spd) customSpeedInterval = std::clamp(spd, 0.01f, 0.50f);
            }
            else if (key == "fruitIndex") {
                int idx; if (inFile >> idx) fruitQuantityIndex = std::clamp(idx, 0, 4);
            }
            else if (key == "customFruit") {
                int qty; if (inFile >> qty) customFruitQuantity = std::max(1, qty);
            }
            else if (key == "gridIndex") {
                int idx; if (inFile >> idx) gridSizeIndex = std::clamp(idx, 0, 6);
            }
            else if (key == "customGrid") {
                int sz; if (inFile >> sz) customGridSize = std::clamp(sz, 10, 60);
            }
            else if (key == "movementStyle") {
                int st; if (inFile >> st) movementStyle = static_cast<MovementStyle>(std::clamp(st, 0, 1));
            }
            else if (key == "vsync") {
                inFile >> vsyncEnabled;
            }
            else if (key == "fpsIndex") {
                int idx; if (inFile >> idx) maxFpsIndex = std::clamp(idx, 0, 5);
            }
            else if (key == "customFps") {
                int fps; if (inFile >> fps) customFpsLimit = std::clamp(fps, 15, 300);
            }
            else if (key == "menuScale") {
                int scale; if (inFile >> scale) menuScaleIndex = std::clamp(scale, 0, 3);
            }
            else if (key == "restartKey") {
                int k; if (inFile >> k) restartKey = static_cast<sf::Keyboard::Key>(k);
            }
            else if (key == "highScore") {
                inFile >> highScore;
            }
        }
    }

    void saveConfig() {
        std::ofstream outFile(CONFIG_FILENAME);
        if (!outFile.is_open()) return;

        outFile << "windowMode " << static_cast<int>(currentWindowMode) << "\n";
        outFile << "resIndex " << currentResIndex << "\n";
        outFile << "speedIndex " << speedIndex << "\n";
        outFile << "customSpeed " << customSpeedInterval << "\n";
        outFile << "fruitIndex " << fruitQuantityIndex << "\n";
        outFile << "customFruit " << customFruitQuantity << "\n";
        outFile << "gridIndex " << gridSizeIndex << "\n";
        outFile << "customGrid " << customGridSize << "\n";
        outFile << "movementStyle " << static_cast<int>(movementStyle) << "\n";
        outFile << "vsync " << vsyncEnabled << "\n";
        outFile << "fpsIndex " << maxFpsIndex << "\n";
        outFile << "customFps " << customFpsLimit << "\n";
        outFile << "menuScale " << menuScaleIndex << "\n";
        outFile << "restartKey " << static_cast<int>(restartKey) << "\n";
        outFile << "highScore " << highScore << "\n";
    }

    void initResolutions() {
        availableModes.clear();
        auto modes = sf::VideoMode::getFullscreenModes();

        for (const auto& mode : modes) {
            if (mode.size.x >= 800 && mode.size.y >= 600) {
                bool isDuplicate = false;
                for (const auto& existing : availableModes) {
                    if (existing.size == mode.size) { isDuplicate = true; break; }
                }
                if (!isDuplicate) availableModes.push_back(mode);
            }
        }

        if (availableModes.empty()) availableModes.push_back(sf::VideoMode::getDesktopMode());
        if (currentResIndex < 0 || currentResIndex >= static_cast<int>(availableModes.size())) currentResIndex = 0;
    }

    void applyVideoSettings() {
        sf::VideoMode selectedMode = availableModes[currentResIndex];

        if (currentWindowMode == WindowMode::FULLSCREEN) {
            window.create(selectedMode, "C++ SFML 3 Snake", sf::Style::None, sf::State::Fullscreen);
        }
        else if (currentWindowMode == WindowMode::BORDERLESS) {
            window.create(sf::VideoMode::getDesktopMode(), "C++ SFML 3 Snake", sf::Style::None, sf::State::Windowed);
        }
        else {
            window.create(selectedMode, "C++ SFML 3 Snake", sf::Style::Default, sf::State::Windowed);
        }

        window.setVerticalSyncEnabled(vsyncEnabled);
        if (!vsyncEnabled) {
            int fps = getActiveFpsLimit();
            window.setFramerateLimit(fps == 0 ? 0 : fps);
        }
        else {
            window.setFramerateLimit(0);
        }

        window.setMouseCursorVisible(state != GameState::PLAYING);
        recalculateLayout();
    }

    void recalculateLayout() {
        sf::Vector2u windowSize = window.getSize();
        float screenW = static_cast<float>(windowSize.x);
        float screenH = static_cast<float>(windowSize.y);

        int activeGridSize = getActiveGridSize();

        cellSize = std::floor((screenH * 0.65f) / activeGridSize);
        if (cellSize < 4.0f) cellSize = 4.0f;

        boardWidth = activeGridSize * cellSize;
        boardHeight = activeGridSize * cellSize;

        offsetX = (screenW - boardWidth) / 2.0f;
        offsetY = (screenH - boardHeight) / 2.0f + 25.0f;
    }

    void spawnFoods() {
        int targetCount = getActiveFruitQuantity();
        int activeGridSize = getActiveGridSize();

        int maxPossible = (activeGridSize * activeGridSize) - static_cast<int>(snake.size());
        if (targetCount > maxPossible) targetCount = std::max(1, maxPossible);

        std::uniform_int_distribution<int> dist(0, activeGridSize - 1);
        int attempts = 0;
        const int MAX_ATTEMPTS = 500;

        while (foods.size() < static_cast<size_t>(targetCount) && attempts < MAX_ATTEMPTS) {
            attempts++;
            Point newFood = { dist(rng), dist(rng) };

            bool positionOccupied = false;
            for (const auto& segment : snake) if (segment == newFood) { positionOccupied = true; break; }
            for (const auto& f : foods) if (f == newFood) { positionOccupied = true; break; }

            if (!positionOccupied) foods.push_back(newFood);
        }
    }

    std::vector<std::string> getDropdownOptions(int optionIdx) const {
        switch (optionIdx) {
        case 0: return { "Windowed", "Fullscreen", "Borderless" };
        case 1: {
            std::vector<std::string> resList;
            for (const auto& mode : availableModes) {
                resList.push_back(std::to_string(mode.size.x) + "x" + std::to_string(mode.size.y));
            }
            return resList;
        }
        case 2: return { "Off", "On" };
        case 3: return { "30 FPS", "60 FPS", "120 FPS", "144 FPS", "240 FPS", "Custom (" + std::to_string(customFpsLimit) + ")" };
        case 4: return { "Smooth", "Retro" };
        case 5: return { "Slow", "Medium", "Fast", "Insane", "Custom (" + formatFloat(customSpeedInterval, 2) + "s)" };
        case 6: return { "1", "2", "3", "5", "Custom (" + std::to_string(customFruitQuantity) + ")" };
        case 7: return { "10x10", "15x15", "20x20", "25x25", "30x30", "40x40", "Custom (" + std::to_string(customGridSize) + ")" };
        case 8: return menuScaleLabels;
        default: return {};
        }
    }

    int getCurrentDropdownSelection(int optionIdx) const {
        switch (optionIdx) {
        case 0: return static_cast<int>(currentWindowMode);
        case 1: return currentResIndex;
        case 2: return vsyncEnabled ? 1 : 0;
        case 3: return maxFpsIndex;
        case 4: return (movementStyle == MovementStyle::SMOOTH) ? 0 : 1;
        case 5: return speedIndex;
        case 6: return fruitQuantityIndex;
        case 7: return gridSizeIndex;
        case 8: return menuScaleIndex;
        default: return 0;
        }
    }

    void selectDropdownOption(int optionIdx, int choiceIdx) {
        switch (optionIdx) {
        case 0:
            currentWindowMode = static_cast<WindowMode>(choiceIdx);
            applyVideoSettings();
            break;
        case 1:
            currentResIndex = choiceIdx;
            applyVideoSettings();
            break;
        case 2:
            vsyncEnabled = (choiceIdx == 1);
            applyVideoSettings();
            break;
        case 3:
            maxFpsIndex = choiceIdx;
            if (maxFpsIndex == 5) {
                isEditingText = true;
                activeCustomTarget = 3; // Lock target to FPS
                textInputBuffer = std::to_string(customFpsLimit);
            }
            else applyVideoSettings();
            break;
        case 4:
            movementStyle = (choiceIdx == 0) ? MovementStyle::SMOOTH : MovementStyle::RETRO;
            break;
        case 5:
            speedIndex = choiceIdx;
            if (speedIndex == 4) {
                isEditingText = true;
                activeCustomTarget = 5; // Lock target to Speed
                textInputBuffer = formatFloat(customSpeedInterval, 2);
            }
            break;
        case 6:
            fruitQuantityIndex = choiceIdx;
            if (fruitQuantityIndex == 4) {
                isEditingText = true;
                activeCustomTarget = 6; // Lock target to Fruit Quantity
                textInputBuffer = std::to_string(customFruitQuantity);
            }
            spawnFoods();
            break;
        case 7:
            gridSizeIndex = choiceIdx;
            if (gridSizeIndex == 6) {
                isEditingText = true;
                activeCustomTarget = 7; // Lock target to Grid Size
                textInputBuffer = std::to_string(customGridSize);
            }
            if (customFruitQuantity > getMaxAllowedFruits()) customFruitQuantity = getMaxAllowedFruits();
            recalculateLayout();
            reset();
            break;
        case 8:
            menuScaleIndex = choiceIdx;
            break;
        }
        saveConfig();
    }

    sf::FloatRect getItemRect(const std::string& label, float centerX, float itemY, unsigned int fontSize) {
        sf::Text text(font, label, fontSize);
        sf::FloatRect bounds = text.getLocalBounds();
        float paddingX = 25.0f;
        float paddingY = 6.0f;
        float w = bounds.size.x + paddingX * 2.0f;
        float h = bounds.size.y + paddingY * 2.0f;
        return sf::FloatRect({ centerX - w / 2.0f, itemY - h / 2.0f }, { w, h });
    }

    void updateMouseHover(sf::Vector2f mousePos) {
        float centerX = offsetX + boardWidth / 2.0f;
        float scale = getMenuScale();

        if (state == GameState::PAUSED) {
            std::vector<std::string> items = { "Resume", "Options", "Restart Game", "Quit Game" };
            float startY = offsetY + (boardHeight * 0.2f);
            for (size_t i = 0; i < items.size(); ++i) {
                float itemY = startY + (i + 1) * (35.0f * scale);
                if (getItemRect(items[i], centerX, itemY, static_cast<unsigned int>(18.0f * scale)).contains(mousePos)) {
                    selectedMenuIndex = static_cast<int>(i);
                    break;
                }
            }
        }
        else if (state == GameState::GAME_OVER) {
            std::vector<std::string> items = { "Play Again (" + getKeyName(restartKey) + ")", "Options", "Quit Game" };
            float startY = offsetY + (boardHeight * 0.2f);
            for (size_t i = 0; i < items.size(); ++i) {
                float itemY = startY + (i + 1) * (35.0f * scale);
                if (getItemRect(items[i], centerX, itemY, static_cast<unsigned int>(18.0f * scale)).contains(mousePos)) {
                    selectedMenuIndex = static_cast<int>(i);
                    break;
                }
            }
        }
        else if (state == GameState::OPTIONS) {
            float startY = offsetY + 30.0f;
            float rowSpacing = 30.0f * scale;

            // Hover checks for open dropdown items
            if (openDropdownIndex != -1) {
                std::vector<std::string> opts = getDropdownOptions(openDropdownIndex);
                float boxY = startY + (openDropdownIndex + 1) * rowSpacing;
                float boxW = 180.0f * scale;
                float itemH = 24.0f * scale;

                hoveredDropdownOption = -1;
                for (size_t optIdx = 0; optIdx < opts.size(); ++optIdx) {
                    float itemY = boxY + (24.0f * scale) + (optIdx * itemH);
                    sf::FloatRect itemRect({ centerX + (10.0f * scale), itemY - (12.0f * scale) }, { boxW, itemH });
                    if (itemRect.contains(mousePos)) {
                        hoveredDropdownOption = static_cast<int>(optIdx);
                        break;
                    }
                }
                return;
            }

            // Hover check for main menu rows
            for (size_t i = 0; i < 11; ++i) {
                float itemY = startY + (i + 1) * rowSpacing;
                sf::FloatRect boxRect({ centerX - (200.0f * scale), itemY - (14.0f * scale) }, { 400.0f * scale, 28.0f * scale });
                if (boxRect.contains(mousePos)) {
                    selectedMenuIndex = static_cast<int>(i);
                    break;
                }
            }
        }
    }

public:
    SnakeGame()
        : state(GameState::PLAYING),
        previousState(GameState::PLAYING),
        rng(std::random_device{}()),
        score(0),
        highScore(0),
        selectedMenuIndex(0),
        openDropdownIndex(-1),
        hoveredDropdownOption(-1),
        movementStyle(MovementStyle::SMOOTH),
        vsyncEnabled(false),
        maxFpsIndex(1),
        customFpsLimit(60),
        restartKey(sf::Keyboard::Key::R),
        isRebindingKey(false),
        isEditingText(false),
        activeCustomTarget(-1)
    {
        fpsOptions = { 30, 60, 120, 144, 240, 60 };

        // Grid Options
        gridSizes = { 10, 15, 20, 25, 30, 40, 20 };
        gridLabels = { "10x10", "15x15", "20x20", "25x25", "30x30", "40x40", "Custom" };
        gridSizeIndex = 2; customGridSize = 20;

        // Speed Options
        speedIntervals = { 0.15f, 0.10f, 0.06f, 0.03f, 0.08f };
        speedLabels = { "Slow", "Medium", "Fast", "Insane", "Custom" };
        speedIndex = 1; customSpeedInterval = 0.08f;

        // Fruit Options
        fruitQuantities = { 1, 2, 3, 5, 4 };
        fruitQuantityIndex = 0; customFruitQuantity = 4;

        // Menu Scaling Options (80%, 100%, 125%, 150%)
        menuScaleFactors = { 0.80f, 1.00f, 1.25f, 1.50f };
        menuScaleLabels = { "80%", "100%", "125%", "150%" };
        menuScaleIndex = 1; // Default 100%

        currentWindowMode = WindowMode::FULLSCREEN;
        currentResIndex = 0;

        loadConfig();
        initResolutions();
        applyVideoSettings();

        std::string fontPath = "arial.ttf";
        if (!std::filesystem::exists(fontPath)) fontPath = "C:/Windows/Fonts/arial.ttf";
        if (std::filesystem::exists(fontPath)) {
            if (!font.openFromFile(fontPath)) std::cerr << "Warning: Failed to load font\n";
        }

        reset();
    }

    void reset() {
        int activeGridSize = getActiveGridSize();
        snake = {
            {activeGridSize / 2, activeGridSize / 2},
            {activeGridSize / 2 - 1, activeGridSize / 2},
            {activeGridSize / 2 - 2, activeGridSize / 2}
        };
        prevSnake = snake;
        dir = RIGHT;
        inputQueue.clear();
        score = 0;
        foods.clear();
        spawnFoods();
        moveClock.restart();
    }

    void pushDirection(Direction newDir) {
        Direction lastDir = inputQueue.empty() ? dir : inputQueue.back();

        if (inputQueue.size() < MAX_QUEUE_SIZE) {
            if ((newDir == UP && lastDir != DOWN) ||
                (newDir == DOWN && lastDir != UP) ||
                (newDir == LEFT && lastDir != RIGHT) ||
                (newDir == RIGHT && lastDir != LEFT))
            {
                if (newDir != lastDir) inputQueue.push_back(newDir);
            }
        }
    }

    void applyTextInputValue() {
        if (!isEditingText) return;
        try {
            if (!textInputBuffer.empty()) {
                // EXPLICIT TARGET LOCKING FIX: Check activeCustomTarget instead of selectedMenuIndex
                if (activeCustomTarget == 3 && maxFpsIndex == 5) {
                    customFpsLimit = std::clamp(std::stoi(textInputBuffer), 15, 300);
                }
                else if (activeCustomTarget == 5 && speedIndex == 4) {
                    customSpeedInterval = std::clamp(std::stof(textInputBuffer), 0.01f, 0.50f);
                }
                else if (activeCustomTarget == 6 && fruitQuantityIndex == 4) {
                    customFruitQuantity = std::clamp(std::stoi(textInputBuffer), 1, getMaxAllowedFruits());
                    spawnFoods();
                }
                else if (activeCustomTarget == 7 && gridSizeIndex == 6) {
                    customGridSize = std::clamp(std::stoi(textInputBuffer), 10, 60);
                    recalculateLayout();
                    reset();
                }
            }
        }
        catch (...) {}

        isEditingText = false;
        activeCustomTarget = -1;
        textInputBuffer.clear();
        applyVideoSettings();
        saveConfig();
    }

    void handleInput() {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            else if (const auto* textEntered = event->getIf<sf::Event::TextEntered>()) {
                if (isEditingText) {
                    uint32_t unicode = textEntered->unicode;
                    if (unicode == 8) { // Backspace
                        if (!textInputBuffer.empty()) textInputBuffer.pop_back();
                    }
                    else if (unicode == 13 || unicode == 27) { // Enter or Escape
                        applyTextInputValue();
                    }
                    else if ((unicode >= '0' && unicode <= '9') || (unicode == '.' && activeCustomTarget == 5)) {
                        if (textInputBuffer.size() < 6) textInputBuffer += static_cast<char>(unicode);
                    }
                }
            }
            else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
                if (state != GameState::PLAYING) {
                    sf::Vector2f mousePos = window.mapPixelToCoords(mouseMoved->position);
                    updateMouseHover(mousePos);
                }
            }
            else if (const auto* mouseClick = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (state != GameState::PLAYING && mouseClick->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mousePos = window.mapPixelToCoords(mouseClick->position);

                    if (isEditingText) {
                        applyTextInputValue();
                        continue;
                    }

                    if (state == GameState::OPTIONS) {
                        float centerX = offsetX + boardWidth / 2.0f;
                        float startY = offsetY + 30.0f;
                        float scale = getMenuScale();
                        float rowSpacing = 30.0f * scale;

                        // Dropdown selection click handling
                        if (openDropdownIndex != -1) {
                            std::vector<std::string> opts = getDropdownOptions(openDropdownIndex);
                            float boxY = startY + (openDropdownIndex + 1) * rowSpacing;
                            float itemH = 24.0f * scale;

                            bool clickedOption = false;
                            for (size_t optIdx = 0; optIdx < opts.size(); ++optIdx) {
                                float itemY = boxY + (24.0f * scale) + (optIdx * itemH);
                                sf::FloatRect itemRect({ centerX + (10.0f * scale), itemY - (12.0f * scale) }, { 180.0f * scale, itemH });
                                if (itemRect.contains(mousePos)) {
                                    selectDropdownOption(openDropdownIndex, static_cast<int>(optIdx));
                                    openDropdownIndex = -1;
                                    clickedOption = true;
                                    break;
                                }
                            }
                            if (!clickedOption) openDropdownIndex = -1; // Close on outside click
                            continue;
                        }

                        // Options rows click handling
                        for (size_t i = 0; i < 11; ++i) {
                            float itemY = startY + (i + 1) * rowSpacing;
                            sf::FloatRect boxRect({ centerX - (200.0f * scale), itemY - (14.0f * scale) }, { 400.0f * scale, 28.0f * scale });

                            if (boxRect.contains(mousePos)) {
                                selectedMenuIndex = static_cast<int>(i);
                                if (i < 9) {
                                    openDropdownIndex = static_cast<int>(i);
                                }
                                else if (i == 9) {
                                    isRebindingKey = true;
                                }
                                else if (i == 10) {
                                    setGameState(previousState);
                                    selectedMenuIndex = 0;
                                }
                                break;
                            }
                        }
                    }
                    else if (state == GameState::PAUSED) {
                        updateMouseHover(mousePos);
                        if (selectedMenuIndex == 0) setGameState(GameState::PLAYING);
                        else if (selectedMenuIndex == 1) { previousState = GameState::PAUSED; setGameState(GameState::OPTIONS); selectedMenuIndex = 0; }
                        else if (selectedMenuIndex == 2) { reset(); setGameState(GameState::PLAYING); }
                        else if (selectedMenuIndex == 3) window.close();
                    }
                    else if (state == GameState::GAME_OVER) {
                        updateMouseHover(mousePos);
                        if (selectedMenuIndex == 0) { reset(); setGameState(GameState::PLAYING); }
                        else if (selectedMenuIndex == 1) { previousState = GameState::GAME_OVER; setGameState(GameState::OPTIONS); selectedMenuIndex = 0; }
                        else if (selectedMenuIndex == 2) window.close();
                    }
                }
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                sf::Keyboard::Key key = keyPressed->code;

                if (isRebindingKey) {
                    if (key != sf::Keyboard::Key::Escape) restartKey = key;
                    isRebindingKey = false;
                    saveConfig();
                    continue;
                }

                if (isEditingText) {
                    if (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Escape) applyTextInputValue();
                    continue;
                }

                if (key == restartKey) {
                    reset();
                    setGameState(GameState::PLAYING);
                    continue;
                }

                if (key == sf::Keyboard::Key::Escape) {
                    if (openDropdownIndex != -1) { openDropdownIndex = -1; continue; }
                    if (state == GameState::PLAYING) { setGameState(GameState::PAUSED); selectedMenuIndex = 0; }
                    else if (state == GameState::PAUSED) { setGameState(GameState::PLAYING); }
                    else if (state == GameState::OPTIONS) { setGameState(previousState); selectedMenuIndex = 0; }
                    continue;
                }

                if (state == GameState::PLAYING) {
                    if (key == sf::Keyboard::Key::Up || key == sf::Keyboard::Key::W) pushDirection(UP);
                    else if (key == sf::Keyboard::Key::Down || key == sf::Keyboard::Key::S) pushDirection(DOWN);
                    else if (key == sf::Keyboard::Key::Left || key == sf::Keyboard::Key::A) pushDirection(LEFT);
                    else if (key == sf::Keyboard::Key::Right || key == sf::Keyboard::Key::D) pushDirection(RIGHT);
                }
                else if (state == GameState::PAUSED) {
                    handleMenuNavigation(key, 4, [this]() {
                        if (selectedMenuIndex == 0) setGameState(GameState::PLAYING);
                        else if (selectedMenuIndex == 1) { previousState = GameState::PAUSED; setGameState(GameState::OPTIONS); selectedMenuIndex = 0; }
                        else if (selectedMenuIndex == 2) { reset(); setGameState(GameState::PLAYING); }
                        else if (selectedMenuIndex == 3) window.close();
                        });
                }
                else if (state == GameState::GAME_OVER) {
                    handleMenuNavigation(key, 3, [this]() {
                        if (selectedMenuIndex == 0) { reset(); setGameState(GameState::PLAYING); }
                        else if (selectedMenuIndex == 1) { previousState = GameState::GAME_OVER; setGameState(GameState::OPTIONS); selectedMenuIndex = 0; }
                        else if (selectedMenuIndex == 2) window.close();
                        });
                }
                else if (state == GameState::OPTIONS) {
                    if (openDropdownIndex != -1) {
                        std::vector<std::string> opts = getDropdownOptions(openDropdownIndex);
                        int currentSel = getCurrentDropdownSelection(openDropdownIndex);
                        if (key == sf::Keyboard::Key::Up || key == sf::Keyboard::Key::W) {
                            currentSel = (currentSel - 1 + opts.size()) % opts.size();
                            selectDropdownOption(openDropdownIndex, currentSel);
                        }
                        else if (key == sf::Keyboard::Key::Down || key == sf::Keyboard::Key::S) {
                            currentSel = (currentSel + 1) % opts.size();
                            selectDropdownOption(openDropdownIndex, currentSel);
                        }
                        else if (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Space) {
                            openDropdownIndex = -1;
                        }
                    }
                    else {
                        if (key == sf::Keyboard::Key::Up || key == sf::Keyboard::Key::W) {
                            selectedMenuIndex = (selectedMenuIndex - 1 + 11) % 11;
                        }
                        else if (key == sf::Keyboard::Key::Down || key == sf::Keyboard::Key::S) {
                            selectedMenuIndex = (selectedMenuIndex + 1) % 11;
                        }
                        else if (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Space) {
                            if (selectedMenuIndex < 9) openDropdownIndex = selectedMenuIndex;
                            else if (selectedMenuIndex == 9) isRebindingKey = true;
                            else if (selectedMenuIndex == 10) { setGameState(previousState); selectedMenuIndex = 0; }
                        }
                    }
                }
            }
        }
    }

    void handleMenuNavigation(sf::Keyboard::Key key, int maxItems, std::function<void()> onSelect) {
        if (key == sf::Keyboard::Key::Up || key == sf::Keyboard::Key::W) {
            selectedMenuIndex = (selectedMenuIndex - 1 + maxItems) % maxItems;
        }
        else if (key == sf::Keyboard::Key::Down || key == sf::Keyboard::Key::S) {
            selectedMenuIndex = (selectedMenuIndex + 1) % maxItems;
        }
        else if (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::Space) {
            onSelect();
        }
    }

    void update() {
        if (state != GameState::PLAYING) return;

        if (moveClock.getElapsedTime().asSeconds() >= getActiveSpeedInterval()) {
            moveClock.restart();

            if (!inputQueue.empty()) {
                dir = inputQueue.front();
                inputQueue.pop_front();
            }

            prevSnake = snake;
            Point head = snake.front();
            switch (dir) {
            case UP:    head.y--; break;
            case DOWN:  head.y++; break;
            case LEFT:  head.x--; break;
            case RIGHT: head.x++; break;
            }

            int activeGridSize = getActiveGridSize();

            if (head.x < 0 || head.x >= activeGridSize || head.y < 0 || head.y >= activeGridSize) {
                setGameState(GameState::GAME_OVER);
                selectedMenuIndex = 0;
                return;
            }

            for (const auto& segment : snake) {
                if (head == segment) {
                    setGameState(GameState::GAME_OVER);
                    selectedMenuIndex = 0;
                    return;
                }
            }

            snake.insert(snake.begin(), head);

            auto foodIt = std::find(foods.begin(), foods.end(), head);
            if (foodIt != foods.end()) {
                score += 10;
                if (score > highScore) { highScore = score; saveConfig(); }
                foods.erase(foodIt);
                spawnFoods();
                prevSnake.push_back(prevSnake.back());
            }
            else {
                snake.pop_back();
            }
        }
    }

    void renderText(const std::string& str, float x, float y, unsigned int size, sf::Color color, bool center = false) {
        sf::Text text(font, str, size);
        text.setFillColor(color);
        if (center) {
            sf::FloatRect bounds = text.getLocalBounds();
            text.setOrigin({ bounds.position.x + bounds.size.x / 2.0f, bounds.position.y + bounds.size.y / 2.0f });
        }
        text.setPosition({ x, y });
        window.draw(text);
    }

    void render() {
        window.clear(sf::Color(15, 23, 42));

        // Playfield
        sf::RectangleShape playfield({ boardWidth, boardHeight });
        playfield.setPosition({ offsetX, offsetY });
        playfield.setFillColor(sf::Color(2, 6, 23));
        playfield.setOutlineThickness(3.0f);
        playfield.setOutlineColor(sf::Color(51, 65, 85));
        window.draw(playfield);

        // Score Bar
        renderText("SCORE: " + std::to_string(score) + "   HIGH SCORE: " + std::to_string(highScore) + "   [RESTART: " + getKeyName(restartKey) + "]",
            offsetX, offsetY - (cellSize * 1.2f), static_cast<unsigned int>(cellSize * 0.65f), sf::Color::White);

        // Render Food
        for (const auto& f : foods) {
            sf::RectangleShape foodShape({ cellSize - 2.0f, cellSize - 2.0f });
            foodShape.setPosition({ offsetX + f.x * cellSize + 1.0f, offsetY + f.y * cellSize + 1.0f });
            foodShape.setFillColor(sf::Color(239, 68, 68));
            window.draw(foodShape);
        }

        // Snake rendering logic
        float progress = 1.0f;
        if (movementStyle == MovementStyle::SMOOTH && state == GameState::PLAYING) {
            progress = std::min(1.0f, moveClock.getElapsedTime().asSeconds() / getActiveSpeedInterval());
        }

        float pad = 1.0f;
        float fullSize = cellSize - (2.0f * pad);

        for (size_t i = 1; i < snake.size(); ++i) {
            sf::RectangleShape segment({ fullSize, fullSize });
            segment.setPosition({ offsetX + snake[i].x * cellSize + pad, offsetY + snake[i].y * cellSize + pad });
            segment.setFillColor(sf::Color(34, 197, 94));
            window.draw(segment);
        }

        if (!snake.empty() && !prevSnake.empty()) {
            Point hCurr = snake[0];
            Point hPrev = prevSnake[0];

            int dx = hCurr.x - hPrev.x;
            int dy = hCurr.y - hPrev.y;

            sf::RectangleShape headShape;
            headShape.setFillColor(sf::Color(74, 222, 128));

            float animW = fullSize * progress;
            float animH = fullSize * progress;

            float cellX = offsetX + hCurr.x * cellSize + pad;
            float cellY = offsetY + hCurr.y * cellSize + pad;

            if (dx > 0) {
                headShape.setSize({ animW, fullSize });
                headShape.setPosition({ cellX, cellY });
            }
            else if (dx < 0) {
                headShape.setSize({ animW, fullSize });
                headShape.setPosition({ cellX + (fullSize - animW), cellY });
            }
            else if (dy > 0) {
                headShape.setSize({ fullSize, animH });
                headShape.setPosition({ cellX, cellY });
            }
            else if (dy < 0) {
                headShape.setSize({ fullSize, animH });
                headShape.setPosition({ cellX, cellY + (fullSize - animH) });
            }
            else {
                headShape.setSize({ fullSize, fullSize });
                headShape.setPosition({ cellX, cellY });
            }
            window.draw(headShape);
        }

        if (prevSnake.size() == snake.size() && prevSnake.size() > 1) {
            Point tOld = prevSnake.back();
            Point tNew = snake.back();

            int dx = tNew.x - tOld.x;
            int dy = tNew.y - tOld.y;

            sf::RectangleShape tailShape;
            tailShape.setFillColor(sf::Color(34, 197, 94));

            float remW = fullSize * (1.0f - progress);
            float remH = fullSize * (1.0f - progress);

            float cellX = offsetX + tOld.x * cellSize + pad;
            float cellY = offsetY + tOld.y * cellSize + pad;

            if (remW > 0.0f || remH > 0.0f) {
                if (dx > 0) {
                    tailShape.setSize({ remW, fullSize });
                    tailShape.setPosition({ cellX + (fullSize - remW), cellY });
                }
                else if (dx < 0) {
                    tailShape.setSize({ remW, fullSize });
                    tailShape.setPosition({ cellX, cellY });
                }
                else if (dy > 0) {
                    tailShape.setSize({ fullSize, remH });
                    tailShape.setPosition({ cellX, cellY + (fullSize - remH) });
                }
                else if (dy < 0) {
                    tailShape.setSize({ fullSize, remH });
                    tailShape.setPosition({ cellX, cellY });
                }
                else {
                    tailShape.setSize({ remW, remH });
                    tailShape.setPosition({ cellX, cellY });
                }
                window.draw(tailShape);
            }
        }

        // Overlays
        if (state == GameState::PAUSED) {
            drawMenuOverlay("PAUSED", { "Resume", "Options", "Restart Game", "Quit Game" });
        }
        else if (state == GameState::GAME_OVER) {
            drawMenuOverlay("GAME OVER", { "Play Again (" + getKeyName(restartKey) + ")", "Options", "Quit Game" });
        }
        else if (state == GameState::OPTIONS) {
            drawOptionsMenuOverlay();
        }

        window.display();
    }

    void drawMenuOverlay(const std::string& title, const std::vector<std::string>& items) {
        sf::RectangleShape overlay({ boardWidth, boardHeight });
        overlay.setPosition({ offsetX, offsetY });
        overlay.setFillColor(sf::Color(15, 23, 42, 230));
        window.draw(overlay);

        float centerX = offsetX + boardWidth / 2.0f;
        float startY = offsetY + (boardHeight * 0.2f);
        float scale = getMenuScale();

        renderText(title, centerX, startY, static_cast<unsigned int>(26.0f * scale), sf::Color(239, 68, 68), true);

        for (size_t i = 0; i < items.size(); ++i) {
            sf::Color itemColor = (static_cast<int>(i) == selectedMenuIndex) ? sf::Color(74, 222, 128) : sf::Color::White;
            std::string prefix = (static_cast<int>(i) == selectedMenuIndex) ? "> " : "  ";
            renderText(prefix + items[i], centerX, startY + (i + 1) * (35.0f * scale),
                static_cast<unsigned int>(18.0f * scale), itemColor, true);
        }
    }

    void drawOptionsMenuOverlay() {
        sf::RectangleShape overlay({ boardWidth, boardHeight });
        overlay.setPosition({ offsetX, offsetY });
        overlay.setFillColor(sf::Color(15, 23, 42, 245));
        window.draw(overlay);

        float centerX = offsetX + boardWidth / 2.0f;
        float startY = offsetY + 30.0f;
        float scale = getMenuScale();
        float rowSpacing = 30.0f * scale;

        renderText("SETTINGS", centerX, startY, static_cast<unsigned int>(22.0f * scale), sf::Color(59, 130, 246), true);

        std::vector<std::string> labels = {
            "Display Mode", "Resolution", "VSync", "FPS Limit",
            "Movement Style", "Snake Speed", "Fruit Count", "Grid Size",
            "Menu Scale", "Restart Key", "BACK"
        };

        for (size_t i = 0; i < labels.size(); ++i) {
            float itemY = startY + (i + 1) * rowSpacing;
            sf::Color textColor = (static_cast<int>(i) == selectedMenuIndex) ? sf::Color(74, 222, 128) : sf::Color::White;

            if (i < 9) { // Dropdown Options
                renderText(labels[i] + ":", centerX - (180.0f * scale), itemY, static_cast<unsigned int>(14.0f * scale), textColor);

                std::vector<std::string> opts = getDropdownOptions(static_cast<int>(i));
                int currSel = getCurrentDropdownSelection(static_cast<int>(i));
                std::string selText = (currSel < static_cast<int>(opts.size())) ? opts[currSel] : "Select";

                sf::RectangleShape dropBox({ 180.0f * scale, 24.0f * scale });
                dropBox.setPosition({ centerX + (10.0f * scale), itemY - (4.0f * scale) });
                dropBox.setFillColor(sf::Color(30, 41, 59));
                dropBox.setOutlineThickness(1.5f);
                dropBox.setOutlineColor((static_cast<int>(i) == selectedMenuIndex) ? sf::Color(74, 222, 128) : sf::Color(71, 85, 105));
                window.draw(dropBox);

                renderText(selText + " v", centerX + (20.0f * scale), itemY, static_cast<unsigned int>(13.0f * scale), sf::Color::White);
            }
            else if (i == 9) { // Keybinding
                renderText(labels[i] + ":", centerX - (180.0f * scale), itemY, static_cast<unsigned int>(14.0f * scale), textColor);
                std::string keyStr = isRebindingKey ? "[ Press Any Key... ]" : getKeyName(restartKey);
                renderText(keyStr, centerX + (10.0f * scale), itemY, static_cast<unsigned int>(14.0f * scale), sf::Color(250, 204, 21));
            }
            else { // Back Button
                renderText(labels[i], centerX, itemY, static_cast<unsigned int>(16.0f * scale), textColor, true);
            }
        }

        // Render Modal Text Field when Custom Value Editing is Active
        if (isEditingText) {
            sf::RectangleShape textOverlay({ 320.0f * scale, 90.0f * scale });
            textOverlay.setPosition({ centerX - (160.0f * scale), startY + (selectedMenuIndex + 1) * rowSpacing - (20.0f * scale) });
            textOverlay.setFillColor(sf::Color(15, 23, 42));
            textOverlay.setOutlineThickness(2.0f);
            textOverlay.setOutlineColor(sf::Color(250, 204, 21));
            window.draw(textOverlay);

            renderText("TYPE VALUE & PRESS ENTER:", centerX, startY + (selectedMenuIndex + 1) * rowSpacing - (10.0f * scale),
                static_cast<unsigned int>(12.0f * scale), sf::Color(250, 204, 21), true);

            renderText("[ " + textInputBuffer + "_ ]", centerX, startY + (selectedMenuIndex + 1) * rowSpacing + (20.0f * scale),
                static_cast<unsigned int>(16.0f * scale), sf::Color::White, true);
        }

        // Render Dropdown Expansion with Light Green Hover Highlighting
        if (openDropdownIndex != -1) {
            std::vector<std::string> opts = getDropdownOptions(openDropdownIndex);
            float boxY = startY + (openDropdownIndex + 1) * rowSpacing;
            float boxW = 180.0f * scale;
            float itemH = 24.0f * scale;

            sf::RectangleShape listBg({ boxW, static_cast<float>(opts.size() * itemH + (6.0f * scale)) });
            listBg.setPosition({ centerX + (10.0f * scale), boxY + (20.0f * scale) });
            listBg.setFillColor(sf::Color(15, 23, 42));
            listBg.setOutlineThickness(2.0f);
            listBg.setOutlineColor(sf::Color(74, 222, 128));
            window.draw(listBg);

            for (size_t optIdx = 0; optIdx < opts.size(); ++optIdx) {
                float itemY = boxY + (24.0f * scale) + (optIdx * itemH);
                bool isCurrent = (getCurrentDropdownSelection(openDropdownIndex) == static_cast<int>(optIdx));
                bool isHovered = (hoveredDropdownOption == static_cast<int>(optIdx));

                // Soft green hover highlight backdrop
                if (isHovered) {
                    sf::RectangleShape hoverHighlight({ boxW - (4.0f * scale), itemH });
                    hoverHighlight.setPosition({ centerX + (12.0f * scale), itemY - (10.0f * scale) });
                    hoverHighlight.setFillColor(sf::Color(34, 197, 94, 60)); // Soft Green
                    window.draw(hoverHighlight);
                }

                sf::Color optColor = isHovered ? sf::Color(134, 239, 172) : (isCurrent ? sf::Color(74, 222, 128) : sf::Color::White);
                renderText(opts[optIdx], centerX + (20.0f * scale), itemY, static_cast<unsigned int>(12.0f * scale), optColor);
            }
        }
    }

    void run() {
        while (window.isOpen()) {
            handleInput();
            update();
            render();
        }
    }
};

int main() {
    SnakeGame game;
    game.run();
    return 0;
}