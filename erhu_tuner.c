// Erhu Tuner v1.3 — minimal UI (no ERHU box, boxed selections)
// Controls: LEFT=Inner(D4), RIGHT=Outer(A4), OK=play/stop, UP/DOWN=volume, hold BACK=quit
#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <stdlib.h>

// --- Tones ---
#define D4 293.66f
#define A4 440.00f

// --- Audio ---
#define VOLUME_DEFAULT 0.5f
#define VOLUME_STEP    0.05f

// ~1 mm on Flipper screen ≈ ~6 px (safe)
#define MARGIN 6

typedef enum { SelInner = 0, SelOuter = 1 } ErhuSelection;

typedef struct {
    FuriMutex*   mutex;
    Gui*         gui;
    ViewPort*    vp;
    float        volume;
    ErhuSelection selection;
    bool         playing;
    bool         running;
} ErhuState;

static inline float sel_to_freq(ErhuSelection s) { return (s == SelInner) ? D4 : A4; }

static void erhu_play(ErhuState* st) {
    if(!st->playing) {
        if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(1000)) {
            furi_hal_speaker_start(sel_to_freq(st->selection), st->volume);
            st->playing = true;
        }
    }
}
static void erhu_stop(ErhuState* st) {
    if(st->playing && furi_hal_speaker_is_mine()) furi_hal_speaker_stop();
    st->playing = false;
}
static inline void erhu_toggle(ErhuState* st) { st->playing ? erhu_stop(st) : erhu_play(st); }

// -------------------- Layout --------------------
// Screen is 128x64. Keep >= MARGIN from edges, ~MARGIN between elements.
#define TITLE_Y        (MARGIN + 12)   // "ERHU" (no box), centered
#define SUBTITLE_Y     (TITLE_Y + 10)  // "Tuning Fork" ~1 mm below title

// Selection boxes near bottom with >= 1 mm bottom margin
#define SEL_BOX_H      14
#define SEL_BOX_W      56              // wide enough for text inside
#define SEL_BOX_Y      (64 - MARGIN - SEL_BOX_H)
#define SEL_LEFT_X     (MARGIN)
#define SEL_RIGHT_X    (128 - MARGIN - SEL_BOX_W)

static void render_cb(Canvas* canvas, void* ctx) {
    ErhuState* st = ctx;
    furi_mutex_acquire(st->mutex, FuriWaitForever);

    // --- Title (no surrounding frame) ---
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, TITLE_Y, AlignCenter, AlignBottom, "ERHU");

    // --- Subtitle ---
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, SUBTITLE_Y, AlignCenter, AlignTop, "Tuning Fork");

    // --- Selection boxes (wrap labels) ---
    // Box outlines (only the selected one)
    if(st->selection == SelInner) {
        canvas_draw_rframe(canvas, SEL_LEFT_X,  SEL_BOX_Y, SEL_BOX_W, SEL_BOX_H, 3);
    } else {
        canvas_draw_rframe(canvas, SEL_RIGHT_X, SEL_BOX_Y, SEL_BOX_W, SEL_BOX_H, 3);
    }

    // Labels centered inside boxes
    const int16_t inner_cx = SEL_LEFT_X  + SEL_BOX_W/2;
    const int16_t outer_cx = SEL_RIGHT_X + SEL_BOX_W/2;
    const int16_t box_cy   = SEL_BOX_Y   + SEL_BOX_H/2;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, inner_cx, box_cy, AlignCenter, AlignCenter, "Inner (D4)");
    canvas_draw_str_aligned(canvas, outer_cx, box_cy, AlignCenter, AlignCenter, "Outer (A4)");

    furi_mutex_release(st->mutex);
}

static void input_cb(InputEvent* e, void* ctx) {
    ErhuState* st = ctx;

    if(e->type == InputTypeShort || e->type == InputTypeRepeat) {
        switch(e->key) {
            case InputKeyLeft:  st->selection = SelInner; break;
            case InputKeyRight: st->selection = SelOuter; break;
            case InputKeyOk:    erhu_toggle(st);          break;
            case InputKeyUp:
                if(st->volume <= 1.0f - VOLUME_STEP) st->volume += VOLUME_STEP;
                if(st->playing) erhu_play(st);
                break;
            case InputKeyDown:
                if(st->volume >= VOLUME_STEP) st->volume -= VOLUME_STEP;
                if(st->playing) erhu_play(st);
                break;
            default: break;
        }
    } else if(e->type == InputTypeLong && e->key == InputKeyBack) {
        st->running = false; // quit
    }
}

int32_t erhu_tuner_app() {
    ErhuState* st = malloc(sizeof(ErhuState));
    st->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    st->gui   = furi_record_open(RECORD_GUI);
    st->vp    = view_port_alloc();

    st->volume    = VOLUME_DEFAULT;
    st->selection = SelOuter;
    st->playing   = false;
    st->running   = true;

    view_port_draw_callback_set(st->vp, render_cb, st);
    view_port_input_callback_set(st->vp, input_cb, st);
    gui_add_view_port(st->gui, st->vp, GuiLayerFullscreen);

    while(st->running) furi_delay_ms(50);

    erhu_stop(st);
    gui_remove_view_port(st->gui, st->vp);
    view_port_free(st->vp);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(st->mutex);
    free(st);
    return 0;
}
