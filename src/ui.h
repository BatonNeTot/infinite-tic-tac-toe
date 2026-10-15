#ifndef INFINITE_UI_H
#define INFINITE_UI_H

#include "raylib.h"

#define OUTLINE_COLOR DARKGRAY
#define TEXT_COLOR BLACK

#define LABEL_BUFFER_SIZE 256

typedef void(*LabelCallback)(int);
typedef void(*LabelEditCallback)(int, char);

int add_empty_label(void);

void set_labels_focus(int id);

char* get_label_buffer(int id);

void set_label_center(int id, Vector2 center);

void set_label_size(int id, float size);

void set_label_visibility(int id, bool visible);

void set_label_callback(int id, LabelCallback callback);

void set_label_edit_callback(int id, LabelEditCallback callback);

void update_label(int id);

void show_label_popup(int id);

void update_labels_popups();

bool process_labels();

void draw_labels();

#endif // INFINITE_UI_H