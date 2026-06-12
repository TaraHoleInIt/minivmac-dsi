/*
	OSGLUDSI.c

	Copyright (C) 2026 Paul C. Pratt, Tara Keeling

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/*
	Operating System GLUe for SDL (1.2 and 2.0) library

	All operating system dependent code for the
	SDL Library should go here.

	This is also the "reference" implementation. General
	comments about what the platform dependent code
	does should go here, and not be repeated for each
	platform. Such comments are labeled with "OSGLUxxx common".

	The SDL port can be used to create a more native port. Once
	the SDL port runs on a new platform, the source code for
	Mini vMac and SDL can be merged together. Then any SDL code
	not used for this platform is removed, then a lot of clean
	up is done step by step to remove the rest of the SDL code.
	This technique is particular useful if you are not very
	familiar with the new platform. It is long but straightforward,
	and you can learn about the platform as you go. The Cocoa
	port was created this way, with no previous knowledge of
	Cocoa or Objective-C.

	The main entry point 'main' is at the end of this file.
*/

#include "OSGCOMUI.h"
#include "OSGCOMUD.h"

#ifdef WantOSGLUDSI

#include "lvgl.h"
#include "ui.h"
#include "ui_selections.h"

LOCALVAR blnr HaveSoundOut = falseblnr;

/* --- some simple utilities --- */

GLOBALOSGLUPROC MyMoveBytes(anyp srcPtr, anyp destPtr, si5b byteCount)
{
	(void) memcpy((char *)destPtr, (char *)srcPtr, byteCount);
}

LOCALINLINEFUNC int clamp( int value, int min, int max ) {
	value = ( value < min ) ? min : value;
	value = ( value > max ) ? max : value;

	return value;
}

/* --- control mode and internationalization --- */

#define NeedCell2PlainAsciiMap 1

#define dbglog_OSGInit (1 && dbglog_HAVE)

#include "INTLCHAR.h"

LOCALVAR char *d_arg = NULL;
LOCALVAR char *n_arg = NULL;

#if CanGetAppPath
LOCALVAR char *app_parent = NULL;
LOCALVAR char *pref_dir = NULL;
#endif

#define MyPathSep '/'

LOCALFUNC tMacErr ChildPath(char *x, char *y, char **r)
{
	tMacErr err = mnvm_miscErr;
	int nx = strlen(x);
	int ny = strlen(y);
	{
		if ((nx > 0) && (MyPathSep == x[nx - 1])) {
			--nx;
		}
		{
			int nr = nx + 1 + ny;
			char *p = malloc(nr + 1);
			if (p != NULL) {
				char *p2 = p;
				(void) memcpy(p2, x, nx);
				p2 += nx;
				*p2++ = MyPathSep;
				(void) memcpy(p2, y, ny);
				p2 += ny;
				*p2 = 0;
				*r = p;
				err = mnvm_noErr;
			}
		}
	}

	return err;
}

LOCALPROC MyMayFree(char *p)
{
	if (NULL != p) {
		free(p);
	}
}

/* --- sending debugging info to file --- */

#if dbglog_HAVE

#ifndef dbglog_ToStdErr
#define dbglog_ToStdErr 0
#endif
#ifndef dbglog_ToSDL_Log
#define dbglog_ToSDL_Log 0
#endif

#if ! dbglog_ToStdErr
LOCALVAR FILE *dbglog_File = NULL;
#endif

LOCALFUNC blnr dbglog_open0(void)
{
#if dbglog_ToStdErr || dbglog_ToSDL_Log
	return trueblnr;
#else
#if CanGetAppPath
	if (NULL == app_parent)
#endif
	{
		dbglog_File = fopen("dbglog.txt", "w");
	}
#if CanGetAppPath
	else {
		char *t = NULL;

		if (mnvm_noErr == ChildPath(app_parent, "dbglog.txt", &t)) {
			dbglog_File = fopen(t, "w");
		}

		MyMayFree(t);
	}
#endif

	return (NULL != dbglog_File);
#endif
}

LOCALPROC dbglog_write0(char *s, uimr L)
{
#if dbglog_ToStdErr
	(void) fwrite(s, 1, L, stderr);
#elif dbglog_ToSDL_Log
	char t[256 + 1];

	if (L > 256) {
		L = 256;
	}
	(void) memcpy(t, s, L);
	t[L] = 1;

	SDL_Log("%s", t);
#else
	if (dbglog_File != NULL) {
		(void) fwrite(s, 1, L, dbglog_File);
	}
#endif
}

LOCALPROC dbglog_close0(void)
{
#if ! dbglog_ToStdErr
	if (dbglog_File != NULL) {
		fclose(dbglog_File);
		dbglog_File = NULL;
	}
#endif
}

#endif

/* --- information about the environment --- */

#define WantColorTransValid 0

#include "COMOSGLU.h"

#include "PBUFSTDC.h"

#include "CONTROLM.h"

/* --- text translation --- */

LOCALPROC NativeStrFromCStr(char *r, char *s)
{
	ui3b ps[ClStrMaxLength];
	int i;
	int L;

	ClStrFromSubstCStr(&L, ps, s);

	for (i = 0; i < L; ++i) {
		r[i] = Cell2PlainAsciiMap[ps[i]];
	}

	r[L] = 0;
}

/* --- drives --- */

/*
	OSGLUxxx common:
	define NotAfileRef to some value that is different
	from any valid open file reference.
*/
#define NotAfileRef NULL

#ifndef UseRWops
#define UseRWops 0
#endif

#if UseRWops
#define MyFilePtr SDL_RWops *
#define MySeek SDL_RWseek
	/*
		unlike fseek, SDL_RWseek returns nonzero value on success
	*/
#define MySeekSet RW_SEEK_SET
#define MySeekCur RW_SEEK_CUR
#define MySeekEnd RW_SEEK_END
#define MyFileRead(ptr, size, nmemb, stream) \
	SDL_RWread(stream, ptr, size, nmemb)
#define MyFileWrite(ptr, size, nmemb, stream) \
	SDL_RWwrite(stream, ptr, size, nmemb)
#define MyFileTell SDL_RWtell
#define MyFileClose SDL_RWclose
#define MyFileOpen SDL_RWFromFile
#else
#define MyFilePtr FILE *
#define MySeek fseek
#define MySeekSet SEEK_SET
#define MySeekCur SEEK_CUR
#define MySeekEnd SEEK_END
#define MyFileRead fread
#define MyFileWrite fwrite
#define MyFileTell ftell
#define MyFileClose fclose
#define MyFileOpen fopen
#define MyFileEof feof
#endif

LOCALVAR MyFilePtr Drives[NumDrives]; /* open disk image files */

LOCALPROC InitDrives(void)
{
	/*
		This isn't really needed, Drives[i] and DriveNames[i]
		need not have valid values when not vSonyIsInserted[i].
	*/
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		Drives[i] = NotAfileRef;
	}
}

GLOBALOSGLUFUNC tMacErr vSonyTransfer(blnr IsWrite, ui3p Buffer,
	tDrive Drive_No, ui5r Sony_Start, ui5r Sony_Count,
	ui5r *Sony_ActCount)
{
	/*
		OSGLUxxx common:
		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	tMacErr err = mnvm_miscErr;
	MyFilePtr refnum = Drives[Drive_No];
	ui5r NewSony_Count = 0;

	if (MySeek(refnum, Sony_Start, MySeekSet) >= 0) {
		if (IsWrite) {
			NewSony_Count = MyFileWrite(Buffer, 1, Sony_Count, refnum);
		} else {
			NewSony_Count = MyFileRead(Buffer, 1, Sony_Count, refnum);
		}

		if (NewSony_Count == Sony_Count) {
			err = mnvm_noErr;
		}
	}

	if (nullpr != Sony_ActCount) {
		*Sony_ActCount = NewSony_Count;
	}

	return err; /*& figure out what really to return &*/
}

GLOBALOSGLUFUNC tMacErr vSonyGetSize(tDrive Drive_No, ui5r *Sony_Count)
{
	/*
		OSGLUxxx common:
		set Sony_Count to the size of disk image number Drive_No.

		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	tMacErr err = mnvm_miscErr;
	MyFilePtr refnum = Drives[Drive_No];
	long v;

	if (MySeek(refnum, 0, MySeekEnd) >= 0) {
		v = MyFileTell(refnum);
		if (v >= 0) {
			*Sony_Count = v;
			err = mnvm_noErr;
		}
	}

	return err; /*& figure out what really to return &*/
}

LOCALFUNC tMacErr vSonyEject0(tDrive Drive_No, blnr deleteit)
{
	/*
		OSGLUxxx common:
		close disk image number Drive_No.

		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	MyFilePtr refnum = Drives[Drive_No];

	DiskEjectedNotify(Drive_No);

	MyFileClose(refnum);
	Drives[Drive_No] = NotAfileRef; /* not really needed */

	return mnvm_noErr;
}

GLOBALOSGLUFUNC tMacErr vSonyEject(tDrive Drive_No)
{
	return vSonyEject0(Drive_No, falseblnr);
}

LOCALPROC UnInitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		if (vSonyIsInserted(i)) {
			(void) vSonyEject(i);
		}
	}
}

LOCALFUNC blnr Sony_Insert0(MyFilePtr refnum, blnr locked,
	char *drivepath)
{
	/*
		OSGLUxxx common:
		Given reference to open file, mount it as a disk image file.
		if "locked", then mount it as a locked disk.
	*/

	tDrive Drive_No;
	blnr IsOk = falseblnr;

	if (! FirstFreeDisk(&Drive_No)) {
		MacMsg(kStrTooManyImagesTitle, kStrTooManyImagesMessage,
			falseblnr);
	} else {
		/* printf("Sony_Insert0 %d\n", (int)Drive_No); */

		{
			Drives[Drive_No] = refnum;
			DiskInsertNotify(Drive_No, locked);

			IsOk = trueblnr;
		}
	}

	if (! IsOk) {
		MyFileClose(refnum);
	}

	return IsOk;
}

LOCALFUNC blnr Sony_Insert1(char *drivepath, blnr silentfail)
{
	blnr locked = falseblnr;
	/* printf("Sony_Insert1 %s\n", drivepath); */
	MyFilePtr refnum = MyFileOpen(drivepath, "rb+");
	if (NULL == refnum) {
		locked = trueblnr;
		refnum = MyFileOpen(drivepath, "rb");
	}
	if (NULL == refnum) {
		if (! silentfail) {
			MacMsg(kStrOpenFailTitle, kStrOpenFailMessage, falseblnr);
		}
	} else {
		return Sony_Insert0(refnum, locked, drivepath);
	}
	return falseblnr;
}

LOCALFUNC tMacErr LoadMacRomFrom(char *path)
{
	tMacErr err;
	MyFilePtr ROM_File;
	int File_Size;

	ROM_File = MyFileOpen(path, "rb");
	if (NULL == ROM_File) {
		err = mnvm_fnfErr;
	} else {
		File_Size = MyFileRead(ROM, 1, kROM_Size, ROM_File);
		if (File_Size != kROM_Size) {
#ifdef MyFileEof
			if (MyFileEof(ROM_File))
#else
			if (File_Size > 0)
#endif
			{
				MacMsgOverride(kStrShortROMTitle,
					kStrShortROMMessage);
				err = mnvm_eofErr;
			} else {
				MacMsgOverride(kStrNoReadROMTitle,
					kStrNoReadROMMessage);
				err = mnvm_miscErr;
			}
		} else {
			err = ROM_IsValid();
		}
		MyFileClose(ROM_File);
	}

	return err;
}

LOCALFUNC blnr Sony_Insert2(char *s)
{
	char *d =
#if CanGetAppPath
		(NULL == d_arg) ? app_parent :
#endif
		d_arg;
	blnr IsOk = falseblnr;

	if (NULL == d) {
		IsOk = Sony_Insert1(s, trueblnr);
	} else
	{
		char *t = NULL;

		if (mnvm_noErr == ChildPath(d, s, &t)) {
			IsOk = Sony_Insert1(t, trueblnr);
		}

		MyMayFree(t);
	}

	return IsOk;
}

LOCALFUNC blnr Sony_InsertIth(int i)
{
	blnr v;

	if ((i > 9) || ! FirstFreeDisk(nullpr)) {
		v = falseblnr;
	} else {
		char s[] = "disk?.dsk";

		s[4] = '0' + i;

		v = Sony_Insert2(s);
	}

	return v;
}

LOCALFUNC blnr LoadInitialImages(void)
{
	if (! AnyDiskInserted()) {
		int i;

		for (i = 1; Sony_InsertIth(i); ++i) {
			/* stop on first error (including file not found) */
		}
	}

	return trueblnr;
}

/* --- ROM --- */

LOCALVAR char *rom_path = NULL;

#if CanGetAppPath
LOCALFUNC tMacErr LoadMacRomFromPrefDir(void)
{
	tMacErr err;
	char *t = NULL;
	char *t2 = NULL;

	if (NULL == pref_dir) {
		err = mnvm_fnfErr;
	} else
	if (mnvm_noErr != (err =
		ChildPath(pref_dir, "mnvm_rom", &t)))
	{
		/* fail */
	} else
	if (mnvm_noErr != (err =
		ChildPath(t, RomFileName, &t2)))
	{
		/* fail */
	} else
	{
		err = LoadMacRomFrom(t2);
	}

	MyMayFree(t2);
	MyMayFree(t);

	return err;
}
#endif

LOCALFUNC tMacErr LoadMacRomFromAppPar(void)
{
	tMacErr err;
	char *d =
#if CanGetAppPath
		(NULL == d_arg) ? app_parent :
#endif
		d_arg;

	if (NULL == d) {
		err = mnvm_fnfErr;
	} else
	{
		char *t = NULL;

		if (mnvm_noErr != (err =
			ChildPath(d, RomFileName, &t)))
		{
			/* fail */
		} else
		{
			err = LoadMacRomFrom(t);
		}

		MyMayFree(t);
	}

	return err;
}

LOCALFUNC blnr LoadMacRom(void)
{
	tMacErr err;

	if ((NULL == rom_path)
		|| (mnvm_fnfErr == (err = LoadMacRomFrom(rom_path))))
	if (mnvm_fnfErr == (err = LoadMacRomFromAppPar()))
#if CanGetAppPath
	if (mnvm_fnfErr == (err = LoadMacRomFromPrefDir()))
#endif
	if (mnvm_fnfErr == (err = LoadMacRomFrom(RomFileName)))
	{
		MacMsgOverride( kStrNoROMTitle, kStrNoROMMessage );
	}

	return trueblnr; /* keep launching Mini vMac, regardless */
}

LOCALVAR blnr wasInitOK = falseblnr;

/* --- DS(i) system definitions --- */

LOCALFUNC void systemSetupTimers( void );
uint32_t systemGetTicks( void );
LOCALFUNC void systemIRQTimer1( void );

/* --- DS(i) system globals --- */

LOCALVAR volatile uint32_t sysTickCounter = 0;

/* --- DS(i) system functions --- */

LOCALFUNC void systemIRQTimer1( void ) {
	sysTickCounter+= 65536;
}

LOCALFUNC void systemSetupTimers( void ) {
	TIMER0_CR = TIMER_DIV_64 | TIMER_ENABLE;
	TIMER0_DATA = TIMER_FREQ_64( 1000 );

	TIMER1_CR = TIMER_CASCADE | TIMER_ENABLE | TIMER_IRQ_REQ;
	TIMER1_DATA = 0;

	irqSet( IRQ_TIMER1, systemIRQTimer1 );
	irqEnable( IRQ_TIMER1 );
}

uint32_t systemGetTicks( void ) {
	return sysTickCounter + TIMER1_DATA;
}

/* --- DS(i) input globals --- */

static touchPosition inputTouchPos = {
	.px = 0,
	.py = 0,
	.rawx = 0,
	.rawy = 0,
	.z1 = 0,
	.z2 = 0
};

static int inputKeysDown = 0;
static int inputKeysHeld = 0;
static int inputKeysUp = 0;

LOCALVAR UIMouseMode inputMouseMode = UI_SEL_MOUSE_MODE_SCALED;
LOCALVAR UIMouseButton inputMouseButton = UI_SEL_MOUSE_BUTTON_L;

LOCALVAR int inputMouseButtonBit = 0;
LOCALVAR int inputMouseAcceleration = 2;

/* --- DS(i) video definitions --- */

typedef void ( *RenderScreenProc ) ( void );

typedef enum {
	VIDEO_SCREEN_ON_MAIN_UI_ON_TOUCH = 0,
	VIDEO_SCREEN_ON_TOUCH_UI_ON_MAIN
} ScreenSetup;

LOCALFUNC void videoChangeSetup( ScreenSetup setup );
LOCALFUNC void videoSwapScreens( void );
LOCALFUNC blnr videoUIHasFocus( void );

LOCALFUNC void videoSetupFBTexture( void );
LOCALFUNC void videoCopyFBTexture( void );
LOCALFUNC void videoSetupMaskTexture( void );
LOCALFUNC void videoVBlankIRQ( void );
LOCALFUNC void videoCalcStrips( void );
LOCALFUNC void videoInit( void );
LOCALFUNC int videoSetupPalette( uint16_t* palette );

LOCALFUNC void videoFrameScaled( void );
LOCALFUNC void videoFrameUnscaled( void );

LOCALFUNC void videoWriteDebug( const char* text );
LOCALFUNC void videoWriteMessage( const char* text );

LOCALFUNC void videoSetupLVGL( void );
LOCALFUNC void videoLVGLFlush( lv_display_t* disp, const lv_area_t* area, uint8_t* buf );
LOCALFUNC void videoLVGLTouchRead( lv_indev_t* indev, lv_indev_data_t* data );

LOCALFUNC void videoSetScaled( void );
LOCALFUNC void videoSetUnscaled( void );

LOCALPROC uiSetupDefaults( void );

/* --- DS(i) video globals --- */

static uint8_t maskTexture[ ( 512 * 8 ) / 4 ];

static int videoFBTexture = 0;
static int videoMaskTexture = 0;

static int palOddTexture = 0;
static int palOddTexture_Inverted = 0;

static int palEvenTexture = 0;
static int palEvenTexture_Inverted = 0;

static int palGreyTexture = 0;
static int palGreyTexture_Inverted = 0;

static int palSubpixelRGB = 0;
static int palSubpixelBGR = 0;

static int textureStarts[ 64 ];
static int textureEnds[ 64 ];

static int screenStarts[ 64 ];
static int screenEnds[ 64 ];

static int textureStartsScaled[ 128 ];
static int textureEndsScaled[ 128 ];

static int textureStartsScaledOffset[ 128 ];
static int textureEndsScaledOffset[ 128 ];

static int screenStartsScaled[ 128 ];
static int screenEndsScaled[ 128 ];

static volatile int videoScrollX = 0;
static volatile int videoScrollY = 0;

static volatile RenderScreenProc renderFunc = NULL;
static volatile int renderNeedsRefresh = 1;

static volatile uint32_t vblankCount = 0;

static PrintConsole videoMacMsgConsole;

static volatile int videoMacMsgBG = 0;
static volatile int videoMacMsgBGOn = 0;

static volatile int videoUIBGSub = 0;

static uint8_t lvGfxBuffer[ SCREEN_WIDTH * SCREEN_HEIGHT ];
static lv_display_t* lvDisplay = NULL;
static lv_indev_t* lvInput = NULL;

static ScreenSetup videoSetup = VIDEO_SCREEN_ON_MAIN_UI_ON_TOUCH;

static volatile int videoIsInverted = 0;
static volatile int videoIsSubpixel = 0;

static volatile UISubpxOrder videoSubpixelMain = UI_SEL_SUBPX_ORDER_RGB;
static volatile UISubpxOrder videoSubpixelSub = UI_SEL_SUBPX_ORDER_RGB;

/* --- DS(i) video functions --- */

LOCALINLINEFUNC int videoGetOddTexture( void ) {
	return ( videoIsInverted ) ? palOddTexture_Inverted : palOddTexture;
}

LOCALINLINEFUNC int videoGetEvenTexture( void ) {
	return ( videoIsInverted ) ? palEvenTexture_Inverted : palEvenTexture;
}

LOCALINLINEFUNC int videoGetGreyTexture( void ) {
	int tex = 0;

	if ( videoIsSubpixel == 1 ) {
		if ( videoSetup == VIDEO_SCREEN_ON_MAIN_UI_ON_TOUCH )
			return ( videoSubpixelMain == UI_SEL_SUBPX_ORDER_RGB ) ? palSubpixelRGB : palSubpixelBGR;
		else
			return ( videoSubpixelSub == UI_SEL_SUBPX_ORDER_RGB ) ? palSubpixelRGB : palSubpixelBGR;
	}

	return ( videoIsInverted ) ? palGreyTexture_Inverted : palGreyTexture;
}

LOCALFUNC void videoChangeSetup( ScreenSetup setup ) {
	switch ( setup ) {
		case VIDEO_SCREEN_ON_MAIN_UI_ON_TOUCH:
			lcdMainOnTop( );
			break;
		case VIDEO_SCREEN_ON_TOUCH_UI_ON_MAIN:
			lcdMainOnBottom( );
			break;
		default:
			break;
	};

	videoSetup = setup;
	renderNeedsRefresh = 1;
}

LOCALFUNC void videoSwapScreens( void ) {
	switch ( videoSetup ) {
		case VIDEO_SCREEN_ON_MAIN_UI_ON_TOUCH:
			videoChangeSetup( VIDEO_SCREEN_ON_TOUCH_UI_ON_MAIN );
			break;
		case VIDEO_SCREEN_ON_TOUCH_UI_ON_MAIN:
			videoChangeSetup( VIDEO_SCREEN_ON_MAIN_UI_ON_TOUCH );
			break;
		default:
			break;
	};
}

LOCALFUNC void videoSetScaled( void ) {
	renderFunc = videoFrameScaled;
	renderNeedsRefresh = 1;
}

LOCALFUNC void videoSetUnscaled( void ) {
	renderFunc = videoFrameUnscaled;
	renderNeedsRefresh = 1;
}

LOCALFUNC blnr videoUIHasFocus( void ) {
	return ( videoSetup == VIDEO_SCREEN_ON_MAIN_UI_ON_TOUCH ) ? trueblnr : falseblnr;
}

LOCALFUNC void videoSetupLVGL( void ) {
	int i = 0;

	videoUIBGSub = bgInitSub( 2, BgType_Bmp8, BgSize_B8_256x256, 0, 0 );

	for ( i = 0; i < 256; i++ ) {
		BG_PALETTE_SUB[ i ] = RGB8( i, i, i );
	}

    lv_init( );
    lv_tick_set_cb( systemGetTicks );

    assert( ( lvDisplay = lv_display_create( SCREEN_WIDTH, SCREEN_HEIGHT ) ) != NULL );
    assert( ( lvInput = lv_indev_create( ) ) != NULL );

    lv_display_set_flush_cb( lvDisplay, videoLVGLFlush );
    lv_display_set_buffers( lvDisplay, lvGfxBuffer, NULL, sizeof( lvGfxBuffer ), LV_DISPLAY_RENDER_MODE_FULL );
    lv_display_set_color_format( lvDisplay, LV_COLOR_FORMAT_L8 );

    lv_indev_set_type( lvInput, LV_INDEV_TYPE_POINTER );
    lv_indev_set_read_cb( lvInput, videoLVGLTouchRead );

    ui_init( );
	uiSetupDefaults( );

	// HACKHACKHACK
	// This is just so the initial settings are reflected in the emulator
	uiDisplayTabValueChangedCallback( NULL );
	uiEmulatorTabValueChangedCallback( NULL );
	uiMouseTabValueChangedCallback( NULL );
}

LOCALFUNC void videoLVGLFlush( lv_display_t* disp, const lv_area_t* area, uint8_t* buf ) {
    uint16_t* dst = ( uint16_t* ) bgGetGfxPtr( videoUIBGSub );

	DC_FlushRange( buf, sizeof( lvGfxBuffer ) );
    dmaCopyWords( 3, buf, dst, sizeof( lvGfxBuffer ) );
    lv_display_flush_ready( disp );
}

LOCALFUNC void videoLVGLTouchRead( lv_indev_t* indev, lv_indev_data_t* data ) {
    if ( ( inputKeysHeld & KEY_TOUCH ) && videoUIHasFocus( ) ) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = inputTouchPos.px;
        data->point.y = inputTouchPos.py;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

LOCALFUNC void videoWriteDebug( const char* text ) {
}

LOCALFUNC void videoWriteMessage( const char* text ) {
	consoleSelect( &videoMacMsgConsole );
	puts( text );
}

LOCALFUNC void videoInit( void ) {
    videoCalcStrips( );

    videoSetMode( MODE_0_3D );
	videoSetModeSub( MODE_5_2D );

    vramSetBankA( VRAM_A_TEXTURE );
    vramSetBankB( VRAM_B_TEXTURE );
	vramSetBankC( VRAM_C_SUB_BG );
    vramSetBankE( VRAM_E_TEX_PALETTE );

	vramSetBankD( VRAM_D_MAIN_BG_0x06000000 );

    glInit( );
    glEnable( GL_TEXTURE_2D );
    glEnable( GL_BLEND );

    glClearColor( 0, 0, 0, 31 );
    glClearPolyID( 63 );
    glClearDepth( 0x7FFF );

    glViewport( 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 );

    videoSetupMaskTexture( );
    videoSetupFBTexture( );

    videoSetUnscaled( );

	consoleInit( &videoMacMsgConsole, 1, BgType_Text4bpp, BgSize_T_256x256, 0, 1, true, true );
	videoMacMsgBG = bgInitHidden( 1, BgType_Text4bpp, BgSize_T_256x256, 0, 1 );

	REG_BG0CNT |= BG_PRIORITY_3;
	REG_BG1CNT |= BG_PRIORITY_0;

	consoleSelect( &videoMacMsgConsole );
	consoleSetColor( &videoMacMsgConsole, CONSOLE_LIGHT_RED );

	videoSetupLVGL( );

    irqSet( IRQ_VBLANK, videoVBlankIRQ );
    irqEnable( IRQ_VBLANK );
}

LOCALFUNC void videoCalcStrips( void ) {
    float ts = 0.0f;
    float te = 0.0f;
    float ss = 0.0f;
    float se = 0.0f;
    int i = 0;

    for ( i = 0; i < 512; i+= 8 ) {
        ts = ( ( float ) ( i - 1 ) ) / 512.0f;
        te = ts + ( 8.0f / 512.0f );

        ss = ( ( float ) i ) / 256.0f;
        se = ss + ( 8.0f / 256.0f );

        textureStarts[ i >> 3 ] = floattof32( ts );
        textureEnds[ i >> 3 ] = floattof32( te );

        screenStarts[ i >> 3 ] = floattof32( ss );
        screenEnds[ i >> 3 ] = floattof32( se );
    }

    for ( i = 0; i < 512; i+= 4 ) {
        ts = ( ( float ) ( i - 1 ) ) / 256.0f;
        te = ts + ( 4.0f / 256.0f );

        ss = ( ( float ) i ) / 256.0f;
        se = ss + ( 4.0f / 256.0f );       
        
        textureStartsScaled[ i >> 2 ] = floattof32( ts );
        textureEndsScaled[ i >> 2 ] = floattof32( te );

        screenStartsScaled[ i >> 2 ] = floattof32( ss );
        screenEndsScaled[ i >> 2 ] = floattof32( se );

        ts+= ( 1.0f / 512.0f );
        te+= ( 1.0f / 512.0f );

        textureStartsScaledOffset[ i >> 2 ] = floattof32( ts );
        textureEndsScaledOffset[ i >> 2 ] = floattof32( te );
    }
}

LOCALFUNC void videoSetupMaskTexture( void ) {
    uint16_t texPalette[ 4 ] = {
        RGB15( 0, 0, 31 ),
        RGB15( 0, 31, 0 ),
        RGB15( 0, 31, 0 ),
        RGB15( 0, 31, 0 )
    };
    int texId = 0;

    for ( int i = 0; i < sizeof( maskTexture ); i++ )
        maskTexture[ i ] = 0b00010001;

    assert( glGenTextures( 1, &texId ) == 1 );
    assert( glBindTexture( 0, texId ) == 1 );
    assert( glTexImageNtr2D( GL_RGB4, 512, 8, TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, NULL, NULL ) == 1 );
    assert( glColorTableNtr( 4, texPalette ) == 1 );

    videoMaskTexture = texId;

    vramSetBankA( VRAM_A_LCD );
        DC_FlushRange( maskTexture, sizeof( maskTexture ) );
        dmaCopyWords( 3, maskTexture, glGetTexturePointer( texId ), sizeof( maskTexture ) );
    vramSetBankA( VRAM_A_TEXTURE );
}

LOCALFUNC int videoSetupPalette( uint16_t* palette ) {
	int tid = -1;

	assert( glGenTextures( 1, &tid ) );
    assert( glBindTexture( 0, tid ) == 1 );
    assert( glTexImageNtr2D( GL_NOTEXTURE, 0, 0, 0, NULL, NULL ) == 1 );
    assert( glColorTableNtr( 4, palette ) == 1 );

	return tid;
}

LOCALFUNC void videoSetupFBTexture( void ) {
    uint16_t texPaletteOdd[ 4 ] = {
        RGB15( 31, 31, 31 ),
        RGB15( 0, 0, 0 ),
        RGB15( 31, 31, 31 ),
        RGB15( 0, 0, 0 )
    };
    uint16_t texPaletteOdd_Inverted[ 4 ] = {
        RGB15( 0, 0, 0 ),
        RGB15( 31, 31, 31 ),
        RGB15( 0, 0, 0 ),
        RGB15( 31, 31, 31 )
    };
    uint16_t texPaletteEven[ 4 ] = {
        RGB15( 31, 31, 31 ),
        RGB15( 31, 31, 31 ),
        RGB15( 0, 0, 0 ),
        RGB15( 0, 0, 0 )
    };
    uint16_t texPaletteEven_Inverted[ 4 ] = {
        RGB15( 0, 0, 0 ),
        RGB15( 0, 0, 0 ),
        RGB15( 31, 31, 31 ),
        RGB15( 31, 31, 31 )
    };
    uint16_t texPaletteGrey[ 4 ] = {
        RGB15( 31, 31, 31 ),
        RGB15( 15, 15, 15 ),
        RGB15( 15, 15, 15 ),
        RGB15( 0, 0, 0 )
    };
    uint16_t texPaletteGrey_Inverted[ 4 ] = {
        RGB15( 0, 0, 0 ),
        RGB15( 15, 15, 15 ),
		RGB15( 15, 15, 15 ),
        RGB15( 31, 31, 31 )
    };
    uint16_t texPaletteSubpixelRGB[ 4 ] = {
        RGB15( 31, 31, 31 ),	// White
        RGB15( 0, 15, 31 ),		// _gB
        RGB15( 31, 15, 0 ),		// Rg_
        RGB15( 0, 0, 0 )		// Black
    };
    uint16_t texPaletteSubpixelBGR[ 4 ] = {
        RGB15( 31, 31, 31 ),	// White
        RGB15( 31, 15, 0 ),		// Rg_
        RGB15( 0, 15, 31 ),		// _gB
        RGB15( 0, 0, 0 )		// Black
    };

	palOddTexture = videoSetupPalette( texPaletteOdd );
	palOddTexture_Inverted = videoSetupPalette( texPaletteOdd_Inverted );

	palEvenTexture = videoSetupPalette( texPaletteEven );
	palEvenTexture_Inverted = videoSetupPalette( texPaletteEven_Inverted );

	palGreyTexture = videoSetupPalette( texPaletteGrey );
	palGreyTexture_Inverted = videoSetupPalette( texPaletteGrey_Inverted );

	palSubpixelRGB = videoSetupPalette( texPaletteSubpixelRGB );
	palSubpixelBGR = videoSetupPalette( texPaletteSubpixelBGR );

	assert( glGenTextures( 1, &videoFBTexture ) );
    assert( glBindTexture( 0, videoFBTexture ) == 1 );
    assert( glTexImageNtr2D( GL_RGB4, 256, 512, TEXGEN_TEXCOORD, NULL, NULL ) == 1 );
}

LOCALFUNC void videoFrameScaled( void ) {
    static const int orthoLeft = floattof32( 0.0f );
    static const int orthoRight = floattof32( 1.0f );
    static const int orthoBottom = floattof32( 1.0f );
    static const int orthoTop = floattof32( 0.0f );
    static const int orthoNear = floattof32( 0.1f );
    static const int orthoFar = floattof32( 20.0f );
    static const int eye[ 3 ] = {
        floattof32( 0.0f ),
        floattof32( 0.0f ),
        floattof32( 1.0f )
    };
    static const int lookAt[ 3 ] = {
        floattof32( 0.0f ),
        floattof32( 0.0f ),
        floattof32( 0.0f )
    };
    static const int up[ 3 ] = {
        floattof32( 0.0f ),
        floattof32( 1.0f ),
        floattof32( 0.0f )
    };
    static const int fZero = floattof32( 0.0f );
    static const int fOne = floattof32( 1.0f );
    static const int depthNine = floattof32( 9.0f );
    static const int depthTen = floattof32( 10.0f );
    static const int screenScaleY = floattof32( 512.0f / vMacScreenHeight );
    static const int texOffsetStart = floattof32( 1.0f / 512.0f );
    static const int texOffsetEnd = fOne + floattof32( 1.0f / 512.0f );
    int i = 0;

    if ( ! renderNeedsRefresh )
        return;

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );

    glOrthof32( orthoLeft, orthoRight, orthoBottom, orthoTop, orthoNear, orthoFar );

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity( );

    gluLookAtf32(
        eye[ 0 ], eye[ 1 ], eye[ 2 ],
        lookAt[ 0 ], lookAt[ 1 ], lookAt[ 2 ],
        up[ 0 ], up[ 1 ], up[ 2 ]
    );

    glColor3b( 255, 255, 255 );

    glPolyFmt( POLY_ALPHA( 31 ) | POLY_CULL_NONE );
    glBegin( GL_QUAD );
        /* Odd pixels */
        glBindTexture( 0, videoFBTexture );
        glAssignColorTable( videoFBTexture, videoGetGreyTexture( ) );
        for ( i = 0; i < 256; i += 4 ) {
            // Top-left
            glTexCoord2f32( textureEndsScaled[ i >> 2 ], fZero );
            glVertex3v16( screenStartsScaled[ i >> 2 ], fZero, depthNine );

            // Top-right
            glTexCoord2f32( textureStartsScaled[ i >> 2 ], fZero );
            glVertex3v16( screenEndsScaled[ i >> 2 ], fZero, depthNine );

            // Bottom-right
            glTexCoord2f32( textureStartsScaled[ i >> 2 ], fOne );
            glVertex3v16( screenEndsScaled[ i >> 2 ], screenScaleY, depthNine );

            // Bottom-left
            glTexCoord2f32( textureEndsScaled[ i >> 2 ], fOne );
            glVertex3v16( screenStartsScaled[ i >> 2 ], screenScaleY, depthNine );
        }
    glEnd( );

    glPolyFmt( POLY_ALPHA( 15 ) | POLY_CULL_NONE );
    glBegin( GL_QUAD );
        for ( i = 0; i < 256; i += 4 ) {
            // Top-left
            glTexCoord2f32( textureEndsScaled[ i >> 2 ], texOffsetStart );
            glVertex3v16( screenStartsScaled[ i >> 2 ], fZero, depthTen );

            // Top-right
            glTexCoord2f32( textureStartsScaled[ i >> 2 ], texOffsetStart );
            glVertex3v16( screenEndsScaled[ i >> 2 ], fZero, depthTen );

            // Bottom-right
            glTexCoord2f32( textureStartsScaled[ i >> 2 ], texOffsetEnd );
            glVertex3v16( screenEndsScaled[ i >> 2 ], screenScaleY, depthTen );

            // Bottom-left
            glTexCoord2f32( textureEndsScaled[ i >> 2 ], texOffsetEnd );
            glVertex3v16( screenStartsScaled[ i >> 2 ], screenScaleY, depthTen );
        }
    glEnd( );

    glFlush( 0 );
}

LOCALFUNC void videoFrameUnscaled( void ) {
    static const int orthoLeft = floattof32( 0.0f );
    static const int orthoRight = floattof32( 1.0f );
    static const int orthoBottom = floattof32( 1.0f );
    static const int orthoTop = floattof32( 0.0f );
    static const int orthoNear = floattof32( 0.1f );
    static const int orthoFar = floattof32( 20.0f );
    static const int eye[ 3 ] = {
        floattof32( 0.0f ),
        floattof32( 0.0f ),
        floattof32( 1.0f )
    };
    static const int lookAt[ 3 ] = {
        floattof32( 0.0f ),
        floattof32( 0.0f ),
        floattof32( 0.0f )
    };
    static const int up[ 3 ] = {
        floattof32( 0.0f ),
        floattof32( 1.0f ),
        floattof32( 0.0f )
    };
    static const int scaleX = floattof32( 256.0f );
    static const int scaleY = floattof32( 192.0f );
    static const int translateZ = floattof32( 1.0f );
    static const int texCoordsMask[ 4 ][ 2 ] = {
        { floattof32( 0.0f ), floattof32( 0.0f ) }, // Top-left
        { floattof32( 1.0f ), floattof32( 0.0f ) }, // Top-right
        { floattof32( 1.0f ), floattof32( 1.0f ) }, // Bottom-right
        { floattof32( 0.0f ), floattof32( 1.0f ) }  // Bottom-left
    };
    static const int vertCoordsMask[ 4 ][ 3 ] = {
        { floattof32( 0.0f ), floattof32( 0.0f ), floattof32( 10.0f ) },    // Top-left
        { floattof32( 2.0f ), floattof32( 0.0f ), floattof32( 10.0f ) },    // Top-right
        { floattof32( 2.0f ), floattof32( 2.0f ), floattof32( 10.0f ) },    // Bottom-right
        { floattof32( 0.0f ), floattof32( 2.0f ), floattof32( 10.0f ) }     // Bottom-left
    };
    static const int fZero = floattof32( 0.0f );
    static const int fOne = floattof32( 1.0f );
    static const int depthNine = floattof32( 9.0f );
    static const int depthTen = floattof32( 10.0f );
    static const int screenScaleY = floattof32( 2.6666666667f );

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );

    glOrthof32( orthoLeft, orthoRight, orthoBottom, orthoTop, orthoNear, orthoFar );

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity( );

    gluLookAtf32(
        eye[ 0 ], eye[ 1 ], eye[ 2 ],
        lookAt[ 0 ], lookAt[ 1 ], lookAt[ 2 ],
        up[ 0 ], up[ 1 ], up[ 2 ]
    );

    glColor3b( 255, 255, 255 );

    glTranslatef32( 
        -( divf32( inttof32( videoScrollX ), scaleX  ) ),
        -( divf32( inttof32( videoScrollY ), scaleY ) ),
        translateZ
    );

    glPolyFmt( POLY_ALPHA( 31 ) | POLY_CULL_NONE | POLY_DEPTH_TEST_LESS );
    glBegin( GL_QUADS );
        /* Odd pixels */
        glBindTexture( 0, videoFBTexture );
        glAssignColorTable( videoFBTexture, videoGetOddTexture( ) );

        for ( int i = 0; i < 512; i += 8 ) {
            // Top-left
            glTexCoord2f32( textureEnds[ i >> 3 ], fZero );
            glVertex3v16( screenStarts[ i >> 3 ], fZero, depthNine );

            // Top-right
            glTexCoord2f32( textureStarts[ i >> 3 ], fZero );
            glVertex3v16( screenEnds[ i >> 3 ], fZero, depthNine );

            // Bottom-right
            glTexCoord2f32( textureStarts[ i >> 3 ], fOne );
            glVertex3v16( screenEnds[ i >> 3 ], screenScaleY, depthNine );

            // Bottom-left
            glTexCoord2f32( textureEnds[ i >> 3 ], fOne );
            glVertex3v16( screenStarts[ i >> 3 ], screenScaleY, depthNine );
        }

        /* Mask texture */
        glBindTexture( 0, videoMaskTexture );

        // Top-left
        glTexCoord2f32( texCoordsMask[ 0 ][ 0 ], texCoordsMask[ 0 ][ 1 ] );
        glVertex3v16( vertCoordsMask[ 0 ][ 0 ], vertCoordsMask[ 0 ][ 1 ], vertCoordsMask[ 0 ][ 2 ] );

        // Top-right
        glTexCoord2f32( texCoordsMask[ 1 ][ 0 ], texCoordsMask[ 1 ][ 1 ] );
        glVertex3v16( vertCoordsMask[ 1 ][ 0 ], vertCoordsMask[ 1 ][ 1 ], vertCoordsMask[ 1 ][ 2 ] );

        // Bottom-right
        glTexCoord2f32( texCoordsMask[ 2 ][ 0 ], texCoordsMask[ 2 ][ 1 ] );
        glVertex3v16( vertCoordsMask[ 2 ][ 0 ], vertCoordsMask[ 2 ][ 1 ], vertCoordsMask[ 2 ][ 2 ] );

        // Bottom-left
        glTexCoord2f32( texCoordsMask[ 3 ][ 0 ], texCoordsMask[ 3 ][ 1 ] );
        glVertex3v16( vertCoordsMask[ 3 ][ 0 ], vertCoordsMask[ 3 ][ 1 ], vertCoordsMask[ 3 ][ 2 ] );
    glEnd( );

    glPolyFmt( POLY_ALPHA( 31 ) | POLY_CULL_NONE | POLY_DEPTH_TEST_EQUAL );
    glBegin( GL_QUADS );
        /* Even pixels */
        glBindTexture( 0, videoFBTexture );
        glAssignColorTable( videoFBTexture, videoGetEvenTexture( ) );

        for ( int i = 0; i < 512; i += 8 ) {
            // Top-left
            glTexCoord2f32( textureEnds[ i >> 3 ], fZero );
            glVertex3v16( screenStarts[ i >> 3 ], fZero, depthTen );

            // Top-right
            glTexCoord2f32( textureStarts[ i >> 3 ], fZero );
            glVertex3v16( screenEnds[ i >> 3 ], fZero, depthTen );

            // Bottom-right
            glTexCoord2f32( textureStarts[ i >> 3 ], fOne );
            glVertex3v16( screenEnds[ i >> 3 ], screenScaleY, depthTen );

            // Bottom-left
            glTexCoord2f32( textureEnds[ i >> 3 ], fOne );
            glVertex3v16( screenStarts[ i >> 3], screenScaleY, depthTen );
        }
    glEnd( );
    glFlush( 0 );
}

/* --- UI System Definitions --- */

#define MaxKeyboardEvents 256

/* --- UI System Types --- */

typedef struct {
	int macKey;
	int down;
} KeyboardEvent;

typedef struct {
	int macKey;
	int shiftMod;
} MacKeyDef;

/* --- UI System Globals --- */

LOCALVAR UIEmulatorSpeed speedSetting = UI_SEL_EMULATOR_SPEED_UNLIMITED;

LOCALVAR KeyboardEvent keyboardEvents[ MaxKeyboardEvents ];
LOCALVAR int keyboardEventCount = 0;
LOCALVAR int keyboardEventPos = 0;

LOCALVAR const MacKeyDef keyboardConversionTable[ 128 ] = {
	// Non printable chars
	{ -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 },
	{ -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 },
	{ -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 },
	{ -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 },
	{ MKC_Space, 0 },		// Space
	{ MKC_1, 1 },			// !
	{ MKC_SingleQuote, 1 },	// "
	{ MKC_3, 1 },			// #
	{ MKC_4, 1 },			// $
	{ MKC_5, 1 },			// %
	{ MKC_7, 1 },			// &
	{ MKC_SingleQuote, 0 },	// '
	{ MKC_9, 1 },			// (
	{ MKC_0, 1 },			// )
	{ MKC_8, 1 },			// *
	{ MKC_Equal, 1 },		// +
	{ MKC_Comma, 0 },		// ,
	{ MKC_Minus, 0 },		// -
	{ MKC_Period, 0 },		// .
	{ MKC_Slash, 0 },		// /
	{ MKC_0, 0 },			// 0
	{ MKC_1, 0 },			// 1
	{ MKC_2, 0 },			// 2
	{ MKC_3, 0 },			// 3
	{ MKC_4, 0 },			// 4
	{ MKC_5, 0 },			// 5
	{ MKC_6, 0 },			// 6
	{ MKC_7, 0 },			// 7
	{ MKC_8, 0 },			// 8
	{ MKC_9, 0 },			// 9
	{ MKC_SemiColon, 1 },	// :
	{ MKC_SemiColon, 0 },	// ;
	{ MKC_Comma, 1 },		// <
	{ MKC_Equal, 0 },		// =
	{ MKC_Period, 1 },		// >
	{ MKC_Slash, 1 },		// ?

	{ MKC_2, 1 },			// @
	{ MKC_A, 1 },			// A
	{ MKC_B, 1 },			// B
	{ MKC_C, 1 },			// C
	{ MKC_D, 1 },			// D
	{ MKC_E, 1 },			// E
	{ MKC_F, 1 },			// F
	{ MKC_G, 1 },			// G
	{ MKC_H, 1 },			// H
	{ MKC_I, 1 },			// I
	{ MKC_J, 1 },			// J
	{ MKC_K, 1 },			// K
	{ MKC_L, 1 },			// L
	{ MKC_M, 1 },			// M
	{ MKC_N, 1 },			// N
	{ MKC_O, 1 },			// O
	{ MKC_P, 1 },			// P
	{ MKC_Q, 1 },			// Q
	{ MKC_R, 1 },			// R
	{ MKC_S, 1 },			// S
	{ MKC_T, 1 },			// T
	{ MKC_U, 1 },			// U
	{ MKC_V, 1 },			// V
	{ MKC_W, 1 },			// W
	{ MKC_X, 1 },			// X
	{ MKC_Y, 1 },			// Y
	{ MKC_Z, 1 },			// Z
	{ MKC_LeftBracket, 0 },	// [
	{ MKC_BackSlash, 0 },	// backslash
	{ MKC_RightBracket, 0 },// ]
	{ MKC_6, 1 },			// ^
	{ MKC_Minus, 1 },		// _

	{ MKC_Grave, 0 },		// `
	{ MKC_A, 0 },			// a
	{ MKC_B, 0 },			// b
	{ MKC_C, 0 },			// c
	{ MKC_D, 0 },			// d
	{ MKC_E, 0 },			// e
	{ MKC_F, 0 },			// f
	{ MKC_G, 0 },			// g
	{ MKC_H, 0 },			// h
	{ MKC_I, 0 },			// i
	{ MKC_J, 0 },			// j
	{ MKC_K, 0 },			// k
	{ MKC_L, 0 },			// l
	{ MKC_M, 0 },			// m
	{ MKC_N, 0 },			// n
	{ MKC_O, 0 },			// o
	{ MKC_P, 0 },			// p
	{ MKC_Q, 0 },			// q
	{ MKC_R, 0 },			// r
	{ MKC_S, 0 },			// s
	{ MKC_T, 0 },			// t
	{ MKC_U, 0 },			// u
	{ MKC_V, 0 },			// v
	{ MKC_W, 0 },			// w
	{ MKC_X, 0 },			// x
	{ MKC_Y, 0 },			// y
	{ MKC_Z, 0 },			// z
	{ MKC_LeftBracket, 1 },	// {
	{ MKC_BackSlash, 1 },	// |
	{ MKC_RightBracket, 1 },// }
	{ MKC_Grave, 1 },		// ~
	{ MKC_None, 0 }			// del
};

/* --- Keyboard stuff --- */

LOCALPROC keyboardClearEvents( void ) {
	// 0xFF == -1 == not a key
	memset( keyboardEvents, 0xFF, sizeof( keyboardEvents ) );

	keyboardEventCount = 0;
	keyboardEventPos = 0;
}

LOCALPROC keyboardAddKeyEvent0( int macKey, int down ) {
	assert( keyboardEventCount < MaxKeyboardEvents );

	keyboardEvents[ keyboardEventCount ].macKey = macKey;
	keyboardEvents[ keyboardEventCount++ ].down = down;
}

LOCALPROC keyboardAddKeyEvent( int macKey, int shiftMod ) {
	if ( shiftMod )
		keyboardAddKeyEvent0( MKC_Shift, 1 );

	keyboardAddKeyEvent0( macKey, 1 );
	keyboardAddKeyEvent0( macKey, 0 );

	if ( shiftMod )
		keyboardAddKeyEvent0( MKC_Shift, 0 );
}

LOCALPROC keyboardMacKeyFromKeyboard( int c, int* outMacKey, int* outShiftMod ) {
	assert( outMacKey != NULL );
	assert( outShiftMod != NULL );

	*outMacKey = keyboardConversionTable[ c ].macKey;
	*outShiftMod = keyboardConversionTable[ c ].shiftMod;
}

LOCALPROC keyboardMacKeyFromString( const char* str ) {
	int macKey = 0;
	int shiftMod = 0;

	while ( *str ) {
		keyboardMacKeyFromKeyboard( *str, &macKey, &shiftMod );
		keyboardAddKeyEvent( macKey, shiftMod );

		str++;
	}
}

/* --- LVGL Helpers --- */

LOCALFUNC blnr uiIsChecked( lv_obj_t* obj ) {
	return lv_obj_has_state( obj, LV_STATE_CHECKED );
}

LOCALFUNC void uiGetDropdownselected( lv_obj_t* obj, char* buf, size_t bufLen ) {
	lv_dropdown_get_selected_str( obj, buf, bufLen );
}

LOCALPROC uiSetupDefaults( void ) {
	// Mouse tab
	lv_dropdown_set_selected( ui_uiDropdownMouseMode, inputMouseMode );
	lv_slider_set_value( ui_uiSliderAcceleration, inputMouseAcceleration, LV_ANIM_OFF );
	lv_dropdown_set_selected( ui_uiDropdownMouseButton, inputMouseButton );
	lv_dropdown_set_selected( ui_uiDropdownEmuSpeed, UI_SEL_EMULATOR_SPEED_UNLIMITED );
}

/* --- LVGL Events --- */

void uiDisplayTabValueChangedCallback( lv_event_t* e ) {
	if ( uiIsChecked( ui_uiSwitchScale ) )
		videoSetScaled( );
	else
		videoSetUnscaled( );

	videoIsInverted = uiIsChecked( ui_uiSwitchInvert ) ? 1 : 0;
	videoIsSubpixel = uiIsChecked( ui_uiSwitchSubpixelMode ) ? 1 : 0;
	
	videoSubpixelMain = lv_dropdown_get_selected( ui_uiDropdownSubpxTop );
	videoSubpixelSub = lv_dropdown_get_selected( ui_uiDropdownSubpxBottom );

	renderNeedsRefresh = 1;
}

void uiCallbackEmulatorReset( lv_event_t* e ) {
	WantMacReset = trueblnr;
}

void uiCallbackEmulatorExit( lv_event_t* e ) {
	ForceMacOff = trueblnr;
}

void uiEmulatorTabValueChangedCallback( lv_event_t* e ) {
#ifdef MySoundEnabled

	if ( wasInitOK ) {
		if ( uiIsChecked( ui_uiSwitchSound ) )
			HaveSoundOut = trueblnr;
		else
			HaveSoundOut = falseblnr;
#endif

		speedSetting = lv_dropdown_get_selected( ui_uiDropdownEmuSpeed );

		switch ( speedSetting ) {
			case UI_SEL_EMULATOR_SPEED_1X:
				SetSpeedValue( 0 );
				break;
			case UI_SEL_EMULATOR_SPEED_2X:
				SetSpeedValue( 1 );
				break;
			case UI_SEL_EMULATOR_SPEED_UNLIMITED:
				SetSpeedValue( -1 );
				break;
			default:
				speedSetting = UI_SEL_EMULATOR_SPEED_1X;
				SetSpeedValue( 0 );
				break;
		};
	}
}

void uiKeyboardInsertCallback( lv_event_t* e ) {
	// Do nothing if we're already spitting out text
	if ( keyboardEventCount == 0 )
		keyboardMacKeyFromString( lv_textarea_get_text( ui_uiKeyboardTextArea ) );
}

void uiKeyboardBKSPCallback( lv_event_t* e ) {
	if ( keyboardEventCount == 0 )
		keyboardAddKeyEvent( MKC_BackSpace, 0 );
}

void uiKeyboardCommandCallback( lv_event_t* e ) {
	Keyboard_UpdateKeyMap2( MKC_Command, ( uiIsChecked( ui_uiCommandCheckbox ) ) ? trueblnr : falseblnr );
}

void uiKeyboardOptionCallback( lv_event_t* e ) {
	Keyboard_UpdateKeyMap2( MKC_Option, ( uiIsChecked( ui_uiOptionCheckbox ) ) ? trueblnr : falseblnr );
}

void uiKeyboardESCCallback( lv_event_t* e ) {
	if ( keyboardEventCount == 0 )
		keyboardAddKeyEvent( MKC_Escape, 0 );
}

void uiMouseTabValueChangedCallback( lv_event_t* e ) {
	inputMouseMode = lv_dropdown_get_selected( ui_uiDropdownMouseMode );
	inputMouseAcceleration = lv_slider_get_value( ui_uiSliderAcceleration );

	switch ( lv_dropdown_get_selected( ui_uiDropdownMouseButton ) ) {
    	case UI_SEL_MOUSE_BUTTON_LEFT:
			inputMouseButtonBit = KEY_LEFT;
			break;
    	case UI_SEL_MOUSE_BUTTON_RIGHT:
			inputMouseButtonBit = KEY_RIGHT;
			break;
    	case UI_SEL_MOUSE_BUTTON_UP:
			inputMouseButtonBit = KEY_UP;
			break;
    	case UI_SEL_MOUSE_BUTTON_DOWN:
			inputMouseButtonBit = KEY_DOWN;
			break;
    	case UI_SEL_MOUSE_BUTTON_A:
			inputMouseButtonBit = KEY_A;
			break;
    	case UI_SEL_MOUSE_BUTTON_B:
			inputMouseButtonBit = KEY_B;
			break;
    	case UI_SEL_MOUSE_BUTTON_X:
			inputMouseButtonBit = KEY_X;
			break;
    	case UI_SEL_MOUSE_BUTTON_Y:
			inputMouseButtonBit = KEY_Y;
			break;
    	case UI_SEL_MOUSE_BUTTON_L:
			inputMouseButtonBit = KEY_L;
			break;
    	case UI_SEL_MOUSE_BUTTON_R:
			inputMouseButtonBit = KEY_R;
			break;
		default:
			inputMouseButtonBit = 0;
			break;
	};
}

#include "SCRNEMDV.h"

LOCALFUNC void videoCopyFBTexture( void ) {
	if ( screencurrentbuff == NULL )
		return;

	DC_FlushRange( screencurrentbuff, vMacScreenNumBytes );

    vramSetBankA( VRAM_A_LCD );
    dmaCopyWordsAsynch( 3, screencurrentbuff, glGetTexturePointer( videoFBTexture ), vMacScreenNumBytes );
}

LOCALFUNC void videoVBlankIRQ( void ) {
    assert( renderFunc != NULL );

    videoCopyFBTexture( );

	if ( renderNeedsRefresh == 1 )
    	( *renderFunc ) ( );

	renderNeedsRefresh = 0;
    vblankCount++;

	while ( dmaBusy( 3 ) )
	;

    vramSetBankA( VRAM_A_TEXTURE );
}

/* --- video out --- */

#if MayFullScreen && (2 == SDL_MAJOR_VERSION)
LOCALVAR int hOffset;
LOCALVAR int vOffset;
#endif

#if VarFullScreen
LOCALVAR blnr UseFullScreen = (WantInitFullScreen != 0);
#endif

#if EnableMagnify
LOCALVAR blnr UseMagnify = (WantInitMagnify != 0);
#endif

LOCALVAR blnr gBackgroundFlag = falseblnr;
LOCALVAR blnr gTrueBackgroundFlag = falseblnr;
LOCALVAR blnr CurSpeedStopped = trueblnr;

LOCALPROC MyDrawChangesAndClear(void)
{
}

GLOBALOSGLUPROC DoneWithDrawingForTick(void)
{
#if EnableFSMouseMotion
	if (HaveMouseMotion) {
		AutoScrollScreen();
	}
#endif
	MyDrawChangesAndClear();
}

/* --- mouse --- */

/* cursor hiding */

LOCALVAR blnr HaveCursorHidden = falseblnr;
LOCALVAR blnr WantCursorHidden = falseblnr;

LOCALPROC ForceShowCursor(void)
{
}

/* cursor moving */

/*
	OSGLUxxx common:
	When "EnableFSMouseMotion" the platform
	specific code can get relative mouse
	motion, instead of absolute coordinates
	on the emulated screen. It should
	set HaveMouseMotion to true when
	it is doing this (normally when in
	full screen mode.)

	This can usually be implemented by
	hiding the platform specific cursor,
	and then keeping it within a box,
	moving the cursor back to the center whenever
	it leaves the box. This requires the
	ability to move the cursor (in MyMoveMouse).
*/

#ifndef HaveWorkingWarp
#define HaveWorkingWarp 1
#endif

#if EnableMoveMouse && HaveWorkingWarp
LOCALFUNC blnr MyMoveMouse(si4b h, si4b v)
{
	/*
		OSGLUxxx common:
		Move the cursor to the point h, v on the emulated screen.
		If detect that this fails return falseblnr,
			otherwise return trueblnr.
		(On some platforms it is possible to move the curser,
			but there is no way to detect failure.)
	*/

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		h -= ViewHStart;
		v -= ViewVStart;
	}
#endif

	return trueblnr;
}
#endif

/* cursor state */

LOCALPROC MousePositionNotify(int NewMousePosh, int NewMousePosv)
{
	blnr ShouldHaveCursorHidden = trueblnr;

#if EnableMagnify
	if (UseMagnify) {
		NewMousePosh /= MyWindowScale;
		NewMousePosv /= MyWindowScale;
	}
#endif

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		NewMousePosh += ViewHStart;
		NewMousePosv += ViewVStart;
	}
#endif

#if EnableFSMouseMotion
	if (HaveMouseMotion) {
		MyMousePositionSetDelta(NewMousePosh - SavedMouseH,
			NewMousePosv - SavedMouseV);
		SavedMouseH = NewMousePosh;
		SavedMouseV = NewMousePosv;
	} else
#endif
	{
		if (NewMousePosh < 0) {
			NewMousePosh = 0;
			ShouldHaveCursorHidden = falseblnr;
		} else if (NewMousePosh >= vMacScreenWidth) {
			NewMousePosh = vMacScreenWidth - 1;
			ShouldHaveCursorHidden = falseblnr;
		}
		if (NewMousePosv < 0) {
			NewMousePosv = 0;
			ShouldHaveCursorHidden = falseblnr;
		} else if (NewMousePosv >= vMacScreenHeight) {
			NewMousePosv = vMacScreenHeight - 1;
			ShouldHaveCursorHidden = falseblnr;
		}

#if VarFullScreen
		if (UseFullScreen)
#endif
#if MayFullScreen
		{
			ShouldHaveCursorHidden = trueblnr;
		}
#endif

		/* if (ShouldHaveCursorHidden || CurMouseButton) */
		/*
			for a game like arkanoid, would like mouse to still
			move even when outside window in one direction
		*/
		MyMousePositionSet(NewMousePosh, NewMousePosv);
	}

	WantCursorHidden = ShouldHaveCursorHidden;
}

LOCALPROC MousePositionNotifyRelative(int deltah, int deltav)
{
	blnr ShouldHaveCursorHidden = trueblnr;

	MyMousePositionSetDelta(deltah,
		deltav);

	WantCursorHidden = ShouldHaveCursorHidden;
}

LOCALPROC CheckMouseState(void)
{
}

/* --- keyboard input --- */

LOCALPROC DisableKeyRepeat(void)
{
	/*
		OSGLUxxx common:
		If possible and useful, disable keyboard autorepeat.
	*/
}

LOCALPROC RestoreKeyRepeat(void)
{
	/*
		OSGLUxxx common:
		Undo any effects of DisableKeyRepeat.
	*/
}

LOCALPROC ReconnectKeyCodes3(void)
{
}

LOCALPROC DisconnectKeyCodes3(void)
{
	DisconnectKeyCodes2();
	MyMouseButtonSet(falseblnr);
}

/* --- time, date, location --- */

#define dbglog_TimeStuff (0 && dbglog_HAVE)

LOCALVAR ui5b TrueEmulatedTime = 0;
	/*
		OSGLUxxx common:
		The amount of time the program has
		been running, measured in Macintosh
		"ticks". There are 60.14742 ticks per
		second.

		(time when the emulation is
		stopped for more than a few ticks
		should not be counted.)
	*/

#define HaveWorkingTime 1

#define MyInvTimeDivPow 16
#define MyInvTimeDiv (1 << MyInvTimeDivPow)
#define MyInvTimeDivMask (MyInvTimeDiv - 1)
#define MyInvTimeStep 1089590 /* 1000 / 60.14742 * MyInvTimeDiv */

LOCALVAR uint32_t LastTime;

LOCALVAR uint32_t NextIntTime;
LOCALVAR ui5b NextFracTime;

LOCALPROC IncrNextTime(void)
{
	NextFracTime += MyInvTimeStep;
	NextIntTime += (NextFracTime >> MyInvTimeDivPow);
	NextFracTime &= MyInvTimeDivMask;
}

LOCALPROC InitNextTime(void)
{
	NextIntTime = LastTime;
	NextFracTime = 0;
	IncrNextTime();
}

LOCALVAR ui5b NewMacDateInSeconds;

LOCALFUNC blnr UpdateTrueEmulatedTime(void)
{
	/*
		OSGLUxxx common:
		Update TrueEmulatedTime. Needs to convert between how the host
		operating system measures time and Macintosh ticks.
	*/

	uint32_t LatestTime;
	si5b TimeDiff;

	LatestTime = systemGetTicks( );

	if (LatestTime != LastTime) {

		NewMacDateInSeconds = LatestTime / 1000;
			/* no date and time api in SDL */

		LastTime = LatestTime;
		TimeDiff = (LatestTime - NextIntTime);
			/* this should work even when time wraps */
		if (TimeDiff >= 0) {
			if (TimeDiff > 256) {
				/* emulation interrupted, forget it */
				++TrueEmulatedTime;
				InitNextTime();

#if dbglog_TimeStuff
				dbglog_writelnNum("emulation interrupted",
					TrueEmulatedTime);
#endif
			} else {
				do {
					++TrueEmulatedTime;
					IncrNextTime();
					TimeDiff = (LatestTime - NextIntTime);
				} while (TimeDiff >= 0);
			}
			return trueblnr;
		} else {
			if (TimeDiff < -256) {
#if dbglog_TimeStuff
				dbglog_writeln("clock set back");
#endif
				/* clock goofed if ever get here, reset */
				InitNextTime();
			}
		}
	}

	return falseblnr;
}


LOCALFUNC blnr CheckDateTime(void)
{
	/*
		OSGLUxxx common:
		Update CurMacDateInSeconds, the number
		of seconds since midnight January 1, 1904.

		return true if CurMacDateInSeconds is
		different than it was on the last
		call to CheckDateTime.
	*/

	if (CurMacDateInSeconds != NewMacDateInSeconds) {
		CurMacDateInSeconds = NewMacDateInSeconds;
		return trueblnr;
	} else {
		return falseblnr;
	}
}

LOCALPROC StartUpTimeAdjust(void)
{
	/*
		OSGLUxxx common:
		prepare to call UpdateTrueEmulatedTime.

		will be called again when haven't been
		regularly calling UpdateTrueEmulatedTime,
		(such as the emulation has been stopped).
	*/


	LastTime = systemGetTicks( );
	InitNextTime();
}

LOCALFUNC blnr InitLocationDat(void)
{
#if dbglog_OSGInit
	dbglog_writeln("enter InitLocationDat");
#endif

	LastTime = systemGetTicks( );
	InitNextTime();
	NewMacDateInSeconds = LastTime / 1000;
	CurMacDateInSeconds = NewMacDateInSeconds;

	return trueblnr;
}

/* --- sound --- */

#if MySoundEnabled

#define SOUND_SAMPLERATE 22255 /* = round(7833600 * 2 / 704) */

#define kLn2SoundBuffers 4 /* kSoundBuffers must be a power of two */
#define kSoundBuffers (1 << kLn2SoundBuffers)
#define kSoundBuffMask (kSoundBuffers - 1)

#define DesiredMinFilledSoundBuffs 3
	/*
		if too big then sound lags behind emulation.
		if too small then sound will have pauses.
	*/

#define kLnOneBuffLen 8
#define kLnAllBuffLen (kLn2SoundBuffers + kLnOneBuffLen)
#define kOneBuffLen (1UL << kLnOneBuffLen)
#define kAllBuffLen (1UL << kLnAllBuffLen)
#define kLnOneBuffSz (kLnOneBuffLen + kLn2SoundSampSz - 3)
#define kLnAllBuffSz (kLnAllBuffLen + kLn2SoundSampSz - 3)
#define kOneBuffSz (1UL << kLnOneBuffSz)
#define kAllBuffSz (1UL << kLnAllBuffSz)
#define kOneBuffMask (kOneBuffLen - 1)
#define kAllBuffMask (kAllBuffLen - 1)
#define dbhBufferSize (kAllBuffSz + kOneBuffSz)

#define dbglog_SoundStuff (0 && dbglog_HAVE)
#define dbglog_SoundBuffStats (0 && dbglog_HAVE)

LOCALVAR tpSoundSamp TheSoundBuffer = nullpr;
volatile static ui4b ThePlayOffset;
volatile static ui4b TheFillOffset;
volatile static ui4b MinFilledSoundBuffs;
#if dbglog_SoundBuffStats
volatile LOCALVAR ui4b MaxFilledSoundBuffs;
#endif
volatile LOCALVAR ui4b TheWriteOffset;

LOCALPROC MySound_Init0(void)
{
	ThePlayOffset = 0;
	TheFillOffset = 0;
	TheWriteOffset = 0;
}

LOCALPROC MySound_Start0(void)
{
	/* Reset variables */
	MinFilledSoundBuffs = kSoundBuffers + 1;
#if dbglog_SoundBuffStats
	MaxFilledSoundBuffs = 0;
#endif
}

GLOBALOSGLUFUNC tpSoundSamp MySound_BeginWrite(ui4r n, ui4r *actL)
{
	ui4b ToFillLen = kAllBuffLen - (TheWriteOffset - ThePlayOffset);
	ui4b WriteBuffContig =
		kOneBuffLen - (TheWriteOffset & kOneBuffMask);

	if (WriteBuffContig < n) {
		n = WriteBuffContig;
	}
	if (ToFillLen < n) {
		/* overwrite previous buffer */
#if dbglog_SoundStuff
		dbglog_writeln("sound buffer over flow");
#endif
		TheWriteOffset -= kOneBuffLen;
	}

	*actL = n;
	return TheSoundBuffer + (TheWriteOffset & kAllBuffMask);
}

#if 4 == kLn2SoundSampSz
LOCALPROC ConvertSoundBlockToNative(tpSoundSamp p)
{
	int i;

	for (i = kOneBuffLen; --i >= 0; ) {
		*p++ -= 0x8000;
	}
}
#else
#if 0
#define ConvertSoundBlockToNative(p)
#else
LOCALPROC ConvertSoundBlockToNative( tpSoundSamp Ptr ) {
	int i = 0;

	for ( i = 0; i < kOneBuffLen; i++ ) {
		*Ptr++-= 0x80;
	}
}
#endif
#endif

LOCALPROC MySound_WroteABlock(void)
{
#if (4 == kLn2SoundSampSz || 1)
	ui4b PrevWriteOffset = TheWriteOffset - kOneBuffLen;
	tpSoundSamp p = TheSoundBuffer + (PrevWriteOffset & kAllBuffMask);
#endif

#if dbglog_SoundStuff
	dbglog_writeln("enter MySound_WroteABlock");
#endif

	ConvertSoundBlockToNative( ( tpSoundSamp ) p );

	TheFillOffset = TheWriteOffset;

#if dbglog_SoundBuffStats
	{
		ui4b ToPlayLen = TheFillOffset
			- ThePlayOffset;
		ui4b ToPlayBuffs = ToPlayLen >> kLnOneBuffLen;

		if (ToPlayBuffs > MaxFilledSoundBuffs) {
			MaxFilledSoundBuffs = ToPlayBuffs;
		}
	}
#endif
}

LOCALFUNC blnr MySound_EndWrite0(ui4r actL)
{
	blnr v;

	TheWriteOffset += actL;

	if (0 != (TheWriteOffset & kOneBuffMask)) {
		v = falseblnr;
	} else {
		/* just finished a block */

		MySound_WroteABlock();

		v = trueblnr;
	}

	return v;
}

LOCALPROC MySound_SecondNotify0(void)
{
	if (MinFilledSoundBuffs <= kSoundBuffers) {
		if (MinFilledSoundBuffs > DesiredMinFilledSoundBuffs) {
#if dbglog_SoundStuff
			dbglog_writeln("MinFilledSoundBuffs too high");
#endif
			IncrNextTime();
		} else if (MinFilledSoundBuffs < DesiredMinFilledSoundBuffs) {
#if dbglog_SoundStuff
			dbglog_writeln("MinFilledSoundBuffs too low");
#endif
			++TrueEmulatedTime;
		}
#if dbglog_SoundBuffStats
		dbglog_writelnNum("MinFilledSoundBuffs",
			MinFilledSoundBuffs);
		dbglog_writelnNum("MaxFilledSoundBuffs",
			MaxFilledSoundBuffs);
		MaxFilledSoundBuffs = 0;
#endif
		MinFilledSoundBuffs = kSoundBuffers + 1;
	}
}

typedef ui4r trSoundTemp;

#define kCenterTempSound 0x8000

#define AudioStepVal 0x0040

#if 3 == kLn2SoundSampSz
#define ConvertTempSoundSampleFromNative(v) ((v) << 8)
#elif 4 == kLn2SoundSampSz
#define ConvertTempSoundSampleFromNative(v) ((v) + kCenterSound)
#else
#error "unsupported kLn2SoundSampSz"
#endif

#if 3 == kLn2SoundSampSz
#define ConvertTempSoundSampleToNative(v) ((v) >> 8)
#elif 4 == kLn2SoundSampSz
#define ConvertTempSoundSampleToNative(v) ((v) - kCenterSound)
#else
#error "unsupported kLn2SoundSampSz"
#endif

LOCALPROC SoundRampTo(trSoundTemp *last_val, trSoundTemp dst_val,
	tpSoundSamp *stream, int *len)
{
	trSoundTemp diff;
	tpSoundSamp p = *stream;
	int n = *len;
	trSoundTemp v1 = *last_val;

	while ((v1 != dst_val) && (0 != n)) {
		if (v1 > dst_val) {
			diff = v1 - dst_val;
			if (diff > AudioStepVal) {
				v1 -= AudioStepVal;
			} else {
				v1 = dst_val;
			}
		} else {
			diff = dst_val - v1;
			if (diff > AudioStepVal) {
				v1 += AudioStepVal;
			} else {
				v1 = dst_val;
			}
		}

		--n;
		*p++ = ConvertTempSoundSampleToNative(v1);
	}

	*stream = p;
	*len = n;
	*last_val = v1;
}

struct MySoundR {
	tpSoundSamp fTheSoundBuffer;
	volatile ui4b (*fPlayOffset);
	volatile ui4b (*fFillOffset);
	volatile ui4b (*fMinFilledSoundBuffs);

	volatile trSoundTemp lastv;

	blnr wantplaying;
	blnr HaveStartedPlaying;
};
typedef struct MySoundR MySoundR;

static void my_audio_callback(void *udata, uint8_t *stream, int len)
{
	ui4b ToPlayLen;
	ui4b FilledSoundBuffs;
	int i;
	MySoundR *datp = (MySoundR *)udata;
	tpSoundSamp CurSoundBuffer = datp->fTheSoundBuffer;
	ui4b CurPlayOffset = *datp->fPlayOffset;
	trSoundTemp v0 = datp->lastv;
	trSoundTemp v1 = v0;
	tpSoundSamp dst = (tpSoundSamp)stream;

#if kLn2SoundSampSz > 3
	len >>= (kLn2SoundSampSz - 3);
#endif

#if dbglog_SoundStuff
	dbglog_writeln("Enter my_audio_callback");
	dbglog_writelnNum("len", len);
#endif

label_retry:
	ToPlayLen = *datp->fFillOffset - CurPlayOffset;
	FilledSoundBuffs = ToPlayLen >> kLnOneBuffLen;

	if (! datp->wantplaying) {
#if dbglog_SoundStuff
		dbglog_writeln("playing end transistion");
#endif

		SoundRampTo(&v1, kCenterTempSound, &dst, &len);

		ToPlayLen = 0;
	} else if (! datp->HaveStartedPlaying) {
#if dbglog_SoundStuff
		dbglog_writeln("playing start block");
#endif

		if ((ToPlayLen >> kLnOneBuffLen) < 8) {
			ToPlayLen = 0;
		} else {
			tpSoundSamp p = datp->fTheSoundBuffer
				+ (CurPlayOffset & kAllBuffMask);
			trSoundTemp v2 = ConvertTempSoundSampleFromNative(*p);

#if dbglog_SoundStuff
			dbglog_writeln("have enough samples to start");
#endif

			SoundRampTo(&v1, v2, &dst, &len);

			if (v1 == v2) {
#if dbglog_SoundStuff
				dbglog_writeln("finished start transition");
#endif

				datp->HaveStartedPlaying = trueblnr;
			}
		}
	}

	if (0 == len) {
		/* done */

		if (FilledSoundBuffs < *datp->fMinFilledSoundBuffs) {
			*datp->fMinFilledSoundBuffs = FilledSoundBuffs;
		}
	} else if (0 == ToPlayLen) {

#if dbglog_SoundStuff
		dbglog_writeln("under run");
#endif
		for (i = 0; i < len; ++i) {
			*dst++ = ConvertTempSoundSampleToNative(v1);
		}
		*datp->fMinFilledSoundBuffs = 0;
	} else {
		ui4b PlayBuffContig = kAllBuffLen
			- (CurPlayOffset & kAllBuffMask);
		tpSoundSamp p = CurSoundBuffer
			+ (CurPlayOffset & kAllBuffMask);

		if (ToPlayLen > PlayBuffContig) {
			ToPlayLen = PlayBuffContig;
		}
		if (ToPlayLen > len) {
			ToPlayLen = len;
		}

		for (i = 0; i < ToPlayLen; ++i) {
			*dst++ = *p++;
		}
		v1 = ConvertTempSoundSampleFromNative(p[-1]);

		CurPlayOffset += ToPlayLen;
		len -= ToPlayLen;

		*datp->fPlayOffset = CurPlayOffset;

		goto label_retry;
	}

	datp->lastv = v1;
}

volatile LOCALVAR MySoundR cur_audio;

#define SampleCount 512

LOCALPROC MySound_Stop(void)
{
#if dbglog_SoundStuff
	dbglog_writeln("enter MySound_Stop");
#endif

	if (cur_audio.wantplaying && HaveSoundOut) {
		ui4r retry_limit = 50; /* half of a second */

		cur_audio.wantplaying = falseblnr;

label_retry:
		if (kCenterTempSound == cur_audio.lastv) {
#if dbglog_SoundStuff
			dbglog_writeln("reached kCenterTempSound");
#endif

			/* done */
		} else if (0 == --retry_limit) {
#if dbglog_SoundStuff
			dbglog_writeln("retry limit reached");
#endif
			/* done */
		} else
		{
			/*
				give time back, particularly important
				if got here on a suspend event.
			*/

#if dbglog_SoundStuff
			dbglog_writeln("busy, so sleep");
#endif

			//(void) SDL_Delay(10);

			goto label_retry;
		}

		//SDL_PauseAudio(1);
		//ndspChnSetPaused( 0, true );
		#warning TARA: Pause streaming here
	}

#if dbglog_SoundStuff
	dbglog_writeln("leave MySound_Stop");
#endif
}

LOCALVAR void* SoundBuffer = NULL;

LOCALPROC MySound_Start(void)
{
	if ((! cur_audio.wantplaying) && HaveSoundOut) {
		MySound_Start0();
		cur_audio.lastv = kCenterTempSound;
		cur_audio.HaveStartedPlaying = falseblnr;
		cur_audio.wantplaying = trueblnr;
	}
}

LOCALPROC MySound_UnInit(void)
{
	mmStreamClose( );
}

static mm_word audioCallback( mm_word length, mm_addr dest, mm_stream_formats format ) {
	DC_FlushRange( TheSoundBuffer, dbhBufferSize );
	my_audio_callback( &cur_audio, dest, length );

	return length;
}

LOCALFUNC blnr MySound_Init(void)
{
	mm_ds_system mmDS = {
		.mod_count = 0,
		.samp_count = 0,
		.mem_bank = 0,
		.fifo_channel = FIFO_MAXMOD
	};
	mm_stream mmStream = {
		.sampling_rate = SOUND_SAMPLERATE,
		.buffer_length = SampleCount,
		.callback = audioCallback,
		.format = MM_STREAM_8BIT_MONO,
		.timer = MM_TIMER3,
		.manual = false
	};

	MySound_Init0();

	cur_audio.fTheSoundBuffer = TheSoundBuffer;
	cur_audio.fPlayOffset = &ThePlayOffset;
	cur_audio.fFillOffset = &TheFillOffset;
	cur_audio.fMinFilledSoundBuffs = &MinFilledSoundBuffs;
	cur_audio.wantplaying = falseblnr;

	HaveSoundOut = trueblnr;
	lv_obj_add_state( ui_uiSwitchSound, LV_STATE_CHECKED );

	assert( mmInit( &mmDS ) == true );
	mmStreamOpen( &mmStream );

	MySound_Start( );

	return trueblnr; /* keep going, even if no sound */
}

GLOBALOSGLUPROC MySound_EndWrite(ui4r actL)
{
	if (MySound_EndWrite0(actL)) {
	}
}

LOCALPROC MySound_SecondNotify(void)
{
	if (HaveSoundOut) {
		MySound_SecondNotify0();
	}
}

#endif

/* --- basic dialogs --- */

LOCALPROC CheckSavedMacMsg(void)
{
	/*
		OSGLUxxx common:
		This is currently only used in the
		rare case where there is a message
		still pending as the program quits.
	*/

	if (nullpr != SavedBriefMsg) {
		char briefMsg0[ClStrMaxLength + 1];
		char longMsg0[ClStrMaxLength + 1];

		NativeStrFromCStr(briefMsg0, SavedBriefMsg);
		NativeStrFromCStr(longMsg0, SavedLongMsg);

		consoleSelect( &videoMacMsgConsole );
		consoleClear( );

		videoWriteMessage( briefMsg0 );
		videoWriteMessage( "\n\n" );
		videoWriteMessage( longMsg0 );

		videoWriteMessage( "\n\n\nPress START to dismiss.\n" );
		videoWriteDebug( "" );

		bgShow( videoMacMsgBG );
		videoMacMsgBGOn = 1;

		SavedBriefMsg = nullpr;
	}
}

/* --- clipboard --- */

#if IncludeHostTextClipExchange
LOCALFUNC uimr MacRoman2UniCodeSize(ui3b *s, uimr L)
{
	uimr i;
	ui3r x;
	uimr n;
	uimr v = 0;

	for (i = 0; i < L; ++i) {
		x = *s++;
		if (x < 128) {
			n = 1;
		} else {
			switch (x) {
				case 0x80: n = 2; break;
					/* LATIN CAPITAL LETTER A WITH DIAERESIS */
				case 0x81: n = 2; break;
					/* LATIN CAPITAL LETTER A WITH RING ABOVE */
				case 0x82: n = 2; break;
					/* LATIN CAPITAL LETTER C WITH CEDILLA */
				case 0x83: n = 2; break;
					/* LATIN CAPITAL LETTER E WITH ACUTE */
				case 0x84: n = 2; break;
					/* LATIN CAPITAL LETTER N WITH TILDE */
				case 0x85: n = 2; break;
					/* LATIN CAPITAL LETTER O WITH DIAERESIS */
				case 0x86: n = 2; break;
					/* LATIN CAPITAL LETTER U WITH DIAERESIS */
				case 0x87: n = 2; break;
					/* LATIN SMALL LETTER A WITH ACUTE */
				case 0x88: n = 2; break;
					/* LATIN SMALL LETTER A WITH GRAVE */
				case 0x89: n = 2; break;
					/* LATIN SMALL LETTER A WITH CIRCUMFLEX */
				case 0x8A: n = 2; break;
					/* LATIN SMALL LETTER A WITH DIAERESIS */
				case 0x8B: n = 2; break;
					/* LATIN SMALL LETTER A WITH TILDE */
				case 0x8C: n = 2; break;
					/* LATIN SMALL LETTER A WITH RING ABOVE */
				case 0x8D: n = 2; break;
					/* LATIN SMALL LETTER C WITH CEDILLA */
				case 0x8E: n = 2; break;
					/* LATIN SMALL LETTER E WITH ACUTE */
				case 0x8F: n = 2; break;
					/* LATIN SMALL LETTER E WITH GRAVE */
				case 0x90: n = 2; break;
					/* LATIN SMALL LETTER E WITH CIRCUMFLEX */
				case 0x91: n = 2; break;
					/* LATIN SMALL LETTER E WITH DIAERESIS */
				case 0x92: n = 2; break;
					/* LATIN SMALL LETTER I WITH ACUTE */
				case 0x93: n = 2; break;
					/* LATIN SMALL LETTER I WITH GRAVE */
				case 0x94: n = 2; break;
					/* LATIN SMALL LETTER I WITH CIRCUMFLEX */
				case 0x95: n = 2; break;
					/* LATIN SMALL LETTER I WITH DIAERESIS */
				case 0x96: n = 2; break;
					/* LATIN SMALL LETTER N WITH TILDE */
				case 0x97: n = 2; break;
					/* LATIN SMALL LETTER O WITH ACUTE */
				case 0x98: n = 2; break;
					/* LATIN SMALL LETTER O WITH GRAVE */
				case 0x99: n = 2; break;
					/* LATIN SMALL LETTER O WITH CIRCUMFLEX */
				case 0x9A: n = 2; break;
					/* LATIN SMALL LETTER O WITH DIAERESIS */
				case 0x9B: n = 2; break;
					/* LATIN SMALL LETTER O WITH TILDE */
				case 0x9C: n = 2; break;
					/* LATIN SMALL LETTER U WITH ACUTE */
				case 0x9D: n = 2; break;
					/* LATIN SMALL LETTER U WITH GRAVE */
				case 0x9E: n = 2; break;
					/* LATIN SMALL LETTER U WITH CIRCUMFLEX */
				case 0x9F: n = 2; break;
					/* LATIN SMALL LETTER U WITH DIAERESIS */
				case 0xA0: n = 3; break;
					/* DAGGER */
				case 0xA1: n = 2; break;
					/* DEGREE SIGN */
				case 0xA2: n = 2; break;
					/* CENT SIGN */
				case 0xA3: n = 2; break;
					/* POUND SIGN */
				case 0xA4: n = 2; break;
					/* SECTION SIGN */
				case 0xA5: n = 3; break;
					/* BULLET */
				case 0xA6: n = 2; break;
					/* PILCROW SIGN */
				case 0xA7: n = 2; break;
					/* LATIN SMALL LETTER SHARP S */
				case 0xA8: n = 2; break;
					/* REGISTERED SIGN */
				case 0xA9: n = 2; break;
					/* COPYRIGHT SIGN */
				case 0xAA: n = 3; break;
					/* TRADE MARK SIGN */
				case 0xAB: n = 2; break;
					/* ACUTE ACCENT */
				case 0xAC: n = 2; break;
					/* DIAERESIS */
				case 0xAD: n = 3; break;
					/* NOT EQUAL TO */
				case 0xAE: n = 2; break;
					/* LATIN CAPITAL LETTER AE */
				case 0xAF: n = 2; break;
					/* LATIN CAPITAL LETTER O WITH STROKE */
				case 0xB0: n = 3; break;
					/* INFINITY */
				case 0xB1: n = 2; break;
					/* PLUS-MINUS SIGN */
				case 0xB2: n = 3; break;
					/* LESS-THAN OR EQUAL TO */
				case 0xB3: n = 3; break;
					/* GREATER-THAN OR EQUAL TO */
				case 0xB4: n = 2; break;
					/* YEN SIGN */
				case 0xB5: n = 2; break;
					/* MICRO SIGN */
				case 0xB6: n = 3; break;
					/* PARTIAL DIFFERENTIAL */
				case 0xB7: n = 3; break;
					/* N-ARY SUMMATION */
				case 0xB8: n = 3; break;
					/* N-ARY PRODUCT */
				case 0xB9: n = 2; break;
					/* GREEK SMALL LETTER PI */
				case 0xBA: n = 3; break;
					/* INTEGRAL */
				case 0xBB: n = 2; break;
					/* FEMININE ORDINAL INDICATOR */
				case 0xBC: n = 2; break;
					/* MASCULINE ORDINAL INDICATOR */
				case 0xBD: n = 2; break;
					/* GREEK CAPITAL LETTER OMEGA */
				case 0xBE: n = 2; break;
					/* LATIN SMALL LETTER AE */
				case 0xBF: n = 2; break;
					/* LATIN SMALL LETTER O WITH STROKE */
				case 0xC0: n = 2; break;
					/* INVERTED QUESTION MARK */
				case 0xC1: n = 2; break;
					/* INVERTED EXCLAMATION MARK */
				case 0xC2: n = 2; break;
					/* NOT SIGN */
				case 0xC3: n = 3; break;
					/* SQUARE ROOT */
				case 0xC4: n = 2; break;
					/* LATIN SMALL LETTER F WITH HOOK */
				case 0xC5: n = 3; break;
					/* ALMOST EQUAL TO */
				case 0xC6: n = 3; break;
					/* INCREMENT */
				case 0xC7: n = 2; break;
					/* LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
				case 0xC8: n = 2; break;
					/* RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
				case 0xC9: n = 3; break;
					/* HORIZONTAL ELLIPSIS */
				case 0xCA: n = 2; break;
					/* NO-BREAK SPACE */
				case 0xCB: n = 2; break;
					/* LATIN CAPITAL LETTER A WITH GRAVE */
				case 0xCC: n = 2; break;
					/* LATIN CAPITAL LETTER A WITH TILDE */
				case 0xCD: n = 2; break;
					/* LATIN CAPITAL LETTER O WITH TILDE */
				case 0xCE: n = 2; break;
					/* LATIN CAPITAL LIGATURE OE */
				case 0xCF: n = 2; break;
					/* LATIN SMALL LIGATURE OE */
				case 0xD0: n = 3; break;
					/* EN DASH */
				case 0xD1: n = 3; break;
					/* EM DASH */
				case 0xD2: n = 3; break;
					/* LEFT DOUBLE QUOTATION MARK */
				case 0xD3: n = 3; break;
					/* RIGHT DOUBLE QUOTATION MARK */
				case 0xD4: n = 3; break;
					/* LEFT SINGLE QUOTATION MARK */
				case 0xD5: n = 3; break;
					/* RIGHT SINGLE QUOTATION MARK */
				case 0xD6: n = 2; break;
					/* DIVISION SIGN */
				case 0xD7: n = 3; break;
					/* LOZENGE */
				case 0xD8: n = 2; break;
					/* LATIN SMALL LETTER Y WITH DIAERESIS */
				case 0xD9: n = 2; break;
					/* LATIN CAPITAL LETTER Y WITH DIAERESIS */
				case 0xDA: n = 3; break;
					/* FRACTION SLASH */
				case 0xDB: n = 3; break;
					/* EURO SIGN */
				case 0xDC: n = 3; break;
					/* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
				case 0xDD: n = 3; break;
					/* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
				case 0xDE: n = 3; break;
					/* LATIN SMALL LIGATURE FI */
				case 0xDF: n = 3; break;
					/* LATIN SMALL LIGATURE FL */
				case 0xE0: n = 3; break;
					/* DOUBLE DAGGER */
				case 0xE1: n = 2; break;
					/* MIDDLE DOT */
				case 0xE2: n = 3; break;
					/* SINGLE LOW-9 QUOTATION MARK */
				case 0xE3: n = 3; break;
					/* DOUBLE LOW-9 QUOTATION MARK */
				case 0xE4: n = 3; break;
					/* PER MILLE SIGN */
				case 0xE5: n = 2; break;
					/* LATIN CAPITAL LETTER A WITH CIRCUMFLEX */
				case 0xE6: n = 2; break;
					/* LATIN CAPITAL LETTER E WITH CIRCUMFLEX */
				case 0xE7: n = 2; break;
					/* LATIN CAPITAL LETTER A WITH ACUTE */
				case 0xE8: n = 2; break;
					/* LATIN CAPITAL LETTER E WITH DIAERESIS */
				case 0xE9: n = 2; break;
					/* LATIN CAPITAL LETTER E WITH GRAVE */
				case 0xEA: n = 2; break;
					/* LATIN CAPITAL LETTER I WITH ACUTE */
				case 0xEB: n = 2; break;
					/* LATIN CAPITAL LETTER I WITH CIRCUMFLEX */
				case 0xEC: n = 2; break;
					/* LATIN CAPITAL LETTER I WITH DIAERESIS */
				case 0xED: n = 2; break;
					/* LATIN CAPITAL LETTER I WITH GRAVE */
				case 0xEE: n = 2; break;
					/* LATIN CAPITAL LETTER O WITH ACUTE */
				case 0xEF: n = 2; break;
					/* LATIN CAPITAL LETTER O WITH CIRCUMFLEX */
				case 0xF0: n = 3; break;
					/* Apple logo */
				case 0xF1: n = 2; break;
					/* LATIN CAPITAL LETTER O WITH GRAVE */
				case 0xF2: n = 2; break;
					/* LATIN CAPITAL LETTER U WITH ACUTE */
				case 0xF3: n = 2; break;
					/* LATIN CAPITAL LETTER U WITH CIRCUMFLEX */
				case 0xF4: n = 2; break;
					/* LATIN CAPITAL LETTER U WITH GRAVE */
				case 0xF5: n = 2; break;
					/* LATIN SMALL LETTER DOTLESS I */
				case 0xF6: n = 2; break;
					/* MODIFIER LETTER CIRCUMFLEX ACCENT */
				case 0xF7: n = 2; break;
					/* SMALL TILDE */
				case 0xF8: n = 2; break;
					/* MACRON */
				case 0xF9: n = 2; break;
					/* BREVE */
				case 0xFA: n = 2; break;
					/* DOT ABOVE */
				case 0xFB: n = 2; break;
					/* RING ABOVE */
				case 0xFC: n = 2; break;
					/* CEDILLA */
				case 0xFD: n = 2; break;
					/* DOUBLE ACUTE ACCENT */
				case 0xFE: n = 2; break;
					/* OGONEK */
				case 0xFF: n = 2; break;
					/* CARON */
				default: n = 1; break;
					/* shouldn't get here */
			}
		}
		v += n;
	}

	return v;
}
#endif

#if IncludeHostTextClipExchange
LOCALPROC MacRoman2UniCodeData(ui3b *s, uimr L, char *t)
{
	uimr i;
	ui3r x;

	for (i = 0; i < L; ++i) {
		x = *s++;
		if (x < 128) {
			*t++ = x;
		} else {
			switch (x) {
				case 0x80: *t++ = 0xC3; *t++ = 0x84; break;
					/* LATIN CAPITAL LETTER A WITH DIAERESIS */
				case 0x81: *t++ = 0xC3; *t++ = 0x85; break;
					/* LATIN CAPITAL LETTER A WITH RING ABOVE */
				case 0x82: *t++ = 0xC3; *t++ = 0x87; break;
					/* LATIN CAPITAL LETTER C WITH CEDILLA */
				case 0x83: *t++ = 0xC3; *t++ = 0x89; break;
					/* LATIN CAPITAL LETTER E WITH ACUTE */
				case 0x84: *t++ = 0xC3; *t++ = 0x91; break;
					/* LATIN CAPITAL LETTER N WITH TILDE */
				case 0x85: *t++ = 0xC3; *t++ = 0x96; break;
					/* LATIN CAPITAL LETTER O WITH DIAERESIS */
				case 0x86: *t++ = 0xC3; *t++ = 0x9C; break;
					/* LATIN CAPITAL LETTER U WITH DIAERESIS */
				case 0x87: *t++ = 0xC3; *t++ = 0xA1; break;
					/* LATIN SMALL LETTER A WITH ACUTE */
				case 0x88: *t++ = 0xC3; *t++ = 0xA0; break;
					/* LATIN SMALL LETTER A WITH GRAVE */
				case 0x89: *t++ = 0xC3; *t++ = 0xA2; break;
					/* LATIN SMALL LETTER A WITH CIRCUMFLEX */
				case 0x8A: *t++ = 0xC3; *t++ = 0xA4; break;
					/* LATIN SMALL LETTER A WITH DIAERESIS */
				case 0x8B: *t++ = 0xC3; *t++ = 0xA3; break;
					/* LATIN SMALL LETTER A WITH TILDE */
				case 0x8C: *t++ = 0xC3; *t++ = 0xA5; break;
					/* LATIN SMALL LETTER A WITH RING ABOVE */
				case 0x8D: *t++ = 0xC3; *t++ = 0xA7; break;
					/* LATIN SMALL LETTER C WITH CEDILLA */
				case 0x8E: *t++ = 0xC3; *t++ = 0xA9; break;
					/* LATIN SMALL LETTER E WITH ACUTE */
				case 0x8F: *t++ = 0xC3; *t++ = 0xA8; break;
					/* LATIN SMALL LETTER E WITH GRAVE */
				case 0x90: *t++ = 0xC3; *t++ = 0xAA; break;
					/* LATIN SMALL LETTER E WITH CIRCUMFLEX */
				case 0x91: *t++ = 0xC3; *t++ = 0xAB; break;
					/* LATIN SMALL LETTER E WITH DIAERESIS */
				case 0x92: *t++ = 0xC3; *t++ = 0xAD; break;
					/* LATIN SMALL LETTER I WITH ACUTE */
				case 0x93: *t++ = 0xC3; *t++ = 0xAC; break;
					/* LATIN SMALL LETTER I WITH GRAVE */
				case 0x94: *t++ = 0xC3; *t++ = 0xAE; break;
					/* LATIN SMALL LETTER I WITH CIRCUMFLEX */
				case 0x95: *t++ = 0xC3; *t++ = 0xAF; break;
					/* LATIN SMALL LETTER I WITH DIAERESIS */
				case 0x96: *t++ = 0xC3; *t++ = 0xB1; break;
					/* LATIN SMALL LETTER N WITH TILDE */
				case 0x97: *t++ = 0xC3; *t++ = 0xB3; break;
					/* LATIN SMALL LETTER O WITH ACUTE */
				case 0x98: *t++ = 0xC3; *t++ = 0xB2; break;
					/* LATIN SMALL LETTER O WITH GRAVE */
				case 0x99: *t++ = 0xC3; *t++ = 0xB4; break;
					/* LATIN SMALL LETTER O WITH CIRCUMFLEX */
				case 0x9A: *t++ = 0xC3; *t++ = 0xB6; break;
					/* LATIN SMALL LETTER O WITH DIAERESIS */
				case 0x9B: *t++ = 0xC3; *t++ = 0xB5; break;
					/* LATIN SMALL LETTER O WITH TILDE */
				case 0x9C: *t++ = 0xC3; *t++ = 0xBA; break;
					/* LATIN SMALL LETTER U WITH ACUTE */
				case 0x9D: *t++ = 0xC3; *t++ = 0xB9; break;
					/* LATIN SMALL LETTER U WITH GRAVE */
				case 0x9E: *t++ = 0xC3; *t++ = 0xBB; break;
					/* LATIN SMALL LETTER U WITH CIRCUMFLEX */
				case 0x9F: *t++ = 0xC3; *t++ = 0xBC; break;
					/* LATIN SMALL LETTER U WITH DIAERESIS */
				case 0xA0: *t++ = 0xE2; *t++ = 0x80; *t++ = 0xA0; break;
					/* DAGGER */
				case 0xA1: *t++ = 0xC2; *t++ = 0xB0; break;
					/* DEGREE SIGN */
				case 0xA2: *t++ = 0xC2; *t++ = 0xA2; break;
					/* CENT SIGN */
				case 0xA3: *t++ = 0xC2; *t++ = 0xA3; break;
					/* POUND SIGN */
				case 0xA4: *t++ = 0xC2; *t++ = 0xA7; break;
					/* SECTION SIGN */
				case 0xA5: *t++ = 0xE2; *t++ = 0x80; *t++ = 0xA2; break;
					/* BULLET */
				case 0xA6: *t++ = 0xC2; *t++ = 0xB6; break;
					/* PILCROW SIGN */
				case 0xA7: *t++ = 0xC3; *t++ = 0x9F; break;
					/* LATIN SMALL LETTER SHARP S */
				case 0xA8: *t++ = 0xC2; *t++ = 0xAE; break;
					/* REGISTERED SIGN */
				case 0xA9: *t++ = 0xC2; *t++ = 0xA9; break;
					/* COPYRIGHT SIGN */
				case 0xAA: *t++ = 0xE2; *t++ = 0x84; *t++ = 0xA2; break;
					/* TRADE MARK SIGN */
				case 0xAB: *t++ = 0xC2; *t++ = 0xB4; break;
					/* ACUTE ACCENT */
				case 0xAC: *t++ = 0xC2; *t++ = 0xA8; break;
					/* DIAERESIS */
				case 0xAD: *t++ = 0xE2; *t++ = 0x89; *t++ = 0xA0; break;
					/* NOT EQUAL TO */
				case 0xAE: *t++ = 0xC3; *t++ = 0x86; break;
					/* LATIN CAPITAL LETTER AE */
				case 0xAF: *t++ = 0xC3; *t++ = 0x98; break;
					/* LATIN CAPITAL LETTER O WITH STROKE */
				case 0xB0: *t++ = 0xE2; *t++ = 0x88; *t++ = 0x9E; break;
					/* INFINITY */
				case 0xB1: *t++ = 0xC2; *t++ = 0xB1; break;
					/* PLUS-MINUS SIGN */
				case 0xB2: *t++ = 0xE2; *t++ = 0x89; *t++ = 0xA4; break;
					/* LESS-THAN OR EQUAL TO */
				case 0xB3: *t++ = 0xE2; *t++ = 0x89; *t++ = 0xA5; break;
					/* GREATER-THAN OR EQUAL TO */
				case 0xB4: *t++ = 0xC2; *t++ = 0xA5; break;
					/* YEN SIGN */
				case 0xB5: *t++ = 0xC2; *t++ = 0xB5; break;
					/* MICRO SIGN */
				case 0xB6: *t++ = 0xE2; *t++ = 0x88; *t++ = 0x82; break;
					/* PARTIAL DIFFERENTIAL */
				case 0xB7: *t++ = 0xE2; *t++ = 0x88; *t++ = 0x91; break;
					/* N-ARY SUMMATION */
				case 0xB8: *t++ = 0xE2; *t++ = 0x88; *t++ = 0x8F; break;
					/* N-ARY PRODUCT */
				case 0xB9: *t++ = 0xCF; *t++ = 0x80; break;
					/* GREEK SMALL LETTER PI */
				case 0xBA: *t++ = 0xE2; *t++ = 0x88; *t++ = 0xAB; break;
					/* INTEGRAL */
				case 0xBB: *t++ = 0xC2; *t++ = 0xAA; break;
					/* FEMININE ORDINAL INDICATOR */
				case 0xBC: *t++ = 0xC2; *t++ = 0xBA; break;
					/* MASCULINE ORDINAL INDICATOR */
				case 0xBD: *t++ = 0xCE; *t++ = 0xA9; break;
					/* GREEK CAPITAL LETTER OMEGA */
				case 0xBE: *t++ = 0xC3; *t++ = 0xA6; break;
					/* LATIN SMALL LETTER AE */
				case 0xBF: *t++ = 0xC3; *t++ = 0xB8; break;
					/* LATIN SMALL LETTER O WITH STROKE */
				case 0xC0: *t++ = 0xC2; *t++ = 0xBF; break;
					/* INVERTED QUESTION MARK */
				case 0xC1: *t++ = 0xC2; *t++ = 0xA1; break;
					/* INVERTED EXCLAMATION MARK */
				case 0xC2: *t++ = 0xC2; *t++ = 0xAC; break;
					/* NOT SIGN */
				case 0xC3: *t++ = 0xE2; *t++ = 0x88; *t++ = 0x9A; break;
					/* SQUARE ROOT */
				case 0xC4: *t++ = 0xC6; *t++ = 0x92; break;
					/* LATIN SMALL LETTER F WITH HOOK */
				case 0xC5: *t++ = 0xE2; *t++ = 0x89; *t++ = 0x88; break;
					/* ALMOST EQUAL TO */
				case 0xC6: *t++ = 0xE2; *t++ = 0x88; *t++ = 0x86; break;
					/* INCREMENT */
				case 0xC7: *t++ = 0xC2; *t++ = 0xAB; break;
					/* LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
				case 0xC8: *t++ = 0xC2; *t++ = 0xBB; break;
					/* RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
				case 0xC9: *t++ = 0xE2; *t++ = 0x80; *t++ = 0xA6; break;
					/* HORIZONTAL ELLIPSIS */
				case 0xCA: *t++ = 0xC2; *t++ = 0xA0; break;
					/* NO-BREAK SPACE */
				case 0xCB: *t++ = 0xC3; *t++ = 0x80; break;
					/* LATIN CAPITAL LETTER A WITH GRAVE */
				case 0xCC: *t++ = 0xC3; *t++ = 0x83; break;
					/* LATIN CAPITAL LETTER A WITH TILDE */
				case 0xCD: *t++ = 0xC3; *t++ = 0x95; break;
					/* LATIN CAPITAL LETTER O WITH TILDE */
				case 0xCE: *t++ = 0xC5; *t++ = 0x92; break;
					/* LATIN CAPITAL LIGATURE OE */
				case 0xCF: *t++ = 0xC5; *t++ = 0x93; break;
					/* LATIN SMALL LIGATURE OE */
				case 0xD0: *t++ = 0xE2; *t++ = 0x80; *t++ = 0x93; break;
					/* EN DASH */
				case 0xD1: *t++ = 0xE2; *t++ = 0x80; *t++ = 0x94; break;
					/* EM DASH */
				case 0xD2: *t++ = 0xE2; *t++ = 0x80; *t++ = 0x9C; break;
					/* LEFT DOUBLE QUOTATION MARK */
				case 0xD3: *t++ = 0xE2; *t++ = 0x80; *t++ = 0x9D; break;
					/* RIGHT DOUBLE QUOTATION MARK */
				case 0xD4: *t++ = 0xE2; *t++ = 0x80; *t++ = 0x98; break;
					/* LEFT SINGLE QUOTATION MARK */
				case 0xD5: *t++ = 0xE2; *t++ = 0x80; *t++ = 0x99; break;
					/* RIGHT SINGLE QUOTATION MARK */
				case 0xD6: *t++ = 0xC3; *t++ = 0xB7; break;
					/* DIVISION SIGN */
				case 0xD7: *t++ = 0xE2; *t++ = 0x97; *t++ = 0x8A; break;
					/* LOZENGE */
				case 0xD8: *t++ = 0xC3; *t++ = 0xBF; break;
					/* LATIN SMALL LETTER Y WITH DIAERESIS */
				case 0xD9: *t++ = 0xC5; *t++ = 0xB8; break;
					/* LATIN CAPITAL LETTER Y WITH DIAERESIS */
				case 0xDA: *t++ = 0xE2; *t++ = 0x81; *t++ = 0x84; break;
					/* FRACTION SLASH */
				case 0xDB: *t++ = 0xE2; *t++ = 0x82; *t++ = 0xAC; break;
					/* EURO SIGN */
				case 0xDC: *t++ = 0xE2; *t++ = 0x80; *t++ = 0xB9; break;
					/* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
				case 0xDD: *t++ = 0xE2; *t++ = 0x80; *t++ = 0xBA; break;
					/* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
				case 0xDE: *t++ = 0xEF; *t++ = 0xAC; *t++ = 0x81; break;
					/* LATIN SMALL LIGATURE FI */
				case 0xDF: *t++ = 0xEF; *t++ = 0xAC; *t++ = 0x82; break;
					/* LATIN SMALL LIGATURE FL */
				case 0xE0: *t++ = 0xE2; *t++ = 0x80; *t++ = 0xA1; break;
					/* DOUBLE DAGGER */
				case 0xE1: *t++ = 0xC2; *t++ = 0xB7; break;
					/* MIDDLE DOT */
				case 0xE2: *t++ = 0xE2; *t++ = 0x80; *t++ = 0x9A; break;
					/* SINGLE LOW-9 QUOTATION MARK */
				case 0xE3: *t++ = 0xE2; *t++ = 0x80; *t++ = 0x9E; break;
					/* DOUBLE LOW-9 QUOTATION MARK */
				case 0xE4: *t++ = 0xE2; *t++ = 0x80; *t++ = 0xB0; break;
					/* PER MILLE SIGN */
				case 0xE5: *t++ = 0xC3; *t++ = 0x82; break;
					/* LATIN CAPITAL LETTER A WITH CIRCUMFLEX */
				case 0xE6: *t++ = 0xC3; *t++ = 0x8A; break;
					/* LATIN CAPITAL LETTER E WITH CIRCUMFLEX */
				case 0xE7: *t++ = 0xC3; *t++ = 0x81; break;
					/* LATIN CAPITAL LETTER A WITH ACUTE */
				case 0xE8: *t++ = 0xC3; *t++ = 0x8B; break;
					/* LATIN CAPITAL LETTER E WITH DIAERESIS */
				case 0xE9: *t++ = 0xC3; *t++ = 0x88; break;
					/* LATIN CAPITAL LETTER E WITH GRAVE */
				case 0xEA: *t++ = 0xC3; *t++ = 0x8D; break;
					/* LATIN CAPITAL LETTER I WITH ACUTE */
				case 0xEB: *t++ = 0xC3; *t++ = 0x8E; break;
					/* LATIN CAPITAL LETTER I WITH CIRCUMFLEX */
				case 0xEC: *t++ = 0xC3; *t++ = 0x8F; break;
					/* LATIN CAPITAL LETTER I WITH DIAERESIS */
				case 0xED: *t++ = 0xC3; *t++ = 0x8C; break;
					/* LATIN CAPITAL LETTER I WITH GRAVE */
				case 0xEE: *t++ = 0xC3; *t++ = 0x93; break;
					/* LATIN CAPITAL LETTER O WITH ACUTE */
				case 0xEF: *t++ = 0xC3; *t++ = 0x94; break;
					/* LATIN CAPITAL LETTER O WITH CIRCUMFLEX */
				case 0xF0: *t++ = 0xEF; *t++ = 0xA3; *t++ = 0xBF; break;
					/* Apple logo */
				case 0xF1: *t++ = 0xC3; *t++ = 0x92; break;
					/* LATIN CAPITAL LETTER O WITH GRAVE */
				case 0xF2: *t++ = 0xC3; *t++ = 0x9A; break;
					/* LATIN CAPITAL LETTER U WITH ACUTE */
				case 0xF3: *t++ = 0xC3; *t++ = 0x9B; break;
					/* LATIN CAPITAL LETTER U WITH CIRCUMFLEX */
				case 0xF4: *t++ = 0xC3; *t++ = 0x99; break;
					/* LATIN CAPITAL LETTER U WITH GRAVE */
				case 0xF5: *t++ = 0xC4; *t++ = 0xB1; break;
					/* LATIN SMALL LETTER DOTLESS I */
				case 0xF6: *t++ = 0xCB; *t++ = 0x86; break;
					/* MODIFIER LETTER CIRCUMFLEX ACCENT */
				case 0xF7: *t++ = 0xCB; *t++ = 0x9C; break;
					/* SMALL TILDE */
				case 0xF8: *t++ = 0xC2; *t++ = 0xAF; break;
					/* MACRON */
				case 0xF9: *t++ = 0xCB; *t++ = 0x98; break;
					/* BREVE */
				case 0xFA: *t++ = 0xCB; *t++ = 0x99; break;
					/* DOT ABOVE */
				case 0xFB: *t++ = 0xCB; *t++ = 0x9A; break;
					/* RING ABOVE */
				case 0xFC: *t++ = 0xC2; *t++ = 0xB8; break;
					/* CEDILLA */
				case 0xFD: *t++ = 0xCB; *t++ = 0x9D; break;
					/* DOUBLE ACUTE ACCENT */
				case 0xFE: *t++ = 0xCB; *t++ = 0x9B; break;
					/* OGONEK */
				case 0xFF: *t++ = 0xCB; *t++ = 0x87; break;
					/* CARON */
				default: *t++ = '?'; break;
					/* shouldn't get here */
			}
		}
	}
}
#endif

#if IncludeHostTextClipExchange
GLOBALOSGLUFUNC tMacErr HTCEexport(tPbuf i)
{
	/*
		OSGLUxxx common:
		PBuf i is an array of Macintosh
		style characters. (using the
		MacRoman character set.)

		Should export this Buffer to the
		native clipboard, performing character
		set translation, and eof character translation
		as needed.

		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	tMacErr err;
	char *p;
	ui3p s = PbufDat[i];
	uimr L = PbufSize[i];
	uimr sz = MacRoman2UniCodeSize(s, L);

	if (NULL == (p = malloc(sz + 1))) {
		err = mnvm_miscErr;
	} else {
		MacRoman2UniCodeData(s, L, p);
		p[sz] = 0;

		// TARA:
		// TODO

		// if (0 != SDL_SetClipboardText(p)) {
		// 	err = mnvm_miscErr;
		// } else {
		// 	err = mnvm_noErr;
		// }

		free(p);
	}

	return err;
}
#endif

#if IncludeHostTextClipExchange
LOCALFUNC tMacErr UniCodeStrLength(char *s, uimr *r)
{
	tMacErr err;
	ui3r t;
	ui3r t2;
	char *p = s;
	uimr L = 0;

label_retry:
	if (0 == (t = *p++)) {
		err = mnvm_noErr;
		/* done */
	} else
	if (0 == (0x80 & t)) {
		/* One-byte code */
		L += 1;
		goto label_retry;
	} else
	if (0 == (0x40 & t)) {
		/* continuation code, error */
		err = mnvm_miscErr;
	} else
	if (0 == (t2 = *p++)) {
		err = mnvm_miscErr;
	} else
	if (0x80 != (0xC0 & t2)) {
		/* not a continuation code, error */
		err = mnvm_miscErr;
	} else
	if (0 == (0x20 & t)) {
		/* two bytes */
		L += 2;
		goto label_retry;
	} else
	if (0 == (t2 = *p++)) {
		err = mnvm_miscErr;
	} else
	if (0x80 != (0xC0 & t2)) {
		/* not a continuation code, error */
		err = mnvm_miscErr;
	} else
	if (0 == (0x10 & t)) {
		/* three bytes */
		L += 3;
		goto label_retry;
	} else
	if (0 == (t2 = *p++)) {
		err = mnvm_miscErr;
	} else
	if (0x80 != (0xC0 & t2)) {
		/* not a continuation code, error */
		err = mnvm_miscErr;
	} else
	if (0 == (0x08 & t)) {
		/* four bytes */
		L += 5;
		goto label_retry;
	} else
	{
		err = mnvm_miscErr;
		/* longer code not supported yet */
	}

	*r = L;
	return err;
}
#endif

#if IncludeHostTextClipExchange
LOCALFUNC ui3r UniCodePoint2MacRoman(ui5r x)
{
/*
	adapted from
		http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/ROMAN.TXT
*/
	ui3r y;

	if (x < 128) {
		y = x;
	} else {
		switch (x) {
			case 0x00C4: y = 0x80; break;
				/* LATIN CAPITAL LETTER A WITH DIAERESIS */
			case 0x00C5: y = 0x81; break;
				/* LATIN CAPITAL LETTER A WITH RING ABOVE */
			case 0x00C7: y = 0x82; break;
				/* LATIN CAPITAL LETTER C WITH CEDILLA */
			case 0x00C9: y = 0x83; break;
				/* LATIN CAPITAL LETTER E WITH ACUTE */
			case 0x00D1: y = 0x84; break;
				/* LATIN CAPITAL LETTER N WITH TILDE */
			case 0x00D6: y = 0x85; break;
				/* LATIN CAPITAL LETTER O WITH DIAERESIS */
			case 0x00DC: y = 0x86; break;
				/* LATIN CAPITAL LETTER U WITH DIAERESIS */
			case 0x00E1: y = 0x87; break;
				/* LATIN SMALL LETTER A WITH ACUTE */
			case 0x00E0: y = 0x88; break;
				/* LATIN SMALL LETTER A WITH GRAVE */
			case 0x00E2: y = 0x89; break;
				/* LATIN SMALL LETTER A WITH CIRCUMFLEX */
			case 0x00E4: y = 0x8A; break;
				/* LATIN SMALL LETTER A WITH DIAERESIS */
			case 0x00E3: y = 0x8B; break;
				/* LATIN SMALL LETTER A WITH TILDE */
			case 0x00E5: y = 0x8C; break;
				/* LATIN SMALL LETTER A WITH RING ABOVE */
			case 0x00E7: y = 0x8D; break;
				/* LATIN SMALL LETTER C WITH CEDILLA */
			case 0x00E9: y = 0x8E; break;
				/* LATIN SMALL LETTER E WITH ACUTE */
			case 0x00E8: y = 0x8F; break;
				/* LATIN SMALL LETTER E WITH GRAVE */
			case 0x00EA: y = 0x90; break;
				/* LATIN SMALL LETTER E WITH CIRCUMFLEX */
			case 0x00EB: y = 0x91; break;
				/* LATIN SMALL LETTER E WITH DIAERESIS */
			case 0x00ED: y = 0x92; break;
				/* LATIN SMALL LETTER I WITH ACUTE */
			case 0x00EC: y = 0x93; break;
				/* LATIN SMALL LETTER I WITH GRAVE */
			case 0x00EE: y = 0x94; break;
				/* LATIN SMALL LETTER I WITH CIRCUMFLEX */
			case 0x00EF: y = 0x95; break;
				/* LATIN SMALL LETTER I WITH DIAERESIS */
			case 0x00F1: y = 0x96; break;
				/* LATIN SMALL LETTER N WITH TILDE */
			case 0x00F3: y = 0x97; break;
				/* LATIN SMALL LETTER O WITH ACUTE */
			case 0x00F2: y = 0x98; break;
				/* LATIN SMALL LETTER O WITH GRAVE */
			case 0x00F4: y = 0x99; break;
				/* LATIN SMALL LETTER O WITH CIRCUMFLEX */
			case 0x00F6: y = 0x9A; break;
				/* LATIN SMALL LETTER O WITH DIAERESIS */
			case 0x00F5: y = 0x9B; break;
				/* LATIN SMALL LETTER O WITH TILDE */
			case 0x00FA: y = 0x9C; break;
				/* LATIN SMALL LETTER U WITH ACUTE */
			case 0x00F9: y = 0x9D; break;
				/* LATIN SMALL LETTER U WITH GRAVE */
			case 0x00FB: y = 0x9E; break;
				/* LATIN SMALL LETTER U WITH CIRCUMFLEX */
			case 0x00FC: y = 0x9F; break;
				/* LATIN SMALL LETTER U WITH DIAERESIS */
			case 0x2020: y = 0xA0; break;
				/* DAGGER */
			case 0x00B0: y = 0xA1; break;
				/* DEGREE SIGN */
			case 0x00A2: y = 0xA2; break;
				/* CENT SIGN */
			case 0x00A3: y = 0xA3; break;
				/* POUND SIGN */
			case 0x00A7: y = 0xA4; break;
				/* SECTION SIGN */
			case 0x2022: y = 0xA5; break;
				/* BULLET */
			case 0x00B6: y = 0xA6; break;
				/* PILCROW SIGN */
			case 0x00DF: y = 0xA7; break;
				/* LATIN SMALL LETTER SHARP S */
			case 0x00AE: y = 0xA8; break;
				/* REGISTERED SIGN */
			case 0x00A9: y = 0xA9; break;
				/* COPYRIGHT SIGN */
			case 0x2122: y = 0xAA; break;
				/* TRADE MARK SIGN */
			case 0x00B4: y = 0xAB; break;
				/* ACUTE ACCENT */
			case 0x00A8: y = 0xAC; break;
				/* DIAERESIS */
			case 0x2260: y = 0xAD; break;
				/* NOT EQUAL TO */
			case 0x00C6: y = 0xAE; break;
				/* LATIN CAPITAL LETTER AE */
			case 0x00D8: y = 0xAF; break;
				/* LATIN CAPITAL LETTER O WITH STROKE */
			case 0x221E: y = 0xB0; break;
				/* INFINITY */
			case 0x00B1: y = 0xB1; break;
				/* PLUS-MINUS SIGN */
			case 0x2264: y = 0xB2; break;
				/* LESS-THAN OR EQUAL TO */
			case 0x2265: y = 0xB3; break;
				/* GREATER-THAN OR EQUAL TO */
			case 0x00A5: y = 0xB4; break;
				/* YEN SIGN */
			case 0x00B5: y = 0xB5; break;
				/* MICRO SIGN */
			case 0x2202: y = 0xB6; break;
				/* PARTIAL DIFFERENTIAL */
			case 0x2211: y = 0xB7; break;
				/* N-ARY SUMMATION */
			case 0x220F: y = 0xB8; break;
				/* N-ARY PRODUCT */
			case 0x03C0: y = 0xB9; break;
				/* GREEK SMALL LETTER PI */
			case 0x222B: y = 0xBA; break;
				/* INTEGRAL */
			case 0x00AA: y = 0xBB; break;
				/* FEMININE ORDINAL INDICATOR */
			case 0x00BA: y = 0xBC; break;
				/* MASCULINE ORDINAL INDICATOR */
			case 0x03A9: y = 0xBD; break;
				/* GREEK CAPITAL LETTER OMEGA */
			case 0x00E6: y = 0xBE; break;
				/* LATIN SMALL LETTER AE */
			case 0x00F8: y = 0xBF; break;
				/* LATIN SMALL LETTER O WITH STROKE */
			case 0x00BF: y = 0xC0; break;
				/* INVERTED QUESTION MARK */
			case 0x00A1: y = 0xC1; break;
				/* INVERTED EXCLAMATION MARK */
			case 0x00AC: y = 0xC2; break;
				/* NOT SIGN */
			case 0x221A: y = 0xC3; break;
				/* SQUARE ROOT */
			case 0x0192: y = 0xC4; break;
				/* LATIN SMALL LETTER F WITH HOOK */
			case 0x2248: y = 0xC5; break;
				/* ALMOST EQUAL TO */
			case 0x2206: y = 0xC6; break;
				/* INCREMENT */
			case 0x00AB: y = 0xC7; break;
				/* LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
			case 0x00BB: y = 0xC8; break;
				/* RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
			case 0x2026: y = 0xC9; break;
				/* HORIZONTAL ELLIPSIS */
			case 0x00A0: y = 0xCA; break;
				/* NO-BREAK SPACE */
			case 0x00C0: y = 0xCB; break;
				/* LATIN CAPITAL LETTER A WITH GRAVE */
			case 0x00C3: y = 0xCC; break;
				/* LATIN CAPITAL LETTER A WITH TILDE */
			case 0x00D5: y = 0xCD; break;
				/* LATIN CAPITAL LETTER O WITH TILDE */
			case 0x0152: y = 0xCE; break;
				/* LATIN CAPITAL LIGATURE OE */
			case 0x0153: y = 0xCF; break;
				/* LATIN SMALL LIGATURE OE */
			case 0x2013: y = 0xD0; break;
				/* EN DASH */
			case 0x2014: y = 0xD1; break;
				/* EM DASH */
			case 0x201C: y = 0xD2; break;
				/* LEFT DOUBLE QUOTATION MARK */
			case 0x201D: y = 0xD3; break;
				/* RIGHT DOUBLE QUOTATION MARK */
			case 0x2018: y = 0xD4; break;
				/* LEFT SINGLE QUOTATION MARK */
			case 0x2019: y = 0xD5; break;
				/* RIGHT SINGLE QUOTATION MARK */
			case 0x00F7: y = 0xD6; break;
				/* DIVISION SIGN */
			case 0x25CA: y = 0xD7; break;
				/* LOZENGE */
			case 0x00FF: y = 0xD8; break;
				/* LATIN SMALL LETTER Y WITH DIAERESIS */
			case 0x0178: y = 0xD9; break;
				/* LATIN CAPITAL LETTER Y WITH DIAERESIS */
			case 0x2044: y = 0xDA; break;
				/* FRACTION SLASH */
			case 0x20AC: y = 0xDB; break;
				/* EURO SIGN */
			case 0x2039: y = 0xDC; break;
				/* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
			case 0x203A: y = 0xDD; break;
				/* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
			case 0xFB01: y = 0xDE; break;
				/* LATIN SMALL LIGATURE FI */
			case 0xFB02: y = 0xDF; break;
				/* LATIN SMALL LIGATURE FL */
			case 0x2021: y = 0xE0; break;
				/* DOUBLE DAGGER */
			case 0x00B7: y = 0xE1; break;
				/* MIDDLE DOT */
			case 0x201A: y = 0xE2; break;
				/* SINGLE LOW-9 QUOTATION MARK */
			case 0x201E: y = 0xE3; break;
				/* DOUBLE LOW-9 QUOTATION MARK */
			case 0x2030: y = 0xE4; break;
				/* PER MILLE SIGN */
			case 0x00C2: y = 0xE5; break;
				/* LATIN CAPITAL LETTER A WITH CIRCUMFLEX */
			case 0x00CA: y = 0xE6; break;
				/* LATIN CAPITAL LETTER E WITH CIRCUMFLEX */
			case 0x00C1: y = 0xE7; break;
				/* LATIN CAPITAL LETTER A WITH ACUTE */
			case 0x00CB: y = 0xE8; break;
				/* LATIN CAPITAL LETTER E WITH DIAERESIS */
			case 0x00C8: y = 0xE9; break;
				/* LATIN CAPITAL LETTER E WITH GRAVE */
			case 0x00CD: y = 0xEA; break;
				/* LATIN CAPITAL LETTER I WITH ACUTE */
			case 0x00CE: y = 0xEB; break;
				/* LATIN CAPITAL LETTER I WITH CIRCUMFLEX */
			case 0x00CF: y = 0xEC; break;
				/* LATIN CAPITAL LETTER I WITH DIAERESIS */
			case 0x00CC: y = 0xED; break;
				/* LATIN CAPITAL LETTER I WITH GRAVE */
			case 0x00D3: y = 0xEE; break;
				/* LATIN CAPITAL LETTER O WITH ACUTE */
			case 0x00D4: y = 0xEF; break;
				/* LATIN CAPITAL LETTER O WITH CIRCUMFLEX */
			case 0xF8FF: y = 0xF0; break;
				/* Apple logo */
			case 0x00D2: y = 0xF1; break;
				/* LATIN CAPITAL LETTER O WITH GRAVE */
			case 0x00DA: y = 0xF2; break;
				/* LATIN CAPITAL LETTER U WITH ACUTE */
			case 0x00DB: y = 0xF3; break;
				/* LATIN CAPITAL LETTER U WITH CIRCUMFLEX */
			case 0x00D9: y = 0xF4; break;
				/* LATIN CAPITAL LETTER U WITH GRAVE */
			case 0x0131: y = 0xF5; break;
				/* LATIN SMALL LETTER DOTLESS I */
			case 0x02C6: y = 0xF6; break;
				/* MODIFIER LETTER CIRCUMFLEX ACCENT */
			case 0x02DC: y = 0xF7; break;
				/* SMALL TILDE */
			case 0x00AF: y = 0xF8; break;
				/* MACRON */
			case 0x02D8: y = 0xF9; break;
				/* BREVE */
			case 0x02D9: y = 0xFA; break;
				/* DOT ABOVE */
			case 0x02DA: y = 0xFB; break;
				/* RING ABOVE */
			case 0x00B8: y = 0xFC; break;
				/* CEDILLA */
			case 0x02DD: y = 0xFD; break;
				/* DOUBLE ACUTE ACCENT */
			case 0x02DB: y = 0xFE; break;
				/* OGONEK */
			case 0x02C7: y = 0xFF; break;
				/* CARON */
			default: y = '?'; break;
				/* unrecognized */
		}
	}

	return y;
}
#endif

#if IncludeHostTextClipExchange
LOCALPROC UniCodeStr2MacRoman(char *s, char *r)
{
	tMacErr err;
	ui3r t;
	ui3r t2;
	ui3r t3;
	ui3r t4;
	ui5r v;
	char *p = s;
	char *q = r;

label_retry:
	if (0 == (t = *p++)) {
		err = mnvm_noErr;
		/* done */
	} else
	if (0 == (0x80 & t)) {
		*q++ = t;
		goto label_retry;
	} else
	if (0 == (0x40 & t)) {
		/* continuation code, error */
		err = mnvm_miscErr;
	} else
	if (0 == (t2 = *p++)) {
		err = mnvm_miscErr;
	} else
	if (0x80 != (0xC0 & t2)) {
		/* not a continuation code, error */
		err = mnvm_miscErr;
	} else
	if (0 == (0x20 & t)) {
		/* two bytes */
		v = t & 0x1F;
		v = (v << 6) | (t2 & 0x3F);
		*q++ = UniCodePoint2MacRoman(v);
		goto label_retry;
	} else
	if (0 == (t3 = *p++)) {
		err = mnvm_miscErr;
	} else
	if (0x80 != (0xC0 & t3)) {
		/* not a continuation code, error */
		err = mnvm_miscErr;
	} else
	if (0 == (0x10 & t)) {
		/* three bytes */
		v = t & 0x0F;
		v = (v << 6) | (t3 & 0x3F);
		v = (v << 6) | (t2 & 0x3F);
		*q++ = UniCodePoint2MacRoman(v);
		goto label_retry;
	} else
	if (0 == (t4 = *p++)) {
		err = mnvm_miscErr;
	} else
	if (0x80 != (0xC0 & t4)) {
		/* not a continuation code, error */
		err = mnvm_miscErr;
	} else
	if (0 == (0x08 & t)) {
		/* four bytes */
		v = t & 0x07;
		v = (v << 6) | (t4 & 0x3F);
		v = (v << 6) | (t3 & 0x3F);
		v = (v << 6) | (t2 & 0x3F);
		*q++ = UniCodePoint2MacRoman(v);
		goto label_retry;
	} else
	{
		err = mnvm_miscErr;
		/* longer code not supported yet */
	}
}
#endif

#if IncludeHostTextClipExchange
GLOBALOSGLUFUNC tMacErr HTCEimport(tPbuf *r)
{
	/*
		OSGLUxxx common:
		Import the native clipboard as text,
		and convert it to Macintosh format,
		in a Pbuf.

		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/

	tMacErr err;
	uimr L;
	char *s = NULL;
	tPbuf t = NotAPbuf;

	// TARA:
	// TODO
	if (NULL == (s = NULL/*SDL_GetClipboardText()*/)) {
		err = mnvm_miscErr;
	} else
	if (mnvm_noErr != (err =
		UniCodeStrLength(s, &L)))
	{
		/* fail */
	} else
	if (mnvm_noErr != (err =
		PbufNew(L, &t)))
	{
		/* fail */
	} else
	{
		err = mnvm_noErr;

		UniCodeStr2MacRoman(s, PbufDat[t]);
		*r = t;
		t = NotAPbuf;
	}

	if (NotAPbuf != t) {
		PbufDispose(t);
	}

	return err;
}
#endif

/* --- event handling for main window --- */

#define UseMotionEvents 1

#if UseMotionEvents
LOCALVAR blnr CaughtMouse = falseblnr;
#endif

LOCALPROC HandleTheEvent( void ) {
}

/* --- main window creation and disposal --- */

LOCALVAR int my_argc;
LOCALVAR char **my_argv;

LOCALFUNC blnr Screen_Init(void)
{
#if dbglog_OSGInit
	dbglog_writeln("enter Screen_Init");
#endif

	InitKeyCodes();

	return trueblnr;
}

#if MayFullScreen
LOCALVAR blnr GrabMachine = falseblnr;
#endif

#if MayFullScreen
LOCALPROC GrabTheMachine(void)
{
}
#endif

#if MayFullScreen
LOCALPROC UngrabMachine(void)
{
}
#endif

#if EnableFSMouseMotion && HaveWorkingWarp
LOCALPROC MyMouseConstrain(void)
{
	si4b shiftdh;
	si4b shiftdv;

	if (SavedMouseH < ViewHStart + (ViewHSize / 4)) {
		shiftdh = ViewHSize / 2;
	} else if (SavedMouseH > ViewHStart + ViewHSize - (ViewHSize / 4)) {
		shiftdh = - ViewHSize / 2;
	} else {
		shiftdh = 0;
	}
	if (SavedMouseV < ViewVStart + (ViewVSize / 4)) {
		shiftdv = ViewVSize / 2;
	} else if (SavedMouseV > ViewVStart + ViewVSize - (ViewVSize / 4)) {
		shiftdv = - ViewVSize / 2;
	} else {
		shiftdv = 0;
	}
	if ((shiftdh != 0) || (shiftdv != 0)) {
		SavedMouseH += shiftdh;
		SavedMouseV += shiftdv;
		if (! MyMoveMouse(SavedMouseH, SavedMouseV)) {
			HaveMouseMotion = falseblnr;
		}
	}
}
#endif

LOCALFUNC blnr CreateMainWindow(void)
{
	return trueblnr;
}

LOCALPROC CloseMainWindow(void)
{
}

#if EnableRecreateW
LOCALFUNC blnr ReCreateMainWindow(void)
{
	ForceShowCursor(); /* hide/show cursor api is per window */

#if MayFullScreen
	if (GrabMachine) {
		GrabMachine = falseblnr;
		UngrabMachine();
	}
#endif

#if EnableMagnify
	UseMagnify = WantMagnify;
#endif
#if VarFullScreen
	UseFullScreen = WantFullScreen;
#endif

	(void) CreateMainWindow();

	if (HaveCursorHidden) {
		(void) MyMoveMouse(CurMouseH, CurMouseV);
	}

	return trueblnr;
}
#endif

LOCALPROC ZapWinStateVars(void)
{
}

#if VarFullScreen
LOCALPROC ToggleWantFullScreen(void)
{
	WantFullScreen = ! WantFullScreen;
}
#endif

/* --- SavedTasks --- */

LOCALPROC LeaveBackground(void)
{
	ReconnectKeyCodes3();
	DisableKeyRepeat();
}

LOCALPROC EnterBackground(void)
{
	RestoreKeyRepeat();
	DisconnectKeyCodes3();

	ForceShowCursor();
}

LOCALPROC LeaveSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Start();
#endif

	StartUpTimeAdjust();
}

LOCALPROC EnterSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Stop();
#endif
}

LOCALPROC CheckForSavedTasks(void)
{
	if (MyEvtQNeedRecover) {
		MyEvtQNeedRecover = falseblnr;

		/* attempt cleanup, MyEvtQNeedRecover may get set again */
		MyEvtQTryRecoverFromFull();
	}

#if EnableFSMouseMotion && HaveWorkingWarp
	if (HaveMouseMotion) {
		MyMouseConstrain();
	}
#endif

	if (RequestMacOff) {
		RequestMacOff = falseblnr;
		if (AnyDiskInserted()) {
			MacMsgOverride(kStrQuitWarningTitle,
				kStrQuitWarningMessage);
		} else {
			ForceMacOff = trueblnr;
		}
	}

	if (ForceMacOff) {
		return;
	}

	if (gTrueBackgroundFlag != gBackgroundFlag) {
		gBackgroundFlag = gTrueBackgroundFlag;
		if (gTrueBackgroundFlag) {
			EnterBackground();
		} else {
			LeaveBackground();
		}
	}

	if (CurSpeedStopped != (SpeedStopped ||
		(gBackgroundFlag && ! RunInBackground
#if EnableAutoSlow && 0
			&& (QuietSubTicks >= 4092)
#endif
		)))
	{
		CurSpeedStopped = ! CurSpeedStopped;
		if (CurSpeedStopped) {
			EnterSpeedStopped();
		} else {
			LeaveSpeedStopped();
		}
	}

	if ((nullpr != SavedBriefMsg) & ! MacMsgDisplayed) {
		MacMsgDisplayOn();
	}

#if EnableRecreateW
	if (0
#if EnableMagnify
		|| (UseMagnify != WantMagnify)
#endif
#if VarFullScreen
		|| (UseFullScreen != WantFullScreen)
#endif
		)
	{
		(void) ReCreateMainWindow();
	}
#endif

#if MayFullScreen
	if (GrabMachine != (
#if VarFullScreen
		UseFullScreen &&
#endif
		! (gTrueBackgroundFlag || CurSpeedStopped)))
	{
		GrabMachine = ! GrabMachine;
		if (GrabMachine) {
			GrabTheMachine();
		} else {
			UngrabMachine();
		}
	}
#endif

	if (NeedWholeScreenDraw) {
		NeedWholeScreenDraw = falseblnr;
		ScreenChangedAll();
	}

#if NeedRequestIthDisk
	if (0 != RequestIthDisk) {
		Sony_InsertIth(RequestIthDisk);
		RequestIthDisk = 0;
	}
#endif

	if (HaveCursorHidden != (WantCursorHidden
		&& ! (gTrueBackgroundFlag || CurSpeedStopped)))
	{
		HaveCursorHidden = ! HaveCursorHidden;
	}
}

/* --- command line parsing --- */

LOCALFUNC blnr ScanCommandLine(void)
{
	char *pa;
	int i = 1;

#if dbglog_OSGInit
	dbglog_writeln("enter ScanCommandLine"); /*^*/
#endif

label_retry:
	if (i < my_argc) {
		pa = my_argv[i++];
		if ('-' == pa[0]) {
			if ((0 == strcmp(pa, "--rom"))
				|| (0 == strcmp(pa, "-r")))
			{
				if (i < my_argc) {
					rom_path = my_argv[i++];
					goto label_retry;
				}
			} else
			if (0 == strcmp(pa, "-n"))
			{
				if (i < my_argc) {
					n_arg = my_argv[i++];
					goto label_retry;
				}
			} else
			if (0 == strcmp(pa, "-d"))
			{
				if (i < my_argc) {
					d_arg = my_argv[i++];
					goto label_retry;
				}
			} else
			if (('p' == pa[1]) && ('s' == pa[2]) && ('n' == pa[3]))
			{
				/* seen in OS X. ignore */
				goto label_retry;
			} else
			{
				MacMsg(kStrBadArgTitle, kStrBadArgMessage, falseblnr);
#if dbglog_HAVE
				dbglog_writeln("bad command line argument");
				dbglog_writeln(pa);
#endif
			}
		} else {
			(void) Sony_Insert1(pa, falseblnr);
			goto label_retry;
		}
	}

	return trueblnr;
}

/* --- main program flow --- */

LOCALPROC WaitForTheNextEvent(void)
{
}

#define HandleDSToMacKey( dskey, mackey ) do { \
	if ( inputKeysDown & dskey ) \
		Keyboard_UpdateKeyMap2( mackey, trueblnr ); \
	if ( inputKeysUp & dskey ) \
		Keyboard_UpdateKeyMap2( mackey, falseblnr ); \
} while ( 0 )

LOCALPROC inputMouseMove_DPAD( void ) {
	int dx = 0;
	int dy = 0;

	dx = ( inputKeysHeld & KEY_LEFT ) ? -1 : dx;
	dx = ( inputKeysHeld & KEY_RIGHT ) ? 1 : dx;

	dy = ( inputKeysHeld & KEY_UP ) ? -1 : dy;
	dy = ( inputKeysHeld & KEY_DOWN ) ? 1 : dy;

	MousePositionNotifyRelative( 
		dx * inputMouseAcceleration,
		dy * inputMouseAcceleration
	);
}

LOCALPROC inputMouseMove_ABXY( void ) {
	int dx = 0;
	int dy = 0;

	dx = ( inputKeysHeld & KEY_Y ) ? -1 : dx;
	dx = ( inputKeysHeld & KEY_A ) ? 1 : dx;

	dy = ( inputKeysHeld & KEY_X ) ? -1 : dy;
	dy = ( inputKeysHeld & KEY_B ) ? 1 : dy;

	MousePositionNotifyRelative( 
		dx * inputMouseAcceleration,
		dy * inputMouseAcceleration
	);
}

LOCALPROC inputMouseMove_Scaled( void ) {
	static const int fxMacScreenHeight = inttof32( vMacScreenHeight );
	static const int fxDSScreenHeight = inttof32( SCREEN_HEIGHT );
	int mouseX = 0;
	int mouseY = 0;

	if ( inputKeysHeld & KEY_TOUCH && ! videoUIHasFocus( ) ) {
		mouseX = inputTouchPos.px << 1;
		mouseY = inttof32( inputTouchPos.py );

		mouseY = divf32( mouseY, fxDSScreenHeight );
		mouseY = mulf32( mouseY, fxMacScreenHeight );

		MousePositionNotify( mouseX, f32toint( mouseY ) );
	}
}

LOCALPROC inputMouseMove_Trackpad( void ) {
	static int lastTouchX = 0;
	static int lastTouchY = 0;
	int dx = 0;
	int dy = 0;

	if ( ! videoUIHasFocus( ) ) {
		if ( inputKeysDown & KEY_TOUCH ) {
			lastTouchX = inputTouchPos.px;
			lastTouchY = inputTouchPos.py;
		}

		if ( inputKeysHeld & KEY_TOUCH ) {
			dx = inputTouchPos.px - lastTouchX;
			dy = inputTouchPos.py - lastTouchY;

			MousePositionNotifyRelative( 
				dx * inputMouseAcceleration, 
				dy * inputMouseAcceleration
			);

			lastTouchX = inputTouchPos.px;
			lastTouchY = inputTouchPos.py;
		}

		if ( inputKeysUp & KEY_TOUCH ) {
			dx = 0;
			dy = 0;
		}
	}
}

LOCALPROC CheckForSystemEvents(void) {
	static uint32_t nextLVGLTick = 0;
	static uint32_t nextInputScan = 0;
	KeyboardEvent* theEvent = NULL;

	uint32_t tickNow = 0;
	uint16_t lastMouseX = 0;
	uint16_t lastMouseY = 0;

	tickNow = systemGetTicks( );

	if ( keyboardEventCount ) {
		theEvent = &keyboardEvents[ keyboardEventPos++ ];

		Keyboard_UpdateKeyMap2( theEvent->macKey, theEvent->down );

		if ( keyboardEventPos >= keyboardEventCount ) {
			keyboardEventPos = 0;
			keyboardEventCount = 0;
		}
	}

	scanKeys( );

	inputKeysDown = keysDown( );
	inputKeysHeld = keysHeld( );
	inputKeysUp = keysUp( );

	if ( ( inputKeysDown & KEY_TOUCH ) || ( inputKeysHeld & KEY_TOUCH ) )
		touchRead( &inputTouchPos );

	if ( tickNow >= nextLVGLTick && videoUIHasFocus( ) ) {
		lv_timer_handler( );
		nextLVGLTick = tickNow + 50;
	}

	if ( inputKeysHeld ) {
		switch ( inputMouseMode ) {
			case UI_SEL_MOUSE_MODE_DPAD:
				inputMouseMove_DPAD( );
				break;
			case UI_SEL_MOUSE_MODE_ABXY:
				inputMouseMove_ABXY( );
				break;
			case UI_SEL_MOUSE_MODE_SCALED:
				inputMouseMove_Scaled( );
				break;
			case UI_SEL_MOUSE_MODE_TRACKPAD: 
				inputMouseMove_Trackpad( );
			default:
				break;
		};
	}

	if ( ( CurMouseH != lastMouseX ) || ( CurMouseV != lastMouseY ) ) {
		videoScrollX = CurMouseH - ( SCREEN_WIDTH / 2 );
		videoScrollY = CurMouseV - ( SCREEN_HEIGHT / 2 );

		videoScrollX = ( videoScrollX < 0 ) ? 0 : videoScrollX;
		videoScrollX = ( videoScrollX > ( vMacScreenWidth - SCREEN_WIDTH ) ) ? vMacScreenWidth - SCREEN_WIDTH : videoScrollX;

		videoScrollY = ( videoScrollY < 0 ) ? 0 : videoScrollY;
		videoScrollY = ( videoScrollY > ( vMacScreenHeight - SCREEN_HEIGHT ) ) ? vMacScreenHeight - SCREEN_HEIGHT : videoScrollY;

		renderNeedsRefresh = 1;
	}

	if ( inputKeysDown & inputMouseButtonBit )
		MyMouseButtonSet( trueblnr );

	if ( inputKeysUp & inputMouseButtonBit )
		MyMouseButtonSet( falseblnr );	

	if ( inputKeysUp & KEY_START ) {
		if ( videoMacMsgBGOn ) {
			videoMacMsgBGOn = 0;

			bgHide( videoMacMsgBG );
			MacMsgDisplayOff( );
		}
	}

	if ( inputKeysUp & KEY_SELECT ) {
		videoSwapScreens( );
	}

	lastMouseX = CurMouseH;
	lastMouseY = CurMouseV;
}

/*
	OSGLUxxx common:
	In general, attempt to emulate one Macintosh tick (1/60.14742
	seconds) for every tick of real time. When done emulating
	one tick, wait for one tick of real time to elapse, by
	calling WaitForNextTick.

	But, Mini vMac can run the emulation at greater than 1x speed, up to
	and including running as fast as possible, by emulating extra cycles
	at the end of the emulated tick. In this case, the extra emulation
	should continue only as long as the current real time tick is not
	over - until ExtraTimeNotOver returns false.
*/

GLOBALOSGLUFUNC blnr ExtraTimeNotOver(void)
{
	UpdateTrueEmulatedTime();

	return TrueEmulatedTime == OnTrueTime;
}

GLOBALOSGLUPROC WaitForNextTick(void)
{
label_retry:
	CheckForSystemEvents();
	CheckForSavedTasks();
	CheckSavedMacMsg( );

	if (ForceMacOff) {
		return;
	}

	if (CurSpeedStopped) {
		DoneWithDrawingForTick();
		WaitForTheNextEvent();
		goto label_retry;
	}

#if ! HaveWorkingTime
	++TrueEmulatedTime;
#endif

	if (ExtraTimeNotOver()) {
		//swiWaitForIRQ( );
		goto label_retry;
	}

	if (CheckDateTime()) {
		//printf( "emlagtime = %d\n", EmLagTime );
#if MySoundEnabled
		MySound_SecondNotify();
#endif
#if EnableDemoMsg
		DemoModeSecondNotify();
#endif
	}

	if ((! gBackgroundFlag)
#if UseMotionEvents
		&& (! CaughtMouse)
#endif
		)
	{
		CheckMouseState();
	}

	OnTrueTime = TrueEmulatedTime;

#if dbglog_TimeStuff
	dbglog_writelnNum("WaitForNextTick, OnTrueTime", OnTrueTime);
#endif
}

/* --- platform independent code can be thought of as going here --- */

#include "PROGMAIN.h"

LOCALPROC ZapOSGLUVars(void)
{
	/*
		OSGLUxxx common:
		Set initial values of variables for
		platform dependent code, where not
		done using c initializers. (such
		as for arrays.)
	*/

	InitDrives();
	ZapWinStateVars();
}

LOCALPROC ReserveAllocAll(void)
{
#if dbglog_HAVE
	dbglog_ReserveAlloc();
#endif
	ReserveAllocOneBlock(&ROM, kROM_Size, 5, falseblnr);

	ReserveAllocOneBlock(&screencomparebuff,
		vMacScreenNumBytes, 5, trueblnr);
#if UseControlKeys
	ReserveAllocOneBlock(&CntrlDisplayBuff,
		vMacScreenNumBytes, 5, falseblnr);
#endif

#if MySoundEnabled
	ReserveAllocOneBlock((ui3p *)&TheSoundBuffer,
		dbhBufferSize, 5, falseblnr);
#endif

	EmulationReserveAlloc();
}

LOCALFUNC blnr AllocMyMemory(void)
{
	uimr n;
	blnr IsOk = falseblnr;

	ReserveAllocOffset = 0;
	ReserveAllocBigBlock = nullpr;
	ReserveAllocAll();
	n = ReserveAllocOffset;
	ReserveAllocBigBlock = (ui3p)calloc(1, n);
	if (NULL == ReserveAllocBigBlock) {
		MacMsg(kStrOutOfMemTitle, kStrOutOfMemMessage, trueblnr);
	} else {
		ReserveAllocOffset = 0;
		ReserveAllocAll();
		if (n != ReserveAllocOffset) {
			/* oops, program error */
		} else {
			IsOk = trueblnr;
		}
	}

	return IsOk;
}

LOCALPROC UnallocMyMemory(void)
{
	if (nullpr != ReserveAllocBigBlock) {
		free((char *)ReserveAllocBigBlock);
	}
}

#if CanGetAppPath
LOCALFUNC blnr InitWhereAmI(void)
{
	return trueblnr; /* keep going regardless */
}
#endif

#if CanGetAppPath
LOCALPROC UninitWhereAmI(void)
{
}
#endif

LOCALFUNC blnr InitFS( void ) {
	if ( nitroFSInit( NULL ) ) {
		app_parent = "nitro:/data/minivmac/";
		pref_dir = "nitro:/data/minivmac/";

		chdir( app_parent );
		return trueblnr;
	}
	
	if ( fatInitDefault( ) ) {
		app_parent = "sd:/data/minivmac/";
		pref_dir = "sd:/data/minivmac/";

		chdir( app_parent );
		return trueblnr;
	}

	MacMsg( "No filesystem", "Could not mount a filesystem.", trueblnr );

	return falseblnr;
}

LOCALFUNC blnr InitOSGLU(void)
{
	/*
		OSGLUxxx common:
		Run all the initializations needed for the program.
	*/

	videoInit( );
	systemSetupTimers( );

#if dbglog_HAVE
	if (dbglog_open())
#endif
	if ( InitFS( ) )
	if (Screen_Init())
	if (AllocMyMemory())
#if CanGetAppPath
	if (InitWhereAmI())
#endif
	if (ScanCommandLine())
	if (CreateMainWindow())
	if (LoadMacRom())
	if (LoadInitialImages())
	if (InitLocationDat())
#if MySoundEnabled
	if (MySound_Init())
#endif
	if (WaitForRom())
	{
		return trueblnr;
	}
	return falseblnr;
}

LOCALPROC UnInitOSGLU(void)
{
	/*
		OSGLUxxx common:
		Do all clean ups needed before the program quits.
	*/

	// if (MacMsgDisplayed) {
	// 	MacMsgDisplayOff();
	// }

	RestoreKeyRepeat();
#if MayFullScreen
	UngrabMachine();
#endif
#if MySoundEnabled
	MySound_Stop();
#endif
#if MySoundEnabled
	MySound_UnInit();
#endif
#if IncludePbufs
	UnInitPbufs();
#endif
	UnInitDrives();

	ForceShowCursor();

#if dbglog_HAVE
	dbglog_close();
#endif

#if CanGetAppPath
	UninitWhereAmI();
#endif
	UnallocMyMemory();

	CheckSavedMacMsg();

	CloseMainWindow();
}

int main(int argc, char **argv)
{
	uint32_t exitTime = 0;
	uint32_t timeNow = 0;

	my_argc = argc;
	my_argv = argv;

	ZapOSGLUVars();
	if (InitOSGLU()) {
		ProgramMain();
	}

	UnInitOSGLU();

	if ( videoMacMsgBGOn ) {
		do {
			swiWaitForVBlank( );
			CheckForSystemEvents( );
		} while ( ! ( inputKeysDown & KEY_START ) );
	}

	return 0;
}

#endif /* WantOSGLUSDL */
