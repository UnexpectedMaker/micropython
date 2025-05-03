#include "py/runtime.h"
#include "py/mphal.h"
#include "py/objarray.h"
#include "esp_log.h"

#if MICROPY_PY_MACHINE_RGB
extern const mp_obj_type_t machine_rgb_type;
#endif

// Core esp_lcd types and ops for RGB interface
#include "esp_lcd_types.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"

#if 0
#define DEBUG_printf(...) mp_printf(&mp_plat_print, __VA_ARGS__); \
                          mp_printf(&mp_plat_print, "\n")
#endif

// RGB panel object
typedef struct _machine_rgb_obj_t {
    mp_obj_base_t           base;
    esp_lcd_panel_handle_t  panel;
    mp_int_t                width;
    mp_int_t                height;
	mp_buffer_info_t 		bufinfo;
} machine_rgb_obj_t;

static mp_obj_t machine_rgb_make_new(const mp_obj_type_t *type,
                                     size_t n_args, size_t n_kw,
                                     const mp_obj_t *args) {
    // DEBUG_printf("make_new start");
    enum {
        ARG_width, ARG_height, ARG_data_pins, ARG_hsync, ARG_vsync, ARG_de, ARG_pclk, ARG_freq, ARG_num_fbs,
        ARG_psram_trans_align, ARG_sram_trans_align,
        ARG_bits_per_pixel, ARG_disp_gpio_num,
        ARG_hsync_idle_low, ARG_vsync_idle_low,
        ARG_hsync_back_porch, ARG_hsync_front_porch, ARG_hsync_pulse_width,
        ARG_vsync_back_porch, ARG_vsync_front_porch, ARG_vsync_pulse_width,
		ARG_on_demand, ARG_fb_in_psram, ARG_bounce_buffer_size_px
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_width,     MP_ARG_REQUIRED|MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_height,    MP_ARG_REQUIRED|MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_data_pins, MP_ARG_REQUIRED|MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_hsync,     MP_ARG_REQUIRED|MP_ARG_INT,  {.u_int = -1} },
        { MP_QSTR_vsync,     MP_ARG_REQUIRED|MP_ARG_INT,  {.u_int = -1} },
        { MP_QSTR_de,        MP_ARG_REQUIRED|MP_ARG_INT,  {.u_int = -1} },
        { MP_QSTR_pclk,      MP_ARG_REQUIRED|MP_ARG_INT,  {.u_int = -1} },
        { MP_QSTR_freq,      MP_ARG_REQUIRED|MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_num_fbs,          MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = 1} },
        { MP_QSTR_psram_trans_align,MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = 64} },
        { MP_QSTR_sram_trans_align, MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = 8} },
        { MP_QSTR_bits_per_pixel,   MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_disp_gpio_num,    MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = -1} },
        { MP_QSTR_hsync_idle_low,   MP_ARG_KW_ONLY|MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_vsync_idle_low,   MP_ARG_KW_ONLY|MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_hsync_back_porch, MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_hsync_front_porch,MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_hsync_pulse_width,MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_vsync_back_porch, MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_vsync_front_porch,MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_vsync_pulse_width,MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = 0} },
		{ MP_QSTR_on_demand,   		MP_ARG_KW_ONLY|MP_ARG_BOOL, {.u_bool = false} },
		{ MP_QSTR_fb_in_psram,   	MP_ARG_KW_ONLY|MP_ARG_BOOL, {.u_bool = true} },
		{ MP_QSTR_bounce_buffer_size_px, MP_ARG_KW_ONLY|MP_ARG_INT,  {.u_int = 0} },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args,
        MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    // Extract data_pins
    size_t data_width;
    mp_obj_t *pins_obj;
    mp_obj_list_get(vals[ARG_data_pins].u_obj, &data_width, &pins_obj);
    int *pins = m_new(int, data_width);
    for (size_t i = 0; i < data_width; i++) {
        pins[i] = mp_obj_get_int(pins_obj[i]);
    }
    // DEBUG_printf("pins loaded");

    // Create object
    machine_rgb_obj_t *self = m_new_obj(machine_rgb_obj_t);
    self->base.type = &machine_rgb_type;
    self->width  = vals[ARG_width].u_int;
    self->height = vals[ARG_height].u_int;

    // Build the panel config
    esp_lcd_rgb_panel_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.clk_src           = LCD_CLK_SRC_DEFAULT;
	cfg.psram_trans_align = vals[ARG_psram_trans_align].u_int;
    cfg.sram_trans_align  = vals[ARG_sram_trans_align].u_int;
    cfg.data_width        = (uint8_t)data_width;
    cfg.bits_per_pixel    = (uint8_t)vals[ARG_bits_per_pixel].u_int;
    cfg.num_fbs           = vals[ARG_num_fbs].u_int;
    cfg.disp_gpio_num     = vals[ARG_disp_gpio_num].u_int;
    cfg.pclk_gpio_num     = vals[ARG_pclk].u_int;
    cfg.vsync_gpio_num    = vals[ARG_vsync].u_int;
    cfg.hsync_gpio_num    = vals[ARG_hsync].u_int;
    cfg.de_gpio_num       = vals[ARG_de].u_int;

    for (size_t i = 0; i < data_width; i++) {
        cfg.data_gpio_nums[i] = pins[i];
    }

	cfg.bounce_buffer_size_px = vals[ARG_bounce_buffer_size_px].u_int;

    cfg.flags.fb_in_psram = vals[ARG_fb_in_psram].u_bool;
	cfg.flags.refresh_on_demand = vals[ARG_on_demand].u_bool;
	cfg.flags.no_fb = (vals[ARG_num_fbs].u_int == 0);

    cfg.timings.pclk_hz = vals[ARG_freq].u_int;
    cfg.timings.h_res   = vals[ARG_width].u_int;
    cfg.timings.v_res   = vals[ARG_height].u_int;
    cfg.timings.hsync_pulse_width = vals[ARG_hsync_pulse_width].u_int;
    cfg.timings.hsync_back_porch  = vals[ARG_hsync_back_porch].u_int;
    cfg.timings.hsync_front_porch = vals[ARG_hsync_front_porch].u_int;
    cfg.timings.vsync_pulse_width = vals[ARG_vsync_pulse_width].u_int;
    cfg.timings.vsync_back_porch  = vals[ARG_vsync_back_porch].u_int;
    cfg.timings.vsync_front_porch = vals[ARG_vsync_front_porch].u_int;
    cfg.timings.flags.hsync_idle_low = vals[ARG_hsync_idle_low].u_bool;
    cfg.timings.flags.vsync_idle_low = vals[ARG_vsync_idle_low].u_bool;
    // DEBUG_printf("cfg ready");

    // Create and init panel
    esp_err_t err;
    err = esp_lcd_new_rgb_panel(&cfg, &self->panel);
    // DEBUG_printf("new_rgb_panel err=%d", err);

    if (err != ESP_OK) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("new_rgb_panel failed"));
    }

    err = esp_lcd_panel_reset(self->panel);
    // DEBUG_printf("reset err=%d", err);
    if (err != ESP_OK) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("panel_reset failed"));
    }
    err = esp_lcd_panel_init(self->panel);
    // DEBUG_printf("init err=%d", err);
    if (err != ESP_OK) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("panel_init failed"));
    }

	void *fb;
    err = esp_lcd_rgb_panel_get_frame_buffer(self->panel, 1, &fb);
	// DEBUG_printf("panel_get_framebuffer err=%d", err);
    if (err != ESP_OK) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("panel_get_framebuffer failed"));
    }
	self->bufinfo.buf = (uint8_t *)fb;
	self->bufinfo.len = 2 * (cfg.timings.h_res * cfg.timings.v_res );
	self->bufinfo.typecode = 'H' | MP_OBJ_ARRAY_TYPECODE_FLAG_RW;

    return MP_OBJ_FROM_PTR(self);
}

// deinit()
static mp_obj_t machine_rgb_deinit(mp_obj_t self_in) {
    machine_rgb_obj_t *self = MP_OBJ_TO_PTR(self_in);
    // DEBUG_printf("deinit");
    esp_lcd_panel_del(self->panel);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_rgb_deinit_obj, machine_rgb_deinit);

// get_buffer() – return the raw FB as a Python bytearray (no copy)
static mp_obj_t machine_rgb_get_buffer(mp_obj_t self_in) {
    machine_rgb_obj_t *self = MP_OBJ_TO_PTR(self_in);
    // bufinfo.buf is a uint8_t*, bufinfo.len is number of bytes
    return mp_obj_new_bytearray_by_ref(self->bufinfo.len, self->bufinfo.buf);
}
static MP_DEFINE_CONST_FUN_OBJ_1(machine_rgb_get_buffer_obj, machine_rgb_get_buffer);

// blit(buf, x=0, y=0, w=-1, h=-1)
static mp_obj_t machine_rgb_blit(size_t n_args, const mp_obj_t *args) {
    machine_rgb_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    int x = 0, y = 0, w = self->width, h = self->height;

    // if you pass x,y
    if (n_args >= 3) {
        x = mp_obj_get_int(args[1]);
        y = mp_obj_get_int(args[2]);
    }
    // if you also pass w,h
    if (n_args == 5) {
        w = mp_obj_get_int(args[3]);
        h = mp_obj_get_int(args[4]);
    }

    // use the driver‐allocated PSRAM buffer directly
    esp_lcd_panel_draw_bitmap(self->panel, x, y, w, h, self->bufinfo.buf);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_rgb_blit_obj, 1, 5, machine_rgb_blit);

static const mp_rom_map_elem_t machine_rgb_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&machine_rgb_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_blit),   MP_ROM_PTR(&machine_rgb_blit_obj)   },
	{ MP_ROM_QSTR(MP_QSTR_get_buffer),MP_ROM_PTR(&machine_rgb_get_buffer_obj) },
};
static MP_DEFINE_CONST_DICT(machine_rgb_locals_dict, machine_rgb_locals_dict_table);

// type definition
MP_DEFINE_CONST_OBJ_TYPE(
    machine_rgb_type,
    MP_QSTR_RGB,
    MP_TYPE_FLAG_NONE,
    make_new, machine_rgb_make_new,
    locals_dict, &machine_rgb_locals_dict
);
