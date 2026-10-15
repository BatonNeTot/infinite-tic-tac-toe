#include "ui.h"

#include "common.h"

#include "limits.h"

#define MAX_LABELS 16
#define SPACING 1

typedef struct  {
    Vector2 center;
    Vector2 size;
    LabelCallback callback;
    LabelEditCallback edit_callback;
    unsigned char alpha;
    float font_size;
    bool visible;
    bool editable;
    char p_buffer[LABEL_BUFFER_SIZE];
} Label;

static Label labels[MAX_LABELS] = {0};
static int labels_count = 0;

#define POPUP_DURATION_TIME 1.375
#define POPUP_HIDDING_TIME 0.125

typedef struct  {
    int id;
    double hide_start;
    double hide_end;
} Popup;

static int labels_focus = -1;
static Popup labels_popup[16] = {{.id = -1}}; 

int add_empty_label(void) {
    if (labels_count >= MAX_LABELS) {
        return -1;
    }

    int id = labels_count++;
    labels[id].p_buffer[0] = '\0';
    labels[id].alpha = UCHAR_MAX;
    return id;
}

void set_labels_focus(int id) {
    labels_focus = id;
}

char* get_label_buffer(int id) {
    return labels[id].p_buffer;
}

void set_label_center(int id, Vector2 center) {
    labels[id].center = center;
}

void set_label_size(int id, float size) {
    labels[id].font_size = size;
}

void set_label_visibility(int id, bool visible) {
    labels[id].visible = visible;
}

void set_label_callback(int id, LabelCallback callback) {
    labels[id].callback = callback;
}

void set_label_edit_callback(int id, LabelEditCallback callback) {
    labels[id].edit_callback = callback;
}

void update_label(int id) {
    Font font = GetFontDefault();
    labels[id].size = MeasureTextEx(font, labels[id].p_buffer, labels[id].font_size, SPACING);
}

void show_label_popup(int id) {
    double current_time = GetTime();

    labels[id].visible = true;
    labels[id].alpha = UCHAR_MAX;

    Popup popup = {
        .id = id,
        .hide_start = current_time + POPUP_DURATION_TIME - POPUP_HIDDING_TIME,
        .hide_end = current_time + POPUP_DURATION_TIME,
    };
    
    for (int i = 0; ; ++i) {
        Popup old_popup = labels_popup[i];
        labels_popup[i] = popup;
        popup = old_popup;

        if (labels_popup[i].id == -1) {
            break;
        } else {
            id = labels_popup[i].id;
            labels[id].center.x = GetScreenWidth() / 2;
            labels[id].center.y = 50 + i * 100;
            update_label(id);
        }
    }
}

void update_labels_popups() {
    
    for (int i = 0; ; ++i) {
        int id = labels_popup[i].id;
        
        if (id == -1) {
            break;
        } else {
            labels[id].center.x = GetScreenWidth() / 2;
            labels[id].center.y = 50 + i * 100;
            update_label(id);
        }
    }
}

static int last_label_pressed = -1;
static int find_label_under_mouse() {
    int mouse_x = GetMouseX();
    int mouse_y = GetMouseY();
    for (int i = labels_count - 1; i >= 0; --i) {
        Label label = labels[i];

        if (!label.visible) {
            continue;
        }

        if (
            mouse_x >= label.center.x - label.size.x / 2 - STROKE_WIDTH&&
            mouse_x <= label.center.x + label.size.x / 2 + STROKE_WIDTH &&
            mouse_y >= label.center.y - label.size.y / 2 - STROKE_WIDTH &&
            mouse_y <= label.center.y + label.size.y / 2 + STROKE_WIDTH
        ) {
            return i;
        }
    }
    return -1;
}

bool process_labels() {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        last_label_pressed = find_label_under_mouse();
        return last_label_pressed >= 0;
    } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        return last_label_pressed >= 0;
    } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        int current_label_pressed = find_label_under_mouse();
        labels_focus = current_label_pressed;

        if (last_label_pressed >= 0) {
            if (last_label_pressed == current_label_pressed) {
                Label label = labels[last_label_pressed];
                if (label.callback) {
                    label.callback(last_label_pressed);
                }
            }

            last_label_pressed = -1;
            return true;
        }
    } else if (labels_focus >= 0) {
        Label label = labels[labels_focus];
        if (label.edit_callback) {
            int key = GetCharPressed();

            while (key > 0) {
                if (key >= 32 && key <= 125) {
                    label.edit_callback(labels_focus, (char)key);
                }

                key = GetCharPressed();  // Check next character in the queue
            }

            if (IsKeyPressed(KEY_BACKSPACE)) {
                label.edit_callback(labels_focus, 0x08);
            }

            if (IsKeyPressed(KEY_ENTER)) {
                label.edit_callback(labels_focus, '\n');
            }

        }
    }
    return false;
}

void draw_labels() {
    //*
    double current_time = GetTime();
    for (int i = 0; labels_popup[i].id != -1; ++i) {
        if (current_time > labels_popup[i].hide_end) {
            labels[labels_popup[i].id].visible = false;
            labels_popup[i].id = -1;
            break;
        }
        if (current_time > labels_popup[i].hide_start) {
            double past_time = POPUP_HIDDING_TIME - (current_time - labels_popup[i].hide_start);
            labels[labels_popup[i].id].alpha = (unsigned char)(UCHAR_MAX * (past_time / POPUP_HIDDING_TIME));
        }
    }//*/

    Font font = GetFontDefault();
    for (int i = 0; i < labels_count; ++i) {
        Label label = labels[i];

        if (!label.visible) {
            continue;
        }

        Color color;

        color = OUTLINE_COLOR;
        color.a = label.alpha;
        DrawRectangleV(
            (Vector2){ label.center.x - label.size.x / 2 - STROKE_WIDTH * 2, label.center.y - label.size.y / 2 - STROKE_WIDTH * 2},
            (Vector2){ label.size.x + STROKE_WIDTH * 4, label.size.y + STROKE_WIDTH * 4}, 
            color
        );
        color = BACKGROUND_COLOR;
        color.a = label.alpha;
        DrawRectangleV(
            (Vector2){ label.center.x - label.size.x / 2 - STROKE_WIDTH, label.center.y - label.size.y / 2 - STROKE_WIDTH},
            (Vector2){ label.size.x + STROKE_WIDTH * 2, label.size.y + STROKE_WIDTH * 2}, 
            color
        );
        color = TEXT_COLOR;
        color.a = label.alpha;
        DrawTextEx(font, label.p_buffer, 
            (Vector2){ label.center.x - label.size.x / 2, label.center.y - label.size.y / 2 }, 
            label.font_size, SPACING, color);
    }
}
