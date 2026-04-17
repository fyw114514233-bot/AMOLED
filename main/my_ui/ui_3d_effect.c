// #include "ui.h"

// #define NUM_ICONS 10
// #define UI_ICON_MAX_DRAG_X 150
// #define UI_ICON_MAX_DRAG_Y 180
// #define UI_ICON_DRAG_THRESHOLD 6

// typedef struct {
//     lv_coord_t x;
//     lv_coord_t y;
// } ui_icon_position_t;

// static lv_obj_t * icon_image_array[NUM_ICONS];
// static const ui_icon_position_t icon_base_positions[NUM_ICONS] = {
//     {0, 0},
//     {0, -150},
//     {-92, -96},
//     {92, -96},
//     {-170, -26},
//     {170, -26},
//     {-92, 68},
//     {92, 68},
//     {-170, 136},
//     {170, 136},
// };
// static lv_img_dsc_t * const icon_sources[NUM_ICONS] = {
//     (lv_img_dsc_t *)&ui_img_146967865,
//     (lv_img_dsc_t *)&ui_img_894936796,
//     (lv_img_dsc_t *)&ui_img_976659900,
//     (lv_img_dsc_t *)&ui_img_129493919,
//     (lv_img_dsc_t *)&ui_img_1250551528,
//     (lv_img_dsc_t *)&ui_img_455216396,
//     (lv_img_dsc_t *)&ui_img_1794977545,
//     (lv_img_dsc_t *)&ui_img_2019210241,
//     (lv_img_dsc_t *)&ui_img_1776925411,
//     (lv_img_dsc_t *)&ui_img_1893047984,
// };
// static lv_coord_t icon_last_x[NUM_ICONS];
// static lv_coord_t icon_last_y[NUM_ICONS];
// static uint16_t icon_last_zoom[NUM_ICONS];

// static lv_point_t ui_icon_press_point = {0, 0};
// static lv_coord_t ui_icon_drag_x = 0;
// static lv_coord_t ui_icon_drag_y = 0;
// static lv_coord_t ui_icon_drag_start_x = 0;
// static lv_coord_t ui_icon_drag_start_y = 0;
// static bool ui_icon_pressed = false;
// static bool ui_icon_dragging = false;
// static bool ui_icon_drag_recent = false;

// static lv_coord_t ui_icon_abs(lv_coord_t value)
// {
//     return (value < 0) ? -value : value;
// }

// static lv_coord_t ui_icon_max(lv_coord_t a, lv_coord_t b)
// {
//     return (a > b) ? a : b;
// }

// static lv_coord_t ui_icon_min(lv_coord_t a, lv_coord_t b)
// {
//     return (a < b) ? a : b;
// }

// static lv_coord_t ui_icon_clamp(lv_coord_t value, lv_coord_t min_value, lv_coord_t max_value)
// {
//     if(value < min_value) {
//         return min_value;
//     }
//     if(value > max_value) {
//         return max_value;
//     }
//     return value;
// }

// static uint16_t ui_icon_compute_zoom(lv_coord_t x, lv_coord_t y)
// {
//     lv_coord_t dx = ui_icon_abs(x);
//     lv_coord_t dy = ui_icon_abs(y);
//     lv_coord_t dist = ui_icon_max(dx, dy) + (ui_icon_min(dx, dy) / 2);

//     if(dist < 70) {
//         return 330;
//     }
//     if(dist < 120) {
//         return 292;
//     }
//     if(dist < 170) {
//         return 256;
//     }
//     if(dist < 220) {
//         return 220;
//     }

//     return 188;
// }

// static void ui_update_watch_layout(void)
// {
//     uint8_t i;

//     for(i = 0; i < NUM_ICONS; i++) {
//         lv_coord_t x;
//         lv_coord_t y;
//         uint16_t zoom;

//         if(icon_image_array[i] == NULL) {
//             continue;
//         }

//         x = icon_base_positions[i].x + ui_icon_drag_x;
//         y = icon_base_positions[i].y + ui_icon_drag_y;
//         zoom = ui_icon_compute_zoom(x, y);

//         if(icon_last_x[i] != x) {
//             lv_obj_set_x(icon_image_array[i], x);
//             icon_last_x[i] = x;
//         }

//         if(icon_last_y[i] != y) {
//             lv_obj_set_y(icon_image_array[i], y);
//             icon_last_y[i] = y;
//         }

//         if(icon_last_zoom[i] != zoom) {
//             lv_img_set_zoom(icon_image_array[i], zoom);
//             icon_last_zoom[i] = zoom;
//         }
//     }
// }

// static void ui_watch_touch_event_cb(lv_event_t * e)
// {
//     lv_event_code_t code = lv_event_get_code(e);
//     lv_indev_t * indev = lv_indev_get_act();

//     LV_UNUSED(e);

//     if(indev == NULL || ui_The_C_is_open()) {
//         return;
//     }

//     if(code == LV_EVENT_PRESSED) {
//         lv_indev_get_point(indev, &ui_icon_press_point);
//         ui_icon_drag_start_x = ui_icon_drag_x;
//         ui_icon_drag_start_y = ui_icon_drag_y;
//         ui_icon_pressed = true;
//         ui_icon_dragging = false;
//         ui_icon_drag_recent = false;
//         return;
//     }

//     if(code == LV_EVENT_PRESSING) {
//         lv_point_t point;
//         lv_coord_t dx;
//         lv_coord_t dy;

//         if(!ui_icon_pressed) {
//             return;
//         }

//         lv_indev_get_point(indev, &point);
//         dx = point.x - ui_icon_press_point.x;
//         dy = point.y - ui_icon_press_point.y;

//         if(!ui_icon_dragging &&
//            ui_icon_abs(dx) < UI_ICON_DRAG_THRESHOLD &&
//            ui_icon_abs(dy) < UI_ICON_DRAG_THRESHOLD) {
//             return;
//         }

//         ui_icon_drag_x = ui_icon_clamp(ui_icon_drag_start_x + dx, -UI_ICON_MAX_DRAG_X, UI_ICON_MAX_DRAG_X);
//         ui_icon_drag_y = ui_icon_clamp(ui_icon_drag_start_y + dy, -UI_ICON_MAX_DRAG_Y, UI_ICON_MAX_DRAG_Y);
//         ui_icon_drag_recent = true;
//         ui_icon_dragging = true;
//         ui_update_watch_layout();
//         return;
//     }

//     if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
//         ui_icon_pressed = false;
//         ui_icon_dragging = false;
//     }
// }

// bool ui_3d_effect_is_dragging(void)
// {
//     if(ui_icon_dragging) {
//         return true;
//     }

//     if(ui_icon_drag_recent) {
//         ui_icon_drag_recent = false;
//         return true;
//     }

//     return false;
// }

// static void ui_prepare_original_panel(lv_obj_t * panel)
// {
//     if(panel == NULL) {
//         return;
//     }

//     lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
// }

// static lv_obj_t * ui_create_icon_image(lv_obj_t * parent, lv_img_dsc_t * src)
// {
//     lv_obj_t * img = lv_img_create(parent);

//     lv_img_set_src(img, src);
//     lv_obj_set_align(img, LV_ALIGN_CENTER);
//     lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
//     lv_obj_add_flag(img, LV_OBJ_FLAG_GESTURE_BUBBLE);
//     lv_img_set_pivot(img, 32, 32);

//     return img;
// }

// void init_3d_ui_effect(void)
// {
//     uint8_t i;
//     lv_obj_t * panels[NUM_ICONS] = {
//         ui_Panel3,
//         ui_Panel1,
//         ui_Panel4,
//         ui_Panel5,
//         ui_Panel6,
//         ui_Panel7,
//         ui_Panel8,
//         ui_Panel9,
//         ui_Panel13,
//         ui_Panel14,
//     };

//     ui_icon_drag_x = 0;
//     ui_icon_drag_y = 0;
//     ui_icon_drag_start_x = 0;
//     ui_icon_drag_start_y = 0;
//     ui_icon_pressed = false;
//     ui_icon_dragging = false;
//     ui_icon_drag_recent = false;

//     lv_obj_add_flag(ui_Mune, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_GESTURE_BUBBLE);
//     lv_obj_add_event_cb(ui_Mune, ui_watch_touch_event_cb, LV_EVENT_ALL, NULL);

//     for(i = 0; i < NUM_ICONS; i++) {
//         icon_last_x[i] = 32767;
//         icon_last_y[i] = 32767;
//         icon_last_zoom[i] = 0;

//         ui_prepare_original_panel(panels[i]);
//         icon_image_array[i] = ui_create_icon_image(ui_Mune, icon_sources[i]);
//     }

//     ui_update_watch_layout();
// }
