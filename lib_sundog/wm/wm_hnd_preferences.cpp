/*
    wm_hnd_preferences.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2011 - 2022 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#if defined(OS_ANDROID) && !defined(NOFILEUTILS)
    #include "main/android/sundog_bridge.h"
#endif

struct prefs_data
{
    WINDOWPTR win;
    WINDOWPTR close;
    WINDOWPTR sections;
    int list_xsize;
    int cur_section;
    WINDOWPTR cur_section_window;
    
    int correct_ysize;
};

int prefs_close_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_data* data = (prefs_data*)user_data;

    remove_window( data->win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    
    if( wm->prefs_restart_request )
    {
	if( dialog( 0, wm_get_string( STR_WM_PREFS_CHANGED ), wm_get_string( STR_WM_YESNO ), wm ) == 0 )
	{
	    wm->exit_request = 1;
	    wm->restart_request = 1;
	}
    }
    
    return 0;
}

int prefs_sections_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_data* data = (prefs_data*)user_data;
    slist_data* ldata = list_get_data( data->sections, wm );
    
    if( ldata )
    {
	if( list_get_last_action( win, wm ) == LIST_ACTION_ESCAPE )
	{
	    //ESCAPE KEY:
	    prefs_close_handler( data, 0, wm );
	    return 0;
	}
	int sel = ldata->selected_item;
	if( (unsigned)sel < (unsigned)wm->prefs_sections )
	{
	    if( sel != data->cur_section )
	    {
		data->cur_section = sel;
		//Close old section:
		remove_window( data->cur_section_window, wm );
		//Open new section:
		data->cur_section_window = new_window( "Section", 0, 0, 1, 1, data->win->color, data->win, (int(*)(sundog_event*,window_manager*)) wm->prefs_section_handlers[ data->cur_section ], wm );
		set_window_controller( data->cur_section_window, 0, wm, (WCMD)wm->interelement_space * 2 + data->list_xsize, CEND );
		set_window_controller( data->cur_section_window, 1, wm, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->cur_section_window, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->cur_section_window, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		show_window( data->cur_section_window );
		bring_to_front( data->close, wm );
		//Resize prefs window:
		int new_ysize = wm->prefs_section_ysize + wm->interelement_space * 3 + wm->button_ysize;
		if( new_ysize < wm->large_window_ysize )
		    new_ysize = wm->large_window_ysize;
		resize_window_with_decorator( data->win, 0, new_ysize, wm );
		//Show it:
		recalc_regions( wm );
		draw_window( wm->root_win, wm );
	    }
	}
    }
    
    return 0;
}

int prefs_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    prefs_data* data = (prefs_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( prefs_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;
		
		wm->prefs_restart_request = false;

		data->list_xsize = 16;
		wm->prefs_sections = 0;
		while( 1 )
		{
		    const char* section_name = wm->prefs_section_names[ wm->prefs_sections ];
		    if( section_name == 0 ) break;
		    int x = font_string_x_size( section_name, win->font, wm ) + wm->interelement_space2;
		    if( x > data->list_xsize ) data->list_xsize = x;
		    wm->prefs_sections++;
		}
		
		wm->opt_list_without_scrollbar = true;
		data->sections = new_window( "Sections", 0, 0, 1, 1, wm->list_background, win, list_handler, wm );
		set_window_controller( data->sections, 0, wm, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->sections, 1, wm, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->sections, 2, wm, (WCMD)wm->interelement_space + data->list_xsize, CEND );
		set_window_controller( data->sections, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space * 2, CEND );
		set_handler( data->sections, prefs_sections_handler, data, wm );
		slist_data* l = list_get_data( data->sections, wm );
		for( int i = 0; i < wm->prefs_sections; i++ )
		{
		    const char* section_name = wm->prefs_section_names[ i ];
		    slist_add_item( section_name, 0, l );
		}
		l->selected_item = 0;

		data->cur_section = 0;
		data->cur_section_window = new_window( "Section", 0, 0, 1, 1, win->color, win, (int(*)(sundog_event*,window_manager*)) wm->prefs_section_handlers[ 0 ], wm );
		set_window_controller( data->cur_section_window, 0, wm, (WCMD)wm->interelement_space * 2 + data->list_xsize, CEND );
		set_window_controller( data->cur_section_window, 1, wm, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->cur_section_window, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->cur_section_window, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		
		int x = wm->interelement_space;
		const char* bname;
		int bxsize;
		
		bname = wm_get_string( STR_WM_CLOSE ); bxsize = button_get_optimal_xsize( bname, win->font, false, wm );
		data->close = new_window( bname, x, 0, bxsize, 1, wm->button_color, win, button_handler, wm );
		set_handler( data->close, prefs_close_handler, data, wm );
		set_window_controller( data->close, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->close, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space, CEND );
		
		//data->correct_ysize = win->ysize;
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    break;
	case EVT_CLOSEREQUEST:
            {
		prefs_close_handler( data, NULL, wm );
            }
            retval = 1;
            break;
	case EVT_BUTTONDOWN:
	    if( evt->key == KEY_ESCAPE )
	    {
		send_event( win, EVT_CLOSEREQUEST, wm );
		retval = 1;
	    }
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    retval = 1;
	    wm->prefs_win = 0;
	    break;
	    
	case EVT_SCREENRESIZE:
	    if( !win->visible ) break; //recalc_controllers() will not be performed, so we will get the wrong window parameters
	case EVT_BEFORESHOW:
	    //resize_window_with_decorator( win, 0, data->correct_ysize, wm );
	    break;
    }
    return retval;
}

//
//
//

struct prefs_ui_data
{
    WINDOWPTR win;
    WINDOWPTR window_pars;
    WINDOWPTR sysbars;
    WINDOWPTR maxfps;
    WINDOWPTR angle;
    WINDOWPTR control;
    WINDOWPTR zoom_btns;
    WINDOWPTR dclick;
    WINDOWPTR color;
    WINDOWPTR fonts;
    WINDOWPTR scale;
    WINDOWPTR virt_kbd;
    WINDOWPTR keymap;
    WINDOWPTR lang;
    WINDOWPTR hide_recent;
    WINDOWPTR show_hidden;
};

const int g_fps_vals[] = { 10,20,30,40,50,60,120,250,500,1000 };
const int g_fps_vals_num = 10;

void prefs_ui_reinit( WINDOWPTR win )
{
    prefs_ui_data* data = (prefs_ui_data*)win->data;
    window_manager* wm = data->win->wm;

    char ts[ 512 ];
    const char* v = "";

    if( data->sysbars )
    {
	if( sprofile_get_int_value( KEY_NOSYSBARS, 0, 0 ) != 0 )
	    data->sysbars->color = BUTTON_HIGHLIGHT_COLOR;
	else
	    data->sysbars->color = wm->button_color;
    }

    if( data->maxfps )
    {
	int m = -1;
	int fps = sprofile_get_int_value( "maxfps", -1, 0 );
	int fps2 = fps;
	if( fps <= 0 )
	{
	    //Auto:
	    fps2 = wm->max_fps;
	    m = 0;
	}
	else
	{
	    for( int i = 0; i < g_fps_vals_num; i++ )
		if( g_fps_vals[ i ] == fps )
		    { m = i + 1; break; }
	}
	sprintf( ts, "%s = %d", wm_get_string( STR_WM_MAXFPS ), fps2 );
	button_set_text( data->maxfps, ts );
	button_set_menu_val( data->maxfps, m );
    }

    if( data->angle )
    {
	int a = sprofile_get_int_value( KEY_ROTATE, 0, 0 );
	sprintf( ts, "%s = %d", wm_get_string( STR_WM_UI_ROTATION ), a );
	button_set_text( data->angle, ts );
	button_set_menu_val( data->angle, a / 90 );
    }

    if( data->control )
    {
	int m = 0;
	int tc = sprofile_get_int_value( KEY_TOUCHCONTROL, -1, 0 );
	int pc = sprofile_get_int_value( KEY_PENCONTROL, -1, 0 );
	if( tc == -1 && pc == -1 ) v = wm_get_string( STR_WM_AUTO );
	if( tc >= 0 ) { m = 1; v = wm_get_string( STR_WM_CTL_FINGERS ); }
	if( pc >= 0 ) { m = 2; v = wm_get_string( STR_WM_CTL_PEN ); }
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_CTL_TYPE ), v );
	button_set_text( data->control, ts );
	button_set_menu_val( data->control, m );
    }

    if( data->zoom_btns )
    {
	int m = 0;
	int vv = sprofile_get_int_value( KEY_SHOW_ZOOM_BUTTONS, -1, 0 );
	v = wm_get_string( STR_WM_AUTO );
	if( vv == 0 ) { m = 2; v = wm_get_string( STR_WM_NO_CAP ); }
	if( vv == 1 ) { m = 1; v = wm_get_string( STR_WM_YES_CAP ); }
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_SHOW_ZOOM_BTNS ), v );
	button_set_text( data->zoom_btns, ts );
	button_set_menu_val( data->zoom_btns, m );
    }
    
    if( data->dclick )
    {
	int m = -1;
	int t = sprofile_get_int_value( KEY_DOUBLECLICK, -1, 0 );
	if( t == -1 )
	{
	    m = 0;
	    sprintf( ts, "%s = %s (%d%s)", wm_get_string( STR_WM_DOUBLE_CLICK_TIME ), wm_get_string( STR_WM_AUTO ), wm->double_click_time, wm_get_string( STR_WM_MS ) );
	}
	else
	{
	    if( t >= 100 ) m = ( t - 100 ) / 50 + 1;
	    sprintf( ts, "%s = %d%s", wm_get_string( STR_WM_DOUBLE_CLICK_TIME ), t, wm_get_string( STR_WM_MS ) );
	}
	button_set_text( data->dclick, ts );
	button_set_menu_val( data->dclick, m );
    }
    
    if( data->virt_kbd )
    {
	int m = 0;
	int vk = -1;
	if( sprofile_get_int_value( KEY_SHOW_VIRT_KBD, -1, 0 ) != -1 ) vk = 1;
	if( sprofile_get_int_value( KEY_HIDE_VIRT_KBD, -1, 0 ) != -1 ) vk = 0;
	if( vk == -1 ) v = wm_get_string( STR_WM_AUTO );
	if( vk == 0 ) { m = 2; v = wm_get_string( STR_WM_OFF_CAP ); }
	if( vk == 1 ) { m = 1; v = wm_get_string( STR_WM_ON_CAP ); }
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_SHOW_KBD ), v );
	button_set_text( data->virt_kbd, ts );
	button_set_menu_val( data->virt_kbd, m );
    }
    
    if( data->lang )
    {
	int m = 0;
	char* lang = sprofile_get_str_value( KEY_LANG, 0, 0 );
	v = wm_get_string( STR_WM_AUTO );
	if( smem_strstr( lang, "en_" ) ) { m = 1; v = "English"; }
	if( smem_strstr( lang, "ru_" ) ) { m = 2; v = "Русский"; }
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_LANG ), v );
	button_set_text( data->lang, ts );
	button_set_menu_val( data->lang, m );
    }

    if( data->hide_recent )
    {
	if( sprofile_get_int_value( KEY_FDIALOG_NORECENT, -1, 0 ) != -1 )
    	    data->hide_recent->color = BUTTON_HIGHLIGHT_COLOR;
        else
	    data->hide_recent->color = wm->button_color;
    }

    if( data->show_hidden )
    {
	if( sprofile_get_int_value( KEY_FDIALOG_SHOWHIDDEN, -1, 0 ) != -1 )
    	    data->show_hidden->color = BUTTON_HIGHLIGHT_COLOR;
        else
	    data->show_hidden->color = wm->button_color;
    }
}

int prefs_ui_sysbars_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int val = sprofile_get_int_value( KEY_NOSYSBARS, 0, 0 );
    if( val == 0 )
	val = 1;
    else
	val = 0;
    if( val == 0 )
	sprofile_remove_key( KEY_NOSYSBARS, 0 );
    else
	sprofile_set_int_value( KEY_NOSYSBARS, val, 0 );
    sprofile_save( 0 );
    
    wm->prefs_restart_request = true;

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}    

int prefs_ui_fps_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;
    
    int new_fps = -1;
    int v = win->action_result;
    if( v == 0 ) new_fps = 0;
    if( v >= 1 && v < g_fps_vals_num + 1 ) new_fps = g_fps_vals[ v - 1 ];
    if( new_fps == 0 )
	sprofile_remove_key( "maxfps", 0 );
    if( new_fps > 0 )
	sprofile_set_int_value( "maxfps", new_fps, 0 );
    if( new_fps != -1 )
    {
	sprofile_save( 0 );
	wm->prefs_restart_request = true;
    }
    
    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

int prefs_ui_angle_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int prev_angle = sprofile_get_int_value( KEY_ROTATE, 0, 0 );    
    int angle = win->action_result * 90;
    if( angle >= 0 && angle <= 270 )
    {
	if( prev_angle != angle )
	{
	    if( angle )
		sprofile_set_int_value( KEY_ROTATE, angle, 0 );
	    else
		sprofile_remove_key( KEY_ROTATE, 0 );
	    sprofile_save( 0 );
	    wm->prefs_restart_request = true;
	}
    }
    
    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

int prefs_ui_control_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int v = win->action_result;
    switch( v )
    {
	case 0:
	    //Auto:
	    sprofile_remove_key( KEY_TOUCHCONTROL, 0 );
	    sprofile_remove_key( KEY_PENCONTROL, 0 );
	    break;
	case 1:
	    //Fingers:
	    sprofile_set_int_value( KEY_TOUCHCONTROL, 1, 0 );
	    sprofile_remove_key( KEY_PENCONTROL, 0 );
	    break;
	case 2:
	    //Pen/Mouse:
	    sprofile_set_int_value( KEY_PENCONTROL, 1, 0 );
	    sprofile_remove_key( KEY_TOUCHCONTROL, 0 );
	    break;
	default:
	    return 0;
	    break;
    }
    sprofile_save( 0 );
    wm->prefs_restart_request = true;

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

int prefs_ui_zoom_btns_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int v = win->action_result;
    switch( v )
    {
	case 0:
	    //Auto:
	    sprofile_remove_key( KEY_SHOW_ZOOM_BUTTONS, 0 );
	    break;
	case 1:
	    //Yes:
	    sprofile_set_int_value( KEY_SHOW_ZOOM_BUTTONS, 1, 0 );
	    break;
	case 2:
	    //No:
	    sprofile_set_int_value( KEY_SHOW_ZOOM_BUTTONS, 0, 0 );
	    break;
	default:
	    return 0;
	    break;
    }
    sprofile_save( 0 );
    wm->prefs_restart_request = true;

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

int prefs_ui_dclick_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int v = win->action_result;
    if( v == 0 )
    {
	//Auto:
	sprofile_remove_key( KEY_DOUBLECLICK, 0 );
	wm->double_click_time = DEFAULT_DOUBLE_CLICK_TIME;
    }
    else
    {
	if( v >= 1 )
	{
	    v++;
	    sprofile_set_int_value( KEY_DOUBLECLICK, v * 50, 0 );
	    wm->double_click_time = v * 50;
	}
    }
    sprofile_save( 0 );

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

int prefs_ui_color_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    colortheme_open( wm );
    return 0;
}

int prefs_ui_fonts_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    char* fonts_menu = (char*)smem_new( 128 );
    fonts_menu[ 0 ] = 0;
    smem_strcat_resize( fonts_menu, wm_get_string( STR_WM_AUTO ) );
    for( int i = 0; i < g_fonts_count; i++ )
    {
	smem_strcat_resize( fonts_menu, "\n" );
	smem_strcat_resize( fonts_menu, g_font_names[ i ] );
    }

    int font_big = sprofile_get_int_value( KEY_FONT_BIG, DEFAULT_FONT_BIG, 0 );
    int font_med_mono = sprofile_get_int_value( KEY_FONT_MEDIUM_MONO, DEFAULT_FONT_MEDIUM_MONO, 0 );
    int font_small = sprofile_get_int_value( KEY_FONT_SMALL, DEFAULT_FONT_SMALL, 0 );

    dialog_item di[ 4 ];
    smem_clear( &di, sizeof( di ) );
    di[ 0 ].type = DIALOG_ITEM_POPUP;
    di[ 0 ].str_val = (char*)wm_get_string( STR_WM_FONT_BIG );
    di[ 0 ].int_val = font_big + 1;
    di[ 0 ].menu = fonts_menu;
    di[ 1 ].type = DIALOG_ITEM_POPUP;
    di[ 1 ].str_val = (char*)wm_get_string( STR_WM_FONT_MEDIUM_MONO );
    di[ 1 ].int_val = font_med_mono + 1;
    di[ 1 ].menu = fonts_menu;
    di[ 2 ].type = DIALOG_ITEM_POPUP;
    di[ 2 ].str_val = (char*)wm_get_string( STR_WM_FONT_SMALL );
    di[ 2 ].int_val = font_small + 1;
    di[ 2 ].menu = fonts_menu;
    di[ 3 ].type = DIALOG_ITEM_NONE;
    wm->opt_dialog_items = di;
    int d = dialog( wm_get_string( STR_WM_FONTS ), 0, wm_get_string( STR_WM_OKCANCEL ), wm );
    if( d == 0 )
    {
	int font_big2 = di[ 0 ].int_val - 1;
	int font_med_mono2 = di[ 1 ].int_val - 1;
	int font_small2 = di[ 2 ].int_val - 1;
	bool changed = false;
	if( font_med_mono != font_med_mono2 )
	{
	    if( font_med_mono2 == DEFAULT_FONT_MEDIUM_MONO || font_med_mono2 < 0 )
		sprofile_remove_key( KEY_FONT_MEDIUM_MONO, 0 );
	    else
		sprofile_set_int_value( KEY_FONT_MEDIUM_MONO, font_med_mono2, 0 );
	    changed = true;
	}
	if( font_big != font_big2 )
	{
	    if( font_big2 == DEFAULT_FONT_BIG || font_big2 < 0 )
	        sprofile_remove_key( KEY_FONT_BIG, 0 );
	    else
		sprofile_set_int_value( KEY_FONT_BIG, font_big2, 0 );
	    changed = true;
	}
	if( font_small != font_small2 )
	{
	    if( font_small2 == DEFAULT_FONT_SMALL || font_small2 < 0 )
		sprofile_remove_key( KEY_FONT_SMALL, 0 );
	    else
		sprofile_set_int_value( KEY_FONT_SMALL, font_small2, 0 );
	    changed = true;
	}
	if( changed )
	{
    	    wm->prefs_restart_request = true;
    	    sprofile_save( 0 );
    	}
    }

    smem_free( fonts_menu );
    return 0;
}

int prefs_ui_scale_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    ui_scale_open( wm );
    return 0;
}

int prefs_ui_virt_kbd_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int v = win->action_result;
    switch( v )
    {
	case 0:
	    //Auto:
	    sprofile_remove_key( KEY_SHOW_VIRT_KBD, 0 );
	    sprofile_remove_key( KEY_HIDE_VIRT_KBD, 0 );
	    break;
	case 1:
	    //Yes:
	    sprofile_set_int_value( KEY_SHOW_VIRT_KBD, 1, 0 );
	    sprofile_remove_key( KEY_HIDE_VIRT_KBD, 0 );
	    break;
	case 2:
	    //No:
	    sprofile_set_int_value( KEY_HIDE_VIRT_KBD, 1, 0 );
	    sprofile_remove_key( KEY_SHOW_VIRT_KBD, 0 );
	    break;
	default:
	    return 0;
	    break;
    }
    sprofile_save( 0 );
    wm->prefs_restart_request = true;

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

int prefs_ui_keymap_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    keymap_open( wm );
    return 0;
}

int prefs_ui_lang_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int v = win->action_result;
    switch( v )
    {
	case 0:
	    //Auto:
	    sprofile_remove_key( KEY_LANG, 0 );
	    break;
	case 1:
	    //English:
	    sprofile_set_str_value( KEY_LANG, "en_US", 0 );
	    break;
	case 2:
	    //Russian:
	    sprofile_set_str_value( KEY_LANG, "ru_RU", 0 );
	    break;
	default:
	    return 0;
	    break;
    }
    sprofile_save( 0 );
    wm->prefs_restart_request = true;

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

int prefs_ui_hide_recent_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    if( sprofile_get_int_value( KEY_FDIALOG_NORECENT, -1, 0 ) != -1 )
        sprofile_remove_key( KEY_FDIALOG_NORECENT, 0 );
    else
        sprofile_set_int_value( KEY_FDIALOG_NORECENT, 1, 0 );
    sprofile_save( 0 );

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

int prefs_ui_show_hidden_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    if( sprofile_get_int_value( KEY_FDIALOG_SHOWHIDDEN, -1, 0 ) != -1 )
        sprofile_remove_key( KEY_FDIALOG_SHOWHIDDEN, 0 );
    else
        sprofile_set_int_value( KEY_FDIALOG_SHOWHIDDEN, 1, 0 );
    sprofile_save( 0 );

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

int prefs_ui_window_pars_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    int screen_xsize = sprofile_get_int_value( KEY_SCREENX, wm->real_window_width, 0 );
    int screen_ysize = sprofile_get_int_value( KEY_SCREENY, wm->real_window_height, 0 );
    int fullscreen = 0;
    if( sprofile_get_int_value( KEY_FULLSCREEN, -1, 0 ) != -1 ) fullscreen = 1;
    dialog_item di[ 6 ];
    smem_clear( &di, sizeof( di ) );
    di[ 0 ].type = DIALOG_ITEM_LABEL;
    di[ 0 ].str_val = (char*)wm_get_string( STR_WM_WINDOW_WIDTH );
    di[ 1 ].type = DIALOG_ITEM_NUMBER;
    di[ 1 ].min = 0;
    di[ 1 ].max = 8000;
    di[ 1 ].int_val = screen_xsize;
    di[ 2 ].type = DIALOG_ITEM_LABEL;
    di[ 2 ].str_val = (char*)wm_get_string( STR_WM_WINDOW_HEIGHT );
    di[ 3 ].type = DIALOG_ITEM_NUMBER;
    di[ 3 ].min = 0;
    di[ 3 ].max = 8000;
    di[ 3 ].int_val = screen_ysize;
    di[ 4 ].type = DIALOG_ITEM_POPUP;
    di[ 4 ].str_val = (char*)wm_get_string( STR_WM_WINDOW_FULLSCREEN );
    di[ 4 ].int_val = fullscreen;
    di[ 4 ].menu = wm_get_string( STR_WM_OFF_ON_MENU );
    di[ 5 ].type = DIALOG_ITEM_NONE;
    wm->opt_dialog_items = di;
    int d = dialog( wm_get_string( STR_WM_WINDOW_PARS ), 0, wm_get_string( STR_WM_OKCANCEL ), wm );
    if( d == 0 )
    {
        bool changed = false;
    	if( di[ 1 ].int_val != screen_xsize || di[ 3 ].int_val != screen_ysize )
    	{
    	    sprofile_set_int_value( KEY_SCREENX, di[ 1 ].int_val, 0 );
    	    sprofile_set_int_value( KEY_SCREENY, di[ 3 ].int_val, 0 );
    	    changed = true;
    	}
    	if( di[ 4 ].int_val != fullscreen )
    	{
    	    if( di[ 4 ].int_val )
    		sprofile_set_int_value( KEY_FULLSCREEN, 1, 0 );
    	    else
    		sprofile_remove_key( KEY_FULLSCREEN, 0 );
    	    changed = true;
    	}
        if( changed )
        {
    	    wm->prefs_restart_request = true;
    	    sprofile_save( 0 );
    	}
    }
    return 0;
}

int prefs_ui_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    prefs_ui_data* data = (prefs_ui_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( prefs_ui_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;
		
		char ts[ 256 ];
		int y = 0;

		data->window_pars = 0;
		data->sysbars = 0;
#ifdef OS_ANDROID
		if( g_android_version_nums[ 0 ] >= 4 )
		{
		    wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		    data->sysbars = new_window( wm_get_string( STR_WM_HIDE_SYSTEM_BARS ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		    set_handler( data->sysbars, prefs_ui_sysbars_handler, data, wm );
		    set_window_controller( data->sysbars, 0, wm, (WCMD)0, CEND );
		    set_window_controller( data->sysbars, 2, wm, CPERC, (WCMD)50, CSUB, (WCMD)wm->interelement_space2, CEND );
		}
#else
    #if !defined(OS_APPLE) && !defined(OS_ANDROID) && !defined(OS_WINCE)
		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->window_pars = new_window( wm_get_string( STR_WM_WINDOW_PARS ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->window_pars, prefs_ui_window_pars_handler, data, wm );
		set_window_controller( data->window_pars, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->window_pars, 2, wm, CPERC, (WCMD)50, CSUB, (WCMD)wm->interelement_space2, CEND );
    #endif
#endif

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->scale = new_window( wm_get_string( STR_WM_UI_SCALE ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->scale, prefs_ui_scale_handler, data, wm );
		if( data->window_pars || data->sysbars )
		    set_window_controller( data->scale, 0, wm, CPERC, (WCMD)50, CEND );
		else
		    set_window_controller( data->scale, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->scale, 2, wm, CPERC, (WCMD)100, CEND );

		y += wm->text_ysize + wm->interelement_space;

		data->angle = 0;
#ifdef SCREEN_ROTATE_SUPPORTED
		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->angle = new_window( wm_get_string( STR_WM_UI_ROTATION ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->angle, prefs_ui_angle_handler, data, wm );
		button_set_menu( data->angle, "0\n90\n180\n270" );
		set_window_controller( data->angle, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->angle, 2, wm, CPERC, (WCMD)50, CSUB, (WCMD)wm->interelement_space2, CEND );
#endif

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->maxfps = new_window( wm_get_string( STR_WM_MAXFPS ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->maxfps, prefs_ui_fps_handler, data, wm );
		ts[ 0 ] = 0;
		smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_AUTO ) );
		for( int i = 0; i < g_fps_vals_num; i++ )
		{
		    char ts2[ 32 ];
		    ts2[ 0 ] = 0xA;
		    int_to_string( g_fps_vals[ i ], ts2 + 1 );
		    smem_strcat( ts, sizeof( ts ), ts2 );
		}
		button_set_menu( data->maxfps, ts );
		if( data->angle )
		    set_window_controller( data->maxfps, 0, wm, CPERC, (WCMD)50, CEND );
		else
		    set_window_controller( data->maxfps, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->maxfps, 2, wm, CPERC, (WCMD)100, CEND );

		y += wm->text_ysize + wm->interelement_space;

		data->color = 0;
		if( ( wm->prefs_flags & PREFS_FLAG_NO_COLOR_THEME ) == 0 )
		{
		    wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		    data->color = new_window( wm_get_string( STR_WM_COLOR_THEME ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		    set_handler( data->color, prefs_ui_color_handler, data, wm );
		    set_window_controller( data->color, 0, wm, (WCMD)0, CEND );
		    set_window_controller( data->color, 2, wm, CPERC, (WCMD)100, CEND );
		}

		data->fonts = 0;
		if( ( wm->prefs_flags & PREFS_FLAG_NO_FONTS ) == 0 )
		{
		    wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		    data->fonts = new_window( wm_get_string( STR_WM_FONTS ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		    set_handler( data->fonts, prefs_ui_fonts_handler, data, wm );
		    if( data->color )
		    {
			set_window_controller( data->color, 2, wm, CPERC, (WCMD)50, CSUB, (WCMD)wm->interelement_space2, CEND );
			set_window_controller( data->fonts, 0, wm, CPERC, (WCMD)50, CEND );
		    }
		    else
		    {
			set_window_controller( data->fonts, 0, wm, (WCMD)0, CEND );
		    }
		    set_window_controller( data->fonts, 2, wm, CPERC, (WCMD)100, CEND );
		}

		if( data->color || data->fonts )
		    y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->control = new_window( wm_get_string( STR_WM_CTL_TYPE ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->control, prefs_ui_control_handler, data, wm );
		button_set_menu( data->control, wm_get_string( STR_WM_CTL_TYPE_MENU ) );
		set_window_controller( data->control, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->control, 2, wm, CPERC, (WCMD)50, CSUB, (WCMD)wm->interelement_space2, CEND );

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->zoom_btns = new_window( wm_get_string( STR_WM_SHOW_ZOOM_BUTTONS ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->zoom_btns, prefs_ui_zoom_btns_handler, data, wm );
		set_window_controller( data->zoom_btns, 0, wm, CPERC, (WCMD)50, CEND );
		set_window_controller( data->zoom_btns, 2, wm, CPERC, (WCMD)100, CEND );
		button_set_menu( data->zoom_btns, wm_get_string( STR_WM_AUTO_YES_NO_MENU ) );

		if( wm->prefs_flags & PREFS_FLAG_NO_CONTROL_TYPE )
		{
		    data->control->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
		    data->zoom_btns->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
		}
		else
		    y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->dclick = new_window( wm_get_string( STR_WM_DOUBLE_CLICK_TIME ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->dclick, prefs_ui_dclick_handler, data, wm );
		ts[ 0 ] = 0;
		smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_AUTO ) );
		smem_strcat( ts, sizeof( ts ), "\n100\n150\n200\n250\n300\n350\n400\n450\n500" );
		button_set_menu( data->dclick, ts );
		set_window_controller( data->dclick, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->dclick, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->virt_kbd = new_window( wm_get_string( STR_WM_SHOW_KBD ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->virt_kbd, prefs_ui_virt_kbd_handler, data, wm );
		button_set_menu( data->virt_kbd, wm_get_string( STR_WM_AUTO_ON_OFF_MENU ) );
		set_window_controller( data->virt_kbd, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->virt_kbd, 2, wm, CPERC, (WCMD)50, CSUB, (WCMD)wm->interelement_space2, CEND );

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->keymap = new_window( wm_get_string( STR_WM_SHORTCUTS_SHORT ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->keymap, prefs_ui_keymap_handler, data, wm );
		set_window_controller( data->keymap, 0, wm, CPERC, (WCMD)50, CEND );
		set_window_controller( data->keymap, 2, wm, CPERC, (WCMD)100, CEND );
		if( wm->prefs_flags & PREFS_FLAG_NO_KEYMAP )
		{
		    data->keymap->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
		    set_window_controller( data->virt_kbd, 2, wm, CPERC, (WCMD)100, CEND );
		}
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->lang = new_window( wm_get_string( STR_WM_LANG ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->lang, prefs_ui_lang_handler, data, wm );
		ts[ 0 ] = 0;
		smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_AUTO ) );
		smem_strcat( ts, sizeof( ts ), "\nEnglish\nРусский" );
		button_set_menu( data->lang, ts );
		set_window_controller( data->lang, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->lang, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->hide_recent = new_window( wm_get_string( STR_WM_HIDE_RECENT_FILES ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->hide_recent, prefs_ui_hide_recent_handler, data, wm );
		set_window_controller( data->hide_recent, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->hide_recent, 2, wm, CPERC, (WCMD)50, CSUB, (WCMD)wm->interelement_space2, CEND );

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->show_hidden = new_window( wm_get_string( STR_WM_SHOW_HIDDEN_FILES ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->show_hidden, prefs_ui_show_hidden_handler, data, wm );
		set_window_controller( data->show_hidden, 0, wm, CPERC, (WCMD)50, CEND );
		set_window_controller( data->show_hidden, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		prefs_ui_reinit( win );

		wm->prefs_section_ysize = y;
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    retval = 1;
	    break;
    }
    return retval;
}

//
//
//

struct prefs_svideo_data
{
    WINDOWPTR win;
    WINDOWPTR cam;
    WINDOWPTR cam_rotate;
};

void prefs_svideo_reinit( WINDOWPTR win )
{
    prefs_svideo_data* data = (prefs_svideo_data*)win->data;
    window_manager* wm = data->win->wm;

    char ts[ 512 ];

    int cam = sprofile_get_int_value( KEY_CAMERA, -1111, 0 );
    if( cam == -1111 )
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_CAMERA ), wm_get_string( STR_WM_AUTO ) );
    else
    {
#if defined(OS_IOS) || defined(OS_ANDROID)
	if( cam >= 0 && cam <= 1 )
	{
	    const char* n;
	    if( cam == 0 ) n = wm_get_string( STR_WM_BACK_CAM );
	    if( cam == 1 ) n = wm_get_string( STR_WM_FRONT_CAM );
	    sprintf( ts, "%s = %d (%s)", wm_get_string( STR_WM_CAMERA ), cam, n );
	}
	else
#endif
	{
	    sprintf( ts, "%s = %d", wm_get_string( STR_WM_CAMERA ), cam );
	}
    }
    button_set_text( data->cam, ts );

    if( data->cam_rotate )
    {
	sprintf( ts, "%s = %d", wm_get_string( STR_WM_CAM_ROTATION ), sprofile_get_int_value( KEY_CAM_ROTATE, 0, 0 ) );
	button_set_text( data->cam_rotate, ts );
    }
}

int prefs_svideo_cam_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_svideo_data* data = (prefs_svideo_data*)user_data;

    int cam = win->action_result;
    if( cam < 0 || cam > 10 ) return 0;
    
    if( cam == 0 )
    {
	if( sprofile_get_int_value( KEY_CAMERA, -1, 0 ) != -1 )
	{
	    sprofile_remove_key( KEY_CAMERA, 0 );
	    wm->prefs_restart_request = true;
	    sprofile_save( 0 );
	}
    }
    if( cam > 0 )
    {
	cam--;
	if( sprofile_get_int_value( KEY_CAMERA, -1, 0 ) != cam )
	{
	    sprofile_set_int_value( KEY_CAMERA, cam, 0 );
	    wm->prefs_restart_request = true;
	    sprofile_save( 0 );
	}
    }

    prefs_svideo_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

int prefs_svideo_cam_rotate_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_svideo_data* data = (prefs_svideo_data*)user_data;

    int prev_angle = sprofile_get_int_value( KEY_CAM_ROTATE, 0, 0 );
    int angle = win->action_result * 90;
    if( angle >= 0 && angle <= 270 )
    {
	if( prev_angle != angle )
	{
	    if( angle )
		sprofile_set_int_value( KEY_CAM_ROTATE, angle, 0 );
	    else
		sprofile_remove_key( KEY_CAM_ROTATE, 0 );
	    sprofile_save( 0 );
	    wm->prefs_restart_request = true;
	}
    }
    
    prefs_svideo_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

int prefs_svideo_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    prefs_svideo_data* data = (prefs_svideo_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( prefs_svideo_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;
		
		char ts[ 512 ];
		int y = 0;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->cam = new_window( wm_get_string( STR_WM_CAMERA ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->cam, prefs_svideo_cam_handler, data, wm );
#if defined(OS_IOS) || defined(OS_ANDROID)
		sprintf( ts, "%s\n0 (%s)\n1 (%s)\n2\n3\n4\n5\n6\n7", wm_get_string( STR_WM_AUTO ), wm_get_string( STR_WM_BACK_CAM ), wm_get_string( STR_WM_FRONT_CAM ) );
#else
		sprintf( ts, "%s\n0\n1\n2\n3\n4\n5\n6\n7", wm_get_string( STR_WM_AUTO ) );
#endif
		button_set_menu( data->cam, ts );
		set_window_controller( data->cam, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->cam, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->cam_rotate = new_window( wm_get_string( STR_WM_CAM_ROTATION ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->cam_rotate, prefs_svideo_cam_rotate_handler, data, wm );
		button_set_menu( data->cam_rotate, "0\n90\n180\n270" );
		set_window_controller( data->cam_rotate, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->cam_rotate, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space2, CEND );
		y += wm->text_ysize + wm->interelement_space;

		prefs_svideo_reinit( win );

		wm->prefs_section_ysize = y;
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    retval = 1;
	    break;
    }
    return retval;
}

//
//
//

struct prefs_audio_data
{
    WINDOWPTR win;
    WINDOWPTR buf;
    WINDOWPTR freq;
    WINDOWPTR driver;
    WINDOWPTR device;
    WINDOWPTR device_in;
    WINDOWPTR opt;
};

const char* prefs_audio_get_driver( window_manager* wm ) //with the driver name instead of AUTO
{
    sundog_sound* snd = wm->sd->ss;

    const char* drv = sprofile_get_str_value( KEY_AUDIODRIVER, 0, 0 );
    if( drv == 0 )
    {
	//Auto:
        if( snd && snd->initialized )
        {
    	    //Already selected by app:
    	    drv = sundog_sound_get_driver_name( snd );
	}
	else
	{
	    //Global default:
	    drv = sundog_sound_get_default_driver();
	}
    }
    
    return drv;
}

void prefs_audio_reinit( WINDOWPTR win )
{
    prefs_audio_data* data = (prefs_audio_data*)win->data;
    window_manager* wm = data->win->wm;
    sundog_sound* snd = wm->sd->ss;
    
    char ts[ 512 ];

    const char* cur_drv = prefs_audio_get_driver( wm );

    char* drv = sprofile_get_str_value( KEY_AUDIODRIVER, 0, 0 );
    if( smem_strstr( drv, "asio" ) )
    {
	rename_window( data->opt, wm_get_string( STR_WM_ADD_OPTIONS_ASIO ) );
	if( show_window2( data->opt ) )
	    recalc_regions( wm );
    }
    else
    {
	if( hide_window2( data->opt ) )
	    recalc_regions( wm );
    }
    if( drv == 0 )
    {
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_DRIVER ), wm_get_string( STR_WM_AUTO ) );
    }
    else
    {
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_DRIVER ), drv );
	char** names = 0;
	char** infos = 0;
	int drivers = sundog_sound_get_drivers( &names, &infos );
	if( ( drivers > 0 ) && names && infos )
	{
	    for( int d = 0; d < drivers; d++ )
	    {
		if( smem_strcmp( names[ d ], drv ) == 0 )
		{
		    sprintf( ts, "%s = %s", wm_get_string( STR_WM_DRIVER ), infos[ d ] );
		    break;
		}
	    }
	    for( int d = 0; d < drivers; d++ )
	    {
		smem_free( names[ d ] );
		smem_free( infos[ d ] );
	    }
	    smem_free( names );
	    smem_free( infos );
	}
    }
    button_set_text( data->driver, ts );

    char* dev = sprofile_get_str_value( KEY_AUDIODEVICE, 0, 0 );
    if( dev == 0 )
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_OUTPUT ), wm_get_string( STR_WM_AUTO ) );
    else
    {
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_OUTPUT ), dev );
	char** names = 0;
	char** infos = 0;
	int devices = sundog_sound_get_devices( cur_drv, &names, &infos, 0 );
	if( ( devices > 0 ) && names && infos )
	{
	    for( int d = 0; d < devices; d++ )
	    {
		if( smem_strcmp( names[ d ], dev ) == 0 )
		{
		    ts[ 0 ] = 0;
		    smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_OUTPUT ) );
		    smem_strcat( ts, sizeof( ts ), " = " );
		    smem_strcat( ts, sizeof( ts ), infos[ d ] );
		    break;
		}
	    }
	    for( int d = 0; d < devices; d++ )
	    {
		smem_free( names[ d ] );
		smem_free( infos[ d ] );
	    }
	    smem_free( names );
	    smem_free( infos );
	}
    }
    button_set_text( data->device, ts );

    dev = sprofile_get_str_value( KEY_AUDIODEVICE_IN, 0, 0 );
    if( dev == 0 )
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_INPUT ), wm_get_string( STR_WM_AUTO ) );
    else
    {
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_INPUT ), dev );
	char** names = 0;
	char** infos = 0;
	int devices = sundog_sound_get_devices( cur_drv, &names, &infos, 1 );
	if( ( devices > 0 ) && names && infos )
	{
	    for( int d = 0; d < devices; d++ )
	    {
		if( smem_strcmp( names[ d ], dev ) == 0 )
		{
		    ts[ 0 ] = 0;
		    smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_INPUT ) );
		    smem_strcat( ts, sizeof( ts ), " = " );
		    smem_strcat( ts, sizeof( ts ), infos[ d ] );
		    break;
		}
	    }
	    for( int d = 0; d < devices; d++ )
	    {
		smem_free( names[ d ] );
		smem_free( infos[ d ] );
	    }
	    smem_free( names );
	    smem_free( infos );
	}
    }
    button_set_text( data->device_in, ts );
    
    int freq = 44100;
    if( snd && snd->initialized )
        freq = snd->freq;
    int size = sprofile_get_int_value( KEY_SOUNDBUFFER, 0, 0 );
    if( size == 0 )
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_BUFFER ), wm_get_string( STR_WM_AUTO ) );
    else
	sprintf( ts, "%s = %d; %d %s", wm_get_string( STR_WM_BUFFER ), size, ( size * 1000 ) / freq, wm_get_string( STR_WM_MS ) );
    button_set_text( data->buf, ts );

    freq = sprofile_get_int_value( KEY_FREQ, 0, 0 );
    if( freq == 0 )
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_SAMPLE_RATE ), wm_get_string( STR_WM_AUTO ) );
    else
	sprintf( ts, "%s = %d %s", wm_get_string( STR_WM_SAMPLE_RATE ), freq, wm_get_string( STR_WM_HZ ) );
    button_set_text( data->freq, ts );
}

int prefs_audio_driver_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_audio_data* data = (prefs_audio_data*)user_data;
    
    size_t menu_size = 8192;
    char* menu = (char*)smem_new( menu_size );
    menu[ 0 ] = 0;
    
    smem_strcat( menu, menu_size, wm_get_string( STR_WM_AUTO ) );
    
    char** names = 0;
    char** infos = 0;
    int drivers = sundog_sound_get_drivers( &names, &infos );
    if( ( drivers > 0 ) && names && infos )
    {
	smem_strcat( menu, menu_size, "\n" );
	
	for( int d = 0; d < drivers; d++ )
	{
	    smem_strcat( menu, menu_size, infos[ d ] );
	    if( d != drivers - 1 )
		smem_strcat( menu, menu_size, "\n" );
	}

	int sel = popup_menu( wm_get_string( STR_WM_DRIVER ), menu, win->screen_x, win->screen_y, wm->menu_color, wm );
	if( (unsigned)sel < (unsigned)drivers + 1 )
	{
	    if( sel == 0 )
	    {
		//Auto:
		if( sprofile_get_str_value( KEY_AUDIODRIVER, 0, 0 ) != 0 )
		{
		    sprofile_remove_key( KEY_AUDIODRIVER, 0 );
		    sprofile_remove_key( KEY_AUDIODEVICE, 0 );
		    sprofile_remove_key( KEY_AUDIODEVICE_IN, 0 );
            	    wm->prefs_restart_request = true;
        	    sprofile_save( 0 );
		}
	    }
	    else
	    {
		if( smem_strcmp( sprofile_get_str_value( KEY_AUDIODRIVER, "", 0 ), names[ sel - 1 ] ) )
		{
		    sprofile_set_str_value( KEY_AUDIODRIVER, names[ sel - 1 ], 0 );
		    sprofile_remove_key( KEY_AUDIODEVICE, 0 );
		    sprofile_remove_key( KEY_AUDIODEVICE_IN, 0 );
		    wm->prefs_restart_request = true;
            	    sprofile_save( 0 );
		}
	    }
	}

	for( int d = 0; d < drivers; d++ )
	{
	    smem_free( names[ d ] );
	    smem_free( infos[ d ] );
	}
	smem_free( names );
	smem_free( infos );
    }
    
    smem_free( menu );    

    prefs_audio_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

int prefs_audio_device_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_audio_data* data = (prefs_audio_data*)user_data;
    sundog_sound* snd = wm->sd->ss;
    
    bool input = 0;
    const char* key = KEY_AUDIODEVICE;
    if( win == data->device_in )
    {
	input = 1;
	key = KEY_AUDIODEVICE_IN;
    }

    const char* cur_drv = prefs_audio_get_driver( wm );
    
    size_t menu_size = 8192;
    char* menu = (char*)smem_new( menu_size );
    menu[ 0 ] = 0;
    
    smem_strcat( menu, menu_size, wm_get_string( STR_WM_AUTO ) );
    
    char** names = 0;
    char** infos = 0;
    int devices = sundog_sound_get_devices( cur_drv, &names, &infos, input );
    if( ( devices > 0 ) && names && infos )
    {
	smem_strcat( menu, menu_size, "\n" );
	
	for( int d = 0; d < devices; d++ )
	{
	    smem_strcat( menu, menu_size, infos[ d ] );
	    if( d != devices - 1 )
		smem_strcat( menu, menu_size, "\n" );
	}

	int sel;
	if( input )
	    sel = popup_menu( wm_get_string( STR_WM_INPUT_DEVICE ), menu, win->screen_x, win->screen_y, wm->menu_color, wm );
	else
	    sel = popup_menu( wm_get_string( STR_WM_DEVICE ), menu, win->screen_x, win->screen_y, wm->menu_color, wm );
	if( (unsigned)sel < (unsigned)devices + 1 )
	{
	    if( sel == 0 )
	    {
		//Auto:
		if( sprofile_get_str_value( key, 0, 0 ) != 0 )
		{
		    sprofile_remove_key( key, 0 );
            	    wm->prefs_restart_request = true;
        	    sprofile_save( 0 );
		}
	    }
	    else
	    {
		if( smem_strcmp( sprofile_get_str_value( key, "", 0 ), names[ sel - 1 ] ) )
		{
		    sprofile_set_str_value( key, names[ sel - 1 ], 0 );
		    wm->prefs_restart_request = true;
            	    sprofile_save( 0 );
		}
	    }
	}

	for( int d = 0; d < devices; d++ )
	{
	    smem_free( names[ d ] );
	    smem_free( infos[ d ] );
	}
	smem_free( names );
	smem_free( infos );
    }
    
    smem_free( menu );    

    prefs_audio_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

static const int g_sound_buf_size_table[] = 
{
    128,
    256, 
    512,
    768,
    1024,
    1280,
    1536,
    1792,
    2048,
    2560,
    3072,
    4096,
    -1
};

int prefs_audio_buf_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_audio_data* data = (prefs_audio_data*)user_data;
    sundog_sound* snd = wm->sd->ss;
    
    size_t menu_size = 8192;
    char* menu = (char*)smem_new( menu_size );
    menu[ 0 ] = 0;
    smem_strcat( menu, menu_size, wm_get_string( STR_WM_AUTO ) );
    
    int i = 0;
    int size = 0;

#if defined(OS_MACOS)
#else
    int freq = 44100;
    if( snd && snd->initialized )
        freq = snd->freq;
    char ts[ 512 ];
    while( 1 )
    {
	size = g_sound_buf_size_table[ i ];
	if( size == -1 ) break;
	sprintf( ts, "\n%d; %d %s", size, ( size * 1000 ) / freq, wm_get_string( STR_WM_MS ) ); 
	smem_strcat( menu, menu_size, ts );
	i++;
    }
#endif
    
    size = 0;
    int v = popup_menu( wm_get_string( STR_WM_BUFFER_SIZE ), menu, win->screen_x, win->screen_y, wm->menu_color, wm );
    if( v > 0 && v < i + 1 )
	size = g_sound_buf_size_table[ v - 1 ];
    
    smem_free( menu );
    
    if( v == 0 || size > 0 )
    {
	if( v == 0 )
	{
	    if( sprofile_get_int_value( KEY_SOUNDBUFFER, -1, 0 ) != -1 )
	    {
		sprofile_remove_key( KEY_SOUNDBUFFER, 0 );
		wm->prefs_restart_request = true;
		sprofile_save( 0 );
	    }
	}
	if( size > 0 )
	{
	    if( sprofile_get_int_value( KEY_SOUNDBUFFER, -1, 0 ) != size )
	    {
		sprofile_set_int_value( KEY_SOUNDBUFFER, size, 0 );
		wm->prefs_restart_request = true;
		sprofile_save( 0 );
	    }
	}
    }

    prefs_audio_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

int prefs_audio_freq_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_audio_data* data = (prefs_audio_data*)user_data;
    
    int freq = 0;
    int v = win->action_result;
    switch( v )
    {
	case 1: freq = 44100; break;
	case 2: freq = 48000; break;
	case 3: freq = 88200; break;
	case 4: freq = 96000; break;
	case 5: freq = 192000; break;
    }
    
    if( v == 0 || freq > 0 )
    {
	if( v == 0 )
	{
	    if( sprofile_get_int_value( KEY_FREQ, -1, 0 ) != -1 )
	    {
		sprofile_remove_key( KEY_FREQ, 0 );
		wm->prefs_restart_request = true;
		sprofile_save( 0 );
	    }
	}
	if( freq > 0 )
	{
	    if( sprofile_get_int_value( KEY_FREQ, -1, 0 ) != freq )
	    {
		sprofile_set_int_value( KEY_FREQ, freq, 0 );
		wm->prefs_restart_request = true;
		sprofile_save( 0 );
	    }
	}
    }

    prefs_audio_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

int prefs_audio_opt_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_audio_data* data = (prefs_audio_data*)user_data;

    char* drv = sprofile_get_str_value( KEY_AUDIODRIVER, 0, 0 );
    if( smem_strstr( drv, "asio" ) )
    {
	dialog_item di[ 5 ];
	smem_clear( &di, sizeof( di ) );
        di[ 0 ].type = DIALOG_ITEM_LABEL;
        di[ 0 ].str_val = (char*)wm_get_string( STR_WM_FIRST_OUT_CH );
        di[ 1 ].type = DIALOG_ITEM_NUMBER;
        di[ 1 ].min = 0;
        di[ 1 ].max = 64;
        di[ 1 ].int_val = sprofile_get_int_value( "audio_ch", 0, 0 );
        di[ 2 ].type = DIALOG_ITEM_LABEL;
        di[ 2 ].str_val = (char*)wm_get_string( STR_WM_FIRST_IN_CH );
        di[ 3 ].type = DIALOG_ITEM_NUMBER;
        di[ 3 ].min = 0;
        di[ 3 ].max = 64;
        di[ 3 ].int_val = sprofile_get_int_value( "audio_ch_in", 0, 0 );
        di[ 4 ].type = DIALOG_ITEM_NONE;
        wm->opt_dialog_items = di;
        int d = dialog( wm_get_string( STR_WM_ASIO_OPTIONS ), 0, wm_get_string( STR_WM_OKCANCEL ), wm );
        if( d == 0 )
        {
    	    bool changed = 0;
    	    if( sprofile_get_int_value( "audio_ch", 0, 0 ) != di[ 1 ].int_val )
    	    {
    		sprofile_set_int_value( "audio_ch", di[ 1 ].int_val, 0 );
    		changed = 1;
    	    }
    	    if( sprofile_get_int_value( "audio_ch_in", 0, 0 ) != di[ 3 ].int_val )
    	    {
    		sprofile_set_int_value( "audio_ch_in", di[ 3 ].int_val, 0 );
    		changed = 1;
    	    }
    	    if( changed )
    	    {
    		wm->prefs_restart_request = true;
        	sprofile_save( 0 );
    	    }
        }
    }
    
    return 0;
}   

int prefs_audio_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    prefs_audio_data* data = (prefs_audio_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( prefs_audio_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;
		
		char ts[ 512 ];
		int y = 0;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->driver = new_window( "Driver", 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->driver, prefs_audio_driver_handler, data, wm );
		set_window_controller( data->driver, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->driver, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->device = new_window( "Output device", 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->device, prefs_audio_device_handler, data, wm );
		set_window_controller( data->device, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->device, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->device_in = new_window( "Input device", 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->device_in, prefs_audio_device_handler, data, wm );
		set_window_controller( data->device_in, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->device_in, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->buf = new_window( "Audio buffer size", 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->buf, prefs_audio_buf_handler, data, wm );
		set_window_controller( data->buf, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->buf, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->freq = new_window( wm_get_string( STR_WM_SAMPLE_RATE ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->freq, prefs_audio_freq_handler, data, wm );
#if defined(ONLY44100)
		sprintf( ts, "%s", wm_get_string( STR_WM_AUTO ) );
#else
		sprintf( ts, "%s\n44100\n48000\n88200\n96000\n192000", wm_get_string( STR_WM_AUTO ) );
#endif
		button_set_menu( data->freq, ts );
		set_window_controller( data->freq, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->freq, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->opt = new_window( wm_get_string( STR_WM_ADD_OPTIONS ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->opt, prefs_audio_opt_handler, data, wm );
		set_window_controller( data->opt, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->opt, 2, wm, CPERC, (WCMD)100, CEND );
		data->opt->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
		y += wm->text_ysize + wm->interelement_space;

		prefs_audio_reinit( win );
		
		wm->prefs_section_ysize = y;
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    {
		wbd_lock( win );
		wm->cur_font_color = wm->color2;
		draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
		sundog_sound* snd = wm->sd->ss;
                if( snd )
                {
                    char ts[ 512 ];
                    int y = data->freq->y + data->freq->ysize + wm->interelement_space;
                    if( data->opt->visible )
                        y = data->opt->y + data->opt->ysize + wm->interelement_space;
                    ts[ 0 ] = 0;
                    smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_CUR_DRIVER ) );
                    smem_strcat( ts, sizeof( ts ), ": " );
                    smem_strcat( ts, sizeof( ts ), sundog_sound_get_driver_info( snd ) );
                    draw_string( ts, 0, y, wm );
                    sprintf( ts, "%s: %d %s", wm_get_string( STR_WM_CUR_SAMPLE_RATE ), snd->freq, wm_get_string( STR_WM_HZ ) );
                    draw_string( ts, 0, y + char_y_size( wm ), wm );
                    sprintf( ts, "%s: %d; %d %s", wm_get_string( STR_WM_CUR_LATENCY ), snd->out_latency, ( snd->out_latency * 1000 ) / snd->freq, wm_get_string( STR_WM_MS ) );
                    draw_string( ts, 0, y + char_y_size( wm ) * 2, wm );
                }
                wbd_draw( wm );
                wbd_unlock( wm );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    retval = 1;
	    break;
    }
    return retval;
}
