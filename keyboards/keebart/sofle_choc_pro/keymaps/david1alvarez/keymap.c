// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#define NUM_TIMEOUT 20 * 60 * 1000  // 10 min milliseconds

// reset activity timestamps on keyboard init for screensaver logic
void keyboard_post_init_user(void) {
    set_activity_timestamps(0,0,0);
}

enum layers {
    BASE,  // default layer
    GAME,  // gaming-compatible layer
    ARROW,  // arrowkey movement
    MOUSE, // mouse movement
    SYM,   // symbols and function keys
    NONE,  // nonfunctional layer for developement use
};

// set default values
uint16_t last_active_layer = BASE;
bool is_screen_saver_active = false;
uint16_t rgb_val = RGB_MATRIX_DEFAULT_VAL;

// RGB matrix setting for each layer
void set_matrix_by_layer(enum layers layer) {
    switch (layer) {
        case BASE: // always use gradient for base layer, simplify logic
            rgb_matrix_set_speed(100);
            rgb_matrix_sethsv(175, 200, rgb_val);
            rgb_matrix_mode(RGB_MATRIX_GRADIENT_UP_DOWN);
            break;
        case GAME:
            rgb_matrix_set_speed_noeeprom(25);
            rgb_matrix_sethsv_noeeprom(0, 200, rgb_val);
            rgb_matrix_mode_noeeprom(RGB_MATRIX_CYCLE_OUT_IN_DUAL);
            break;
        case MOUSE:
            rgb_matrix_set_speed(100);
            rgb_matrix_sethsv_noeeprom(0, 0, rgb_val - 30 > RGB_VAL_MIN ? rgb_val - 30 : RGB_VAL_MIN);
            rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
            break;
        case ARROW:
            rgb_matrix_set_speed(100);
            rgb_matrix_sethsv_noeeprom(0, 0, rgb_val - 50 > RGB_VAL_MIN ? rgb_val - 50 : RGB_VAL_MIN);
            rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
            break;
        case SYM:
            rgb_matrix_set_speed(100);
            rgb_matrix_sethsv_noeeprom(150, 200, rgb_val);
            rgb_matrix_mode_noeeprom(RGB_MATRIX_GRADIENT_UP_DOWN);
            break;
        case NONE: 
            rgb_matrix_set_speed(255);
            rgb_matrix_sethsv_noeeprom(0, 100, rgb_val);
            rgb_matrix_mode_noeeprom(RGB_MATRIX_BREATHING);
            break;
        default:
            return;
    }
}

// Restore lighting effects when diasabling a layer
void lighting_dynamic_rollback(void) {
    if (IS_LAYER_ON(SYM)) {
        set_matrix_by_layer(SYM);
    } else if (IS_LAYER_ON(ARROW)) {
        set_matrix_by_layer(ARROW);
    } else if (IS_LAYER_ON(MOUSE)) {
        set_matrix_by_layer(MOUSE);
    } else if (IS_LAYER_ON(GAME)) {
        set_matrix_by_layer(GAME);
    } else {
        set_matrix_by_layer(BASE);
    }
}

// Reset the board brightness
void reset_rgb_val(void) {
    uint8_t hue = rgb_matrix_get_hue();
    uint8_t sat = rgb_matrix_get_sat();
    rgb_matrix_sethsv(hue, sat, RGB_MATRIX_DEFAULT_VAL);
    rgb_val = RGB_MATRIX_DEFAULT_VAL;
}

// Per-tick check, use for screensaver detection
// Overwrites default/provided matrix_scan_user function behavior
void matrix_scan_user(void) { // matrix screensaver
    if (is_screen_saver_active && last_input_activity_elapsed() < NUM_TIMEOUT) {
        set_matrix_by_layer(last_active_layer);
        layer_move(last_active_layer);
        is_screen_saver_active = false;
    }
    if (!is_screen_saver_active && last_input_activity_elapsed() > NUM_TIMEOUT) {
        rgb_matrix_set_speed_noeeprom(150);
        rgb_matrix_sethsv_noeeprom(0, 255, rgb_val);
        rgb_matrix_mode_noeeprom(RGB_MATRIX_DIGITAL_RAIN);
        is_screen_saver_active = true;
    }
}

// single tap: j, double tap: enable mouse layer
void dance_j(tap_dance_state_t *state, void *user_data) {
    if (state->count == 1) {
        tap_code(KC_J);
    } else {
        layer_on(MOUSE);
        set_matrix_by_layer(MOUSE);
    }
}
// single tap: h, double tap: disable mouse layer
void dance_h(tap_dance_state_t *state, void *user_data) {
    if (state->count == 1) {
        tap_code(KC_H);
    } else {
        layer_off(MOUSE);
        set_matrix_by_layer(last_active_layer);
    }
}

enum tapdances { TD_J_MOUSE_ON, TD_H_MOUSE_OFF };
tap_dance_action_t tap_dance_actions[] = {
    [TD_J_MOUSE_ON] = ACTION_TAP_DANCE_FN(dance_j),
    [TD_H_MOUSE_OFF] = ACTION_TAP_DANCE_FN(dance_h),
};

#define KC_TD_J TD(TD_J_MOUSE_ON)
#define KC_TD_H TD(TD_H_MOUSE_OFF)

// Keypress intercept
// Overwrites default/provided process_record_user function behavior
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (is_screen_saver_active) {
        // require keypress to exit screensaver mode
        return false;
    }   
    switch (keycode) {
        case TG(MOUSE): // toggle mouse layer, returning to GAME or BASE layer as appropriate
            if (record->event.pressed) {
                if (IS_LAYER_ON(MOUSE)) {
                    layer_off(MOUSE);
                    lighting_dynamic_rollback();
                } else {
                    layer_on(MOUSE);
                    set_matrix_by_layer(MOUSE);
                }
            }
            return false;
        case TO(GAME): // toggle between GAME and BASE layers
            if (record->event.pressed) {
                if (IS_LAYER_ON(GAME)) {
                    layer_off(GAME);
                    lighting_dynamic_rollback();
                } else {
                    layer_on(GAME);
                    set_matrix_by_layer(GAME);
                }
            }
            return false;
        case MO(ARROW): // momentary arrow layer
            if (record->event.pressed) {
                layer_on(ARROW);
                set_matrix_by_layer(ARROW);
            } else {
                layer_off(ARROW);
                lighting_dynamic_rollback();
            }
            return false;
        case MO(SYM): // momentary symbols layer
            if (record->event.pressed) {
                layer_on(SYM);
                set_matrix_by_layer(SYM);
            } else {
                layer_off(SYM);
                lighting_dynamic_rollback();
            }
            return false;
        case PB_1: // reset brightness to default, save to persistent storage
            if(record->event.pressed) {
                reset_rgb_val();
            }
            return false;
        case RM_VALU: // increase brightness up to maximum, save to persistent storage
            if (record->event.pressed) {
                rgb_val = rgb_matrix_get_val();
                if (rgb_val + 10 <= RGB_VAL_MAX) {
                    rgb_val += 10;
                } else {
                    rgb_val = RGB_VAL_MAX;
                }
                rgb_matrix_sethsv(rgb_matrix_get_hue(), rgb_matrix_get_sat(), rgb_val);
            }
            return false;
        case RM_VALD: // decrease brightness down to minimum, save to persistent storage
            if (record->event.pressed) {
                rgb_val = rgb_matrix_get_val();
                if (rgb_val - 10 >= RGB_VAL_MIN) {
                    rgb_val -= 10;
                } else {
                    rgb_val = RGB_VAL_MIN;
                } 
                rgb_matrix_sethsv(rgb_matrix_get_hue(), rgb_matrix_get_sat(), rgb_val);
            }
            return false;
        default:
            return true; /* Process all other keycodes normally */
    }
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
/*
 * BASE -- QWERTY layer
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * | Esc  |   1! |   2@ |   3# |   4$ |   5% |                    |   6^ |   7& |   8* |  9(  |  0)  |XXXXXX|
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | `~   |   Q  |   W  |   E  |   R  |   T  |                    |   Y  |   U  |   I  |   O  |  ;:  | Bspc |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | Tab  |   A  |   S  |   D  |   F  |   G  |-------.    ,-------| TD_H |   J  |   K  |   L  |   P  | Entr |
 * |------+------+------+------+------+------|  Caps |    | Mute  |------+------+------+------+------+------|
 * |LShift|   Z  |   X  |   C  |   V  |   B  |-------|    |-------|   N  |   M  |  ,<  |  .>  |  /?  |  \|  |
 * `-----------------------------------------/       /    \       \-----------------------------------------'
 *            | LCTL | LALT | LCMD | MO   | /LShift /      \ Space \  | TT   |RShift| Page | Page |
 *            |      |      |      | SYM  |/       /        \       \ | ARROW|      | Up   | Down |
 *            '-----------------------------------'          '-------''---------------------------'
 */
[BASE] = LAYOUT_split_4x6_5(
    KC_ESC,   KC_1,   KC_2,    KC_3,    KC_4,    KC_5,                         KC_6,     KC_7,     KC_8,    KC_9,    KC_0,     XXXXXXX,
    KC_GRAVE, KC_Q,   KC_W,    KC_E,    KC_R,    KC_T,                         KC_Y,     KC_U,     KC_I,    KC_O,    KC_SCLN,  KC_BSPC,
    KC_TAB,   KC_A,   KC_S,    KC_D,    KC_F,    KC_G,                         KC_TD_H,  KC_TD_J,  KC_K,    KC_L,    KC_P,     KC_ENTER,
    KC_LSFT,  KC_Z,   KC_X,    KC_C,    KC_V,    KC_B,    KC_CAPS,   KC_MUTE,  KC_N,     KC_M,     KC_COMM, KC_DOT,  KC_SLSH,  KC_BSLS,
                      KC_LCTL, KC_LALT, KC_LGUI, MO(SYM), KC_LSFT,   KC_SPC,  MO(ARROW), TG(MOUSE),  KC_PGUP, KC_PGDN
),

/* 
 * GAME -- gaming-compatible layer
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * |      |      |      |      |      |      |                    |      |      |      |      |      |TOGAME|
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |      |      |      |      |      |                    |      |      |      |      |      |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |      |      |      |      |      |-------.    ,-------|      |      |      |      |      |      |
 * |------+------+------+------+------+------|       |    |       |------+------+------+------+------+------|
 * |      |      |      |      |      |      |-------|    |-------|      |      |      |      |      |      |
 * `-----------------------------------------/       /    \       \-----------------------------------------'
 *            | LCMD | LALT | LCTL | Space| /  MO   /      \       \  | Enter|      |      |      |
 *            |      |      |      |      |/  SYM  /        \       \ |      |      |      |      |
 *            '-----------------------------------'          '-------''---------------------------'
 */
 [GAME] = LAYOUT_split_4x6_5(
    _______,  _______,_______, _______, _______, _______,                    _______, _______, _______, _______, _______,  TO(GAME),
    _______,  _______,_______, _______, _______, _______,                    _______, _______, _______, _______, _______,  _______,
    _______,  _______,_______, _______, _______, _______,                    _______, _______, _______, _______, _______,  _______,
    _______,  _______,_______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______,  _______,
                      KC_LGUI, KC_LALT, KC_LCTL, KC_SPC,  MO(SYM),  _______, _______, _______, _______, _______
),

/*
 * MOUSE -- Mouse movement
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * |      |      |      |      |      |      |                    |      |      |      |      |      |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |      |      |      |      |      |                    |      |      |  mUp |      |      |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |      |      |      |      |      |-------.    ,-------| TD_H |mLeft |mDown |mRight|      |      |
 * |------+------+------+------+------+------|       |    |       |------+------+------+------+------+------|
 * |      |      |      |      |      |      |-------|    |-------|  M3  |      |      |      |      |      |
 * `-----------------------------------------/       /    \       \-----------------------------------------'
 *            |      |      |      |      | /       /      \   M1  \  |  M2  |      |      |      |
 *            |      |      |      |      |/       /        \       \ |      |      |      |      |
 *            '-----------------------------------'          '-------''---------------------------'
 */
 [MOUSE] = LAYOUT_split_4x6_5(
    _______,_______,_______,_______,_______,_______,                  _______,_______,_______,_______,_______,_______,
    _______,_______,_______,_______,_______,_______,                  _______,_______, MS_UP ,_______,_______,_______,
    _______,_______,_______,_______,_______,_______,                  KC_TD_H,MS_LEFT,MS_DOWN,MS_RGHT,_______,_______,
    _______,_______,_______,_______,_______,_______,_______,  _______,MS_BTN3,_______,_______,_______,_______,_______,
                _______,_______,_______,_______,_______,          MS_BTN1,MS_BTN2,_______,_______,_______
),

/*
 * ARROW -- Arrow key movement
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * |      |      |      |      |      |      |                    |      |      |      |      |      |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |      |      |      |      |      |                    |      |      |  Up  |      |      |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |      |      |      |      |      |-------.    ,-------|      | Left | Down | Right|      |      |
 * |------+------+------+------+------+------|       |    |       |------+------+------+------+------+------|
 * |      |      |      |      |      |      |-------|    |-------|      |      |      |      |      |      |
 * `-----------------------------------------/       /    \       \-----------------------------------------'
 *            |      |      |      |      | /       /      \       \  |      |      |      |      |
 *            |      |      |      |      |/       /        \       \ |      |      |      |      |
 *            '-----------------------------------'          '-------''---------------------------'
 */
 [ARROW] = LAYOUT_split_4x6_5(
    _______,_______,_______,_______,_______,_______,                  _______,_______,_______,_______,_______,_______,
    _______,_______,_______,_______,_______,_______,                  _______,_______, KC_UP ,_______,_______,_______,
    _______,_______,_______,_______,_______,_______,                  _______,KC_LEFT,KC_DOWN,KC_RGHT,_______,_______,
    _______,_______,_______,_______,_______,_______,_______,  _______,_______,_______,_______,_______,_______,_______,
                _______,_______,_______,_______,_______,          _______,_______,_______,_______,_______
),

/*
 * SYMBOLS -- function keys and symbols
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * |      |   F1 |   F2 |   F3 |   F4 |   F5 |                    |   F6 |   F7 |   F8 |  F9  |  F10 |TOGAME|
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |   !  |   @  |   #  |   $  |   %  |                    |   ^  |   &  |   *  |  F11 |  F12 |  Del |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |   |  |   -  |   =  |   '  |   <  |-------.    ,-------|   [  |   {  |   }  |   (  |   )  |      |
 * |------+------+------+------+------+------|RGB Val|    |       |------+------+------+------+------+------|
 * |      |   \  |   _  |   +  |   "  |   >  |-------|    |-------|   ]  |   >  |   ;  |   :  |   ?  |      |
 * `-----------------------------------------/       /    \       \-----------------------------------------'
 *            |      |      |      |      | /       /      \       \  |      |      | Home | End  |
 *            |      |      |      |      |/       /        \       \ |      |      |      |      |
 *            '-----------------------------------'          '-------''---------------------------'
 */
[SYM] = LAYOUT_split_4x6_5(
    _______, KC_F1,        KC_F2,         KC_F3,       KC_F4,        KC_F5,                             KC_F6,     KC_F7,        KC_F8,        KC_F9,         KC_F10,        TO(GAME),
    _______, LSFT(KC_1),   LSFT(KC_2),    LSFT(KC_3),  LSFT(KC_4),   LSFT(KC_5),                        LSFT(KC_6),LSFT(KC_7),   LSFT(KC_8),   KC_F11,        KC_F12,        KC_DEL,
    _______, LSFT(KC_BSLS),KC_MINS,       KC_EQL,      KC_QUOT,      LSFT(KC_COMM),                     KC_LBRC,   LSFT(KC_LBRC),LSFT(KC_RBRC),LSFT(KC_9),    LSFT(KC_0),    _______,
    _______, KC_BSLS,      LSFT(KC_MINS), LSFT(KC_EQL),LSFT(KC_QUOT),LSFT(KC_DOT),  PB_1,     _______,  KC_RBRC,   LSFT(KC_DOT), KC_SCLN,      LSFT(KC_SCLN), LSFT(KC_SLSH), _______,
                           _______,       _______,     _______,      _______,       _______,  _______,  _______,   _______,      KC_HOME,      KC_END
),

/*
 * Template -- An empty layer for formatting future layers
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * |      |      |      |      |      |      |                    |      |      |      |      |      |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |      |      |      |      |      |                    |      |      |      |      |      |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |      |      |      |      |      |-------.    ,-------|      |      |      |      |      |      |
 * |------+------+------+------+------+------|       |    |       |------+------+------+------+------+------|
 * |      |      |      |      |      |      |-------|    |-------|      |      |      |      |      |      |
 * `-----------------------------------------/       /    \       \-----------------------------------------'
 *            |      |      |      |      | /       /      \       \  |      |      |      |      |
 *            |      |      |      |      |/       /        \       \ |      |      |      |      |
 *            '-----------------------------------'          '-------''---------------------------'
 */
 [NONE] = LAYOUT_split_4x6_5(
    _______,_______,_______,_______,_______,_______,                  _______,_______,_______,_______,_______,_______,
    _______,_______,_______,_______,_______,_______,                  _______,_______,_______,_______,_______,_______,
    _______,_______,_______,_______,_______,_______,                  _______,_______,_______,_______,_______,_______,
    _______,_______,_______,_______,_______,_______,_______,  _______,_______,_______,_______,_______,_______,_______,
                _______,_______,_______,_______,_______,          _______,_______,_______,_______,_______
)

};

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [BASE] = { ENCODER_CCW_CW(KC_WH_D, KC_WH_U), ENCODER_CCW_CW(KC_WH_D, KC_WH_U) },
    [GAME] = { ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______) },
    [MOUSE] = { ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(KC_WH_D, KC_WH_U) },
    [ARROW] = { ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______) },
    [SYM] = { ENCODER_CCW_CW(RM_VALD, RM_VALU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [NONE] = { ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______) },
};
#endif


