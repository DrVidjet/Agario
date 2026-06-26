#include <raylib.h>
#include <raymath.h>
#include <random>
#include <time.h>
#include <cfloat>

#define GRID_SIZE 200

const int FPS = 165;
const int map_size = 100000;
const int map_margin = 100;
const int CHUNK_SIZE = 1000;

const int minPlayerSpeed = 100;
const float startPlayerSpeed = 400;
const int startPlayerSize = 50;

const int enemyCount = map_size / 50;
const int startEnemySize = 50;
const int startEnemySizeBuff = 200;
const float searchEnemyRadius = 3000.0f;
const float enemyActiveRadius = 10000.0f;
const float searchEnemyRadiusSqr = searchEnemyRadius * searchEnemyRadius;

const int scoresCount = map_size * 2;
const int scoreCollision = 21;
const int scoreSize = 10;

bool playerAlive = true;

// ── Scores ──────────────────────────────────────────────────────────────────

class Scores {
public:
    Vector2 pos;
    bool active;
};

// ── Чанки ───────────────────────────────────────────────────────────────────

struct Chunk {
    std::vector<Scores*> scores;
};

Chunk grid[GRID_SIZE][GRID_SIZE];

// Зажимаем индекс чанка в допустимых границах
int ClampChunk(int v) {
    if (v < 0) return 0;
    if (v >= GRID_SIZE) return GRID_SIZE - 1;
    return v;
}

int GetChunkX(Vector2 pos) { return ClampChunk((int)((pos.x + map_size) / CHUNK_SIZE)); }
int GetChunkY(Vector2 pos) { return ClampChunk((int)((pos.y + map_size) / CHUNK_SIZE)); }

void AddScoreToChunk(Scores* s) {
    grid[GetChunkX(s->pos)][GetChunkY(s->pos)].scores.push_back(s);
}

// ── Вспомогательная позиция ──────────────────────────────────────────────────

Vector2 RandPos() {
    return { (float)(-map_size + rand() % (map_size * 2)),
        (float)(-map_size + rand() % (map_size * 2)) };
}

// ── Player ───────────────────────────────────────────────────────────────────

class Player {
public:
    Vector2 pos;
    int size;
    int score;
    float speed;

    void Draw() {
        DrawCircleV(pos, (float)size, GREEN);
    }
};

// ── Enemy ────────────────────────────────────────────────────────────────────

class Enemy {
public:
    Vector2 pos;
    bool active;
    int size;
    float speed;
    Scores* target = nullptr;

    int GetScore() { return size - 50; }

    void Collision(Player& player) {
        if (!active) return;
        if (!CheckCollisionCircles(player.pos, (float)player.size, pos, (float)size)) return;

        if (player.size > size) {
            active = false;
            player.score += GetScore();
            player.size  += GetScore();
        } else {
            playerAlive = false;
        }
    }

    void MoveTo(Vector2 target, float dt) {
        Vector2 dir = Vector2Normalize(Vector2Subtract(target, pos));
        pos.x += dir.x * speed * dt;
        pos.y += dir.y * speed * dt;
    }

    void EatNearbyScores() {
        int cx = GetChunkX(pos);
        int cy = GetChunkY(pos);
        for (int x = ClampChunk(cx - 1); x <= ClampChunk(cx + 1); x++) {
            for (int y = ClampChunk(cy - 1); y <= ClampChunk(cy + 1); y++) {
                for (Scores* s : grid[x][y].scores) {
                    if (!s->active) continue;
                    if (CheckCollisionCircles(pos, (float)size, s->pos, (float)scoreCollision)) {
                        s->active = false;
                        size++;
                        if (speed > minPlayerSpeed) speed--;
                        if (s == target) target = nullptr;
                    }
                }
            }
        }
    }

    void AI(Player& player, float dt, Enemy* enemies, int enemyCount) {
        if (!active) return;
        if (Vector2DistanceSqr(pos, player.pos) > enemyActiveRadius * enemyActiveRadius) return;

        Collision(player);

        // Ищем ближайшее очко
        if (target == nullptr || !target->active) {
            target = nullptr;
            float minDist = searchEnemyRadiusSqr;
            int cx = GetChunkX(pos);
            int cy = GetChunkY(pos);
            for (int x = ClampChunk(cx - 1); x <= ClampChunk(cx + 1); x++) {
                for (int y = ClampChunk(cy - 1); y <= ClampChunk(cy + 1); y++) {
                    for (Scores* s : grid[x][y].scores) {
                        if (!s->active) continue;
                        float d = Vector2DistanceSqr(pos, s->pos);
                        if (d < minDist) { minDist = d; target = s; }
                    }
                }
            }
        }

        // Ищем ближайшего врага меньше нас
        Enemy* enemyTarget = nullptr;
        float minEnemyDist = searchEnemyRadiusSqr;
        for (int i = 0; i < enemyCount; i++) {
            Enemy* e = &enemies[i];
            if (!e->active || e == this || e->size >= size) continue;
            float d = Vector2DistanceSqr(pos, e->pos);
            if (d < minEnemyDist) { minEnemyDist = d; enemyTarget = e; }
        }

        float distToPlayer = Vector2DistanceSqr(pos, player.pos);
        float distToScore  = (target != nullptr)      ? Vector2DistanceSqr(pos, target->pos)      : FLT_MAX;
        float distToEnemy  = (enemyTarget != nullptr)  ? Vector2DistanceSqr(pos, enemyTarget->pos) : FLT_MAX;

        // Определяем приоритет цели: ближайшее из того, что можем съесть
        bool playerInRange = distToPlayer < searchEnemyRadiusSqr;
        bool chasePlayer = playerInRange && (player.size < size) && (distToPlayer < distToScore) && (distToPlayer < distToEnemy);
        bool chaseEnemy  = (enemyTarget != nullptr)  && (distToEnemy  < distToScore) && (distToEnemy  < distToPlayer || !chasePlayer);

        if (chasePlayer) {
            MoveTo(player.pos, dt);
        } else if (chaseEnemy) {
            MoveTo(enemyTarget->pos, dt);
            // Коллизия с врагом-целью
            if (CheckCollisionCircles(pos, (float)size, enemyTarget->pos, (float)enemyTarget->size)) {
                enemyTarget->active = false;
                size += enemyTarget->GetScore();
            }
        } else {
            if (target == nullptr) return;
            MoveTo(target->pos, dt);
        }

        // Очки едим всегда, независимо от того кого преследуем
        EatNearbyScores();
    }

    void Draw(float left, float right, float top, float bottom) {
        if (!active) return;
        if (pos.x < left - startEnemySize || pos.x > right + startEnemySize ||
            pos.y < top  - startEnemySize || pos.y > bottom + startEnemySize) return;
        DrawCircleV(pos, (float)size, RED);
    }
};

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
    srand(time(NULL));
    InitWindow(GetScreenWidth(), GetScreenHeight(), "Camera2D");
    int WIDTH = GetScreenWidth();
    int HEIGHT = GetScreenHeight();
    SetTargetFPS(FPS);

    Camera2D camera = { 0 };
    camera.offset = { WIDTH / 2.0f, HEIGHT / 2.0f };
    camera.zoom   = 1.0f;

    // Игрок
    Player player;
    player.pos   = { 0, 0 };
    player.size  = startPlayerSize;
    player.score = 0;
    player.speed = startPlayerSpeed;

    // Враги
    Enemy enemy[enemyCount];
    for (int i = 0; i < enemyCount; i++) {
        enemy[i].pos    = RandPos();
        enemy[i].active = true;
        enemy[i].size   = startEnemySize + rand() % startEnemySizeBuff;
        enemy[i].speed  = startPlayerSpeed;
    }

    // Очки — сначала создаём, потом регистрируем в чанках
    Scores scores[scoresCount];
    for (int i = 0; i < scoresCount; i++) {
        scores[i].pos    = RandPos();
        scores[i].active = true;
        AddScoreToChunk(&scores[i]);
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Движение игрока
        if (playerAlive) if (IsKeyDown(KEY_W)) player.pos.y -= player.speed * dt;
        if (playerAlive) if (IsKeyDown(KEY_S)) player.pos.y += player.speed * dt;
        if (playerAlive) if (IsKeyDown(KEY_A)) player.pos.x -= player.speed * dt;
        if (playerAlive) if (IsKeyDown(KEY_D)) player.pos.x += player.speed * dt;

        // AI врагов
        if (playerAlive) for (int i = 0; i < enemyCount; i++) enemy[i].AI(player, dt, enemy, enemyCount);

        // Коллизия очков с игроком — тоже через чанки
        {
            int cx = GetChunkX(player.pos);
            int cy = GetChunkY(player.pos);
            for (int x = ClampChunk(cx - 1); x <= ClampChunk(cx + 1); x++) {
                for (int y = ClampChunk(cy - 1); y <= ClampChunk(cy + 1); y++) {
                    for (Scores* s : grid[x][y].scores) {
                        if (!s->active) continue;
                        if (CheckCollisionCircles(player.pos, (float)player.size,
                            s->pos, (float)scoreCollision)) {
                            s->active = false;
                        player.score++;
                        player.size++;
                        if (player.speed > minPlayerSpeed) player.speed--;
                            }
                    }
                }
            }
        }

        // Отрисовка
        BeginDrawing();
        ClearBackground(DARKGRAY);

        float viewWidth  = WIDTH  / camera.zoom;
        float viewHeight = HEIGHT / camera.zoom;
        float left   = camera.target.x - viewWidth  / 2;
        float right  = camera.target.x + viewWidth  / 2;
        float top    = camera.target.y - viewHeight / 2;
        float bottom = camera.target.y + viewHeight / 2;

        BeginMode2D(camera);

        // Сетка
        int startX = ((int)(left   - map_margin) / map_margin) * map_margin;
        int endX   = ((int)(right  + map_margin) / map_margin + 1) * map_margin;
        int startY = ((int)(top    - map_margin) / map_margin) * map_margin;
        int endY   = ((int)(bottom + map_margin) / map_margin + 1) * map_margin;
        for (int x = startX; x <= endX; x += map_margin) DrawLine(x, startY, x, endY, GRAY);
        for (int y = startY; y <= endY; y += map_margin) DrawLine(startX, y, endX, y, GRAY);

        if (playerAlive) player.Draw();
        for (int i = 0; i < enemyCount; i++) enemy[i].Draw(left, right, top, bottom);

        // Очки — рисуем только из видимых чанков
        int cxL = ClampChunk(GetChunkX({left,  top}));
        int cxR = ClampChunk(GetChunkX({right, top}));
        int cyT = ClampChunk(GetChunkY({left,  top}));
        int cyB = ClampChunk(GetChunkY({left,  bottom}));
        for (int x = cxL; x <= cxR; x++)
            for (int y = cyT; y <= cyB; y++)
                for (Scores* s : grid[x][y].scores)
                    if (s->active) DrawCircleV(s->pos, scoreSize, YELLOW);

        // Зум
        float targetZoom = 1.0f / (1.0f + player.size / 500.0f);
        if (targetZoom < 0.02f) targetZoom = 0.02f;

        // Даём игроку крутить колесо ±50% от целевого
        float zoomRange = targetZoom * 0.5f;
        float minZoom = targetZoom - zoomRange;
        float maxZoom = targetZoom + zoomRange;
        if (minZoom < 0.02f) minZoom = 0.02f;

        if (playerAlive) camera.zoom += GetMouseWheelMove() * 0.05f;
        camera.zoom  = Clamp(camera.zoom, minZoom, maxZoom);
        camera.target = player.pos;

        EndMode2D();

        if (playerAlive) {
            DrawText(TextFormat("Pos: %.0f %.0f\nScore: %d", player.pos.x, player.pos.y, player.score), 10, 40, 20, WHITE);
            DrawFPS(10, 10);
        } else {
            int btnWidth = WIDTH/3.2;
            int btnHeight = HEIGHT/7;

            int btnX = (WIDTH - btnWidth) / 2;
            int btnY = (HEIGHT - btnHeight) / 2;

            Rectangle btn = { (float)btnX, (float)btnY, (float)btnWidth, (float)btnHeight };

            Vector2 mouse = GetMousePosition();

            bool hovered = CheckCollisionPointRec(mouse, btn);

            bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

            // логика
            if (clicked)
            {
                player.size = startPlayerSize;
                player.score = 0;
                player.speed = startPlayerSpeed;
                player.pos = RandPos();
                playerAlive = true;
            }

            Color color = hovered ? DARKGRAY : GRAY;

            DrawRectangleRec(btn, color);
            DrawText("Respawn", btn.x+WIDTH/25, btn.y+HEIGHT/30, HEIGHT/10, WHITE);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
