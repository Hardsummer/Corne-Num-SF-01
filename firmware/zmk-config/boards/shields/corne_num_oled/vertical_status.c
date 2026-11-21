/*
 * Corne Num: upright status screen for a 0.91in 128x32 OLED mounted vertically.
 * Everything is drawn into a 32x128 upright bitmap (one uint32 per row) and copied into a single
 * 128x32 canvas with the quarter turn done here, so no LVGL rotation is needed.
 *
 * Left half:  Bluetooth slot + status (or USB), own battery, steaming cup of tea (animated)
 * Right half: link to the left half, own battery, Golden Gate tower under a crescent moon, water rolling (animated)
 * The picture animates only while the keyboard is awake; it stops when the screen blanks.
 */
#include <zephyr/kernel.h>
#include <string.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/battery.h>
#include <zmk/usb.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/activity.h>
#include "art.h"

#define CENTRAL (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

#if CENTRAL
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#else
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/events/split_peripheral_status_changed.h>
#endif

#define VW 32  /* upright width */
#define VH 128 /* upright height */
#define BG lv_color_white() /* the panel runs with inversion-on: white = dark, black = lit */
#define FG lv_color_black()

static uint32_t vfb[VH];
static lv_color_t cbuf[VH * VW];
static lv_obj_t *canv;
static int frame;
#define ANIM_MS 150
#if CENTRAL
#define PIC art_tea
#define PIC_FRAMES ART_TEA_FRAMES
#else
#define PIC art_bridge
#define PIC_FRAMES ART_BRIDGE_FRAMES
#endif

static struct {
    uint8_t batt;
#if CENTRAL
    bool usb_out;
    int profile;
    bool connected, bonded;
#else
    bool linked;
#endif
} st;

/* ---------- tiny drawing kit on the upright bitmap ---------- */
static void pset(int x, int y) {
    if (x >= 0 && x < VW && y >= 0 && y < VH)
        vfb[y] |= 1u << x;
}
static void fill(int x, int y, int w, int h) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            pset(x + i, y + j);
}
static void box(int x, int y, int w, int h) {
    fill(x, y, w, 1);
    fill(x, y + h - 1, w, 1);
    fill(x, y, 1, h);
    fill(x + w - 1, y, 1, h);
}
static void glyph(int x, int y, const char *const *rows, int n) {
    for (int r = 0; r < n; r++)
        for (int c = 0; rows[r][c]; c++)
            if (rows[r][c] == '#')
                pset(x + c, y + r);
}
/* 3x5 font for digits, % and a few letters */
static const char *const *font(char ch) {
    static const char *const f[][5] = {
        {"###", "#.#", "#.#", "#.#", "###"}, {".#.", "##.", ".#.", ".#.", "###"},
        {"###", "..#", "###", "#..", "###"}, {"###", "..#", "###", "..#", "###"},
        {"#.#", "#.#", "###", "..#", "..#"}, {"###", "#..", "###", "..#", "###"},
        {"###", "#..", "###", "#.#", "###"}, {"###", "..#", "..#", "..#", "..#"},
        {"###", "#.#", "###", "#.#", "###"}, {"###", "#.#", "###", "..#", "###"},
        {"#.#", "..#", ".#.", "#..", "#.#"}, /* % */
        {"##.", "#.#", "##.", "#.#", "#.#"}, /* R */
        {"#.#", "#.#", "#.#", "#.#", "###"}, /* U */
        {"###", "#..", "###", "..#", "###"}, /* S */
        {"##.", "#.#", "##.", "#.#", "##."}, /* B */
        {"...", "...", "###", "...", "..."}, /* - */
    };
    if (ch >= '0' && ch <= '9')
        return f[ch - '0'];
    switch (ch) {
    case '%': return f[10];
    case 'R': return f[11];
    case 'U': return f[12];
    case 'S': return f[13];
    case 'B': return f[14];
    default: return f[15];
    }
}
static void text(int x, int y, const char *s) {
    for (; *s; s++, x += 4)
        glyph(x, y, font(*s), 5);
}
static void text_center(int y, const char *s) { text((VW - (4 * (int)strlen(s) - 1)) / 2, y, s); }
static void art(const uint32_t *a, int y0) {
    for (int r = 0; r < 64; r++)
        if (y0 + r < VH)
            vfb[y0 + r] |= a[r];
}

static const char *const BT[] = {"..#...", "..##..", "#.#.#.", ".###..", "..#...",
                                 ".###..", "#.#.#.", "..##..", "..#..."};
static const char *const CHECK[] = {".....#", "....#.", "#..#..", ".##...", "..#..."};
static const char *const CROSS[] = {"#...#", ".#.#.", "..#..", ".#.#.", "#...#"};
static const char *const OPEN[] = {".###.", "#...#", "#...#", "#...#", ".###."};
static const char *const LINK[] = {"..##....", ".#..#...", ".#..#...", "..##....",
                                   "...#.##.", "....#..#", "....#..#", ".....##."};

static void battery(int level) {
    box(3, 15, 22, 10);
    fill(25, 18, 2, 4);
    int w = (18 * level + 50) / 100;
    if (w > 0)
        fill(5, 17, w, 6);
    char s[6];
    snprintf(s, sizeof(s), "%d%%", level);
    text_center(28, s);
}

/* ---------- compose and push to the panel ---------- */
static void render(void) {
    memset(vfb, 0, sizeof(vfb));
#if CENTRAL
    if (st.usb_out) {
        text(4, 4, "USB");
    } else {
        glyph(3, 2, BT, 9);
        char s[3];
        snprintf(s, sizeof(s), "%d", st.profile + 1);
        text(12, 4, s);
        if (!st.bonded)
            glyph(19, 4, OPEN, 5);
        else if (st.connected)
            glyph(19, 4, CHECK, 5);
        else
            glyph(19, 4, CROSS, 5);
    }
    battery(st.batt);
    fill(2, 36, 28, 1);
    art(PIC[frame], 50);
#else
    if (st.linked)
        glyph(3, 2, LINK, 8);
    else
        glyph(4, 3, CROSS, 5);
    text(16, 4, "R");
    battery(st.batt);
    fill(2, 36, 28, 1);
    art(PIC[frame], 50);
#endif
    /* quarter turn: upright (x, y) -> panel (px, py) on the 128x32 canvas */
    for (int y = 0; y < VH; y++) {
        for (int x = 0; x < VW; x++) {
            int px, py;
            if (IS_ENABLED(CONFIG_CORNE_NUM_OLED_FLIP)) {
                px = y;
                py = VW - 1 - x;
            } else {
                px = VH - 1 - y;
                py = x;
            }
            cbuf[py * VH + px] = (vfb[y] >> x) & 1 ? FG : BG;
        }
    }
    if (canv)
        lv_obj_invalidate(canv);
}

/* ---------- state listeners ---------- */
#if CENTRAL
struct conn_state {
    bool usb_out;
    int profile;
    bool connected, bonded;
};
static void conn_cb(struct conn_state s) {
    st.usb_out = s.usb_out;
    st.profile = s.profile;
    st.connected = s.connected;
    st.bonded = s.bonded;
    render();
}
static struct conn_state conn_get(const zmk_event_t *eh) {
    return (struct conn_state){.usb_out = zmk_endpoints_selected().transport == ZMK_TRANSPORT_USB,
                               .profile = zmk_ble_active_profile_index(),
                               .connected = zmk_ble_active_profile_is_connected(),
                               .bonded = !zmk_ble_active_profile_is_open()};
}
ZMK_DISPLAY_WIDGET_LISTENER(cn_conn, struct conn_state, conn_cb, conn_get)
ZMK_SUBSCRIPTION(cn_conn, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(cn_conn, zmk_ble_active_profile_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(cn_conn, zmk_usb_conn_state_changed);
#endif
#else
struct link_state {
    bool linked;
};
static void link_cb(struct link_state s) {
    st.linked = s.linked;
    render();
}
static struct link_state link_get(const zmk_event_t *eh) {
    return (struct link_state){.linked = zmk_split_bt_peripheral_is_connected()};
}
ZMK_DISPLAY_WIDGET_LISTENER(cn_link, struct link_state, link_cb, link_get)
ZMK_SUBSCRIPTION(cn_link, zmk_split_peripheral_status_changed);
#endif

struct batt_state {
    uint8_t level;
};
static void batt_cb(struct batt_state s) {
    st.batt = s.level;
    render();
}
static struct batt_state batt_get(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct batt_state){.level = ev ? ev->state_of_charge : zmk_battery_state_of_charge()};
}
ZMK_DISPLAY_WIDGET_LISTENER(cn_batt, struct batt_state, batt_cb, batt_get)
ZMK_SUBSCRIPTION(cn_batt, zmk_battery_state_changed);

/* ---------- animation: runs on the display work queue while the keyboard is active ---------- */
static void anim_fn(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(anim_work, anim_fn);
static atomic_t anim_on = ATOMIC_INIT(1);
static void anim_fn(struct k_work *w) {
    if (!atomic_get(&anim_on))
        return;
    frame = (frame + 1) % PIC_FRAMES;
    render();
    k_work_schedule_for_queue(zmk_display_work_q(), &anim_work, K_MSEC(ANIM_MS));
}
static int anim_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL)
        return ZMK_EV_EVENT_BUBBLE;
    if (ev->state == ZMK_ACTIVITY_ACTIVE) {
        atomic_set(&anim_on, 1);
        k_work_schedule_for_queue(zmk_display_work_q(), &anim_work, K_MSEC(ANIM_MS));
    } else {
        atomic_set(&anim_on, 0);
        k_work_cancel_delayable(&anim_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(cn_anim, anim_listener);
ZMK_SUBSCRIPTION(cn_anim, zmk_activity_state_changed);

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    canv = lv_canvas_create(screen);
    lv_canvas_set_buffer(canv, cbuf, VH, VW, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(canv, 0, 0);
    render();
#if CENTRAL
    cn_conn_init();
#else
    cn_link_init();
#endif
    cn_batt_init();
    k_work_schedule_for_queue(zmk_display_work_q(), &anim_work, K_MSEC(ANIM_MS));
    return screen;
}
