#include <raylib.h>
#include <raymath.h>
#include <random>
#include <time.h>

const int FPS = 165;
const int WIDTH = 1920;
const int HEIGHT = 1080;
const int map_size = 100000;
const int scoresCount = map_size*2;
const int enemyCount = map_size/100;



Vector2 pos()
{
    Vector2 pos;

    pos.x = -map_size + rand() % (map_size * 2);
    pos.y = -map_size + rand() % (map_size * 2);

    return pos;
}

class Enemy
{
public:
    Vector2 pos;
    bool active;
    int size;
    float speed;

    int GetScore()
    {
        return size - 50;
    }
};

class Scores
{
public:
    Vector2 pos;
    bool active;
};

class Player
{
public:
    Vector2 pos;
    int size;
    int score;
    float speed;
};



int main() {
    srand(time(NULL));

    InitWindow(WIDTH, HEIGHT, "Camera2D");
    SetTargetFPS(FPS);



    // Камера
    Camera2D camera = { 0 };
    camera.offset   = { WIDTH / 2.0f, HEIGHT / 2.0f };
    camera.zoom = 1.0f;


    // Инициализация врагов
    Enemy enemy[enemyCount];
    for (int i = 0; i < enemyCount; i++) enemy[i].pos = pos(), enemy[i].active = true, enemy[i].size = 50+rand()%400, enemy[i].speed = 400;

    // Инициализация очков
    Scores scores[scoresCount];
    for (int i = 0; i < scoresCount; i++) scores[i].pos = pos(), scores[i].active = true;


    // Инициализация игрока
    Player player;
    player.pos = pos();
    player.size = 50;
    player.score = 0;
    player.speed = 400.0f;



    // Основной цикл
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Движение
        if (IsKeyDown(KEY_W)) player.pos.y -= player.speed * dt;
        if (IsKeyDown(KEY_S)) player.pos.y += player.speed * dt;
        if (IsKeyDown(KEY_A)) player.pos.x -= player.speed * dt;
        if (IsKeyDown(KEY_D)) player.pos.x += player.speed * dt;

        // Коллизия врагов
        for (int i = 0; i < enemyCount; i++)
        {
            if (!enemy[i].active)
                continue;

            bool collision =
            CheckCollisionCircles(
                player.pos,
                player.size,
                enemy[i].pos,
                enemy[i].size);

            if (!collision)
                continue;

            if (player.size > enemy[i].size)
            {
                enemy[i].active = false;
                player.score += enemy[i].GetScore();
                player.size += enemy[i].GetScore();
            }
            else
            {
                player.size = 10;
                player.speed = 400;
                player.score = 0;
            }
        }

        // Коллизия очков
        for (int i = 0; i < scoresCount; i++) if (scores[i].active && CheckCollisionCircles(player.pos, player.size,  scores[i].pos, 21)) {
            scores[i].active = false;
            player.score += 1;
            player.size += 1;
            if(player.speed > 100) player.speed -= 1;
        }



        // ОТРИСОВКА
        BeginDrawing();
        ClearBackground(DARKGRAY);

        float viewWidth  = WIDTH / camera.zoom;
        float viewHeight = HEIGHT / camera.zoom;

        float left   = camera.target.x - viewWidth  / 2;
        float right  = camera.target.x + viewWidth  / 2;
        float top    = camera.target.y - viewHeight / 2;
        float bottom = camera.target.y + viewHeight / 2;

        BeginMode2D(camera);
        // Мировая сетка для ориентира
        int margin = 100;
        int startX = ((int)(left - margin) / 100) * 100;
        int endX   = ((int)(right + margin) / 100 + 1) * 100;

        int startY = ((int)(top - margin) / 100) * 100;
        int endY   = ((int)(bottom + margin) / 100 + 1) * 100;

        for (int x = startX; x <= endX; x += 100)
            DrawLine(x, startY, x, endY, GRAY);

        for (int y = startY; y <= endY; y += 100)
            DrawLine(startX, y, endX, y, GRAY);

        // Игрок
        DrawCircleV(player.pos, player.size, GREEN);

        // Враги
        for (int i = 0; i < enemyCount; i++) {
            if (!enemy[i].active) continue;

            if (enemy[i].pos.x < left - 50 ||
                enemy[i].pos.x > right + 50 ||
                enemy[i].pos.y < top - 50 ||
                enemy[i].pos.y > bottom + 50)
                continue;

            DrawCircleV(enemy[i].pos, enemy[i].size, RED);
        }

        // Очки
        for (int i = 0; i < scoresCount; i++) {
            if (!scores[i].active) continue;

            if (scores[i].pos.x < left - 10 ||
                scores[i].pos.x > right + 10 ||
                scores[i].pos.y < top - 10 ||
                scores[i].pos.y > bottom + 10)
                continue;

            DrawCircleV(scores[i].pos, 10, YELLOW);
        }

        float minZoom = 0.2f - player.size / 10000.0f;
        float maxZoom = 1.0f - player.size / 1000.0f;
        if (minZoom < 0.02f)
            minZoom = 0.02f;
        if (maxZoom < 0.02f)
            maxZoom = 0.02f;

        camera.zoom += GetMouseWheelMove() * 0.1f;
        if (camera.zoom < minZoom)
            camera.zoom = minZoom;

        if (camera.zoom > maxZoom)
            camera.zoom = maxZoom;
        // Камера следует за игроком
        camera.target = player.pos;

        EndMode2D();

        // UI рисуется ВНЕ BeginMode2D — в экранных координатах
        DrawText(TextFormat("Pos: %.0f %.0f\nScore: %d", player.pos.x, player.pos.y, player.score), 10, 40, 20, WHITE);
        DrawFPS(10, 10);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
