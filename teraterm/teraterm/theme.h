/*
 * Copyright (C) 1994-1998 T. Teranishi
 * (C) 2022- TeraTerm Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _BG_TYPE { BG_COLOR = 0, BG_PICTURE, BG_WALLPAPER, BG_NONE } BG_TYPE;
typedef enum _BG_PATTERN {
	BG_STRETCH = 0,
	BG_TILE,
	BG_CENTER,
	BG_FIT_WIDTH,
	BG_FIT_HEIGHT,
	BG_AUTOFIT,
	BG_AUTOFILL
} BG_PATTERN;

typedef struct {
	BG_TYPE type;
	BG_PATTERN pattern;
	BOOL antiAlias;
	COLORREF color;
	int alpha;
	char file[MAX_PATH];
} TBGSrc;

typedef struct _BGTheme {
	TBGSrc BGDest;
	TBGSrc BGSrc1;
	TBGSrc BGSrc2;
	int BGReverseTextAlpha;
} BGTheme;

////////////////////
// color theme
////////////////////

typedef struct {
	BOOL change;
	BOOL enable;
	COLORREF fg;
	COLORREF bg;
} TColorSetting;

typedef struct {
	BOOL change;
	BOOL enable;
	COLORREF color[16];
} TAnsiColorSetting;

typedef struct {
	char name[50];
	TColorSetting vt;
	TColorSetting bold;			// SGR 1
	TColorSetting underline;	// SGR 4
	TColorSetting blink;		// SGR 5
	TColorSetting reverse;		// SGR 7
	TColorSetting url;
	TAnsiColorSetting ansicolor;
} TColorTheme;

typedef struct {
	BG_PATTERN id;
	const char *str;
} BG_PATTERN_ST;

// setting / themefile.cpp
const BG_PATTERN_ST *ThemeBGPatternList(int index);

// file / themefile.cpp
void ThemeLoadBG(const wchar_t *file, BGTheme *bg_theme);
void ThemeLoadColor(const wchar_t *fn, TColorTheme *color_theme);
void ThemeLoadColorOld(const wchar_t *file, TColorTheme *theme);
void ThemeLoad(const wchar_t *fname, BGTheme *bg_theme, TColorTheme *color_theme);
void ThemeSaveBG(const BGTheme *bg_theme, const wchar_t *fname);
void ThemeSaveColor(TColorTheme *color_theme, const wchar_t *fname);

// setting / vtdisp.c
void ThemeGetBG(BGTheme *bg_theme);
void ThemeSetBG(const BGTheme *bg_theme);
void ThemeGetColor(TColorTheme *data);
void ThemeSetColor(const TColorTheme *data);
void ThemeGetColorDefault(TColorTheme *data);
void ThemeGetColorDefaultTS(const TTTSet *pts, TColorTheme *color_theme);
void ThemeGetBGDefault(BGTheme *bg_theme);

#ifdef __cplusplus
}
#endif