#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "raylib.h"
#include "net.h"
#include "ui.h"
#include "common.h"

/////////////////////////////////////

typedef enum {
    CELL_EMPTY,
    CELL_CIRCLE,
    CELL_CROSS
} CellState;

#define CELL_SERVER CELL_CIRCLE
#define CELL_CLIENT CELL_CROSS

ARRAY_TYPE_DECLARE(CellState);
typedef struct {
    ARRAY_TYPE(CellState) positive;
    ARRAY_TYPE(CellState) negative;
} Row;

ARRAY_TYPE_DECLARE(Row);
typedef struct {
    ARRAY_TYPE(Row) positive;
    ARRAY_TYPE(Row) negative;
} Board;

typedef struct {
    int x;
    int y;
} Cell;

Cell highlighted_cells[16];
int highlighted_cells_count = 0;

typedef enum {
    GAME_START,
    GAME_AWAITING_CONNECTION,
    GAME_ENTER_IP,
    GAME_CONNECTING,
    GAME_FINISHED_SERVER_WIN,
    GAME_FINISHED_CLIENT_WIN,
    GAME_FINISHED_SERVER_LOSE,
    GAME_FINISHED_CLIENT_LOSE,
    GAME_RUNNING_SERVER_AWAIT,
    GAME_RUNNING_CLIENT_AWAIT,
    GAME_RUNNING_SERVER,
    GAME_RUNNING_CLIENT,

    GAME_CAN_MOVE_VIEW = GAME_FINISHED_SERVER_WIN,
    GAME_CAN_PLACE_MARKER = GAME_RUNNING_SERVER,
    GAME_HINT_NEEDED = GAME_RUNNING_SERVER,
} GameState;

///////////////////////////////////////////

static Board board = {0};
static GameState game_state = GAME_START;

static Vector2 view_center = {0, 0};
static double view_scaling = 1.0;

//////////////////////////////////////////

void reset_board(void) {
    for (int j = 0; j < board.positive.size; ++j) {
        memset(&board.positive._GET(j).positive._GET(0), CELL_EMPTY, board.positive._GET(j).positive.size * sizeof(board.positive._GET(j).positive._GET(0)));
        memset(&board.positive._GET(j).negative._GET(0), CELL_EMPTY, board.positive._GET(j).negative.size * sizeof(board.positive._GET(j).positive._GET(0)));
    }
    for (int j = 0; j < board.negative.size; ++j) {
        memset(&board.negative._GET(j).positive._GET(0), CELL_EMPTY, board.negative._GET(j).positive.size * sizeof(board.positive._GET(j).positive._GET(0)));
        memset(&board.negative._GET(j).negative._GET(0), CELL_EMPTY, board.negative._GET(j).negative.size * sizeof(board.positive._GET(j).positive._GET(0)));
    }
}

CellState get_cell_state(int x, int y) {
    ARRAY_TYPE(Row) *rows;
    if (y >= 0) {
        rows = &board.positive;
    } else {
        rows = &board.negative;
        y = -y - 1;
    }

    if (y >= rows->size) {
        return CELL_EMPTY;
    }

    ARRAY_TYPE(CellState) *cells;
    if (x >= 0) {
        cells = &rows->_GET(y).positive;
    } else {
        cells = &rows->_GET(y).negative;
        x = -x - 1;
    }

    if (x >= cells->size) {
        return CELL_EMPTY;
    }

    return cells->_GET(x);
}

bool set_cell_state(int x, int y, CellState state) {
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

    if (cells->_GET(x) != CELL_EMPTY) {
        return false;
    }

    cells->_GET(x) = state;
    return true;
}

///////////////////////////////////////////////////////

int scaled_stroke_width(float scale) {
    return (int)(STROKE_WIDTH * scale);
}
int scaled_cell_width(float scale) {
    return (int)(CELL_SIZE * scale);
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

void draw_highlights(void) {
    for (int i = 0; i < highlighted_cells_count; ++i) {
        Vector2 view_coords = from_world_to_view(get_world_point_from_coords(
            (Vector2){ highlighted_cells[i].x, highlighted_cells[i].y }));
        DrawRectangleV(
            (Vector2){ view_coords.x - scaled_cell_width(view_scaling) / 2, view_coords.y - scaled_cell_width(view_scaling) / 2 },
            (Vector2){ scaled_cell_width(view_scaling), scaled_cell_width(view_scaling) },
            HIGHLIGHT_COLOR
        );
    }
}

bool is_highlighted(int x, int y) {
    for (int i = 0; i < highlighted_cells_count; ++i) {
        if (highlighted_cells[i].x == x && highlighted_cells[i].y == y) {
            return true;
        }
    }

    return false;
}

void add_highlight(int x, int y) {
    highlighted_cells[highlighted_cells_count].x = x;
    highlighted_cells[highlighted_cells_count].y = y;
    ++highlighted_cells_count;

    Vector2 world_view_max = {
        view_center.x + world_view_width() / 2 - scaled_cell_width(view_scaling) / 2, 
        view_center.y + world_view_height() / 2 - scaled_cell_width(view_scaling) / 2
    };
    Vector2 world_view_min = {
        view_center.x - world_view_width() / 2 + scaled_cell_width(view_scaling) / 2, 
        view_center.y - world_view_height() / 2 + scaled_cell_width(view_scaling) / 2
    };
    Vector2 world_point = get_world_point_from_coords((Vector2){ x, y });

    if (world_point.x > world_view_max.x) {
        view_center.x += world_point.x - world_view_max.x;
    } else if (world_point.x < world_view_min.x) {
        view_center.x += world_point.x - world_view_min.x;
    }
    
    if (world_point.y > world_view_max.y) {
        view_center.y += world_point.y - world_view_max.y;
    } else if (world_point.y < world_view_min.y) {
        view_center.y += world_point.y - world_view_min.y;
    }
}

void clear_highlights() {
    highlighted_cells_count = 0;
}

void draw_circle_raw(Vector2 view_coords, bool highlighted, float scale) {
    Vector2 center = view_coords;
    int radius = scaled_cell_width(scale) / 2 - scaled_stroke_width(scale) * 2.5f;
    DrawCircleV(center, radius, RED); 
    DrawCircleV(center, radius - scaled_stroke_width(scale), 
        highlighted ? HIGHLIGHT_COLOR : BACKGROUND_COLOR); 
}

void draw_cross_raw(Vector2 view_coords, float scale) {
    Vector2 center = view_coords;
    int radius = scaled_cell_width(scale) / 2 - scaled_stroke_width(scale) * 2.5f;
    DrawLineEx(
        (Vector2){center.x - radius, center.y - radius}, 
        (Vector2){center.x + radius, center.y + radius}, 
        scaled_stroke_width(scale), BLUE);
    DrawLineEx(
        (Vector2){center.x + radius, center.y - radius}, 
        (Vector2){center.x - radius, center.y + radius}, 
        scaled_stroke_width(scale), BLUE); 
}

void draw_circle(Vector2 coords) {
    draw_circle_raw(from_world_to_view(get_world_point_from_coords(coords)), is_highlighted((int)coords.x, (int)coords.y), view_scaling);
}

void draw_cross(Vector2 coords) {
    draw_cross_raw(from_world_to_view(get_world_point_from_coords(coords)), view_scaling);
}

void draw_board(void) {
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

void draw_move_hint() {
    if (game_state < GAME_HINT_NEEDED) {
        return;
    }

    float scale = 1.0f;

    DrawCircleV(
        (Vector2){GetScreenWidth() - scaled_cell_width(scale) / 3, GetScreenHeight() - scaled_cell_width(scale) / 3},
        scaled_cell_width(scale * 0.75) + scaled_stroke_width(scale), OUTLINE_COLOR
    );
    DrawCircleV(
        (Vector2){GetScreenWidth() - scaled_cell_width(scale) / 3, GetScreenHeight() - scaled_cell_width(scale) / 3},
        scaled_cell_width(scale * 0.75), BACKGROUND_COLOR
    );

    switch (game_state) {
        case GAME_RUNNING_SERVER:
            draw_circle_raw(
                (Vector2){GetScreenWidth() - scaled_cell_width(scale) / 2, GetScreenHeight() - scaled_cell_width(scale) / 2}, false, scale);
            break;
        case GAME_RUNNING_CLIENT:
            draw_cross_raw(
                (Vector2){GetScreenWidth() - scaled_cell_width(scale) / 2, GetScreenHeight() - scaled_cell_width(scale) / 2}, scale);
            break;
        default: break;
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
            scaled_stroke_width(view_scaling), BLACK);
    }

    for (float y = view_min_y; y <= view_max_y; y += CELL_SIZE) {
        DrawLineEx(
            from_world_to_view((Vector2){view_min.x, y}), 
            from_world_to_view((Vector2){view_max.x, y}), 
            scaled_stroke_width(view_scaling), BLACK);
    }
}

void draw_menu(void) {
    draw_labels();
}

void process_drawings(void) {
    ClearBackground(WHITE);

    draw_highlights();
    draw_grid();
    draw_board();
    draw_move_hint();

    draw_menu();
}

#define LABEL_TEST_SIZE 45

static int failed_host_server_label;
static int disconnected_label;
static int failed_connecting_label;

static int host_server_label;
static int connect_label;

static int await_connection_label;

static int enter_ip_label;
static int ip_address_label;
#define DEFAULT_IP_ADDRESS_STRING " "

static int connecting_label;

static int your_turn_label;
static int wait_turn_label;

static int win_label;
static int lose_label;
static int restart_label;

static void set_game_state_labels_visibility(GameState state, bool visibility) {
    switch (state) {
        case GAME_START: 
            set_label_visibility(host_server_label, visibility);
            set_label_visibility(connect_label, visibility);
            break;
        case GAME_AWAITING_CONNECTION: 
            set_label_visibility(await_connection_label, visibility);
            break;
            case GAME_ENTER_IP: 
            set_label_visibility(enter_ip_label, visibility);
            set_label_visibility(ip_address_label, visibility);
            break;
        case GAME_CONNECTING: 
            set_label_visibility(connecting_label, visibility);
            break;
        case GAME_FINISHED_SERVER_WIN:
        case GAME_FINISHED_CLIENT_WIN:
            set_label_visibility(win_label, visibility);
            break;
        case GAME_FINISHED_SERVER_LOSE:
        case GAME_FINISHED_CLIENT_LOSE:
            set_label_visibility(lose_label, visibility);
            break;
        case GAME_RUNNING_SERVER:
        case GAME_RUNNING_CLIENT:
            set_label_visibility(your_turn_label, visibility);
            break;
        case GAME_RUNNING_SERVER_AWAIT:
        case GAME_RUNNING_CLIENT_AWAIT:
            set_label_visibility(wait_turn_label, visibility);
            break;
        default:
            break;
    }

    switch (state) {
        case GAME_FINISHED_SERVER_WIN:
        case GAME_FINISHED_SERVER_LOSE:
            set_label_visibility(restart_label, visibility);
            break;
        default:
            break;
    }
}

static void set_game_state(GameState state) {
    switch (game_state) {
        case GAME_AWAITING_CONNECTION:
        case GAME_ENTER_IP:
        case GAME_FINISHED_SERVER_WIN:
        case GAME_FINISHED_CLIENT_WIN:
        case GAME_FINISHED_SERVER_LOSE:
        case GAME_FINISHED_CLIENT_LOSE:
            reset_board();
            clear_highlights();
            break;
        default:
            break;
    }

    set_game_state_labels_visibility(game_state, false);
    
    game_state = state;
    set_game_state_labels_visibility(state, true);
    switch (state) {
        case GAME_ENTER_IP:
            set_labels_focus(ip_address_label);
            update_label(ip_address_label);
            break;
        default:
            break;
    }
}

static bool check_winning_conditions(Vector2 origin) {
    int x = (int)origin.x;
    int y = (int)origin.y;

    CellState target_state = get_cell_state(x, y);
    assert(target_state != CELL_EMPTY);

    for (int i = 0; i < WINNING_STROKE_SIZE; ++i) {
        MessageMove cells[WINNING_STROKE_SIZE];
        bool found_win_stroke;

        found_win_stroke = true;
        for (int j = 0; j < WINNING_STROKE_SIZE; ++j) {
            cells[j].x = x - i + j;
            cells[j].y = y;
            if (get_cell_state(cells[j].x, cells[j].y) != target_state) {
                found_win_stroke = false;
                break;
            }
        }
        if (found_win_stroke) {
            send_server_game_end(target_state == CELL_SERVER ? PLAYER_SERVER : PLAYER_CLIENT, &cells[0]);
            clear_highlights();
            for (int i = 0; i < WINNING_STROKE_SIZE; ++i) {
                add_highlight(cells[i].x, cells[i].y);
            }
            set_game_state(target_state == CELL_SERVER ? GAME_FINISHED_SERVER_WIN : GAME_FINISHED_SERVER_LOSE);
            return true;
        }

        found_win_stroke = true;
        for (int j = 0; j < WINNING_STROKE_SIZE; ++j) {
            cells[j].x = x;
            cells[j].y = y - i + j;
            if (get_cell_state(cells[j].x, cells[j].y) != target_state) {
                found_win_stroke = false;
                break;
            }
        }
        if (found_win_stroke) {
            send_server_game_end(target_state == CELL_SERVER ? PLAYER_SERVER : PLAYER_CLIENT, &cells[0]);
            for (int i = 0; i < WINNING_STROKE_SIZE; ++i) {
                add_highlight(cells[i].x, cells[i].y);
            }
            set_game_state(target_state == CELL_SERVER ? GAME_FINISHED_SERVER_WIN : GAME_FINISHED_SERVER_LOSE);
            return true;
        }

        found_win_stroke = true;
        for (int j = 0; j < WINNING_STROKE_SIZE; ++j) {
            cells[j].x = x - i + j;
            cells[j].y = y - i + j;
            if (get_cell_state(cells[j].x, cells[j].y) != target_state) {
                found_win_stroke = false;
                break;
            }
        }
        if (found_win_stroke) {
            send_server_game_end(target_state == CELL_SERVER ? PLAYER_SERVER : PLAYER_CLIENT, &cells[0]);
            for (int i = 0; i < WINNING_STROKE_SIZE; ++i) {
                add_highlight(cells[i].x, cells[i].y);
            }
            set_game_state(target_state == CELL_SERVER ? GAME_FINISHED_SERVER_WIN : GAME_FINISHED_SERVER_LOSE);
            return true;
        }

        found_win_stroke = true;
        for (int j = 0; j < WINNING_STROKE_SIZE; ++j) {
            cells[j].x = x - i + j;
            cells[j].y = y + i - j;
            if (get_cell_state(cells[j].x, cells[j].y) != target_state) {
                found_win_stroke = false;
                break;
            }
        }
        if (found_win_stroke) {
            send_server_game_end(target_state == CELL_SERVER ? PLAYER_SERVER : PLAYER_CLIENT, &cells[0]);
            for (int i = 0; i < WINNING_STROKE_SIZE; ++i) {
                add_highlight(cells[i].x, cells[i].y);
            }
            set_game_state(target_state == CELL_SERVER ? GAME_FINISHED_SERVER_WIN : GAME_FINISHED_SERVER_LOSE);
            return true;
        }
    }

    return false;
}

static unsigned int server_on_connect(uint8_t *p_buffer, unsigned int size) {
    NBN_WriteStream ws;
    NBN_WriteStream_Init(&ws, p_buffer, size);

    // TODO decide who goes first
    Player first_player = GetRandomValue(0, PLAYER_TOTAL - 1);
    NBN_SerializeUInt((NBN_Stream *)&ws, first_player, 0, PLAYER_TOTAL - 1);

    NBN_WriteStream_Flush(&ws);

    if (first_player == PLAYER_SERVER) {
        set_game_state(GAME_RUNNING_SERVER);
    } else {
        set_game_state(GAME_RUNNING_SERVER_AWAIT);
    }

    return sizeof(Player);
}
static void server_on_disconnect() {
    reset_server();
    show_label_popup(disconnected_label);
    set_game_state(GAME_START);
}
static void server_on_message() {
    NBN_MessageInfo msg_info = NBN_GameServer_GetMessageInfo();

    switch (msg_info.type) {
        case MESSAGE_MOVE_DONE: {
            assert(game_state == GAME_RUNNING_SERVER_AWAIT);
            MessageMove* msg = msg_info.data;
            set_cell_state(msg->x, msg->y, CELL_CLIENT);
            clear_highlights();
            add_highlight(msg->x, msg->y);
            set_game_state(GAME_RUNNING_SERVER);
            check_winning_conditions((Vector2){ msg->x, msg->y});
            break;
        }
    }
}

static void client_on_disconnect() {
    reset_client();
    if (game_state == GAME_CONNECTING) {
        show_label_popup(failed_connecting_label);
        set_game_state(GAME_ENTER_IP);
        return;
    }

    show_label_popup(disconnected_label);
    set_game_state(GAME_START);
}
static unsigned int client_on_connect(uint8_t *p_buffer, unsigned int size) {
    if (size < sizeof(Player)) {
        client_on_disconnect();
    }

    NBN_ReadStream rs;
    NBN_ReadStream_Init(&rs, p_buffer, size);

    Player first_player;
    NBN_SerializeUInt((NBN_Stream *)&rs, first_player, 0, PLAYER_TOTAL - 1);

    if (first_player == PLAYER_CLIENT) {
        set_game_state(GAME_RUNNING_CLIENT);
    } else {
        set_game_state(GAME_RUNNING_CLIENT_AWAIT);
    }

    return sizeof(Player);
}
static void client_on_message() {
    NBN_MessageInfo msg_info = NBN_GameClient_GetMessageInfo();

    switch (msg_info.type) {
        case MESSAGE_MOVE_DONE: {
            assert(game_state == GAME_RUNNING_CLIENT_AWAIT);
            MessageMove* msg = msg_info.data;
            set_cell_state(msg->x, msg->y, CELL_SERVER);
            clear_highlights();
            add_highlight(msg->x, msg->y);
            set_game_state(GAME_RUNNING_CLIENT);
            break;
        }
        case MESSAGE_GAME_END: {
            assert(game_state == GAME_RUNNING_CLIENT_AWAIT);
            MessageGameEnd* msg = msg_info.data;
            clear_highlights();
            for (int i = 0; i < WINNING_STROKE_SIZE; ++i) {
                set_cell_state(msg->winning_cells[i].x, msg->winning_cells[i].y, msg->player == PLAYER_SERVER ? CELL_SERVER : CELL_CLIENT);
                add_highlight(msg->winning_cells[i].x, msg->winning_cells[i].y);
            }
            set_game_state(msg->player == PLAYER_CLIENT ? GAME_FINISHED_CLIENT_WIN : GAME_FINISHED_CLIENT_LOSE);
            break;
        }
        case MESSAGE_GAME_RESTART: {
            assert(game_state == GAME_FINISHED_CLIENT_WIN || game_state == GAME_FINISHED_CLIENT_LOSE);
            MessageGameEnd* msg = msg_info.data;
            set_game_state(msg->player == PLAYER_CLIENT ? GAME_RUNNING_CLIENT : GAME_RUNNING_CLIENT_AWAIT);
            break;
        }
    }
}

static void label_callbacks(int id) {
    if (id == host_server_label) {
        set_game_state(GAME_AWAITING_CONNECTION);
        if (!init_server(&server_on_connect, &server_on_disconnect, &server_on_message)) {
            set_game_state(GAME_START);
            show_label_popup(failed_host_server_label);
        }
        return;
    } else if (id == connect_label) {
        set_game_state(GAME_ENTER_IP);
    } else if (id == restart_label) {
        Player first_player = GetRandomValue(0, PLAYER_TOTAL - 1);
        send_server_game_restart(first_player);

        if (first_player == PLAYER_SERVER) {
            set_game_state(GAME_RUNNING_SERVER);
        } else {
            set_game_state(GAME_RUNNING_SERVER_AWAIT);
        }
    }
}

static void ip_address_label_edit_callback(int id, char key) {
    char* p_buffer = get_label_buffer(id);
    int buffer_size = strnlen(p_buffer, LABEL_BUFFER_SIZE);
    if (buffer_size < LABEL_BUFFER_SIZE) {
        const char* p_first_space = strchr(p_buffer, ' ');
        if (p_first_space != NULL) {
            buffer_size = p_first_space - p_buffer;
        }
    }
    if (key == 0x08) {
        p_buffer[buffer_size - 1] = buffer_size >= sizeof(DEFAULT_IP_ADDRESS_STRING) ? '\0' : ' ';
    } else if (key == '\n') {
        if (!init_client(p_buffer, &client_on_connect, &client_on_disconnect, &client_on_message)) {
        show_label_popup(failed_connecting_label);
        } else {
            set_game_state(GAME_CONNECTING);
        }
        return;
    } else {
        p_buffer[buffer_size] = key;
        if (buffer_size >= sizeof(DEFAULT_IP_ADDRESS_STRING)) {
            p_buffer[buffer_size + 1] = '\0';
        }
    }
    update_label(id);
}

void process_networking(void) {
    switch (game_state) {
        case GAME_AWAITING_CONNECTION:
        case GAME_FINISHED_SERVER_WIN:
        case GAME_FINISHED_SERVER_LOSE:
        case GAME_RUNNING_SERVER:
        case GAME_RUNNING_SERVER_AWAIT:
            process_server_messages();
            break;
        case GAME_CONNECTING:
        case GAME_FINISHED_CLIENT_WIN:
        case GAME_FINISHED_CLIENT_LOSE:
        case GAME_RUNNING_CLIENT:
        case GAME_RUNNING_CLIENT_AWAIT:
            process_client_messages();
            break;
        default:
            break;
    }
}

void process_inputs(void) {
    if (game_state >= GAME_CAN_MOVE_VIEW) {
        double wheel_move = GetMouseWheelMove() / 10;
        view_scaling = clamp(view_scaling - wheel_move, MIN_VIEW_SCALING, MAX_VIEW_SCALING);

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta();
            view_center.x -= delta.x / view_scaling;
            view_center.y += delta.y / view_scaling;
        }
    }
    if (game_state < GAME_CAN_PLACE_MARKER) {
        process_labels();
        return;
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        Vector2 coords = get_coords_from_mouse();
        if (game_state == GAME_RUNNING_SERVER && set_cell_state((int)coords.x, (int)coords.y, CELL_SERVER)) {
            if (!check_winning_conditions(coords)) {
                send_server_move((int)coords.x, (int)coords.y);
                clear_highlights();
                add_highlight((int)coords.x, (int)coords.y);
                set_game_state(GAME_RUNNING_SERVER_AWAIT);
            }
        } else if (game_state == GAME_RUNNING_CLIENT && set_cell_state((int)coords.x, (int)coords.y, CELL_CLIENT)) {
            send_client_move((int)coords.x, (int)coords.y);
            clear_highlights();
            add_highlight((int)coords.x, (int)coords.y);
            set_game_state(GAME_RUNNING_CLIENT_AWAIT);
        }
    }
}

static void setup_labels(void) {

    // popups

    failed_host_server_label = add_empty_label();
    sprintf(get_label_buffer(failed_host_server_label), "Failed to start a server");
    set_label_size(failed_host_server_label, LABEL_TEST_SIZE);

    disconnected_label = add_empty_label();
    sprintf(get_label_buffer(disconnected_label), "Disconnected");
    set_label_size(disconnected_label, LABEL_TEST_SIZE);

    failed_connecting_label = add_empty_label();
    sprintf(get_label_buffer(failed_connecting_label), "Failed to connect to a server");
    set_label_size(failed_connecting_label, LABEL_TEST_SIZE);

    ////////////////////////////////////////
    
    host_server_label = add_empty_label();
    sprintf(get_label_buffer(host_server_label), "Host server");
    set_label_size(host_server_label, LABEL_TEST_SIZE);
    set_label_callback(host_server_label, &label_callbacks);
    
    connect_label = add_empty_label();
    sprintf(get_label_buffer(connect_label), "Connect to...");
    set_label_size(connect_label, LABEL_TEST_SIZE);
    set_label_callback(connect_label, &label_callbacks);

    /////////////////////

    await_connection_label = add_empty_label();
    sprintf(get_label_buffer(await_connection_label), "Awaiting connection");
    set_label_size(await_connection_label, LABEL_TEST_SIZE);

    /////////////////////

    enter_ip_label = add_empty_label();
    sprintf(get_label_buffer(enter_ip_label), "Enter ip address");
    set_label_size(enter_ip_label, LABEL_TEST_SIZE);
    
    ip_address_label = add_empty_label();
    sprintf(get_label_buffer(ip_address_label), DEFAULT_IP_ADDRESS_STRING);
    set_label_size(ip_address_label, LABEL_TEST_SIZE);
    set_label_edit_callback(ip_address_label, &ip_address_label_edit_callback);

    ////////////////////////

    connecting_label = add_empty_label();
    sprintf(get_label_buffer(connecting_label), "Connecting...");
    set_label_size(connecting_label, LABEL_TEST_SIZE);

    /////////////////////

    your_turn_label = add_empty_label();
    sprintf(get_label_buffer(your_turn_label), "Your turn");
    set_label_size(your_turn_label, LABEL_TEST_SIZE);

    wait_turn_label = add_empty_label();
    sprintf(get_label_buffer(wait_turn_label), "Wait for your turn");
    set_label_size(wait_turn_label, LABEL_TEST_SIZE);

    /////////////////////

    win_label = add_empty_label();
    sprintf(get_label_buffer(win_label), "You won!");
    set_label_size(win_label, LABEL_TEST_SIZE);

    lose_label = add_empty_label();
    sprintf(get_label_buffer(lose_label), "You lost!");
    set_label_size(lose_label, LABEL_TEST_SIZE);

    restart_label = add_empty_label();
    sprintf(get_label_buffer(restart_label), "Restart");
    set_label_size(restart_label, LABEL_TEST_SIZE);
    set_label_callback(restart_label, &label_callbacks);
} 

static void update_labels_positions(void) {
    
    set_label_center(host_server_label, (Vector2){GetScreenWidth() / 2, GetScreenHeight() / 2 - 50});
    update_label(host_server_label);
    
    set_label_center(connect_label, (Vector2){GetScreenWidth() / 2, GetScreenHeight() / 2 + 50});
    update_label(connect_label);

    /////////////////////

    set_label_center(await_connection_label, (Vector2){GetScreenWidth() / 2, GetScreenHeight() / 2});
    update_label(await_connection_label);

    /////////////////////

    set_label_center(enter_ip_label, (Vector2){GetScreenWidth() / 2, GetScreenHeight() / 2 - 50});
    update_label(enter_ip_label);
    
    set_label_center(ip_address_label, (Vector2){GetScreenWidth() / 2, GetScreenHeight() / 2 + 50});
    update_label(ip_address_label);

    ////////////////////////

    set_label_center(connecting_label, (Vector2){GetScreenWidth() / 2, GetScreenHeight() / 2});
    update_label(connecting_label);

    /////////////////////

    set_label_center(your_turn_label, (Vector2){GetScreenWidth() / 2, 50});
    update_label(your_turn_label);

    set_label_center(wait_turn_label, (Vector2){GetScreenWidth() / 2, 50});
    update_label(wait_turn_label);

    /////////////////////

    set_label_center(win_label, (Vector2){GetScreenWidth() / 2, 50});
    update_label(win_label);

    set_label_center(lose_label, (Vector2){GetScreenWidth() / 2, 50});
    update_label(lose_label);

    set_label_center(restart_label, (Vector2){GetScreenWidth() / 2, GetScreenHeight() - 50});
    update_label(restart_label);

    update_labels_popups();
} 

int main(void) {
    SetTraceLogLevel(LOG_DEBUG);

    NBN_UDP_Register(); // Register the UDP driver

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);    // Window configuration flags
    InitWindow(INIT_SCREEN_WIDTH, INIT_SCREEN_HEIGHT, "infinite tic-tac-toe");
    SetTargetFPS(60);

    setup_labels();
    update_labels_positions();
    set_game_state(GAME_START);

    while (!WindowShouldClose()) 
    {
        if (IsWindowResized()) {
            update_labels_positions();
        }

        process_networking();
        process_inputs();

        BeginDrawing();

        process_drawings();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
