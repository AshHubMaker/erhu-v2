// Erhu Tuner v1.3 (minimal UI, 1mm margins)
// - No box around "ERHU"
// - Keep selection boxes, narrower
// - At least ~1mm from screen edges and ~1mm spacing between elements
// - OK=toggle, Hold BACK=quit, LEFT/RIGHT=select, UP/DOWN=volume (no on-screen volume)
#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <stdlib.h>

#define D4 293.66f
#define A4 440.00f
#define VOLUME_DEFAULT 0.5f
#define VOLUME_STEP 0.05f

// ~1mm margin on Flipper ≈ a few pixels; we choose 6px to be safely >= 1mm
#define MARGIN 6

typedef enum { SelInner = 0, SelOuter = 1 } ErhuSelection;

typedef struct {
    FuriMutex* mutex;
    Gui* gui;
    ViewPort* vp;
    float volume;
    ErhuSelection selection;
    bool playing;
    bool running;
} ErhuState;

static float sel_to_freq(ErhuSelection s) { return s == SelInner ? D4 : A4; }

static void erhu_play(ErhuState* st){
    if(!st->playing){
        if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(1000)){
            furi_hal_speaker_start(sel_to_freq(st->selection), st->volume);
            st->playing = true;
        }
    }
}
static void erhu_stop(ErhuState* st){
    if(st->playing && furi_hal_speaker_is_mine()) furi_hal_speaker_stop();
    st->playing = false;
}
static void erhu_toggle(ErhuState* st){ st->playing ? erhu_stop(st) : erhu_play(st); }

// Layout (128x64). We keep >=MARGIN from edges and ~MARGIN between lines.
#define TITLE_Y       (MARGIN + 12)   // "ERHU" (no box), centered
#define SUBTITLE_Y    (TITLE_Y + 12)  // "Tuning Fork", ~1mm below title
// Selection boxes sit near bottom with >=1mm bottom margin
#define SEL_BOX_H     14
#define SEL_BOX_W     52
#define SEL_BOX_Y     (64 - MARGIN - SEL_BOX_H) // ensures bottom margin
#define SEL_LEFT_X    (MARGIN)
#define SEL_RIGHT_X   (128 - MARGIN - SEL_BOX_W)
#define SEL_LABEL_Y   (SEL_BOX_Y - 2) // label a couple px above boxes

static void render_cb(Canvas* canvas, void* ctx){
    ErhuState* st = ctx;
    furi_mutex_acquire(st->mutex, FuriWaitForever);

    // No outer frame; keep clean edges with margin

    // Title (no box)
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, TITLE_Y, AlignCenter, AlignBottom, "ERHU");

    // Subtitle
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, SUBTITLE_Y, AlignCenter, AlignTop, "Tuning Fork");

    // Selection labels
    canvas_draw_str_aligned(canvas, MARGIN+4, SEL_LABEL_Y, AlignLeft, AlignBottom, "Inner (D4)");
    canvas_draw_str_aligned(canvas, 128 - MARGIN - 4, SEL_LABEL_Y, AlignRight, AlignBottom, "Outer (A4)");

    // Selection highlight boxes (narrow)
    if(st->selection == SelInner){
        canvas_draw_rframe(canvas, SEL_LEFT_X, SEL_BOX_Y, SEL_BOX_W, SEL_BOX_H, 3);
    }else{
        canvas_draw_rframe(canvas, SEL_RIGHT_X, SEL_BOX_Y, SEL_BOX_W, SEL_BOX_H, 3);
    }

    furi_mutex_release(st->mutex);
}

static void input_cb(InputEvent* e, void* ctx){
    ErhuState* st = ctx;
    if(e->type == InputTypeShort || e->type == InputTypeRepeat){
        if(e->key == InputKeyLeft) st->selection = SelInner;
        else if(e->key == InputKeyRight) st->selection = SelOuter;
        else if(e->key == InputKeyOk) erhu_toggle(st);
        else if(e->key == InputKeyUp){
            if(st->volume <= 1.0f - VOLUME_STEP) st->volume += VOLUME_STEP;
            if(st->playing) erhu_play(st);
        } else if(e->key == InputKeyDown){
            if(st->volume >= VOLUME_STEP) st->volume -= VOLUME_STEP;
            if(st->playing) erhu_play(st);
        }
        // Back (short) does nothing
    } else if(e->type == InputTypeLong && e->key == InputKeyBack){
        st->running = false; // quit
    }
}

int32_t erhu_tuner_app(){
    ErhuState* st = malloc(sizeof(ErhuState));
    st->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    st->gui = furi_record_open(RECORD_GUI);
    st->vp = view_port_alloc();
    st->volume = VOLUME_DEFAULT;
    st->selection = SelOuter;
    st->playing = false;
    st->running = true;

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
