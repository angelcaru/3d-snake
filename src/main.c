#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"

#ifdef PLATFORM_WEB
    #include <emscripten/emscripten.h>
#endif // PLATFORM_WEB

#include <time.h>

#define MACRO_VAR(name) _##name##__LINE__
#define BEGIN_END_NAMED(begin, end, i) for (int i = (begin, 0); i < 1; i++, end)
#define BEGIN_END(begin, end) BEGIN_END_NAMED(begin, end, MACRO_VAR(i))
#define Drawing BEGIN_END(BeginDrawing(), EndDrawing())
#define Mode3D(camera) BEGIN_END(BeginMode3D(camera), EndMode3D())

#define GRID_SIZE 10

bool vector3_near_eq(Vector3 a, Vector3 b) {
    return Vector3LengthSqr(Vector3Subtract(a, b)) < 0.01;
}

typedef struct {
    Vector3 points[GRID_SIZE*GRID_SIZE*GRID_SIZE];
    size_t begin;
    size_t size;

    Vector3 dir;
} Snake;
#define SNAKE_AT_NO_PARENS(snake, i) snake->points[((snake->begin + i) + ARRAY_LEN(snake->points)) % ARRAY_LEN(snake->points)]
#define SNAKE_AT(snake, i) SNAKE_AT_NO_PARENS((snake), (i))

void snake_push_head(Snake *snake, Vector3 point) {
    assert(snake->size < ARRAY_LEN(snake->points) && "Snake Overflow");
    SNAKE_AT(snake, snake->size) = point;
    snake->size++;
}

void snake_push_tail(Snake *snake, Vector3 point) {
    assert(snake->size < ARRAY_LEN(snake->points) && "Snake Overflow");
    SNAKE_AT(snake, -1) = point;
    snake->size++;
    snake->begin--;
}

Vector3 snake_pop(Snake *snake) {
    assert(snake->size > 0 && "Snake Underflow");
    Vector3 point = SNAKE_AT(snake, 0);
    snake->size--;
    snake->begin++;
    if (snake->begin == ARRAY_LEN(snake->points)) snake->begin = 0;
    return point;
}

Vector3 snake_head(const Snake *snake) {
    return SNAKE_AT(snake, snake->size - 1);
}

void snake_grow(Snake *snake) {
    Vector3 head = snake_head(snake);
    snake_push_tail(snake, Vector3Subtract(head, snake->dir));
}

bool snake_contains(const Snake *snake, Vector3 point) {
    for (size_t i = 0; i < snake->size; i++) {
        if (vector3_near_eq(point, SNAKE_AT(snake, i))) {
            return true;
        }
    }
    return false;
}

bool snake_update(Snake *snake) {
    snake_pop(snake);
    Vector3 point = Vector3Add(SNAKE_AT(snake, snake->size - 1), snake->dir);
    point.x = ((int)point.x + GRID_SIZE) % GRID_SIZE;
    point.y = ((int)point.y + GRID_SIZE) % GRID_SIZE;
    point.z = ((int)point.z + GRID_SIZE) % GRID_SIZE;
    if (snake_contains(snake, point)) {
        return false;
    }
    snake_push_head(snake, point);
    return true;
}

Vector3 get_keyboard_dir(void) {
    if (IsKeyPressed(KEY_W)) return (Vector3) { 0, 0, -1 };
    if (IsKeyPressed(KEY_A)) return (Vector3) { -1, 0, 0 };
    if (IsKeyPressed(KEY_S)) return (Vector3) { 0, 0, 1 };
    if (IsKeyPressed(KEY_D)) return (Vector3) { 1, 0, 0 };
    if (IsKeyPressed(KEY_UP)) return (Vector3) { 0, 1, 0 };
    if (IsKeyPressed(KEY_DOWN)) return (Vector3) { 0, -1, 0 };

    return Vector3Zero();
}

Vector3 gen_fruit(void) {
    int x = GetRandomValue(0, GRID_SIZE-1);
    int y = GetRandomValue(0, GRID_SIZE-1);
    int z = GetRandomValue(0, GRID_SIZE-1);
    return (Vector3) { x, y, z };
}

typedef struct {
    Vector3 *items;
    size_t count, capacity;
} Dir_Queue;

Vector3 dir_queue_pop(Dir_Queue *dirq) {
    Vector3 dir = dirq->items[0];
    memmove(dirq->items, dirq->items + 1, sizeof(*dirq->items));
    dirq->count--;
    return dir;
}

Vector3 last_dir(Dir_Queue dir_queue, const Snake *snake) {
    return dir_queue.count == 0 ? snake->dir : dir_queue.items[dir_queue.count - 1];
}

typedef struct {
    Snake snake;
    Vector3 fruit;
    Camera camera;
    Dir_Queue dir_queue;
    int score;
    float time;
    bool game_over;
} Game;

void game_init(Game *game) {
    memset(game, 0, sizeof(*game));
    game->snake = (Snake) { .dir = {-1, 0, 0} };
    for (int i = 0; i < 4; i++) {
        snake_push_head(&game->snake, (Vector3) { GRID_SIZE / 2 + 4 - i, GRID_SIZE / 2, GRID_SIZE / 2 });
    }
    SetRandomSeed(time(0));
    game->fruit = gen_fruit();
    game->camera = (Camera) {
        .position = { 0, GRID_SIZE / 2 + 2, GRID_SIZE / 2 + 2 },
        .target = Vector3Zero(),
        .up = { 0.0f, 1.0f, 0.0f },
        .fovy = 100.0f,
        .projection = CAMERA_PERSPECTIVE,
    };
}

#define BACKGROUND_COLOR SKYBLUE
#define GRID_COLOR WHITE
#define SNAKE_COLOR RED
#define FRUIT_COLOR BLUE

void game_update(Game *game) {
    if (IsKeyPressed(KEY_F)) ToggleBorderlessWindowed();
    if (game->game_over) {
        Drawing {
            ClearBackground(RED);
            int width = GetScreenWidth();
            int height = GetScreenHeight();

            const char *text = "Game Over";
            int font_size = (width + height) / (strlen(text) * 2);
            int w = MeasureText(text, font_size);
            DrawText(text, (width - w) / 2, 50, font_size, WHITE);

            text = TextFormat("Score: %d", game->score);
            font_size = (width + height) / (strlen(text) * 3);
            w = MeasureText(text, font_size);
            DrawText(text, (width - w) / 2, height / 2, font_size, WHITE);
        }
        return;
    }

    // TODO: better camera controls that don't conflict with snake WASD
    if (IsKeyDown(KEY_SPACE)) UpdateCamera(&game->camera, CAMERA_ORBITAL);

    Vector3 new_dir = get_keyboard_dir();
    if (!vector3_near_eq(new_dir, Vector3Zero())) {
        Vector3 last = last_dir(game->dir_queue, &game->snake);
        if (!vector3_near_eq(new_dir, last) && !vector3_near_eq(new_dir, Vector3Scale(last, -1))) {
            da_append(&game->dir_queue, new_dir);
        }
    }

    game->time += GetFrameTime();
    if (game->time >= 0.5) {
        game->time = 0;

        if (game->dir_queue.count > 0) {
            Vector3 new_dir = dir_queue_pop(&game->dir_queue);
            game->snake.dir = new_dir;
        }

        if (!snake_update(&game->snake)) {
            game->game_over = true;
            return;
        }
        if (vector3_near_eq(snake_head(&game->snake), game->fruit)) {
            game->score++;
            snake_grow(&game->snake);
            do {
                game->fruit = gen_fruit();
            } while (snake_contains(&game->snake, game->fruit));
        }
    }

    Drawing {
        ClearBackground(BACKGROUND_COLOR);
        Mode3D(game->camera) {
            // Main grid
            for (int x = 0; x < GRID_SIZE; x++) {
                for (int y = 0; y < GRID_SIZE; y++) {
                    for (int z = 0; z < GRID_SIZE; z++) {
                        Vector3 pos = {x, y, z};
                        Vector3 draw_pos = Vector3SubtractValue(pos, GRID_SIZE / 2);
                        //DrawCubeWires(draw_pos, 1, 1, 1, GRID_COLOR);

                        if (vector3_near_eq(pos, game->fruit)) {
                            DrawCube(draw_pos, 1, 1, 1, FRUIT_COLOR);
                        } else if (snake_contains(&game->snake, pos)) {
                            DrawCube(draw_pos, 1, 1, 1, SNAKE_COLOR);
                        }
                    }
                }
            }

            // """Shadows"""
            for (int x = 0; x < GRID_SIZE; x++) {
                for (int y = 0; y < GRID_SIZE; y++) {
                    for (int z = 0; z < GRID_SIZE; z++) {
                        Vector3 pos = {x, y, z};
                        Vector3 draw_pos_bottom = { x - GRID_SIZE / 2, -GRID_SIZE / 2 - 3/2, z - GRID_SIZE / 2};
                        Vector3 draw_pos_top = { x - GRID_SIZE / 2, GRID_SIZE / 2, z - GRID_SIZE / 2};

                        if (snake_contains(&game->snake, pos)) {
                            DrawCubeWires(draw_pos_bottom, 1, 0, 1, SNAKE_COLOR);
                            DrawCubeWires(draw_pos_top, 1, 0, 1, SNAKE_COLOR);
                        }
                    }
                }
            }
            for (int x = 0; x < GRID_SIZE; x++) {
                for (int y = 0; y < GRID_SIZE; y++) {
                    for (int z = 0; z < GRID_SIZE; z++) {
                        Vector3 pos = {x, y, z};
                        Vector3 draw_pos_bottom = { x - GRID_SIZE / 2, -GRID_SIZE / 2 - 3/2, z - GRID_SIZE / 2};
                        Vector3 draw_pos_top = { x - GRID_SIZE / 2, GRID_SIZE / 2, z - GRID_SIZE / 2};

                        if (vector3_near_eq(game->fruit, pos)) {
                            DrawCubeWires(draw_pos_bottom, 1, 0, 1, FRUIT_COLOR);
                            DrawCubeWires(draw_pos_top, 1, 0, 1, FRUIT_COLOR);
                        }
                    }
                }
            }
            DrawCubeWires(Vector3Zero(), GRID_SIZE, GRID_SIZE, GRID_SIZE, GRID_COLOR);
        }

        const char *text = TextFormat("Score: %d", game->score);
        int font_size = 20;
        int w = MeasureText(text, font_size);
        DrawText(text, GetScreenWidth() / 2 - w / 2, 10, font_size, WHITE);

        DrawFPS(10, 10);
    }
}

Game game;

int main(void) {
    game_init(&game);

    InitWindow(640, 480, "3D Snake Game");
    DisableCursor();

#ifdef PLATFORM_WEB
    emscripten_set_main_loop_arg((em_arg_callback_func)game_update, &game, 0, true);
#else
    while (!WindowShouldClose()) game_update(&game);
    CloseWindow();
#endif // PLATFORM_WEB

    printf("Final Score: %d\n", game.score);
    return 0;
}
