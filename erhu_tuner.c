
// Erhu Tuner v1.2 (UI tightened)
#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/gui.h>

#define D4 293.66f
#define A4 440.00f
#define VOLUME_DEFAULT 0.5f
#define VOLUME_STEP 0.05f

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

// Layout (tight)
#define TITLE_BOX_X   34
#define TITLE_BOX_Y   16
#define TITLE_BOX_W   60
#define TITLE_BOX_H   18
#define TITLE_BOX_R   4
#define TITLE_TEXT_Y  24
#define SUBTITLE_Y    38
#define STATUS_Y      10
#define SELECT_Y      54
#define LEFT_BOX_X     6
#define RIGHT_BOX_X    66
#define SELECT_BOX_Y  46
#define SELECT_BOX_W  56
#define SELECT_BOX_H  14
#define SELECT_BOX_R   3

static void render_cb(Canvas* canvas, void* ctx){
    ErhuState* st = ctx;
    furi_mutex_acquire(st->mutex, FuriWaitForever);

    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_draw_rframe(canvas, TITLE_BOX_X, TITLE_BOX_Y, TITLE_BOX_W, TITLE_BOX_H, TITLE_BOX_R);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, TITLE_TEXT_Y, AlignCenter, AlignBottom, "ERHU");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, SUBTITLE_Y, AlignCenter, AlignTop, "Tuning Fork");

    char buf[40];
    int pct = (int)(st->volume * 100.0f + 0.5f);
    const char* state = st->playing ? "Play" : "Ready";
    snprintf(buf, sizeof(buf), "Vol %d%% • %s", pct, state);
    canvas_draw_str_aligned(canvas, 64, STATUS_Y, AlignCenter, AlignTop, buf);

    canvas_draw_str_aligned(canvas, 10, SELECT_Y, AlignLeft, AlignCenter, "< Inner (D4)");
    canvas_draw_str_aligned(canvas, 118, SELECT_Y, AlignRight, AlignCenter, "Outer (A4) >");

    if(st->selection == SelInner){
        canvas_draw_rframe(canvas, LEFT_BOX_X, SELECT_BOX_Y, SELECT_BOX_W, SELECT_BOX_H, SELECT_BOX_R);
    }else{
        canvas_draw_rframe(canvas, RIGHT_BOX_X, SELECT_BOX_Y, SELECT_BOX_W, SELECT_BOX_H, SELECT_BOX_R);
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
    } else if(e->type == InputTypeLong && e->key == InputKeyBack){
        st->running = false;
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
