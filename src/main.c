/******************************************************************************
 *                                                                            *
 *                                "main.c"                                    *
 *                                                                            *
 ******************************************************************************/
/* 1 tab = 4 spaces! */

#ifndef OEMRESOURCE
#define OEMRESOURCE
#endif // OEMRESOURCE

#include <windows.h>
#include <commctrl.h>
#include <io.h>

#include <timeapi.h>
#include <winuser.h>

#include "global.h"
#include "version.h"
#include "resource.h"
#include "main.h"
#include "msx.h"
#include "draw.h"
#include "file.h"
#include "settings.h"
#include "sound.h"
#include "input.h"
#include "crystal.h"
#include "cont.h"
#include "mapper.h"
#include "io.h"
#include "vdp.h"
#include "help.h"
#include "media.h"
#include "z80.h"
#include "netplay.h"
#include "paste.h"
#include "psg.h"
#include "state.h"
#include "reverse.h"
#include "movie.h"
#include "tool.h"
#include "psglogger.h"
#include "tape.h"
#include "update.h"

/* EXTERN */
Main* MAIN;

/* msg callback */
static LRESULT CALLBACK WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch( msg )
	{
		case WM_DROPFILES:
        {
		    media_drop( wParam );
		    return FALSE;
        }
		case WM_PAINT: case WM_SIZE: case WM_SIZING: case WM_MOVE: case WM_MOVING:
        {
             draw_repaint();
             return FALSE;
        }
		case WM_ACTIVATEAPP:
        {
		    MAIN->foreground=wParam;
		    draw_repaint();
		    return FALSE;
        }
		case WM_ENTERMENULOOP: case WM_ENTERSIZEMOVE:
        {
		    MAIN->dialog=TRUE;
		    draw_repaint();
		    return FALSE;
        }
		case WM_EXITMENULOOP: case WM_EXITSIZEMOVE:
        {
            MAIN->dialog=FALSE;
            draw_repaint();
            return FALSE;
        }
		case WM_SYSKEYDOWN:
			if ( !(msx_get_paused()&1) && input_menukey(wParam) ) return FALSE;
        break;
		case WM_CLOSE:
        {
			/* check for recording movie */
			int c = movie_confirm_exit();
			switch ( c )
			{
				case IDCANCEL:      /* don't exit */
                break;
				case IDYES:         // Warning -Wimplicit-fallthrough
                {
					if ( movie_set_custom_name_save(FALSE) ) movie_stop(TRUE);
					movie_reset_custom_name();
                }
				default:
					msx_stop(FALSE);
					KillTimer( MAIN->window, 0 );

					settings_save();

					tool_clean();     psglogger_clean(); movie_clean();
					netplay_clean();  paste_clean();     reverse_clean();
					state_clean();    io_clean();        tape_clean();
					mapper_clean();   psg_clean();       vdp_clean();
					z80_clean();      msx_clean();       draw_clean();
					sound_clean();    cont_clean();      input_clean();
					settings_clean(); update_clean();    file_clean();

					DestroyWindow( hwnd );
					MAIN->window=NULL;
                break;
			}
			return FALSE;
		}
		case WM_DESTROY:
        {
		    PostQuitMessage(0);
		    return FALSE;
        }
		case WM_SYSCOMMAND:
			switch (wParam&0xfff0)
			{
				case SC_KEYMENU:
					if ( !(msx_get_paused()&1) && input_menukey(VK_MENU) && lParam!=0 ) return FALSE;
                break;
				default:
                break;
			}
        break;
		case WM_LBUTTONDBLCLK:
        {
            input_doubleclick();
            return FALSE;
        }
		/* the menu, bon appetit */
		case WM_COMMAND:
        {
			switch (LOWORD(wParam))
			{
            /* file */
				case IDM_MEDIA:
				    if ( !psglogger_is_active() && !netplay_is_active() && !movie_get_active_state() )
                    {
                        MAIN->dialog=TRUE;
                        DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_MEDIA), MAIN->window,(DLGPROC)media_dialog );
                        MAIN->dialog=FALSE;
                    }
                break;
				case IDM_NETPLAY:
                #ifdef MEISEI_KAILLERA
				    if ( !netplay_is_active() && !movie_get_active_state() ) netplay_open();
                #else
                    MessageBox(
                        MAIN->window,
                    #ifdef MEISEI_ESP
                        "El Juego en la Red usa \"kailleraclient.dll\" y ha sido desactivado por su seguridad.\n"
                        "Siempre puede compilar una versión que use \"kailleraclient.dll\" bajo su responsabilidad.",
                        "¡Aviso de Seguridad Importante!",
                    #else
                        "Network Gaming uses \"kailleraclient.dll\" and has been disabled for your security.\n"
                        "You can always compile a version that uses \"kailleraclient.dll\" at your own risk.",
                        "Important Safety Notice!",
                    #endif // MEISEI_ESP
                        MB_OK
                    );
                #endif
                break;
				case IDM_EXITEMU:
                    PostMessage(hwnd,WM_CLOSE,0,0);
                break;
				case IDM_PREVSLOT:
				    state_set_slot(state_get_slot()-1,FALSE);
                break;
				case IDM_NEXTSLOT:
				    state_set_slot(state_get_slot()+1,FALSE);
                break;
				case IDM_STATE_0: case IDM_STATE_1: case IDM_STATE_2: case IDM_STATE_3:
                case IDM_STATE_4: case IDM_STATE_5: case IDM_STATE_6: case IDM_STATE_7:
				case IDM_STATE_8: case IDM_STATE_9: case IDM_STATE_A: case IDM_STATE_B:
                case IDM_STATE_C: case IDM_STATE_D: case IDM_STATE_E: case IDM_STATE_F:
				case IDM_STATE_CUSTOM: case IDM_STATE_M:
					state_set_slot(LOWORD(wParam)-IDM_STATE_0,FALSE);
                break;
				case IDM_STATE_SAVE:
				    if (state_set_custom_name_save()) state_save();
				    state_reset_custom_name();
                break;
				case IDM_STATE_LOAD:
				    if (state_set_custom_name_open()) state_load();
				    state_reset_custom_name();
                break;
				case IDM_MOVIE_PLAY:
                {
					int play=TRUE, stop=FALSE;
					if ( movie_is_recording() )
                    {
						stop=TRUE;
						if (movie_set_custom_name_save(TRUE))
                        {
							movie_stop(FALSE);
							movie_reset_custom_name();
						}
						else play=FALSE;
					}
					if (play&&movie_set_custom_name_open()) movie_play(stop);
					movie_reset_custom_name();
                }
                break;
				case IDM_MOVIE_RECORD:
				    movie_record();
                break;
				case IDM_MOVIE_STOP:
				    if (movie_set_custom_name_save(TRUE)) movie_stop(FALSE);
				    movie_reset_custom_name();
                break;
				case IDM_TIMECODE:
				    movie_set_timecode(movie_get_timecode()^1);
                break;
				case IDM_SCREENSHOT:
				    draw_set_screenshot();
                break;
				case IDM_DIRECTORIES:
				    MAIN->dialog=TRUE;
				    DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_DIRECTORIES), MAIN->window, (DLGPROC)file_directories_dialog );
				    MAIN->dialog=FALSE;
                break;
            /* edit */
				case IDM_HARDRESET:
				    if ( !psglogger_is_active() && !netplay_is_active() && !movie_get_active_state() )
                    {
                        msx_wait();
                        msx_reset(TRUE);
                        msx_wait_done();
                    }
                break;
				case IDM_SOFTRESET:
				    if ( !psglogger_is_active() && !netplay_is_active() && !movie_get_active_state() )
                    {
                        msx_wait();
                        msx_reset(FALSE);
                        msx_wait_done();
                    }
                break;
				case IDM_PASTE:
				    paste_start_text();
                break;
				case IDM_SOUND:
				    msx_wait();
				    sound_set_enabled(sound_get_enabled()^1);
				    msx_wait_done();
                break;
				case IDM_LUMINOISE:
				    msx_wait();
				    vdp_luminoise_set(vdp_luminoise_get()^1);
				    msx_wait_done();
                break;
				case IDM_PAUSEEMU:
				    if (!netplay_is_active())
                    {
                        msx_set_paused(msx_get_paused()^1);
                        main_menu_check( IDM_PAUSEEMU,msx_get_paused()&1 );
                    }
                break;
				case IDM_TIMING:
				    MAIN->dialog=TRUE;
				    DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_TIMING), MAIN->window,(DLGPROC)crystal_timing );
				    MAIN->dialog=FALSE;
                break;
				case IDM_PORT1_J:     case IDM_PORT1_M:   case IDM_PORT1_T:  case IDM_PORT1_HS:
                case IDM_PORT1_MAGIC: case IDM_PORT1_ARK: case IDM_PORT1_NO:
				case IDM_PORT2_J:     case IDM_PORT2_M:   case IDM_PORT2_T:  case IDM_PORT2_HS:
                case IDM_PORT2_MAGIC: case IDM_PORT2_ARK: case IDM_PORT2_NO:
					cont_user_insert( LOWORD(wParam) );
                break;
            /* view */
				case IDM_ZOOM1: case IDM_ZOOM2: case IDM_ZOOM3:
                case IDM_ZOOM4: case IDM_ZOOM5: case IDM_ZOOM6:
                    draw_set_zoom( (LOWORD(wParam)-IDM_ZOOM1)+1 );
                break;
				case IDM_FULLSCREEN:
				    msx_wait();
				    draw_switch_screenmode();
				    msx_wait_done();
                break;
				case IDM_CORRECTASPECT:
				    msx_wait();
				    draw_set_correct_aspect( draw_get_correct_aspect()^1 );
				    draw_set_repaint(2);
				    msx_wait_done();
                break;
				case IDM_STRETCHFIT:
				    msx_wait();
				    draw_set_stretch(draw_get_stretch()^1);
				    draw_set_repaint(2);
				    msx_wait_done();
                break;
				case IDM_RENDERD3D:
				    if ( draw_get_rendertype() != DRAW_RENDERTYPE_D3D ) {
                        draw_set_rendertype(DRAW_RENDERTYPE_D3D);
                        draw_set_recreate();
                    }
                break;
				case IDM_RENDERDD:
				    if ( draw_get_rendertype() != DRAW_RENDERTYPE_DD ) {
                        draw_set_rendertype(DRAW_RENDERTYPE_DD);
                        draw_set_recreate();
                    }
                break;
				case IDM_SOFTRENDER:
				    draw_set_softrender( draw_get_softrender()^1 );
				    draw_set_recreate();
                break;
				case IDM_SHELLMSG:
				    msx_wait();
				    draw_set_text( draw_get_text()^1 );
				    msx_wait_done();
                break;
				case IDM_BFILTER:
				    draw_set_bfilter( draw_get_bfilter()^1 );
                break;
				case IDM_FLIP_X:
				    draw_set_flip_x( draw_get_flip_x()^1 );
				break;
				case IDM_VSYNC:
				    draw_set_vsync(draw_get_vsync()^1);
				    if ( draw_get_rendertype() == DRAW_RENDERTYPE_D3D ) draw_set_recreate();
                break;
				case IDM_VIDEOFORMAT:
				    if ( !netplay_is_active() && !movie_get_active_state() ) draw_switch_vidformat();
                break;
				case IDM_PALETTE:
				    MAIN->dialog=TRUE;
				    DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_PALETTE), MAIN->window, (DLGPROC)draw_palette_settings );
				    MAIN->dialog=FALSE;
                break;
				case IDM_VSRGB:
				    draw_set_surface_change( DRAW_SC_VIDSIGNAL|DRAW_VIDSIGNAL_RGB );
                break;
				case IDM_VSMONOCHROME:
				    draw_set_surface_change( DRAW_SC_VIDSIGNAL|DRAW_VIDSIGNAL_MONOCHROME );
				break;
				case IDM_VSCOMPOSITE:
				    draw_set_surface_change( DRAW_SC_VIDSIGNAL|DRAW_VIDSIGNAL_COMPOSITE );
				break;
				case IDM_VSSVIDEO:
				    draw_set_surface_change( DRAW_SC_VIDSIGNAL|DRAW_VIDSIGNAL_SVIDEO );
				break;
				case IDM_VSCUSTOM:
				    MAIN->dialog=TRUE;
				    DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_VIDEOSIGNAL), MAIN->window,( DLGPROC)draw_change_vidsignal_custom );
				    MAIN->dialog=FALSE;
                break;
            /* advanced */
				case IDM_TILEVIEW:
				    msx_wait(); tool_set_window(TOOL_WINDOW_TILEVIEW,IDM_TILEVIEW); msx_wait_done();
                break;
				case IDM_SPRITEVIEW:
				    msx_wait(); tool_set_window(TOOL_WINDOW_SPRITEVIEW,IDM_SPRITEVIEW); msx_wait_done();
                break;
				case IDM_PSGTOY:
				    msx_wait(); tool_set_window(TOOL_WINDOW_PSGTOY,IDM_PSGTOY); msx_wait_done();
				break;
				case IDM_LAYERB:
				    msx_wait(); vdp_set_bg_enabled(vdp_get_bg_enabled()^1); msx_wait_done();
                break;
				case IDM_LAYERS:
				    msx_wait(); vdp_set_spr_enabled(vdp_get_spr_enabled()^1); msx_wait_done();
                break;
				case IDM_LAYERUNL:
				    msx_wait(); vdp_set_spr_unlim(vdp_get_spr_unlim()^1); msx_wait_done();
                break;
            /* help */
				case IDM_HELP:
				    MAIN->dialog=TRUE;
				    DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_HELP), MAIN->window, (DLGPROC)help_help );
				    MAIN->dialog=FALSE;
                break;
				case IDM_UPDATE:
                #ifdef MEISEI_UPDATE
				    MAIN->dialog=TRUE;
				    if (update_download_thread_is_active()) {
                        LOG_ERROR_WINDOW(
                            MAIN->window,
                        #ifdef MEISEI_ESP
                            "El descargador de firmware todavía está ocupado."
                        #else
                            "Firmware Downloader is still busy."
                        #endif
                        );
				    } else DialogBox(MAIN->module,MAKEINTRESOURCE(IDD_UPDATE),MAIN->window,(DLGPROC)update_dialog);
				    MAIN->dialog=FALSE;
                #else
                    MessageBox(
                        MAIN->window,
                    #ifdef MEISEI_ESP
                        "El descargador de firmware aún no funciona.",
                        "¡Meisei 1.3.3 está en desarollo!",
                    #else
                        "Firmware downloader still not working.",
                        "Meisei 1.3.3 is in development!",
                    #endif // MEISEI_ESP
                        MB_OK
                    );
                #endif
                break;
				case IDM_ABOUT:
				    MAIN->dialog=TRUE;
				    DialogBox( MAIN->module, MAKEINTRESOURCE(IDD_ABOUT), MAIN->window, (DLGPROC)help_about );
				    MAIN->dialog=FALSE;
                break;
            /* etc */
				case IDM_CPUSPEEDPLUS:
				    if ( !netplay_is_active() && !movie_get_active_state() )
                    {
                        msx_wait();
                        crystal_set_cpuspeed( crystal->overclock+1 );
                        msx_wait_done();
                    }
                break;
				case IDM_CPUSPEEDMIN:
				    if (!netplay_is_active()&&!movie_get_active_state())
                    {
                        msx_wait(); crystal_set_cpuspeed(crystal->overclock-1); msx_wait_done();
                    }
                break;
				case IDM_CPUSPEEDNORMAL:
				    if ( !netplay_is_active() && !movie_get_active_state() )
                    {
                        msx_wait(); crystal_set_cpuspeed(100); msx_wait_done();
                    }
                break;
				case IDM_GRABMOUSE:
				    input_set_mouse_cursor_grabbed( input_get_mouse_cursor_grabbed()^1 );
                break;
				case IDM_PASTEBOOT:
				    paste_start_boot();
                break;
				case IDM_TAPEREWIND:
					if (!netplay_is_active()&&!movie_get_active_state()) {
						msx_wait();
						if ( tape_is_inserted() && ( tape_get_pos_cur() + tape_get_cur_block_cur() ) !=0 )
                        {
							reverse_invalidate();
							tape_rewind_cur();
							LOG(
                                LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_TAPERW),
                            #ifdef MEISEI_ESP
                                "cinta rebobinada            "
                            #else
                                "tape rewinded               "
                            #endif // MEISEI_ESP
                            );
						}
						msx_wait_done();
					}
                break;
				default:
				    // Nada por hacer.
                break;
			} // switch( LOWORD(wParam) )
			return FALSE;
        } // WM_COMMAND:
		/* timer timeout */
		case WM_TIMER:
        {
			WaitForSingleObject( MAIN->mutex, INFINITE );

			/* workaround: WM_ACTIVATEAPP should be enough to determine window priority, but for some reason it gets rarely ignored */
			MAIN->foreground = ( MAIN->window == GetForegroundWindow() );

			input_update_leds();
			input_mouse_cursor_update();
			sound_close_sleepthread();
			draw_check_reset(); /* resetting/recreating the renderer device is the responsibility of the thread that created it */

			msx_set_main_interrupt();
			ReleaseMutex(MAIN->mutex);
			return FALSE;
        }
		default:
        break;
	}

	return DefWindowProc( hwnd,msg, wParam, lParam );
}

/* program entry */
int WINAPI WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR arg, int show )
{
    prev = prev;
	WNDCLASSEX wc;
	MSG msg;
	HWND extwnd;
	RECT rect;
	HGDIOBJ bgc_start = GetStockObject( BLACK_BRUSH );

	memset( &wc, 0 ,sizeof( WNDCLASSEX ) );

	InitCommonControls();

	log_init();
	crystal_init();
	draw_setzero();
	draw_create_text_mutex();
	draw_set_text_max_ticks_default();

	if ( CoInitializeEx( NULL, COINIT_APARTMENTTHREADED ) != S_OK )
    {
		LOG(
            LOG_MISC|LOG_ERROR,
        #ifdef MEISEI_ESP
            "¡No se puede inicializar COM!\n"
        #else
            "Can't initialize COM!\n"
        #endif // MEISEI_ESP
        );
		exit(1);
	}

	timeBeginPeriod(1);

	MEM_CREATE( MAIN, sizeof( Main ) );
	MAIN->module = inst;

	#if DEBUG_PROFILER
	PROFILER_INIT();
	#endif

	file_init();
	settings_init();
	file_directories();

	if ( ( MAIN->mutex = CreateMutex( NULL, FALSE, NULL ) ) == NULL )
    {
        LOG(
            LOG_MISC|LOG_ERROR,
        #ifdef MEISEI_ESP
            "¡No se puede crear el mutex principal!\n"
        #else
            "Can't create main mutex!\n"
        #endif // MEISEI_ESP
        );
        exit(1);
    }

	/* register the window class */
	wc.cbSize=sizeof(WNDCLASSEX);				/* struct size */
	wc.lpfnWndProc=WndProc;						/* pointer to wndproc */
	wc.hInstance=MAIN->module;					/* handle to app instance */
	wc.hIconSm=wc.hIcon=(HICON)LoadImage( MAIN->module, MAKEINTRESOURCE(ID_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE ); /* 32x32 icon (alt-tab), and small icon (top left, file) */
	wc.hCursor=(HCURSOR)LoadImage(NULL,MAKEINTRESOURCE(OCR_NORMAL),IMAGE_CURSOR,0,0,LR_SHARED); /* cursor when it's over the window */
	wc.hbrBackground=(HBRUSH)bgc_start;			/* bg colour */
	wc.lpszMenuName=MAKEINTRESOURCE(ID_MENU);	/* name of menu resource */
	wc.lpszClassName=version_get(VERSION_NAME);	/* class name */
	wc.style=CS_DBLCLKS;						/* style */

	if(!RegisterClassEx(&wc))
	{
        LOG(
            LOG_MISC|LOG_ERROR,
        #ifdef MEISEI_ESP
            "¡No se puede registrar la ventana!\n"
        #else
            "Can't register window!\n"
        #endif // MEISEI_ESP
        );
        exit(1);
    }

	#define WIN_STYLE	 WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN
	#define WIN_EX_STYLE	WS_EX_CLIENTEDGE|WS_EX_ACCEPTFILES

	/* get (window size)-(client area size) */
	rect.left=rect.top=rect.right=rect.bottom=0;
	AdjustWindowRectEx(
		&rect,			/* location of rect struct */
		WIN_STYLE,		/* window style */
		TRUE,			/* has menu? */
		WIN_EX_STYLE	/* extended style */
	);
	MAIN->x_plus=rect.right-rect.left;
	MAIN->y_plus=rect.bottom-rect.top;

	MAIN->menu=LoadMenu(MAIN->module,MAKEINTRESOURCE(ID_MENU));

	/* create window */
	MAIN->window=CreateWindowEx (
			WIN_EX_STYLE,					/* extended style */
			wc.lpszClassName,				/* class name */
			version_get(VERSION_NAME),		/* window title */
			WIN_STYLE,						/* window style */
			CW_USEDEFAULT,					/* x placement */
			CW_USEDEFAULT,					/* y placement */
			MAIN->x_plus+256*2,				/* width */
			MAIN->y_plus+192*2,				/* height */
			NULL,							/* parent window handle */
			MAIN->menu,						/* menu handle */
			MAIN->module,					/* app instance handle */
			NULL							/* pointer to window creation data */
	);

	if(MAIN->window==NULL)
    {
        LOG(
            LOG_MISC|LOG_ERROR,
        #ifdef MEISEI_ESP
            "¡No se puede crear la ventana!\n"
        #else
            "Can't create window!\n"
        #endif // MEISEI_ESP
        );
        exit(1);
    }

	vdp_init();
	draw_init();
	main_parent_window( MAIN->window, 0, 0, 0, 0, 0 );

	crystal_settings_load();
	crystal_set_mode(FALSE);

	ShowWindow( MAIN->window,show );
	UpdateWindow( MAIN->window );

    // * * * MEISEI_MENSAJE_INICIO * * *
    /*
    MessageBox(
        MAIN->window,
    #ifdef MEISEI_ESP
        "Esta es una versión en desarrollo de Meisei 1.3.3\nAyuda y buenas iniciativas a \"pipagerardo@gmail.com\"",
        "¡Meisei 1.3.3 está en desarollo!",
    #else
        "This is a development version of Meisei 1.3.3\nHelp and good initiatives to \"pipagerardo@gmail.com\"",
        "Meisei 1.3.3 is in development!",
    #endif
        MB_OK
    );
    */
    // * * * MEISEI_MENSAJE_INICIO * * *

	LOG(
        LOG_MISC|LOG_COLOUR(LC_PINK),
    #ifdef MEISEI_ESP
        "Bienvenido a %s %s, © %s\n",
    #else
        "Welcome to %s %s, © %s\n",
    #endif // MEISEI_ESP
        version_get(VERSION_NAME),
        version_get(VERSION_NUMBER),
        version_get(VERSION_AUTHOR)
    );

	if (draw_get_fullscreen_set()) draw_switch_screenmode();
	draw_draw();
	SetClassLongPtr(MAIN->window,GCLP_HBRBACKGROUND,0);
	DeleteObject(bgc_start);

	netplay_init();
	tool_init();

	media_init();
	mapper_init();
	mapper_open_bios(NULL);
	tape_init();

	/* empty carts */
	mapper_extra_configs_reset(0); mapper_close_cart(0); mapper_set_carttype(0,CARTTYPE_NOMAPPER);
	mapper_extra_configs_reset(1); mapper_close_cart(1); mapper_set_carttype(1,CARTTYPE_NOMAPPER);

	/* try to open "meisei_autorun.rom", else try to open cmdline, else nothing */
	if (
        !( !access( "meisei_autorun.zip", F_OK ) && media_open_single( "meisei_autorun.zip" ) ) &&
         ( !strlen(arg) || !media_open_single(arg) )
    ) {
		char c[STRING_SIZE];
		mapper_close_cart(0);
		main_titlebar( mapper_get_current_name(c) );
	}

	paste_init();
	io_init();
	psg_init();
	sound_init();
	msx_init();
	input_init();
	cont_init();
	state_init();
	reverse_init();
	movie_init();
	psglogger_init();

#if Z80_TRACER
	printf("Z80 tracer enabled - hold key to log to file.\n\n");
#endif

	SetTimer( MAIN->window, 0, 15, NULL );

	msx_start();

	/* messageloop */
	while( GetMessage(&msg,NULL,0,0) > 0 ) {

		extwnd=tool_is_running()?tool_getactivewindow():NULL;
		if (extwnd&&IsDialogMessage(extwnd,&msg)) continue;

		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}

	timeEndPeriod(1);

	CoUninitialize();

	ClipCursor( NULL );
	DestroyMenu( MAIN->menu );
	DestroyIcon( wc.hIconSm );
	DestroyIcon( wc.hIcon );
	CloseHandle( MAIN->mutex );

	UnregisterClass( wc.lpszClassName, MAIN->module );

	MEM_CLEAN(MAIN);
	/* LOG_ERROR unallowed from here */
	LOG(
        LOG_VERBOSE,
    #ifdef MEISEI_ESP
        "limpieza principal\n"
    #else
        "main cleaned\n"
    #endif // MEISEI_ESP
    );
	crystal_clean();

	return msg.wParam;
}

void main_menu_check(UINT id,int en)
{
    CheckMenuItem( MAIN->menu,id,en==FALSE?MF_UNCHECKED:MF_CHECKED);
    DrawMenuBar( MAIN->window );
}
void main_menu_radio( UINT id, UINT first, UINT last )
{
    CheckMenuRadioItem( MAIN->menu, first, last, id, MF_BYCOMMAND );
    DrawMenuBar( MAIN->window );
}
void main_menu_enable(UINT id,int en)
{
    EnableMenuItem( MAIN->menu, id, en == FALSE ? MF_GRAYED:MF_ENABLED );
    DrawMenuBar( MAIN->window );
}

void main_menu_update_enable(void)
{
	int i,j,k;
	#define USED 4

	const UINT affected[][USED+1]={
	/* netplay  movplay  movrec  psglog */
	{  1,       1,       1,      1,    IDM_MEDIA },
	{  1,       1,       1,      0,    IDM_NETPLAY },
	{  1,       1,       1,      1,    IDM_STATE_LOAD },
	{  1,       0,       0,      1,    IDM_MOVIE_PLAY },
	{  1,       1,       1,      1,    IDM_HARDRESET },
	{  1,       1,       1,      1,    IDM_SOFTRESET },
	{  1,       1,       1,      0,    IDM_PORT1 },
	{  1,       1,       1,      0,    IDM_PORT2 },
	{  0,       1,       0,      0,    IDM_PASTE },
	{  1,       0,       0,      0,    IDM_TIMING },
	{  1,       0,       0,      0,    IDM_PAUSEEMU },
	{  1,       1,       1,      0,    IDM_VIDEOFORMAT },
	{  1,       0,       0,      0,    IDM_UPDATE },
	{  0xff,    0,       0,      0,    0 }
	};
	/* other permissions not checked here:
	   1,       1,       1,      1     open file by drag/drop
	   1,       1,       1,      0     tape rewind
	   1,       0,       0,      0     reverse
	   1,       0,       0,      0     speedup/slowdown
	   1,       1,       1,      0     change cpu clockspeed
	   1,       0,       0,      0     50/60hz hack
	   1,       0,       0,      0     non volatile sram
	   1,       1,       1,      0     change emu state with tool
	   0,       1,       0,      1     start ym log
	(+ ddraw/d3d) */

	int current[USED];

	current[0]=netplay_is_active();
	current[1]=movie_is_playing();
	current[2]=movie_is_recording();
	current[3]=psglogger_is_active();

	for (i=0;;i++) {
		if (affected[i][0]==0xff) break;

		k=0;
		for (j=0;j<USED;j++) {
			k |= (current[j]&affected[i][j]);
		}

		EnableMenuItem( MAIN->menu, affected[i][USED], k ? MF_GRAYED:MF_ENABLED );
	}

	DrawMenuBar( MAIN->window );

	#undef USED

	tool_menuchanged();
}

static void main_menu_caption(int get,int bypos,UINT id,char* c)
{
	MENUITEMINFO m;
	memset(&m,0,sizeof(MENUITEMINFO));
	m.cbSize=sizeof(MENUITEMINFO);
	m.fMask=MIIM_STRING;
	m.dwTypeData=c;
	if (get) {
        m.cch=STRING_SIZE; GetMenuItemInfo(MAIN->menu,id,bypos,&m);
    } else {
        SetMenuItemInfo( MAIN->menu, id, bypos, &m );
        DrawMenuBar( MAIN->window );
    } /* drawmenubar doesn't seem to have effect */
}

void main_menu_caption_get( UINT id, char* c ) { main_menu_caption(  TRUE, FALSE, id, c ); }
void main_menu_caption_put( UINT id, char* c ) { main_menu_caption( FALSE, FALSE, id, c ); }


/* position window on its parent */
void main_parent_window( HWND window, UINT xloc, UINT yloc, int xoff, int yoff, int desk )
{
	RECT rect_parent, rect_window,rect;
	int x=0,y=0,xs=0,ys=0;

	HWND parent=GetParent(window);
	memset(&rect_parent,0,sizeof(RECT));
	memset(&rect_window,0,sizeof(RECT));
	memset(&rect,0,sizeof(RECT));

	if (parent==NULL||desk) { parent=GetDesktopWindow(); GetClientRect(parent,&rect_parent); }
	else GetWindowRect(parent,&rect_parent);
	GetWindowRect(window,&rect_window);
	CopyRect(&rect,&rect_parent);

	OffsetRect(&rect_window,-rect_window.left,-rect_window.top);
	OffsetRect(&rect,-rect.left,-rect.top);
	OffsetRect(&rect,-rect_window.right,-rect_window.bottom);

	xs=rect_window.right-rect_window.left;
	ys=rect_window.bottom-rect_window.top;

	/* set location */
	switch (xloc)
	{
		case MAIN_PW_OUTERL: x=rect_parent.left-xs; break;
		case MAIN_PW_LEFT: x=rect_parent.left; break;
		case MAIN_PW_RIGHT: x=rect_parent.right-xs; break;
		case MAIN_PW_OUTERR: x=rect_parent.right; break;
		default: x=rect_parent.left+(rect.right/2.0); break; /* center */
	}
	switch (yloc)
	{
		case MAIN_PW_OUTERL: y=rect_parent.top-ys; break;
		case MAIN_PW_LEFT: y=rect_parent.top; break;
		case MAIN_PW_RIGHT: y=rect_parent.bottom-ys; break;
		case MAIN_PW_OUTERR: y=rect_parent.bottom; break;
		default: y=rect_parent.top+(rect.bottom/2.0); break; /* center */
	}

	x+=xoff; y+=yoff;

	/* check out of bounds */
	if (!SystemParametersInfo(SPI_GETWORKAREA,0,&rect,0)) {
		rect.left=rect.top=0;
		rect.right=GetSystemMetrics(SM_CXFULLSCREEN);
		rect.bottom=GetSystemMetrics(SM_CYFULLSCREEN);
	}
	if (x>(rect.right-xs)) x=rect.right-xs;
	if (x<rect.left) x=rect.left;
	if (y>(rect.bottom-ys)) y=rect.bottom-ys;
	if (y<rect.top) y=rect.top;

	SetWindowPos( window,NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
}

/* set titlebar text */
void main_titlebar(const char* text)
{
	char title[STRING_SIZE]={0};

	if ( text == NULL ) SetWindowText( MAIN->window, version_get(VERSION_NAME) );
	else {
		sprintf( title,"%s - %s",version_get(VERSION_NAME),text );
		SetWindowText( MAIN->window,title );
	}
}
