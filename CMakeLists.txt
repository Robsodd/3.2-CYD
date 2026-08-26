SET(SOURCES ui_comp_barmenu.c
    ui_comp_btnminimise.c
    ui_comp_ledsettings.c
    ui_comp.c
    ui_Calibrate.c
    ui_ScreenHome.c
    ui_ScreenLEDs.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_events.c)

add_library(ui ${SOURCES})
