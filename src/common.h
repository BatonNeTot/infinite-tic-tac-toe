#ifndef INFINITE_COMMON_H
#define INFINITE_COMMON_H

//////////////////////////////////////////////

double clamp(double value, double min, double max);

#define ARRAY_TYPE(type) Array##type
#define ARRAY_TYPE_DECLARE(type) \
typedef struct {\
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

#define WINNING_STROKE_SIZE 5

#define CELL_SIZE 125

#define STROKE_WIDTH 5

#define INIT_SCREEN_WIDTH 800
#define INIT_SCREEN_HEIGHT 600

#define BACKGROUND_COLOR WHITE

#define MAX_VIEW_SCALING 2.0
#define MIN_VIEW_SCALING 0.2

#define GAME_PROTOCOL_NAME "infinite-tic-tac-toe"
#define GAME_PORT 42042

#endif // INFINITE_COMMON_H

