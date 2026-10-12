#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "raylib.h"

//////////////////////////////////////////////

double clamp(double value, double min, double max) {
    return value > max ? max : value < min ? min : value; 
}

#define ARRAY_TYPE(type) Array##type
#define ARRAY_TYPE_DECLARE(type) \
typedef struct Array##type {\
    type *p_data;\
    int size;\
    int capacity;\
} Array##type

#define _GET(index) p_data[(index)]

#define array_push(array, value) \
do {\
    if ((array).capacity <= (array).size) {\
        (array).capacity = (array).capacity == 0 ? 4 : (((array).capacity << 2) + (array).capacity);\
        (array).p_data = MemRealloc((array).p_data, (array).capacity * sizeof((array).p_data[0]));\
    }\
    (array).p_data[(array).size] = (value);\
    ++(array).size;\
} while(0)

///////////////////////////////////////

#define CELL_SIZE 125

#define STROKE_WIDTH 5

#define INIT_SCREEN_WIDTH 800
#define INIT_SCREEN_HEIGHT 600

#define BACKGROUND_COLOR WHITE

static Vector2 view_center = {0, 0};
static double view_scaling = 1.0;

#define MAX_VIEW_SCALING 2.0
#define MIN_VIEW_SCALING 0.2

/////////////////////////////////////

typedef enum CellState {
    CELL_EMPTY,
    CELL_CIRCLE,
    CELL_CROSS
} CellState;

ARRAY_TYPE_DECLARE(CellState);
typedef struct Row {
    ARRAY_TYPE(CellState) positive;
    ARRAY_TYPE(CellState) negative;
} Row;

ARRAY_TYPE_DECLARE(Row);
typedef struct Board {
    ARRAY_TYPE(Row) positive;
    ARRAY_TYPE(Row) negative;
} Board;

static Board board = {0};

void set_cell_state(int x, int y, CellState state) {
    fprintf(stderr, "Setting cell state at %d, %d\n", x, y);
    ARRAY_TYPE(Row) *rows;
    if (y >= 0) {
        rows = &board.positive;
    } else {
        rows = &board.negative;
        y = -y - 1;
    }

    while (y >= rows->size) {
        array_push(*rows, (Row){0});
    }

    ARRAY_TYPE(CellState) *cells;
    if (x >= 0) {
        cells = &rows->_GET(y).positive;
    } else {
        cells = &rows->_GET(y).negative;
        x = -x - 1;
    }

    while (x >= cells->size) {
        array_push(*cells, CELL_EMPTY);
    }

    cells->_GET(x) = state;
}

///////////////////////////////////////////////////////

int view_stroke_width() {
    return (int)(STROKE_WIDTH * view_scaling);
}
int view_cell_width() {
    return (int)(CELL_SIZE * view_scaling);
}

int world_view_width() {
    return (int)(GetScreenWidth() / view_scaling);
}
int world_view_height() {
    return (int)(GetScreenHeight() / view_scaling);
}

Vector2 get_world_view_min() {
    return (Vector2){
        view_center.x - (world_view_width() / 2), 
        view_center.y - (world_view_height() / 2)
    };
}
Vector2 get_world_view_max() {
    return (Vector2){
        view_center.x + (world_view_width() / 2), 
        view_center.y + (world_view_height() / 2)
    };
}
Vector2 get_world_view_origin() {
    return (Vector2){
        view_center.x - (world_view_width() / 2), 
        view_center.y + (world_view_height() / 2)
    };
}

Vector2 from_world_to_view(Vector2 point) {
    Vector2 world_origin = get_world_view_origin();
    return (Vector2){
        ((point.x - world_origin.x) * view_scaling), 
        ((point.y - world_origin.y) * -view_scaling)
    };
}

Vector2 from_view_to_world(Vector2 point) {
    Vector2 world_origin = get_world_view_origin();
    return (Vector2){
        point.x / view_scaling + world_origin.x, 
        point.y / -view_scaling + world_origin.y
        
    };
}

Vector2 get_world_point_from_coords(Vector2 coords) {
    return (Vector2){
        (coords.x + 0.5) * CELL_SIZE,
        (coords.y + 0.5) * CELL_SIZE,
    };
}

Vector2 get_coords_from_mouse() {
    Vector2 world_point = from_view_to_world((Vector2){GetMouseX(), GetMouseY()});
    return (Vector2){
        round(world_point.x / CELL_SIZE - 0.5),
        round(world_point.y / CELL_SIZE - 0.5)
    };
}

///////////////////////////////////////////////////////

void process_inputs(void) {
    double wheel_move = GetMouseWheelMove() / 10;
    view_scaling = clamp(view_scaling - wheel_move, MIN_VIEW_SCALING, MAX_VIEW_SCALING);

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        view_center.x -= delta.x / view_scaling;
        view_center.y += delta.y / view_scaling;
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        Vector2 coords = get_coords_from_mouse();
        set_cell_state((int)coords.x, (int)coords.y, CELL_CIRCLE);
    }
}

void draw_text_centered(const char* pText, float positionX, float positionY, float fontSize, Color color) {
    Font font = GetFontDefault();
    float spacing = 1;
    Vector2 textSize = MeasureTextEx(font, pText, fontSize, spacing);
    DrawTextEx(font, pText, (Vector2){ positionX - textSize.x / 2, positionY - textSize.y / 2 }, fontSize, spacing, color);
}

void draw_circle(Vector2 coords) {
    Vector2 center = from_world_to_view(get_world_point_from_coords(coords));
    int radius = view_cell_width() / 2 - view_stroke_width() * 2.5f;
    DrawCircleV(center, radius, RED); 
    DrawCircleV(center, radius - view_stroke_width(), BACKGROUND_COLOR); 
}

void draw_cross(Vector2 coords) {
    Vector2 center = from_world_to_view(get_world_point_from_coords(coords));
    int radius = view_cell_width() / 2 - view_stroke_width() * 2.5f;
    DrawLineEx(
        (Vector2){center.x - radius, center.y - radius}, 
        (Vector2){center.x + radius, center.y + radius}, 
        view_stroke_width(), BLUE);
    DrawLineEx(
        (Vector2){center.x + radius, center.y - radius}, 
        (Vector2){center.x - radius, center.y + radius}, 
        view_stroke_width(), BLUE); 
}

void draw_board(void) {
    fprintf(stderr, "Drawing board\n");
    for (int j = 0; j < board.positive.size; ++j) {
        int y = j;
        for (int i = 0; i < board.positive._GET(j).positive.size; ++i) {
            int x = i;

            switch (board.positive._GET(j).positive._GET(i)) {
                case CELL_CIRCLE:
                    draw_circle((Vector2){x, y});
                    break;
                case CELL_CROSS:
                    draw_cross((Vector2){x, y});
                    break;
                default:;
            }
        }
        for (int i = 0; i < board.positive._GET(j).negative.size; ++i) {
            int x = -i - 1;

            switch (board.positive._GET(j).negative._GET(i)) {
                case CELL_CIRCLE:
                    draw_circle((Vector2){x, y});
                    break;
                case CELL_CROSS:
                    draw_cross((Vector2){x, y});
                    break;
                default:;
            }
        }
    }
    for (int j = 0; j < board.negative.size; ++j) {
        int y = -j - 1;
        for (int i = 0; i < board.negative._GET(j).positive.size; ++i) {
            int x = i;

            switch (board.negative._GET(j).positive._GET(i)) {
                case CELL_CIRCLE:
                    draw_circle((Vector2){x, y});
                    break;
                case CELL_CROSS:
                    draw_cross((Vector2){x, y});
                    break;
                default:;
            }
        }
        for (int i = 0; i < board.negative._GET(j).negative.size; ++i) {
            int x = -i - 1;

            switch (board.negative._GET(j).negative._GET(i)) {
                case CELL_CIRCLE:
                    draw_circle((Vector2){x, y});
                    break;
                case CELL_CROSS:
                    draw_cross((Vector2){x, y});
                    break;
                default:;
            }
        }
    }
}

void draw_grid(void) {
    int width = GetScreenWidth();
    int height = GetScreenHeight();

    Vector2 view_min = get_world_view_min();
    Vector2 view_max = get_world_view_max();

    int view_min_x = (int)view_min.x - ((int)view_min.x - (CELL_SIZE - 1)) % CELL_SIZE;
    int view_max_x = (int)view_max.x - ((int)view_max.x + (CELL_SIZE - 1)) % CELL_SIZE;
    int view_min_y = (int)view_min.y - ((int)view_min.y - (CELL_SIZE - 1)) % CELL_SIZE;
    int view_max_y = (int)view_max.y - ((int)view_max.y + (CELL_SIZE - 1)) % CELL_SIZE;

    for (float x = view_min_x; x <= view_max_x; x += CELL_SIZE) {
        DrawLineEx(
            from_world_to_view((Vector2){x, view_min.y}), 
            from_world_to_view((Vector2){x, view_max.y}), 
            view_stroke_width(), BLACK);
    }

    for (float y = view_min_y; y <= view_max_y; y += CELL_SIZE) {
        DrawLineEx(
            from_world_to_view((Vector2){view_min.x, y}), 
            from_world_to_view((Vector2){view_max.x, y}), 
            view_stroke_width(), BLACK);
    }
}

void process_drawings(void) {
    ClearBackground(WHITE);

    draw_grid();
    draw_board();
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);    // Window configuration flags
    InitWindow(INIT_SCREEN_WIDTH, INIT_SCREEN_HEIGHT, "infiniti tic-tac-toe");
    SetTargetFPS(60);

    while (!WindowShouldClose()) 
    {
        process_inputs();

        BeginDrawing();

        process_drawings();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
