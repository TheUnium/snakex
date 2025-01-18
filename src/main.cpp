#include "raylib.h"
#include <deque>
#include <random>
#include <vector>
#include <cmath>

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;
constexpr int GRID_SIZE = 20;
constexpr float SNAKE_SPEED = 15.0f;
constexpr int GRID_DOT_SIZE = 2;
constexpr Color BASE_GRID_COLOR = {30, 30, 30, 255};

struct SnakeSegment {
    int x, y;
};

struct Food {
    int x{}, y{};
    float pulseTimer = 0;
    const float PULSE_SPEED = 2.0f;
};

struct GridDot {
    float brightness = 1.0f;
    Color tint = BASE_GRID_COLOR;
    float animTimer = 0.0f;
};

struct Particle {
    Vector2 position;
    Vector2 velocity;
    float rotation;
    float rotationSpeed;
    float size;
    float alpha;
    Color color;
};

struct DeathAnimation {
    std::vector<Particle> particles;
    float timer = 0.0f;
    float DURATION = 3.0f;
    Vector2 deathPosition{};
    bool started = false;
    bool finished = false;
};

class Game {
    std::deque<SnakeSegment> snake;
    Food food;
    int dx = 1, dy = 0;
    int score = 0;
    bool gameOver = false;
    float timeSinceLastMove = 0;
    float screenShake = 0.0f;
    float spaceButtonTilt = 0.0f;
    DeathAnimation deathAnim;
    float gameOverTextScale = 0.0f;
    float menuAlpha = 0.0f;

    std::vector<std::vector<GridDot>> gridDots;
    bool isCollectAnimation = false;
    float collectAnimTimer = 0.0f;

    void InitializeGrid() {
        gridDots.resize(SCREEN_WIDTH / GRID_SIZE);
        for (auto& column : gridDots) {
            column.resize(SCREEN_HEIGHT / GRID_SIZE);
        }
    }

    void UpdateGridAnimations(float deltaTime) {
        food.pulseTimer += deltaTime;
        float pulseBrightness = (std::sin(food.pulseTimer * food.PULSE_SPEED * PI * 2) + 1) * 0.5f;

        for (int x = 0; x < gridDots.size(); x++) {
            for (int y = 0; y < gridDots[x].size(); y++) {
                auto& dot = gridDots[x][y];

                dot.brightness = 1.0f;
                dot.tint = BASE_GRID_COLOR;

                if (x == food.x || y == food.y) {
                    float distance = std::abs(x - food.x) + std::abs(y - food.y);
                    float pulseIntensity = pulseBrightness * (1.0f - (distance * 0.1f));
                    pulseIntensity = std::max(0.0f, pulseIntensity);

                    dot.tint.r = static_cast<unsigned char>(std::min(255.0f, BASE_GRID_COLOR.r + (50.0f * pulseIntensity)));
                    dot.tint.g = static_cast<unsigned char>(std::min(255.0f, BASE_GRID_COLOR.g + (50.0f * pulseIntensity)));
                    dot.tint.b = static_cast<unsigned char>(std::min(255.0f, BASE_GRID_COLOR.b + (50.0f * pulseIntensity)));
                }

                if (isCollectAnimation) {
                    const float normalizedTime = collectAnimTimer / 0.5f;
                    const float rainbow = std::sin(normalizedTime * PI * 2 + (x + y) * 0.5f);
                    dot.tint = ColorFromHSV(rainbow * 360, 0.7f, 1.0f);
                    dot.brightness = 1.0f - normalizedTime;
                }
            }
        }

        if (isCollectAnimation) {
            collectAnimTimer += deltaTime;
            if (collectAnimTimer >= 0.5f) {
                isCollectAnimation = false;
                collectAnimTimer = 0.0f;
            }
        }

        if (screenShake > 0) {
            screenShake *= 0.9f;
            if (screenShake < 0.1f) screenShake = 0;
        }

        if (spaceButtonTilt != 0) {
            spaceButtonTilt *= 0.9f;
            if (std::abs(spaceButtonTilt) < 0.01f) spaceButtonTilt = 0;
        }
    }

    void DrawGrid() const {
        for (int x = 0; x < gridDots.size(); x++) {
            for (int y = 0; y < gridDots[x].size(); y++) {
                const auto& dot = gridDots[x][y];

                const int dotX = x * GRID_SIZE + GRID_SIZE/2 - GRID_DOT_SIZE/2;
                const int dotY = y * GRID_SIZE + GRID_SIZE/2 - GRID_DOT_SIZE/2;
                DrawRectangle(dotX, dotY, GRID_DOT_SIZE, GRID_DOT_SIZE, ColorAlpha(dot.tint, dot.brightness));
            }
        }
    }

    void SpawnFood() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disX(0, (SCREEN_WIDTH / GRID_SIZE) - 1);
        std::uniform_int_distribution<> disY(0, (SCREEN_HEIGHT / GRID_SIZE) - 1);

        food.x = disX(gen);
        food.y = disY(gen);
        food.pulseTimer = 0;

        for (const auto&[x, y] : snake) {
            if (x == food.x && y == food.y) {
                SpawnFood();
                return;
            }
        }
    }

    void InitializeDeathAnimation(int gridX, int gridY) {
        deathAnim.particles.clear();
        deathAnim.timer = 0.0f;
        deathAnim.started = true;
        deathAnim.finished = false;
        deathAnim.deathPosition = {
            static_cast<float>((gridX * GRID_SIZE + GRID_SIZE/2)),
            static_cast<float>((gridY * GRID_SIZE + GRID_SIZE/2))
        };

        for (int i = 0; i < 50; i++) {
            const float angle = GetRandomValue(0, 360) * DEG2RAD;
            const float speed = GetRandomValue(100, 300);

            Particle p{};
            p.position = deathAnim.deathPosition;
            p.velocity = {
                cosf(angle) * speed,
                sinf(angle) * speed
            };
            p.rotation = GetRandomValue(0, 360) * DEG2RAD;
            p.rotationSpeed = GetRandomValue(-720, 720) * DEG2RAD;
            p.size = GetRandomValue(2, 8);
            p.alpha = 1.0f;
            p.color = (i % 2 == 0) ? BLUE : SKYBLUE;

            deathAnim.particles.push_back(p);
        }
    }

    void UpdateDeathAnimation(float deltaTime) {
        if (!deathAnim.started) return;

        deathAnim.timer += deltaTime;

        for (auto& p : deathAnim.particles) {
            p.velocity.y += 980.0f * deltaTime;
            p.position.x += p.velocity.x * deltaTime;
            p.position.y += p.velocity.y * deltaTime;
            p.rotation += p.rotationSpeed * deltaTime;
            p.alpha = std::max(0.0f, 1.0f - (deathAnim.timer / deathAnim.DURATION));
            p.size *= 0.99f;
        }

        if (deathAnim.timer >= deathAnim.DURATION) {
            deathAnim.finished = true;
        }
    }

    void DrawDeathAnimation() {
        if (!deathAnim.started) return;

        for (const auto& p : deathAnim.particles) {
            const Rectangle rect = {
                p.position.x - p.size/2,
                p.position.y - p.size/2,
                p.size,
                p.size
            };

            DrawRectanglePro(
                rect,
                {p.size/2, p.size/2},
                p.rotation * RAD2DEG,
                ColorAlpha(p.color, p.alpha)
            );
        }
    }

    bool CheckCollision() {
        if (snake.front().x < 0 || snake.front().x >= SCREEN_WIDTH / GRID_SIZE ||
            snake.front().y < 0 || snake.front().y >= SCREEN_HEIGHT / GRID_SIZE) {
            if (!gameOver) {
                InitializeDeathAnimation(snake.front().x, snake.front().y);
            }
            return true;
        }

        for (size_t i = 1; i < snake.size(); i++) {
            if (snake.front().x == snake[i].x && snake.front().y == snake[i].y) {
                if (!gameOver) {
                    InitializeDeathAnimation(snake.front().x, snake.front().y);
                }
                return true;
            }
        }
        return false;
    }

    void DrawGameOverScreen() {
        if (gameOverTextScale < 1.0f) {
            gameOverTextScale += 0.05f;
            if (gameOverTextScale > 1.0f) gameOverTextScale = 1.0f;
        }

        if (menuAlpha < 1.0f) {
            menuAlpha += 0.02f;
            if (menuAlpha > 1.0f) menuAlpha = 1.0f;
        }

        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f * menuAlpha));

        const auto* gameOverText = "GAME OVER";
        constexpr int gameOverFontSize = 80;
        const int gameOverTextWidth = MeasureText(gameOverText, gameOverFontSize);
        constexpr Vector2 gameOverPos = {
            SCREEN_WIDTH + 80,
            SCREEN_HEIGHT/2.0f - 60
        };

        DrawTextPro(
            GetFontDefault(),
            gameOverText,
            {gameOverPos.x - gameOverTextWidth * gameOverTextScale, gameOverPos.y},
            {gameOverTextWidth/2.0f, gameOverFontSize/2.0f},
            0,
            static_cast<float>(gameOverFontSize),
            10,
            ColorAlpha(RED, menuAlpha)
        );

        const char* finalScoreText = TextFormat("FINAL SCORE: %d", score);
        constexpr int scoreFontSize = 20;
        const int scoreTextWidth = MeasureText(finalScoreText, scoreFontSize);
        DrawText(
            finalScoreText,
            SCREEN_WIDTH/2 - scoreTextWidth/2,
            static_cast<int>(gameOverPos.y) + 30,
            scoreFontSize,
            ColorAlpha(WHITE, menuAlpha)
        );

        const float floatOffset = sinf(static_cast<float>(GetTime() * 2)) * 5.0f;
        const auto restartText = "Press  ";
        const auto spaceText = "SPACE";
        const auto toRestartText = " to restart";

        constexpr int restartFontSize = 25;
        const int restartTextWidth = MeasureText(restartText, restartFontSize) +
                               MeasureText(spaceText, restartFontSize) +
                               MeasureText(toRestartText, restartFontSize);

        const int textX = SCREEN_WIDTH / 2 - restartTextWidth / 2;
        const int spaceX = textX + MeasureText(restartText, restartFontSize);
        const int textY = static_cast<int>(gameOverPos.y + 140 + floatOffset);

        DrawText(restartText, textX, textY, restartFontSize, ColorAlpha(GRAY, menuAlpha));
        const int spaceWidth = MeasureText(spaceText, restartFontSize);
        DrawRectangle(
            spaceX - 5,
            textY - 2,
            spaceWidth + 10,
            restartFontSize + 4,
            ColorAlpha(WHITE, menuAlpha)
        );

        DrawText(spaceText, spaceX, textY, restartFontSize, ColorAlpha(BLACK, menuAlpha));
        DrawText(toRestartText, spaceX + spaceWidth + 10, textY, restartFontSize, ColorAlpha(GRAY, menuAlpha));
    }

    void DrawSnakeEyes() {
        const auto& head = snake.front();
        constexpr int eyeSize = 2;
        constexpr int eyeOffset = 5;

        Vector2 leftEye = {
            static_cast<float>(head.x * GRID_SIZE + GRID_SIZE / 2 - eyeOffset),
            static_cast<float>(head.y * GRID_SIZE + eyeOffset)
        };
        Vector2 rightEye = {
            static_cast<float>(head.x * GRID_SIZE + GRID_SIZE / 2 - eyeOffset),
            static_cast<float>(head.y * GRID_SIZE + GRID_SIZE - eyeOffset)
        };

        if (dx == 1) {
            leftEye.x += eyeOffset * 2;
            rightEye.x += eyeOffset * 2;
        } else if (dy == 1) {
            leftEye = {
                static_cast<float>(head.x * GRID_SIZE + eyeOffset),
                static_cast<float>(head.y * GRID_SIZE + GRID_SIZE / 2 + eyeOffset)
            };
            rightEye = {
                static_cast<float>(head.x * GRID_SIZE + GRID_SIZE - eyeOffset),
                static_cast<float>(head.y * GRID_SIZE + GRID_SIZE / 2 + eyeOffset)
            };
        } else if (dy == -1) {
            leftEye = {
                static_cast<float>(head.x * GRID_SIZE + eyeOffset),
                static_cast<float>(head.y * GRID_SIZE + GRID_SIZE / 2 - eyeOffset)
            };
            rightEye = {
                static_cast<float>(head.x * GRID_SIZE + GRID_SIZE - eyeOffset),
                static_cast<float>(head.y * GRID_SIZE + GRID_SIZE / 2 - eyeOffset)
            };
        }

        DrawCircle(leftEye.x, leftEye.y, eyeSize, BLACK);
        DrawCircle(rightEye.x, rightEye.y, eyeSize, BLACK);
    }

public:
    Game() {
        InitializeGrid();
        Reset();
    }

    void Update(float deltaTime) {
        UpdateGridAnimations(deltaTime);
        UpdateDeathAnimation(deltaTime);

        if (gameOver) {
            if (IsKeyPressed(KEY_SPACE)) {
                Reset();
                return;
            }
            return;
        }

        if (IsKeyPressed(KEY_UP) && dy == 0) { dx = 0; dy = -1; }
        if (IsKeyPressed(KEY_DOWN) && dy == 0) { dx = 0; dy = 1; }
        if (IsKeyPressed(KEY_LEFT) && dx == 0) { dx = -1; dy = 0; }
        if (IsKeyPressed(KEY_RIGHT) && dx == 0) { dx = 1; dy = 0; }

        timeSinceLastMove += deltaTime;
        if (timeSinceLastMove >= 1.0f / SNAKE_SPEED) {
            const SnakeSegment newHead = {
                snake.front().x + dx,
                snake.front().y + dy
            };
            snake.push_front(newHead);

            if (newHead.x == food.x && newHead.y == food.y) {
                score += 100;
                isCollectAnimation = true;
                collectAnimTimer = 0.0f;
                SpawnFood();
            } else {
                snake.pop_back();
            }

            if (CheckCollision()) {
                gameOver = true;
            }

            timeSinceLastMove = 0;
        }
    }

    void Draw() {
        BeginDrawing();

        if (screenShake > 0) {
            const float offsetX = static_cast<float>(GetRandomValue(-1, 1)) * screenShake;
            const float offsetY = static_cast<float>(GetRandomValue(-1, 1)) * screenShake;
            BeginMode2D((Camera2D){ { offsetX, offsetY }, { 0, 0 }, 0, 1.0f });
        }

        ClearBackground(BLACK);
        DrawGrid();

        DrawRectangle(food.x * GRID_SIZE, food.y * GRID_SIZE, GRID_SIZE, GRID_SIZE, RED);
        DrawText(TextFormat("SCORE: %d", score), 10, 10, 20, WHITE);

        for (const auto& segment : snake) {
            DrawRectangle(
                segment.x * GRID_SIZE,
                segment.y * GRID_SIZE,
                GRID_SIZE - 1,
                GRID_SIZE - 1,
                BLUE
            );
        }

        DrawSnakeEyes();
        DrawDeathAnimation();

        if (screenShake > 0) {
            EndMode2D();
        }

        if (gameOver) {
            DrawGameOverScreen();
        }

        EndDrawing();
    }

    void Reset() {
        snake.clear();
        snake.push_front({5, 5});
        snake.push_front({6, 5});
        snake.push_front({7, 5});
        dx = 1;
        dy = 0;
        score = 0;
        gameOver = false;
        screenShake = 0;
        spaceButtonTilt = 0;
        deathAnim.particles.clear();
        deathAnim.timer = 0.0f;
        deathAnim.started = false;
        deathAnim.finished = false;
        deathAnim.deathPosition = {0, 0};
        gameOverTextScale = 0.0f;
        menuAlpha = 0.0f;

        SpawnFood();
    }

    [[nodiscard]] bool IsGameOver() const { return gameOver; }
};

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SnakeX");
    SetTargetFPS(60);

    Game game;

    while (!WindowShouldClose()) {
        game.Update(GetFrameTime());
        game.Draw();
    }

    CloseWindow();
    return 0;
}