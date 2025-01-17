/*
 * Copyright (C) 1994-1998 T. Teranishi
 * (C) 2004- TeraTerm Project
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
/* IPv6 modification is Copyright(C) 2000 Jun-ya Kato <kato@win6.jp> */

/* TTDLG.DLL, dialog boxes */
#include "teraterm.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <io.h>
#include <direct.h>
#include <commdlg.h>
#include <dlgs.h>
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <assert.h>
#include <winsock2.h>

#include "tttypes.h"
#include "tt-version.h"
#include "ttlib.h"
#include "dlglib.h"
#include "ttcommon.h"
#include "dlg_res.h"
#include "ttdlg.h"
#include "tipwin.h"
#include "comportinfo.h"
#include "codeconv.h"
#include "helpid.h"
#include "asprintf.h"
#include "win32helper.h"
#include "compat_win.h"
#include "ttlib_charset.h"
#include "asprintf.h"
#include "ttwinman.h"

// Oniguruma: Regular expression library
#define ONIG_STATIC
#include "oniguruma.h"

// SFMT: SIMD-oriented Fast Mersenne Twister
#include "SFMT_version_for_teraterm.h"

#undef EFFECT_ENABLED	// エフェクトの有効可否
#undef TEXTURE_ENABLED	// テクスチャの有効可否

#undef DialogBoxParam
#define DialogBoxParam(p1,p2,p3,p4,p5) \
	TTDialogBoxParam(p1,p2,p3,p4,p5)
#undef DialogBox
#define DialogBox(p1,p2,p3,p4) \
	TTDialogBox(p1,p2,p3,p4)
#undef EndDialog
#define EndDialog(p1,p2) \
	TTEndDialog(p1, p2)

static const char *ProtocolFamilyList[] = { "AUTO", "IPv6", "IPv4", NULL };
static const char *NLListRcv[] = {"CR","CR+LF", "LF", "AUTO", NULL};
static const char *NLList[] = {"CR","CR+LF", "LF", NULL};
static const char *TermList[] =
	{"VT100", "VT101", "VT102", "VT282", "VT320", "VT382",
	 "VT420", "VT520", "VT525", NULL};
static WORD Term_TermJ[] =
	{IdVT100, IdVT101, IdVT102, IdVT282, IdVT320, IdVT382,
	 IdVT420, IdVT520, IdVT525};
static WORD TermJ_Term[] = {1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9};
static const char *TermListJ[] =
	{"VT100", "VT100J", "VT101", "VT102", "VT102J", "VT220J", "VT282",
	 "VT320", "VT382", "VT420", "VT520", "VT525", NULL};
static const char *KanjiInList[] = {"^[$@","^[$B",NULL};
static const char *KanjiOutList[] = {"^[(B","^[(J",NULL};
static const char *KanjiOutList2[] = {"^[(B","^[(J","^[(H",NULL};
static const char *RussList2[] = {"Windows","KOI8-R",NULL};
static const char *MetaList[] = {"off", "on", "left", "right", NULL};
static const char *MetaList2[] = {"off", "on", NULL};

static const char *BaudList[] =
	{"110","300","600","1200","2400","4800","9600",
	 "14400","19200","38400","57600","115200",
	 "230400", "460800", "921600", NULL};

static void SetKanjiCodeDropDownList(HWND HDlg, int id, int language, int sel_code)
{
	int i;
	LRESULT sel_index = 0;

	for(i = 0;; i++) {
		LRESULT index;
		const TKanjiList *p = GetKanjiList(i);
		if (p == NULL) {
			break;
		}
		if (p->lang != language) {
			continue;
		}

		index = SendDlgItemMessageA(HDlg, id, CB_ADDSTRING, 0, (LPARAM)p->KanjiCode);
		SendDlgItemMessageA(HDlg, id, CB_SETITEMDATA, index, p->coding);
		if (p->coding == sel_code) {
			sel_index = index;
		}
	}
	SendDlgItemMessageA(HDlg, id, CB_SETCURSEL, sel_index, 0);
}

static INT_PTR CALLBACK TermDlg(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static const DlgTextInfo TextInfosCom[] = {
		{ 0, "DLG_TERM_TITLE" },
		{ IDC_TERMWIDTHLABEL, "DLG_TERM_WIDTHLABEL" },
		{ IDC_TERMISWIN, "DLG_TERM_ISWIN" },
		{ IDC_TERMRESIZE, "DLG_TERM_RESIZE" },
		{ IDC_TERMNEWLINE, "DLG_TERM_NEWLINE" },
		{ IDC_TERMCRRCVLABEL, "DLG_TERM_CRRCV" },
		{ IDC_TERMCRSENDLABEL, "DLG_TERM_CRSEND" },
		{ IDC_TERMIDLABEL, "DLG_TERM_ID" },
		{ IDC_TERMLOCALECHO, "DLG_TERM_LOCALECHO" },
		{ IDC_TERMANSBACKTEXT, "DLG_TERM_ANSBACK" },
		{ IDC_TERMAUTOSWITCH, "DLG_TERM_AUTOSWITCH" },
		{ IDC_TERMKANJILABEL, "DLG_TERM_KANJI" },
		{ IDC_TERMKANJISENDLABEL, "DLG_TERM_KANJISEND" },
		{ IDOK, "BTN_OK" },
		{ IDCANCEL, "BTN_CANCEL" },
		{ IDC_TERMHELP, "BTN_HELP" },
	};
	PTTSet ts;
	WORD w;
	//  char Temp[HostNameMaxLength + 1]; // 81(yutaka)
	char Temp[81]; // 81(yutaka)

	switch (Message) {
		case WM_INITDIALOG:
			ts = (PTTSet)lParam;
			SetWindowLongPtr(Dialog, DWLP_USER, lParam);

			SetDlgTextsW(Dialog, TextInfosCom, _countof(TextInfosCom), ts->UILanguageFileW);
			if (ts->Language==IdJapanese) {
				// 日本語の時だけ4つの項目が存在する
				static const DlgTextInfo TextInfosJp[] = {
					{ IDC_TERMKANA, "DLG_TERM_KANA" },
					{ IDC_TERMKANASEND, "DLG_TERM_KANASEND" },
					{ IDC_TERMKINTEXT, "DLG_TERM_KIN" },
					{ IDC_TERMKOUTTEXT, "DLG_TERM_KOUT" },
				};
				SetDlgTextsW(Dialog, TextInfosJp, _countof(TextInfosJp), ts->UILanguageFileW);
			}

			SetDlgItemInt(Dialog,IDC_TERMWIDTH,ts->TerminalWidth,FALSE);
			SendDlgItemMessage(Dialog, IDC_TERMWIDTH, EM_LIMITTEXT,3, 0);

			SetDlgItemInt(Dialog,IDC_TERMHEIGHT,ts->TerminalHeight,FALSE);
			SendDlgItemMessage(Dialog, IDC_TERMHEIGHT, EM_LIMITTEXT,3, 0);

			SetRB(Dialog,ts->TermIsWin,IDC_TERMISWIN,IDC_TERMISWIN);
			SetRB(Dialog,ts->AutoWinResize,IDC_TERMRESIZE,IDC_TERMRESIZE);
			if ( ts->TermIsWin>0 )
				DisableDlgItem(Dialog,IDC_TERMRESIZE,IDC_TERMRESIZE);

			SetDropDownList(Dialog, IDC_TERMCRRCV, NLListRcv, ts->CRReceive); // add 'LF' (2007.1.21 yutaka), added "AUTO" (9th Apr 2012, tentner)
			SetDropDownList(Dialog, IDC_TERMCRSEND, NLList, ts->CRSend);

			if ( ts->Language!=IdJapanese ) { /* non-Japanese mode */
				if ((ts->TerminalID>=1) &&
					(ts->TerminalID <= sizeof(TermJ_Term)/sizeof(WORD))) {
					w = TermJ_Term[ts->TerminalID-1];
				}
				else {
					w = 1;
				}
				SetDropDownList(Dialog, IDC_TERMID, TermList, w);
			}
			else {
				SetDropDownList(Dialog, IDC_TERMID, TermListJ, ts->TerminalID);
			}

			SetRB(Dialog,ts->LocalEcho,IDC_TERMLOCALECHO,IDC_TERMLOCALECHO);

			if ((ts->FTFlag & FT_BPAUTO)!=0) {
				DisableDlgItem(Dialog,IDC_TERMANSBACKTEXT,IDC_TERMANSBACK);
			}
			else {
				Str2Hex(ts->Answerback,Temp,ts->AnswerbackLen,
					sizeof(Temp)-1,FALSE);
				SetDlgItemText(Dialog, IDC_TERMANSBACK, Temp);
				SendDlgItemMessage(Dialog, IDC_TERMANSBACK, EM_LIMITTEXT,
					sizeof(Temp) - 1, 0);
			}

			SetRB(Dialog,ts->AutoWinSwitch,IDC_TERMAUTOSWITCH,IDC_TERMAUTOSWITCH);

			SetKanjiCodeDropDownList(Dialog, IDC_TERMKANJI, ts->Language, ts->KanjiCode);
			SetKanjiCodeDropDownList(Dialog, IDC_TERMKANJISEND, ts->Language, ts->KanjiCodeSend);
			if (ts->Language==IdJapanese) {
				if ( ts->KanjiCode!=IdJIS ) {
					DisableDlgItem(Dialog,IDC_TERMKANA,IDC_TERMKANA);
				}
				SetRB(Dialog,ts->JIS7Katakana,IDC_TERMKANA,IDC_TERMKANA);
				if ( ts->KanjiCodeSend!=IdJIS ) {
					DisableDlgItem(Dialog,IDC_TERMKANASEND,IDC_TERMKOUT);
				}
				SetRB(Dialog,ts->JIS7KatakanaSend,IDC_TERMKANASEND,IDC_TERMKANASEND);

				{
					const char **kanji_out_list;
					int n;
					n = ts->KanjiIn;
					n = (n <= 0 || 2 < n) ? IdKanjiInB : n;
					SetDropDownList(Dialog, IDC_TERMKIN, KanjiInList, n);

					kanji_out_list = (ts->TermFlag & TF_ALLOWWRONGSEQUENCE) ? KanjiOutList2 : KanjiOutList;
					n = ts->KanjiOut;
					n = (n <= 0 || 3 < n) ? IdKanjiOutB : n;
					SetDropDownList(Dialog, IDC_TERMKOUT, kanji_out_list, n);
				}
			}
			CenterWindow(Dialog, GetParent(Dialog));
			return TRUE;

		case WM_COMMAND:
			ts = (PTTSet)GetWindowLongPtr(Dialog,DWLP_USER);
			switch (LOWORD(wParam)) {
				case IDOK:

					if ( ts!=NULL ) {
						int width, height;

						width = GetDlgItemInt(Dialog, IDC_TERMWIDTH, NULL, FALSE);
						if (width > TermWidthMax) {
							ts->TerminalWidth = TermWidthMax;
						}
						else if (width > 0) {
							ts->TerminalWidth = width;
						}
						else { // 0 以下の場合は変更しない
							;
						}

						height = GetDlgItemInt(Dialog, IDC_TERMHEIGHT, NULL, FALSE);
						if (height > TermHeightMax) {
							ts->TerminalHeight = TermHeightMax;
						}
						else if (height > 0) {
							ts->TerminalHeight = height;
						}
						else { // 0 以下の場合は変更しない
							;
						}

						GetRB(Dialog,&ts->TermIsWin,IDC_TERMISWIN,IDC_TERMISWIN);
						GetRB(Dialog,&ts->AutoWinResize,IDC_TERMRESIZE,IDC_TERMRESIZE);

						if ((w = (WORD)GetCurSel(Dialog, IDC_TERMCRRCV)) > 0) {
							ts->CRReceive = w;
						}
						if ((w = (WORD)GetCurSel(Dialog, IDC_TERMCRSEND)) > 0) {
							ts->CRSend = w;
						}

						if ((w = (WORD)GetCurSel(Dialog, IDC_TERMID)) > 0) {
							if ( ts->Language!=IdJapanese ) { /* non-Japanese mode */
								if (w > sizeof(Term_TermJ)/sizeof(WORD)) w = 1;
								w = Term_TermJ[w-1];
							}
							ts->TerminalID = w;
						}

						GetRB(Dialog,&ts->LocalEcho,IDC_TERMLOCALECHO,IDC_TERMLOCALECHO);

						if ((ts->FTFlag & FT_BPAUTO)==0) {
							GetDlgItemText(Dialog, IDC_TERMANSBACK,Temp,sizeof(Temp));
							ts->AnswerbackLen = Hex2Str(Temp,ts->Answerback,sizeof(ts->Answerback));
						}

						GetRB(Dialog,&ts->AutoWinSwitch,IDC_TERMAUTOSWITCH,IDC_TERMAUTOSWITCH);

						if ((w = (WORD)GetCurSel(Dialog, IDC_TERMKANJI)) > 0) {
							w = (int)SendDlgItemMessageA(Dialog, IDC_TERMKANJI, CB_GETITEMDATA, w - 1, 0);
							ts->KanjiCode = w;
						}
						if ((w = (WORD)GetCurSel(Dialog, IDC_TERMKANJISEND)) > 0) {
							w = (int)SendDlgItemMessageA(Dialog, IDC_TERMKANJISEND, CB_GETITEMDATA, w - 1, 0);
							ts->KanjiCodeSend = w;
						}
						if (ts->Language==IdJapanese) {
							GetRB(Dialog,&ts->JIS7Katakana,IDC_TERMKANA,IDC_TERMKANA);
							GetRB(Dialog,&ts->JIS7KatakanaSend,IDC_TERMKANASEND,IDC_TERMKANASEND);
							if ((w = (WORD)GetCurSel(Dialog, IDC_TERMKIN)) > 0) {
								ts->KanjiIn = w;
							}
							if ((w = (WORD)GetCurSel(Dialog, IDC_TERMKOUT)) > 0) {
								ts->KanjiOut = w;
							}
						}
						else {
							ts->JIS7KatakanaSend=0;
							ts->JIS7Katakana=0;
							ts->KanjiIn = 0;
							ts->KanjiOut = 0;
						}

					}
					EndDialog(Dialog, 1);
					return TRUE;

				case IDCANCEL:
					EndDialog(Dialog, 0);
					return TRUE;

				case IDC_TERMISWIN:
					GetRB(Dialog,&w,IDC_TERMISWIN,IDC_TERMISWIN);
					if ( w==0 ) {
						EnableDlgItem(Dialog,IDC_TERMRESIZE,IDC_TERMRESIZE);
					}
					else {
						DisableDlgItem(Dialog,IDC_TERMRESIZE,IDC_TERMRESIZE);
					}
					break;

				case IDC_TERMKANJI:
					w = (WORD)GetCurSel(Dialog, IDC_TERMKANJI);
					w = (WORD)SendDlgItemMessageA(Dialog, IDC_TERMKANJI, CB_GETITEMDATA, w - 1, 0);
					if (w==IdJIS) {
						EnableDlgItem(Dialog,IDC_TERMKANA,IDC_TERMKANA);
					}
					else {
						DisableDlgItem(Dialog,IDC_TERMKANA,IDC_TERMKANA);
						}
					break;

				case IDC_TERMKANJISEND:
					w = (WORD)GetCurSel(Dialog, IDC_TERMKANJISEND);
					w = (WORD)SendDlgItemMessageA(Dialog, IDC_TERMKANJISEND, CB_GETITEMDATA, w - 1, 0);
					if (w==IdJIS) {
						EnableDlgItem(Dialog,IDC_TERMKANASEND,IDC_TERMKOUT);
					}
					else {
						DisableDlgItem(Dialog,IDC_TERMKANASEND,IDC_TERMKOUT);
					}
					break;

				case IDC_TERMHELP: {
					WPARAM HelpId;
					switch (ts->Language) {
					case IdJapanese:
						HelpId = HlpSetupTerminalJa;
						break;
					case IdEnglish:
						HelpId = HlpSetupTerminalEn;
						break;
					case IdKorean:
						HelpId = HlpSetupTerminalKo;
						break;
					case IdRussian:
						HelpId = HlpSetupTerminalRu;
						break;
					case IdUtf8:
						HelpId = HlpSetupTerminalUtf8;
						break;
					default:
						HelpId = HlpSetupTerminal;
						break;
					}
					PostMessage(GetParent(Dialog),WM_USER_DLGHELP2,HelpId,0);
					break;
				}
			}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
// WinDlg

typedef struct {
	WORD TmpColor[12][6];
	PTTSet ts;
	HFONT SampleFont;
	WORD VTFlag;
} WinDlgWork;

static void DispSample(HWND Dialog, WinDlgWork *work, int IAttr)
{
	int i,x,y;
	COLORREF Text, Back;
	int DX[3];
	TEXTMETRIC Metrics;
	RECT TestRect;
	int FW,FH;
	HDC DC;
	HWND HWndSample;

	HWndSample = GetDlgItem(Dialog, IDC_DRAW_SAMPLE_AREA);
	DC = GetDC(HWndSample);
	Text = RGB(work->TmpColor[IAttr][0],
	           work->TmpColor[IAttr][1],
	           work->TmpColor[IAttr][2]);
	Text = GetNearestColor(DC, Text);
	Back = RGB(work->TmpColor[IAttr][3],
	           work->TmpColor[IAttr][4],
	           work->TmpColor[IAttr][5]);
	Back = GetNearestColor(DC, Back);
	SetTextColor(DC, Text);
	SetBkColor(DC, Back);
	SelectObject(DC,work->SampleFont);
	GetTextMetrics(DC, &Metrics);
	FW = Metrics.tmAveCharWidth;
	FH = Metrics.tmHeight;
	for (i = 0 ; i <= 2 ; i++)
		DX[i] = FW;
	GetClientRect(HWndSample,&TestRect);
	x = (int)((TestRect.left+TestRect.right) / 2 - FW * 1.5);
	y = (TestRect.top+TestRect.bottom-FH) / 2;
	ExtTextOut(DC, x,y, ETO_CLIPPED | ETO_OPAQUE,
	           &TestRect, "ABC", 3, &(DX[0]));
	ReleaseDC(HWndSample,DC);
}

static void ChangeColor(HWND Dialog, WinDlgWork *work, int IAttr, int IOffset)
{
	SetDlgItemInt(Dialog,IDC_WINRED,work->TmpColor[IAttr][IOffset],FALSE);
	SetDlgItemInt(Dialog,IDC_WINGREEN,work->TmpColor[IAttr][IOffset+1],FALSE);
	SetDlgItemInt(Dialog,IDC_WINBLUE,work->TmpColor[IAttr][IOffset+2],FALSE);

	DispSample(Dialog,work,IAttr);
}

static void ChangeSB(HWND Dialog, WinDlgWork *work, int IAttr, int IOffset)
{
	HWND HRed, HGreen, HBlue;

	HRed = GetDlgItem(Dialog, IDC_WINREDBAR);
	HGreen = GetDlgItem(Dialog, IDC_WINGREENBAR);
	HBlue = GetDlgItem(Dialog, IDC_WINBLUEBAR);

	SetScrollPos(HRed,SB_CTL,work->TmpColor[IAttr][IOffset+0],TRUE);
	SetScrollPos(HGreen,SB_CTL,work->TmpColor[IAttr][IOffset+1],TRUE);
	SetScrollPos(HBlue,SB_CTL,work->TmpColor[IAttr][IOffset+2],TRUE);
	ChangeColor(Dialog,work,IAttr,IOffset);
}

static void RestoreVar(HWND Dialog, WinDlgWork* work, int *IAttr, int *IOffset)
{
	WORD w;

	GetRB(Dialog,&w,IDC_WINTEXT,IDC_WINBACK);
	if (w==2) {
		*IOffset = 3;
	}
	else {
		*IOffset = 0;
	}
	if (work->VTFlag>0) {
		*IAttr = GetCurSel(Dialog,IDC_WINATTR);
		if (*IAttr>0) (*IAttr)--;
	}
	else {
		*IAttr = 0;
	}
}

static INT_PTR CALLBACK WinDlg(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static const DlgTextInfo TextInfos[] = {
		{ 0, "DLG_WIN_TITLE" },
		{ IDC_WINTITLELABEL, "DLG_WIN_TITLELABEL" },
		{ IDC_WINCURSOR, "DLG_WIN_CURSOR" },
		{ IDC_WINBLOCK, "DLG_WIN_BLOCK" },
		{ IDC_WINVERT, "DLG_WIN_VERT" },
		{ IDC_WINHORZ, "DLG_WIN_HORZ" },
		{ IDC_FONTBOLD, "DLG_WIN_BOLDFONT" },
		{ IDC_WINHIDETITLE, "DLG_WIN_HIDETITLE" },
		{ IDC_WINHIDEMENU, "DLG_WIN_HIDEMENU" },
		{ IDC_WINAIXTERM16, "DLG_WIN_AIXTERM16" },
		{ IDC_WINXTERM256, "DLG_WIN_XTERM256" },
		{ IDC_WINSCROLL1, "DLG_WIN_SCROLL1" },
		{ IDC_WINSCROLL3, "DLG_WIN_SCROLL3" },
		{ IDC_WINCOLOR, "DLG_WIN_COLOR" },
		{ IDC_WINTEXT, "DLG_WIN_TEXT" },
		{ IDC_WINBACK, "DLG_WIN_BG" },
		{ IDC_WINATTRTEXT, "DLG_WIN_ATTRIB" },
		{ IDC_WINREV, "DLG_WIN_REVERSE" },
		{ IDC_WINREDLABEL, "DLG_WIN_R" },
		{ IDC_WINGREENLABEL, "DLG_WIN_G" },
		{ IDC_WINBLUELABEL, "DLG_WIN_B" },
		{ IDC_WINUSENORMALBG, "DLG_WIN_ALWAYSBG" },
		{ IDOK, "BTN_OK" },
		{ IDCANCEL, "BTN_CANCEL" },
		{ IDC_WINHELP, "BTN_HELP" },
	};
	PTTSet ts;
	HWND Wnd, HRed, HGreen, HBlue;
	int IAttr, IOffset;
	WORD i, pos, ScrollCode, NewPos;
	WinDlgWork *work = (WinDlgWork *)GetWindowLongPtr(Dialog,DWLP_USER);

	switch (Message) {
		case WM_INITDIALOG:
			work = (WinDlgWork *)lParam;;
			SetWindowLongPtr(Dialog, DWLP_USER, lParam);
			ts = work->ts;

			SetDlgTextsW(Dialog, TextInfos, _countof(TextInfos), ts->UILanguageFileW);
			{
				// VTWinとTEKWinでラベルが異なっている
				static const DlgTextInfo TextInfosVT[] = {
					{ IDC_WINCOLOREMU, "DLG_WIN_PCBOLD16" },
				};
				static const DlgTextInfo TextInfosTEK[] = {
					{ IDC_WINCOLOREMU, "DLG_WIN_COLOREMU" },
				};
				const DlgTextInfo *TextInfosVTTEK = (work->VTFlag>0) ? TextInfosVT : TextInfosTEK;
				SetDlgTextsW(Dialog, TextInfosVTTEK, 1, ts->UILanguageFileW);
			}

			SetDlgItemTextA(Dialog, IDC_WINTITLE, ts->Title);
			SendDlgItemMessage(Dialog, IDC_WINTITLE, EM_LIMITTEXT,
			                   sizeof(ts->Title)-1, 0);

			SetRB(Dialog,ts->HideTitle,IDC_WINHIDETITLE,IDC_WINHIDETITLE);
			SetRB(Dialog,ts->EtermLookfeel.BGNoFrame,IDC_NO_FRAME,IDC_NO_FRAME);
			SetRB(Dialog,ts->PopupMenu,IDC_WINHIDEMENU,IDC_WINHIDEMENU);
			if ( ts->HideTitle>0 ) {
				DisableDlgItem(Dialog,IDC_WINHIDEMENU,IDC_WINHIDEMENU);
			} else {
				DisableDlgItem(Dialog,IDC_NO_FRAME,IDC_NO_FRAME);
			}

			if (work->VTFlag>0) {
				SetRB(Dialog, (ts->ColorFlag&CF_PCBOLD16)!=0, IDC_WINCOLOREMU, IDC_WINCOLOREMU);
				SetRB(Dialog, (ts->ColorFlag&CF_AIXTERM16)!=0, IDC_WINAIXTERM16, IDC_WINAIXTERM16);
				SetRB(Dialog, (ts->ColorFlag&CF_XTERM256)!=0,IDC_WINXTERM256,IDC_WINXTERM256);
				ShowDlgItem(Dialog,IDC_WINAIXTERM16,IDC_WINXTERM256);
				ShowDlgItem(Dialog,IDC_WINSCROLL1,IDC_WINSCROLL3);
				SetRB(Dialog,ts->EnableScrollBuff,IDC_WINSCROLL1,IDC_WINSCROLL1);
				SetDlgItemInt(Dialog,IDC_WINSCROLL2,ts->ScrollBuffSize,FALSE);

				SendDlgItemMessage(Dialog, IDC_WINSCROLL2, EM_LIMITTEXT, 6, 0);

				if ( ts->EnableScrollBuff==0 ) {
					DisableDlgItem(Dialog,IDC_WINSCROLL2,IDC_WINSCROLL3);
				}

				for (i = 0 ; i <= 1 ; i++) {
					work->TmpColor[0][i*3]   = GetRValue(ts->VTColor[i]);
					work->TmpColor[0][i*3+1] = GetGValue(ts->VTColor[i]);
					work->TmpColor[0][i*3+2] = GetBValue(ts->VTColor[i]);
					work->TmpColor[1][i*3]   = GetRValue(ts->VTBoldColor[i]);
					work->TmpColor[1][i*3+1] = GetGValue(ts->VTBoldColor[i]);
					work->TmpColor[1][i*3+2] = GetBValue(ts->VTBoldColor[i]);
					work->TmpColor[2][i*3]   = GetRValue(ts->VTBlinkColor[i]);
					work->TmpColor[2][i*3+1] = GetGValue(ts->VTBlinkColor[i]);
					work->TmpColor[2][i*3+2] = GetBValue(ts->VTBlinkColor[i]);
					work->TmpColor[3][i*3]   = GetRValue(ts->VTReverseColor[i]);
					work->TmpColor[3][i*3+1] = GetGValue(ts->VTReverseColor[i]);
					work->TmpColor[3][i*3+2] = GetBValue(ts->VTReverseColor[i]);
					work->TmpColor[4][i*3]   = GetRValue(ts->URLColor[i]);
					work->TmpColor[4][i*3+1] = GetGValue(ts->URLColor[i]);
					work->TmpColor[4][i*3+2] = GetBValue(ts->URLColor[i]);
					work->TmpColor[5][i*3]   = GetRValue(ts->VTUnderlineColor[i]);
					work->TmpColor[5][i*3+1] = GetGValue(ts->VTUnderlineColor[i]);
					work->TmpColor[5][i*3+2] = GetBValue(ts->VTUnderlineColor[i]);
				}
				ShowDlgItem(Dialog,IDC_WINATTRTEXT,IDC_WINATTR);
				{
					static const I18nTextInfo infos[] = {
						{ "DLG_WIN_NORMAL", L"Normal" },
						{ "DLG_WIN_BOLD", L"Bold" },
						{ "DLG_WIN_BLINK", L"Blink" },
						{ "DLG_WIN_REVERSEATTR", L"Reverse" },
						{ NULL, L"URL" },
						{ NULL, L"Underline" },
					};
					SetI18nListW("Tera Term", Dialog, IDC_WINATTR, infos, _countof(infos), ts->UILanguageFileW, 0);
				}
				ShowDlgItem(Dialog,IDC_WINUSENORMALBG,IDC_WINUSENORMALBG);
				SetRB(Dialog,ts->UseNormalBGColor,IDC_WINUSENORMALBG,IDC_WINUSENORMALBG);

				ShowDlgItem(Dialog, IDC_FONTBOLD, IDC_FONTBOLD);
				SetRB(Dialog, (ts->FontFlag & FF_BOLD) > 0, IDC_FONTBOLD,IDC_FONTBOLD);
			}
			else {
				for (i = 0 ; i <=1 ; i++) {
					work->TmpColor[0][i*3]   = GetRValue(ts->TEKColor[i]);
					work->TmpColor[0][i*3+1] = GetGValue(ts->TEKColor[i]);
					work->TmpColor[0][i*3+2] = GetBValue(ts->TEKColor[i]);
				}
				SetRB(Dialog,ts->TEKColorEmu,IDC_WINCOLOREMU,IDC_WINCOLOREMU);
			}
			SetRB(Dialog,1,IDC_WINTEXT,IDC_WINBACK);

			SetRB(Dialog,ts->CursorShape,IDC_WINBLOCK,IDC_WINHORZ);

			IAttr = 0;
			IOffset = 0;

			HRed = GetDlgItem(Dialog, IDC_WINREDBAR);
			SetScrollRange(HRed,SB_CTL,0,255,TRUE);

			HGreen = GetDlgItem(Dialog, IDC_WINGREENBAR);
			SetScrollRange(HGreen,SB_CTL,0,255,TRUE);

			HBlue = GetDlgItem(Dialog, IDC_WINBLUEBAR);
			SetScrollRange(HBlue,SB_CTL,0,255,TRUE);

			ChangeSB(Dialog,work,IAttr,IOffset);

			CenterWindow(Dialog, GetParent(Dialog));

			return TRUE;

		case WM_COMMAND:
			ts = work->ts;
			RestoreVar(Dialog,work,&IAttr,&IOffset);
			assert(IAttr < _countof(work->TmpColor));
			switch (LOWORD(wParam)) {
				case IDOK: {
					WORD w;
					//HDC DC;
					GetDlgItemText(Dialog, IDC_WINTITLE, ts->Title, sizeof(ts->Title));
					GetRB(Dialog, &ts->HideTitle, IDC_WINHIDETITLE, IDC_WINHIDETITLE);
					GetRB(Dialog, &w, IDC_NO_FRAME, IDC_NO_FRAME);
					ts->EtermLookfeel.BGNoFrame = w;
					GetRB(Dialog, &ts->PopupMenu, IDC_WINHIDEMENU, IDC_WINHIDEMENU);
					// DC = GetDC(Dialog);
					if (work->VTFlag > 0) {
						GetRB(Dialog, &i, IDC_WINCOLOREMU, IDC_WINCOLOREMU);
						if (i != 0) {
							ts->ColorFlag |= CF_PCBOLD16;
						}
						else {
							ts->ColorFlag &= ~(WORD)CF_PCBOLD16;
						}
						GetRB(Dialog, &i, IDC_WINAIXTERM16, IDC_WINAIXTERM16);
						if (i != 0) {
							ts->ColorFlag |= CF_AIXTERM16;
						}
						else {
							ts->ColorFlag &= ~(WORD)CF_AIXTERM16;
						}
						GetRB(Dialog, &i, IDC_WINXTERM256, IDC_WINXTERM256);
						if (i != 0) {
							ts->ColorFlag |= CF_XTERM256;
						}
						else {
							ts->ColorFlag &= ~(WORD)CF_XTERM256;
						}
						GetRB(Dialog, &ts->EnableScrollBuff, IDC_WINSCROLL1, IDC_WINSCROLL1);
						if (ts->EnableScrollBuff > 0) {
								ts->ScrollBuffSize =
									GetDlgItemInt(Dialog,IDC_WINSCROLL2,NULL,FALSE);
						}
						for (i = 0; i <= 1; i++) {
							ts->VTColor[i] =
								RGB(work->TmpColor[0][i*3],
									work->TmpColor[0][i*3+1],
									work->TmpColor[0][i*3+2]);
							ts->VTBoldColor[i] =
								RGB(work->TmpColor[1][i*3],
									work->TmpColor[1][i*3+1],
									work->TmpColor[1][i*3+2]);
							ts->VTBlinkColor[i] =
								RGB(work->TmpColor[2][i*3],
									work->TmpColor[2][i*3+1],
									work->TmpColor[2][i*3+2]);
							ts->VTReverseColor[i] =
								RGB(work->TmpColor[3][i*3],
									work->TmpColor[3][i*3+1],
									work->TmpColor[3][i*3+2]);
							ts->URLColor[i] =
								RGB(work->TmpColor[4][i*3],
									work->TmpColor[4][i*3+1],
									work->TmpColor[4][i*3+2]);
							ts->VTUnderlineColor[i] =
								RGB(work->TmpColor[5][i * 3],
									work->TmpColor[5][i * 3 + 1],
									work->TmpColor[5][i * 3 + 2]);
#if 0
							ts->VTColor[i] = GetNearestColor(DC, ts->VTColor[i]);
							ts->VTBoldColor[i] = GetNearestColor(DC, ts->VTBoldColor[i]);
							ts->VTBlinkColor[i] = GetNearestColor(DC, ts->VTBlinkColor[i]);
							ts->VTReverseColor[i] = GetNearestColor(DC, ts->VTReverseColor[i]);
							ts->URLColor[i] = GetNearestColor(DC, ts->URLColor[i]);
#endif
						}
						GetRB(Dialog, &ts->UseNormalBGColor,
							  IDC_WINUSENORMALBG, IDC_WINUSENORMALBG);
						GetRB(Dialog, &i, IDC_FONTBOLD, IDC_FONTBOLD);
						if (i > 0) {
							ts->FontFlag |= FF_BOLD;
						}
						else {
							ts->FontFlag &= ~(WORD)FF_BOLD;
						}
					}
					else {
						for (i = 0; i <= 1; i++) {
							ts->TEKColor[i] =
								RGB(work->TmpColor[0][i*3],
									work->TmpColor[0][i*3+1],
									work->TmpColor[0][i*3+2]);
#if 0
							ts->TEKColor[i] = GetNearestColor(DC, ts->TEKColor[i]);
#endif
						}
						GetRB(Dialog, &ts->TEKColorEmu, IDC_WINCOLOREMU, IDC_WINCOLOREMU);
					}
					// ReleaseDC(Dialog,DC);

					GetRB(Dialog, &ts->CursorShape, IDC_WINBLOCK, IDC_WINHORZ);
					EndDialog(Dialog, 1);
					return TRUE;
				}

				case IDCANCEL:
					EndDialog(Dialog, 0);
					return TRUE;

				case IDC_WINHIDETITLE:
					GetRB(Dialog,&i,IDC_WINHIDETITLE,IDC_WINHIDETITLE);
					if (i>0) {
						DisableDlgItem(Dialog,IDC_WINHIDEMENU,IDC_WINHIDEMENU);
						EnableDlgItem(Dialog,IDC_NO_FRAME,IDC_NO_FRAME);
					}
					else {
						EnableDlgItem(Dialog,IDC_WINHIDEMENU,IDC_WINHIDEMENU);
						DisableDlgItem(Dialog,IDC_NO_FRAME,IDC_NO_FRAME);
					}
					break;

				case IDC_WINSCROLL1:
					GetRB(Dialog,&i,IDC_WINSCROLL1,IDC_WINSCROLL1);
					if ( i>0 ) {
						EnableDlgItem(Dialog,IDC_WINSCROLL2,IDC_WINSCROLL3);
					}
					else {
						DisableDlgItem(Dialog,IDC_WINSCROLL2,IDC_WINSCROLL3);
					}
					break;

				case IDC_WINTEXT:
					IOffset = 0;
					ChangeSB(Dialog,work,IAttr,IOffset);
					break;

				case IDC_WINBACK:
					IOffset = 3;
					ChangeSB(Dialog,work,IAttr,IOffset);
					break;

				case IDC_WINREV:
					i = work->TmpColor[IAttr][0];
					work->TmpColor[IAttr][0] = work->TmpColor[IAttr][3];
					work->TmpColor[IAttr][3] = i;
					i = work->TmpColor[IAttr][1];
					work->TmpColor[IAttr][1] = work->TmpColor[IAttr][4];
					work->TmpColor[IAttr][4] = i;
					i = work->TmpColor[IAttr][2];
					work->TmpColor[IAttr][2] = work->TmpColor[IAttr][5];
					work->TmpColor[IAttr][5] = i;

					ChangeSB(Dialog,work,IAttr,IOffset);
					break;

				case IDC_WINATTR:
					ChangeSB(Dialog,work,IAttr,IOffset);
					break;

				case IDC_WINHELP: {
					const WPARAM HelpId = work->VTFlag > 0 ? HlpSetupWindow : HlpTEKSetupWindow;
					PostMessage(GetParent(Dialog),WM_USER_DLGHELP2,HelpId,0);
					break;
				}
			}
			break;

		case WM_PAINT:
			ts = work->ts;
			RestoreVar(Dialog,work,&IAttr,&IOffset);
			DispSample(Dialog,work,IAttr);
			break;

		case WM_HSCROLL:
			ts = work->ts;
			RestoreVar(Dialog,work,&IAttr,&IOffset);
			HRed = GetDlgItem(Dialog, IDC_WINREDBAR);
			HGreen = GetDlgItem(Dialog, IDC_WINGREENBAR);
			HBlue = GetDlgItem(Dialog, IDC_WINBLUEBAR);
			Wnd = (HWND)lParam;
			ScrollCode = LOWORD(wParam);
			NewPos = HIWORD(wParam);
			if ( Wnd == HRed ) {
				i = IOffset;
			}
			else if ( Wnd == HGreen ) {
				i = IOffset + 1;
			}
			else if ( Wnd == HBlue ) {
				i = IOffset + 2;
			}
			else {
				return TRUE;
			}
			pos = work->TmpColor[IAttr][i];
			switch (ScrollCode) {
				case SB_BOTTOM:
					pos = 255;
					break;
				case SB_LINEDOWN:
					if (pos<255) {
						pos++;
					}
					break;
				case SB_LINEUP:
					if (pos>0) {
						pos--;
					}
					break;
				case SB_PAGEDOWN:
					pos += 16;
					break;
				case SB_PAGEUP:
					if (pos < 16) {
						pos = 0;
					}
					else {
						pos -= 16;
					}
					break;
				case SB_THUMBPOSITION:
					pos = NewPos;
					break;
				case SB_THUMBTRACK:
					pos = NewPos;
					break;
				case SB_TOP:
					pos = 0;
					break;
				default:
					return TRUE;
			}
			if (pos > 255) {
				pos = 255;
			}
			work->TmpColor[IAttr][i] = pos;
			SetScrollPos(Wnd,SB_CTL,pos,TRUE);
			ChangeColor(Dialog,work,IAttr,IOffset);
			return FALSE;
	}
	return FALSE;
}

static INT_PTR CALLBACK KeybDlg(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static const DlgTextInfo TextInfos[] = {
		{ 0, "DLG_KEYB_TITLE" },
		{ IDC_KEYBTRANS, "DLG_KEYB_TRANSMIT" },
		{ IDC_KEYBBS, "DLG_KEYB_BS" },
		{ IDC_KEYBDEL, "DLG_KEYB_DEL" },
		{ IDC_KEYBKEYBTEXT, "DLG_KEYB_KEYB" },
		{ IDC_KEYBMETATEXT, "DLG_KEYB_META" },
		{ IDC_KEYBDISABLE, "DLG_KEYB_DISABLE" },
		{ IDC_KEYBAPPKEY, "DLG_KEYB_APPKEY" },
		{ IDC_KEYBAPPCUR, "DLG_KEYB_APPCUR" },
		{ IDOK, "BTN_OK" },
		{ IDCANCEL, "BTN_CANCEL" },
		{ IDC_KEYBHELP, "BTN_HELP" },
	};
	PTTSet ts;

	switch (Message) {
		case WM_INITDIALOG:
			ts = (PTTSet)lParam;
			SetWindowLongPtr(Dialog, DWLP_USER, lParam);

			SetDlgTextsW(Dialog, TextInfos, _countof(TextInfos), ts->UILanguageFileW);

			SetRB(Dialog,ts->BSKey-1,IDC_KEYBBS,IDC_KEYBBS);
			SetRB(Dialog,ts->DelKey,IDC_KEYBDEL,IDC_KEYBDEL);
			SetRB(Dialog,ts->MetaKey,IDC_KEYBMETA,IDC_KEYBMETA);
			SetRB(Dialog,ts->DisableAppKeypad,IDC_KEYBAPPKEY,IDC_KEYBAPPKEY);
			SetRB(Dialog,ts->DisableAppCursor,IDC_KEYBAPPCUR,IDC_KEYBAPPCUR);

			if (!IsWindowsNTKernel()) {
				SetDropDownList(Dialog, IDC_KEYBMETA, MetaList2, ts->MetaKey + 1);
			}
			else {
				SetDropDownList(Dialog, IDC_KEYBMETA, MetaList, ts->MetaKey + 1);
			}

			if (ts->Language==IdRussian) {
				ShowDlgItem(Dialog,IDC_KEYBKEYBTEXT,IDC_KEYBKEYB);
				SetDropDownList(Dialog, IDC_KEYBKEYB, RussList2, ts->RussKeyb);
			}

			CenterWindow(Dialog, GetParent(Dialog));

			return TRUE;

		case WM_COMMAND:
			ts = (PTTSet)GetWindowLongPtr(Dialog,DWLP_USER);
			switch (LOWORD(wParam)) {
				case IDOK:
					if ( ts!=NULL ) {
						WORD w;

						GetRB(Dialog,&ts->BSKey,IDC_KEYBBS,IDC_KEYBBS);
						ts->BSKey++;
						GetRB(Dialog,&ts->DelKey,IDC_KEYBDEL,IDC_KEYBDEL);
						GetRB(Dialog,&ts->DisableAppKeypad,IDC_KEYBAPPKEY,IDC_KEYBAPPKEY);
						GetRB(Dialog,&ts->DisableAppCursor,IDC_KEYBAPPCUR,IDC_KEYBAPPCUR);
						if ((w = (WORD)GetCurSel(Dialog, IDC_KEYBMETA)) > 0) {
							ts->MetaKey = w - 1;
						}
						if (ts->Language==IdRussian) {
							if ((w = (WORD)GetCurSel(Dialog, IDC_KEYBKEYB)) > 0) {
								ts->RussKeyb = w;
							}
						}
					}
					EndDialog(Dialog, 1);
					return TRUE;

				case IDCANCEL:
					EndDialog(Dialog, 0);
					return TRUE;

				case IDC_KEYBHELP: {
					WPARAM HelpId;
					if (ts->Language==IdRussian) {
						HelpId = HlpSetupKeyboardRuss;
					}
					else {
						HelpId = HlpSetupKeyboard;
					}
					PostMessage(GetParent(Dialog),WM_USER_DLGHELP2,HelpId,0);
					break;
				}
			}
	}
	return FALSE;
}

typedef struct {
	PTTSet pts;
	ComPortInfo_t *ComPortInfoPtr;
	int ComPortInfoCount;
} SerialDlgData;

static const char *DataList[] = {"7 bit","8 bit",NULL};
static const char *ParityList[] = {"none", "odd", "even", "mark", "space", NULL};
static const char *StopList[] = {"1 bit", "2 bit", NULL};
static const char *FlowList[] = {"Xon/Xoff", "RTS/CTS", "DSR/DTR", "none", NULL};
static int g_deltaSumSerialDlg = 0;        // マウスホイールのDelta累積用
static WNDPROC g_defSerialDlgEditWndProc;  // Edit Controlのサブクラス化用
static WNDPROC g_defSerialDlgSpeedComboboxWndProc;  // Combo-box Controlのサブクラス化用
static TipWin *g_SerialDlgSpeedTip;

/*
 * シリアルポート設定ダイアログのOKボタンを接続先に応じて名称を切り替える。
 * 条件判定は OnSetupSerialPort() と合わせる必要がある。
 */
static void serial_dlg_change_OK_button(HWND dlg, int portno, const wchar_t *UILanguageFileW)
{
	wchar_t *uimsg;
	if ( cv.Ready && (cv.PortType != IdSerial) ) {
		uimsg = TTGetLangStrW("Tera Term",
							  "DLG_SERIAL_OK_CONNECTION",
							  L"Connect with &New window",
							  UILanguageFileW);
	} else {
		if (cv.Open) {
			if (portno != cv.ComPort) {
				uimsg = TTGetLangStrW("Tera Term",
									  "DLG_SERIAL_OK_CLOSEOPEN",
									  L"Close and &New open",
									  UILanguageFileW);
			} else {
				uimsg = TTGetLangStrW("Tera Term",
									  "DLG_SERIAL_OK_RESET",
									  L"&New setting",
									  UILanguageFileW);
			}

		} else {
			uimsg = TTGetLangStrW("Tera Term",
								  "DLG_SERIAL_OK_OPEN",
								  L"&New open",
								  UILanguageFileW);
		}
	}
	SetDlgItemTextW(dlg, IDOK, uimsg);
	free(uimsg);
}

/*
 * シリアルポート設定ダイアログのテキストボックスにCOMポートの詳細情報を表示する。
 *
 */
static void serial_dlg_set_comport_info(HWND dlg, SerialDlgData *dlg_data, int port_index)
{
	if (port_index + 1 > dlg_data->ComPortInfoCount) {
		SetDlgItemTextW(dlg, IDC_SERIALTEXT, NULL);
	}
	else {
		const ComPortInfo_t *p = &dlg_data->ComPortInfoPtr[port_index];
		SetDlgItemTextW(dlg, IDC_SERIALTEXT, p->property);
	}
}

/*
 * シリアルポート設定ダイアログのテキストボックスのプロシージャ
 */
static LRESULT CALLBACK SerialDlgEditWindowProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	WORD keys;
	short delta;
	BOOL page;

	switch (msg) {
		case WM_KEYDOWN:
			// Edit control上で CTRL+A を押下すると、テキストを全選択する。
			if (wp == 'A' && GetKeyState(VK_CONTROL) < 0) {
				PostMessage(hWnd, EM_SETSEL, 0, -1);
				return 0;
			}
			break;

		case WM_MOUSEWHEEL:
			// CTRLorSHIFT + マウスホイールの場合、横スクロールさせる。
			keys = GET_KEYSTATE_WPARAM(wp);
			delta = GET_WHEEL_DELTA_WPARAM(wp);
			page = keys & (MK_CONTROL | MK_SHIFT);

			if (page == 0)
				break;

			g_deltaSumSerialDlg += delta;

			if (g_deltaSumSerialDlg >= WHEEL_DELTA) {
				g_deltaSumSerialDlg -= WHEEL_DELTA;
				SendMessage(hWnd, WM_HSCROLL, SB_PAGELEFT , 0);
			} else if (g_deltaSumSerialDlg <= -WHEEL_DELTA) {
				g_deltaSumSerialDlg += WHEEL_DELTA;
				SendMessage(hWnd, WM_HSCROLL, SB_PAGERIGHT, 0);
			}

			break;
	}
    return CallWindowProc(g_defSerialDlgEditWndProc, hWnd, msg, wp, lp);
}

/*
 * シリアルポート設定ダイアログのSPEED(BAUD)のプロシージャ
 */
static LRESULT CALLBACK SerialDlgSpeedComboboxWindowProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	const int tooltip_timeout = 1000;  // msec
	POINT pt;
	int w, h;
	int cx, cy;
	RECT wr;
	wchar_t *uimsg;
	SerialDlgData *dlg_data = (SerialDlgData *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	const wchar_t *UILanguageFileW;

	switch (msg) {
		case WM_MOUSEMOVE:
			UILanguageFileW = dlg_data->pts->UILanguageFileW;
			uimsg = TTGetLangStrW("Tera Term",
								  "DLG_SERIAL_SPEED_TOOLTIP", L"You can directly specify a number", UILanguageFileW);

			// Combo-boxの左上座標を求める
			GetWindowRect(hWnd, &wr);
			pt.x = wr.left;
			pt.y = wr.top;

			// 文字列の縦横サイズを取得する
			TipWinGetTextWidthHeightW(hWnd, uimsg, &w, &h);

			cx = pt.x;
			cy = pt.y - (h + TIP_WIN_FRAME_WIDTH * 6);

			// ツールチップを表示する
			if (g_SerialDlgSpeedTip == NULL) {
				g_SerialDlgSpeedTip = TipWinCreate(hInst, hWnd);
				TipWinSetHideTimer(g_SerialDlgSpeedTip, tooltip_timeout);
			}
			if (!TipWinIsVisible(g_SerialDlgSpeedTip))
				TipWinSetVisible(g_SerialDlgSpeedTip, TRUE);

			TipWinSetTextW(g_SerialDlgSpeedTip, uimsg);
			TipWinSetPos(g_SerialDlgSpeedTip, cx, cy);
			TipWinSetHideTimer(g_SerialDlgSpeedTip, tooltip_timeout);

			free(uimsg);

			break;
	}
    return CallWindowProc(g_defSerialDlgSpeedComboboxWndProc, hWnd, msg, wp, lp);
}

/*
 * シリアルポート設定ダイアログ
 *
 * シリアルポート数が0の時は呼ばれない
 */
static INT_PTR CALLBACK SerialDlg(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static const DlgTextInfo TextInfos[] = {
		{ 0, "DLG_SERIAL_TITLE" },
		{ IDC_SERIALPORT_LABEL, "DLG_SERIAL_PORT" },
		{ IDC_SERIALBAUD_LEBAL, "DLG_SERIAL_BAUD" },
		{ IDC_SERIALDATA_LABEL, "DLG_SERIAL_DATA" },
		{ IDC_SERIALPARITY_LABEL, "DLG_SERIAL_PARITY" },
		{ IDC_SERIALSTOP_LABEL, "DLG_SERIAL_STOP" },
		{ IDC_SERIALFLOW_LABEL, "DLG_SERIAL_FLOW" },
		{ IDC_SERIALDELAY, "DLG_SERIAL_DELAY" },
		{ IDC_SERIALDELAYCHAR_LABEL, "DLG_SERIAL_DELAYCHAR" },
		{ IDC_SERIALDELAYLINE_LABEL, "DLG_SERIAL_DELAYLINE" },
		{ IDOK, "BTN_OK" },
		{ IDCANCEL, "BTN_CANCEL" },
		{ IDC_SERIALHELP, "BTN_HELP" },
	};
	SerialDlgData *dlg_data = (SerialDlgData *)GetWindowLongPtr(Dialog, DWLP_USER);
	PTTSet ts = dlg_data == NULL ? NULL : dlg_data->pts;
	int i, w, sel;
	WORD Flow;

	switch (Message) {
		case WM_INITDIALOG:
			dlg_data = (SerialDlgData *)lParam;
			SetWindowLongPtr(Dialog, DWLP_USER, lParam);
			ts = dlg_data->pts;

			assert(dlg_data->ComPortInfoCount > 0);
			SetDlgTextsW(Dialog, TextInfos, _countof(TextInfos), ts->UILanguageFileW);

			w = 0;

			for (i = 0; i < dlg_data->ComPortInfoCount; i++) {
				ComPortInfo_t *p = dlg_data->ComPortInfoPtr + i;
				wchar_t *EntNameW;

				// MaxComPort を越えるポートは表示しない
				if (i > ts->MaxComPort) {
					continue;
				}

				aswprintf(&EntNameW, L"%s", p->port_name);
				SendDlgItemMessageW(Dialog, IDC_SERIALPORT, CB_ADDSTRING, 0, (LPARAM)EntNameW);
				free(EntNameW);

				if (p->port_no == ts->ComPort) {
					w = i;
				}
			}
			serial_dlg_set_comport_info(Dialog, dlg_data, w);
			SendDlgItemMessage(Dialog, IDC_SERIALPORT, CB_SETCURSEL, w, 0);

			SetDropDownList(Dialog, IDC_SERIALBAUD, BaudList, 0);
			i = sel = 0;
			while (BaudList[i] != NULL) {
				if ((WORD)atoi(BaudList[i]) == ts->Baud) {
					SendDlgItemMessage(Dialog, IDC_SERIALBAUD, CB_SETCURSEL, i, 0);
					sel = 1;
					break;
				}
				i++;
			}
			if (!sel) {
				SetDlgItemInt(Dialog, IDC_SERIALBAUD, ts->Baud, FALSE);
			}

			SetDropDownList(Dialog, IDC_SERIALDATA, DataList, ts->DataBit);
			SetDropDownList(Dialog, IDC_SERIALPARITY, ParityList, ts->Parity);
			SetDropDownList(Dialog, IDC_SERIALSTOP, StopList, ts->StopBit);

			/*
			 * value               display
			 * 1 IdFlowX           1 Xon/Xoff
			 * 2 IdFlowHard        2 RTS/CTS
			 * 3 IdFlowNone        4 none
			 * 4 IdFlowHardDsrDtr  3 DSR/DTR
			 */
			Flow = ts->Flow;
			if (Flow == 3) {
				Flow = 4;
			}
			else if (Flow == 4) {
				Flow = 3;
			}
			SetDropDownList(Dialog, IDC_SERIALFLOW, FlowList, Flow);

			SetDlgItemInt(Dialog,IDC_SERIALDELAYCHAR,ts->DelayPerChar,FALSE);
			SendDlgItemMessage(Dialog, IDC_SERIALDELAYCHAR, EM_LIMITTEXT,4, 0);

			SetDlgItemInt(Dialog,IDC_SERIALDELAYLINE,ts->DelayPerLine,FALSE);
			SendDlgItemMessage(Dialog, IDC_SERIALDELAYLINE, EM_LIMITTEXT,4, 0);

			CenterWindow(Dialog, GetParent(Dialog));

			// Edit controlをサブクラス化する。
			g_deltaSumSerialDlg = 0;
			g_defSerialDlgEditWndProc = (WNDPROC)SetWindowLongPtr(
				GetDlgItem(Dialog, IDC_SERIALTEXT),
				GWLP_WNDPROC,
				(LONG_PTR)SerialDlgEditWindowProc);

			// Combo-box controlをサブクラス化する。
			SetWindowLongPtrW(GetDlgItem(Dialog, IDC_SERIALBAUD), GWLP_USERDATA, (LONG_PTR)dlg_data);
			g_defSerialDlgSpeedComboboxWndProc = (WNDPROC)SetWindowLongPtr(
				GetDlgItem(Dialog, IDC_SERIALBAUD),
				GWLP_WNDPROC,
				(LONG_PTR)SerialDlgSpeedComboboxWindowProc);

			// 現在の接続状態と新しいポート番号の組み合わせで、接続処理が変わるため、
			// それに応じてOKボタンのラベル名を切り替える。
			serial_dlg_change_OK_button(Dialog, dlg_data->ComPortInfoPtr[w].port_no, ts->UILanguageFileW);

			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					if ( ts!=NULL ) {
						char Temp[128];
						memset(Temp, 0, sizeof(Temp));
						GetDlgItemText(Dialog, IDC_SERIALPORT, Temp, sizeof(Temp)-1);
						if (strncmp(Temp, "COM", 3) == 0 && Temp[3] != '\0') {
							ts->ComPort = (WORD)atoi(&Temp[3]);
						}

						GetDlgItemText(Dialog, IDC_SERIALBAUD, Temp, sizeof(Temp)-1);
						if (atoi(Temp) != 0) {
							ts->Baud = (DWORD)atoi(Temp);
						}
						if ((w = (WORD)GetCurSel(Dialog, IDC_SERIALDATA)) > 0) {
							ts->DataBit = w;
						}
						if ((w = (WORD)GetCurSel(Dialog, IDC_SERIALPARITY)) > 0) {
							ts->Parity = w;
						}
						if ((w = (WORD)GetCurSel(Dialog, IDC_SERIALSTOP)) > 0) {
							ts->StopBit = w;
						}
						if ((w = (WORD)GetCurSel(Dialog, IDC_SERIALFLOW)) > 0) {
							/*
							 * display    value
							 * 1 Xon/Xoff 1 IdFlowX
							 * 2 RTS/CTS  2 IdFlowHard
							 * 3 DSR/DTR  4 IdFlowHardDsrDtr
							 * 4 none     3 IdFlowNone
							 */
							Flow = w;
							if (Flow == 3) {
								Flow = 4;
							}
							else if (Flow == 4) {
								Flow = 3;
							}
							ts->Flow = Flow;
						}

						ts->DelayPerChar = GetDlgItemInt(Dialog,IDC_SERIALDELAYCHAR,NULL,FALSE);

						ts->DelayPerLine = GetDlgItemInt(Dialog,IDC_SERIALDELAYLINE,NULL,FALSE);

						ts->PortType = IdSerial;

						// ボーレートが変更されることがあるので、タイトル再表示の
						// メッセージを飛ばすようにした。 (2007.7.21 maya)
						PostMessage(GetParent(Dialog),WM_USER_CHANGETITLE,0,0);
					}

					// ツールチップを廃棄する
					if (g_SerialDlgSpeedTip) {
						TipWinDestroy(g_SerialDlgSpeedTip);
						g_SerialDlgSpeedTip = NULL;
					}

					EndDialog(Dialog, 1);
					return TRUE;

				case IDCANCEL:
					// ツールチップを廃棄する
					if (g_SerialDlgSpeedTip) {
						TipWinDestroy(g_SerialDlgSpeedTip);
						g_SerialDlgSpeedTip = NULL;
					}

					EndDialog(Dialog, 0);
					return TRUE;

				case IDC_SERIALHELP:
					PostMessage(GetParent(Dialog),WM_USER_DLGHELP2,HlpSetupSerialPort,0);
					return TRUE;

				case IDC_SERIALPORT:
					if (HIWORD(wParam) == CBN_SELCHANGE) {
						// リストからCOMポートが選択された
						int portno;
						sel = SendDlgItemMessage(Dialog, IDC_SERIALPORT, CB_GETCURSEL, 0, 0);
						portno = dlg_data->ComPortInfoPtr[sel].port_no;	 // ポート番号

						// 詳細情報を表示する
						serial_dlg_set_comport_info(Dialog, dlg_data, sel);

						// 現在の接続状態と新しいポート番号の組み合わせで、接続処理が変わるため、
						// それに応じてOKボタンのラベル名を切り替える。
						serial_dlg_change_OK_button(Dialog, portno, ts->UILanguageFileW);

						break;

					}

					return TRUE;
			}
	}
	return FALSE;
}

/**
 *	コンボボックスのホスト履歴をファイルに書き出す
 */
static void WriteComboBoxHostHistory(HWND dlg, int dlg_item, int maxhostlist, const wchar_t *SetupFNW)
{
	wchar_t EntNameW[10];
	LRESULT Index;
	int i;

	WritePrivateProfileStringW(L"Hosts", NULL, NULL, SetupFNW);

	Index = SendDlgItemMessageW(dlg, dlg_item, LB_GETCOUNT, 0, 0);
	if (Index == LB_ERR) {
		Index = 0;
	}
	else {
		Index--;
	}
	if (Index > MAXHOSTLIST) {
		Index = MAXHOSTLIST;
	}
	for (i = 1; i <= Index; i++) {
		wchar_t *strW;
		GetDlgItemIndexTextW(dlg, dlg_item, i - 1, &strW);
		_snwprintf_s(EntNameW, _countof(EntNameW), _TRUNCATE, L"Host%i", i);
		WritePrivateProfileStringW(L"Hosts", EntNameW, strW, SetupFNW);
		free(strW);
	}
}

static INT_PTR CALLBACK TCPIPDlg(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static const DlgTextInfo TextInfos[] = {
		{ 0, "DLG_TCPIP_TITLE" },
		{ IDC_TCPIPHOSTLIST, "DLG_TCPIP_HOSTLIST" },
		{ IDC_TCPIPADD, "DLG_TCPIP_ADD" },
		{ IDC_TCPIPUP, "DLG_TCPIP_UP" },
		{ IDC_TCPIPREMOVE, "DLG_TCPIP_REMOVE" },
		{ IDC_TCPIPDOWN, "DLG_TCPIP_DOWN" },
		{ IDC_TCPIPHISTORY, "DLG_TCPIP_HISTORY" },
		{ IDC_TCPIPAUTOCLOSE, "DLG_TCPIP_AUTOCLOSE" },
		{ IDC_TCPIPPORTLABEL, "DLG_TCPIP_PORT" },
		{ IDC_TCPIPTELNET, "DLG_TCPIP_TELNET" },
		{ IDC_TCPIPTELNETKEEPALIVELABEL, "DLG_TCPIP_KEEPALIVE" },
		{ IDC_TCPIPTELNETKEEPALIVESEC, "DLG_TCPIP_KEEPALIVE_SEC" },
		{ IDC_TCPIPTERMTYPELABEL, "DLG_TCPIP_TERMTYPE" },
		{ IDOK, "BTN_OK" },
		{ IDCANCEL, "BTN_CANCEL" },
		{ IDC_TCPIPHELP, "BTN_HELP" },
	};
	PTTSet ts;
	UINT i, Index;
	WORD w;

	switch (Message) {
		case WM_INITDIALOG:
			ts = (PTTSet)lParam;
			SetWindowLongPtr(Dialog, DWLP_USER, lParam);

			SetDlgTextsW(Dialog, TextInfos, _countof(TextInfos), ts->UILanguageFileW);

			SendDlgItemMessage(Dialog, IDC_TCPIPHOST, EM_LIMITTEXT,
			                   HostNameMaxLength-1, 0);

			SetComboBoxHostHistory(Dialog, IDC_TCPIPLIST, MAXHOSTLIST, ts->SetupFNameW);

			/* append a blank item to the bottom */
			SendDlgItemMessage(Dialog, IDC_TCPIPLIST, LB_ADDSTRING, 0, 0);
			SetRB(Dialog,ts->HistoryList,IDC_TCPIPHISTORY,IDC_TCPIPHISTORY);
			SetRB(Dialog,ts->AutoWinClose,IDC_TCPIPAUTOCLOSE,IDC_TCPIPAUTOCLOSE);
			SetDlgItemInt(Dialog,IDC_TCPIPPORT,ts->TCPPort,FALSE);
			SetDlgItemInt(Dialog,IDC_TCPIPTELNETKEEPALIVE,ts->TelKeepAliveInterval,FALSE);
			SetRB(Dialog,ts->Telnet,IDC_TCPIPTELNET,IDC_TCPIPTELNET);
			SetDlgItemText(Dialog, IDC_TCPIPTERMTYPE, ts->TermType);
			SendDlgItemMessage(Dialog, IDC_TCPIPTERMTYPE, EM_LIMITTEXT, sizeof(ts->TermType)-1, 0);

			// SSH接続のときにも TERM を送るので、telnetが無効でも disabled にしない。(2005.11.3 yutaka)
			EnableDlgItem(Dialog,IDC_TCPIPTERMTYPELABEL,IDC_TCPIPTERMTYPE);

			CenterWindow(Dialog, GetParent(Dialog));

			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK: {
					BOOL Ok;

					ts = (PTTSet)GetWindowLongPtr(Dialog,DWLP_USER);
					assert(ts!=NULL);
					WriteComboBoxHostHistory(Dialog, IDC_TCPIPLIST, MAXHOSTLIST, ts->SetupFNameW);
					GetRB(Dialog,&ts->HistoryList,IDC_TCPIPHISTORY,IDC_TCPIPHISTORY);
					GetRB(Dialog,&ts->AutoWinClose,IDC_TCPIPAUTOCLOSE,IDC_TCPIPAUTOCLOSE);
					ts->TCPPort = GetDlgItemInt(Dialog,IDC_TCPIPPORT,&Ok,FALSE);
					if (! Ok) {
						ts->TCPPort = ts->TelPort;
					}
					ts->TelKeepAliveInterval = GetDlgItemInt(Dialog,IDC_TCPIPTELNETKEEPALIVE,&Ok,FALSE);
					GetRB(Dialog,&ts->Telnet,IDC_TCPIPTELNET,IDC_TCPIPTELNET);
					GetDlgItemText(Dialog, IDC_TCPIPTERMTYPE, ts->TermType,
								   sizeof(ts->TermType));
					EndDialog(Dialog, 1);
					return TRUE;
				}
				case IDCANCEL:
					EndDialog(Dialog, 0);
					return TRUE;

				case IDC_TCPIPHOST:
					if (HIWORD(wParam)==EN_CHANGE) {
						wchar_t *host;
						hGetDlgItemTextW(Dialog, IDC_TCPIPHOST, &host);
						if (wcslen(host)==0) {
							DisableDlgItem(Dialog,IDC_TCPIPADD,IDC_TCPIPADD);
						}
						else {
							EnableDlgItem(Dialog,IDC_TCPIPADD,IDC_TCPIPADD);
						}
						free(host);
					}
					break;

				case IDC_TCPIPADD: {
					wchar_t *host;
					hGetDlgItemTextW(Dialog, IDC_TCPIPHOST, &host);
					if (wcslen(host) > 0) {
						Index = SendDlgItemMessage(Dialog,IDC_TCPIPLIST,LB_GETCURSEL,0,0);
						if (Index==(UINT)LB_ERR) {
							Index = 0;
						}

						SendDlgItemMessageW(Dialog, IDC_TCPIPLIST, LB_INSERTSTRING,
						                   Index, (LPARAM)host);

						SetDlgItemText(Dialog, IDC_TCPIPHOST, 0);
						SetFocus(GetDlgItem(Dialog, IDC_TCPIPHOST));
					}
					free(host);
					break;
				}
				case IDC_TCPIPLIST:
					if (HIWORD(wParam)==LBN_SELCHANGE) {
						i = SendDlgItemMessage(Dialog,IDC_TCPIPLIST,LB_GETCOUNT,0,0);
						Index = SendDlgItemMessage(Dialog, IDC_TCPIPLIST, LB_GETCURSEL, 0, 0);
						if ((i<=1) || (Index==(UINT)LB_ERR) || (Index==i-1)) {
							DisableDlgItem(Dialog,IDC_TCPIPUP,IDC_TCPIPDOWN);
						}
						else {
							EnableDlgItem(Dialog,IDC_TCPIPREMOVE,IDC_TCPIPREMOVE);
							if (Index==0) {
								DisableDlgItem(Dialog,IDC_TCPIPUP,IDC_TCPIPUP);
							}
							else {
								EnableDlgItem(Dialog,IDC_TCPIPUP,IDC_TCPIPUP);
							}
							if (Index>=i-2) {
								DisableDlgItem(Dialog,IDC_TCPIPDOWN,IDC_TCPIPDOWN);
							}
							else {
								EnableDlgItem(Dialog,IDC_TCPIPDOWN,IDC_TCPIPDOWN);
							}
						}
					}
					break;

				case IDC_TCPIPUP:
				case IDC_TCPIPDOWN: {
					wchar_t *host;
					i = SendDlgItemMessage(Dialog,IDC_TCPIPLIST,LB_GETCOUNT,0,0);
					Index = SendDlgItemMessage(Dialog, IDC_TCPIPLIST, LB_GETCURSEL, 0, 0);
					if (Index==(UINT)LB_ERR) {
						return TRUE;
					}
					if (LOWORD(wParam)==IDC_TCPIPDOWN) {
						Index++;
					}
					if ((Index==0) || (Index>=i-1)) {
						return TRUE;
					}
					GetDlgItemIndexTextW(Dialog, IDC_TCPIPLIST, Index, &host);
					SendDlgItemMessage(Dialog, IDC_TCPIPLIST, LB_DELETESTRING,
					                   Index, 0);
					SendDlgItemMessageW(Dialog, IDC_TCPIPLIST, LB_INSERTSTRING,
					                   Index-1, (LPARAM)host);
					free(host);
					if (LOWORD(wParam)==IDC_TCPIPUP) {
						Index--;
					}
					SendDlgItemMessage(Dialog, IDC_TCPIPLIST, LB_SETCURSEL,Index,0);
					if (Index==0) {
						DisableDlgItem(Dialog,IDC_TCPIPUP,IDC_TCPIPUP);
					}
					else {
						EnableDlgItem(Dialog,IDC_TCPIPUP,IDC_TCPIPUP);
					}
					if (Index>=i-2) {
						DisableDlgItem(Dialog,IDC_TCPIPDOWN,IDC_TCPIPDOWN);
					}
					else {
						EnableDlgItem(Dialog,IDC_TCPIPDOWN,IDC_TCPIPDOWN);
					}
					SetFocus(GetDlgItem(Dialog, IDC_TCPIPLIST));
					break;
				}

				case IDC_TCPIPREMOVE: {
					wchar_t *host;
					i = SendDlgItemMessage(Dialog,IDC_TCPIPLIST,LB_GETCOUNT,0,0);
					Index = SendDlgItemMessage(Dialog,IDC_TCPIPLIST,LB_GETCURSEL, 0, 0);
					if ((Index==(UINT)LB_ERR) ||
						(Index==i-1)) {
						return TRUE;
					}
					GetDlgItemIndexTextW(Dialog, IDC_TCPIPLIST, Index, &host);
					SendDlgItemMessage(Dialog, IDC_TCPIPLIST, LB_DELETESTRING,
					                   Index, 0);
					SetDlgItemTextW(Dialog, IDC_TCPIPHOST, host);
					DisableDlgItem(Dialog,IDC_TCPIPUP,IDC_TCPIPDOWN);
					SetFocus(GetDlgItem(Dialog, IDC_TCPIPHOST));
					free(host);
					break;
				}

				case IDC_TCPIPTELNET:
					GetRB(Dialog,&w,IDC_TCPIPTELNET,IDC_TCPIPTELNET);
					if (w==1) {
						EnableDlgItem(Dialog,IDC_TCPIPTERMTYPELABEL,IDC_TCPIPTERMTYPE);
						ts = (PTTSet)GetWindowLongPtr(Dialog,DWLP_USER);
						if (ts!=NULL) {
							SetDlgItemInt(Dialog,IDC_TCPIPPORT,ts->TelPort,FALSE);
						}
					}
					else {
						// SSH接続のときにも TERM を送るので、telnetが無効でも disabled にしない。(2005.11.3 yutaka)
						EnableDlgItem(Dialog,IDC_TCPIPTERMTYPELABEL,IDC_TCPIPTERMTYPE);
					}
					break;

				case IDC_TCPIPHELP:
					PostMessage(GetParent(Dialog),WM_USER_DLGHELP2,HlpSetupTCPIP,0);
					break;
			}
	}
	return FALSE;
}

typedef struct {
	PGetHNRec GetHNRec;
	ComPortInfo_t *ComPortInfoPtr;
	int ComPortInfoCount;
} TTXHostDlgData;

static INT_PTR CALLBACK HostDlg(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static const DlgTextInfo TextInfos[] = {
		{ 0, "DLG_HOST_TITLE" },
		{ IDC_HOSTNAMELABEL, "DLG_HOST_TCPIPHOST" },
		{ IDC_HOSTTCPPORTLABEL, "DLG_HOST_TCPIPPORT" },
		{ IDC_HOSTTCPPROTOCOLLABEL, "DLG_HOST_TCPIPPROTOCOL" },
		{ IDC_HOSTSERIAL, "DLG_HOST_SERIAL" },
		{ IDC_HOSTCOMLABEL, "DLG_HOST_SERIALPORT" },
		{ IDOK, "BTN_OK" },
		{ IDCANCEL, "BTN_CANCEL" },
		{ IDC_HOSTHELP, "BTN_HELP" },
	};
	TTXHostDlgData *dlg_data = (TTXHostDlgData *)GetWindowLongPtr(Dialog, DWLP_USER);
	PGetHNRec GetHNRec = dlg_data != NULL ? dlg_data->GetHNRec : NULL;

	switch (Message) {
		case WM_INITDIALOG: {
			WORD i;
			int j;
			GetHNRec = (PGetHNRec)lParam;
			dlg_data = (TTXHostDlgData *)calloc(sizeof(*dlg_data), 1);
			SetWindowLongPtr(Dialog, DWLP_USER, (LPARAM)dlg_data);
			dlg_data->GetHNRec = GetHNRec;

			dlg_data->ComPortInfoPtr = ComPortInfoGet(&dlg_data->ComPortInfoCount);

			SetDlgTextsW(Dialog, TextInfos, _countof(TextInfos), ts.UILanguageFileW);

			// ファイルおよび名前付きパイプの場合、TCP/IP扱いとする。
			if ( GetHNRec->PortType==IdFile ||
				 GetHNRec->PortType==IdNamedPipe
				) {
				GetHNRec->PortType = IdTCPIP;
			}

			SetComboBoxHostHistory(Dialog, IDC_HOSTNAME, MAXHOSTLIST, GetHNRec->SetupFNW);
			ExpandCBWidth(Dialog, IDC_HOSTNAME);

			SendDlgItemMessage(Dialog, IDC_HOSTNAME, EM_LIMITTEXT,
			                   HostNameMaxLength-1, 0);

			SendDlgItemMessage(Dialog, IDC_HOSTNAME, CB_SETCURSEL,0,0);

			SetEditboxEmacsKeybind(Dialog, IDC_HOSTNAME);

			SetRB(Dialog,GetHNRec->Telnet,IDC_HOSTTELNET,IDC_HOSTTELNET);
			SendDlgItemMessage(Dialog, IDC_HOSTTCPPORT, EM_LIMITTEXT,5,0);
			SetDlgItemInt(Dialog,IDC_HOSTTCPPORT,GetHNRec->TCPPort,FALSE);
			for (i=0; ProtocolFamilyList[i]; ++i) {
				SendDlgItemMessage(Dialog, IDC_HOSTTCPPROTOCOL, CB_ADDSTRING,
				                   0, (LPARAM)ProtocolFamilyList[i]);
			}
			SendDlgItemMessage(Dialog, IDC_HOSTTCPPROTOCOL, CB_SETCURSEL,0,0);

			j = 0;
			for (i = 0; i < dlg_data->ComPortInfoCount; i++) {
				ComPortInfo_t *p = dlg_data->ComPortInfoPtr + i;
				wchar_t *EntNameW;
				int index;

				// MaxComPort を越えるポートは表示しない
				if (i > GetHNRec->MaxComPort) {
					continue;
				}
				j++;

				// 使用中のポートは表示しない
				if (CheckCOMFlag(p->port_no) == 1) {
					continue;
				}

				if (p->friendly_name == NULL) {
					aswprintf(&EntNameW, L"%s", p->port_name);
				}
				else {
					aswprintf(&EntNameW, L"%s: %s", p->port_name, p->friendly_name);
				}
				index = (int)SendDlgItemMessageW(Dialog, IDC_HOSTCOM, CB_ADDSTRING, 0, (LPARAM)EntNameW);
				SendDlgItemMessageA(Dialog, IDC_HOSTCOM, CB_SETITEMDATA, index, i);
				free(EntNameW);
			}
			if (j>0) {
				SendDlgItemMessage(Dialog, IDC_HOSTCOM, CB_SETCURSEL,0,0);
			}
			else { /* All com ports are already used */
				GetHNRec->PortType = IdTCPIP;
				DisableDlgItem(Dialog,IDC_HOSTSERIAL,IDC_HOSTSERIAL);
			}
			ExpandCBWidth(Dialog, IDC_HOSTCOM);

			SetRB(Dialog,GetHNRec->PortType,IDC_HOSTTCPIP,IDC_HOSTSERIAL);

			if ( GetHNRec->PortType==IdTCPIP ) {
				DisableDlgItem(Dialog,IDC_HOSTCOMLABEL,IDC_HOSTCOM);
			}
			else {
				DisableDlgItem(Dialog,IDC_HOSTNAMELABEL,IDC_HOSTTCPPORT);
				DisableDlgItem(Dialog,IDC_HOSTTCPPROTOCOLLABEL,IDC_HOSTTCPPROTOCOL);
			}

			CenterWindow(Dialog, GetParent(Dialog));

			return TRUE;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK: {
					if (IsDlgButtonChecked(Dialog, IDC_HOSTTCPIP)) {
						int i;
						BOOL Ok;

						GetHNRec->PortType = IdTCPIP;
						GetDlgItemTextW(Dialog, IDC_HOSTNAME, GetHNRec->HostName, HostNameMaxLength);
						GetRB(Dialog,&GetHNRec->Telnet,IDC_HOSTTELNET,IDC_HOSTTELNET);
						i = GetDlgItemInt(Dialog,IDC_HOSTTCPPORT,&Ok,FALSE);
						if (Ok) {
							GetHNRec->TCPPort = i;
						}
						i = (int)SendDlgItemMessage(Dialog, IDC_HOSTTCPPROTOCOL, CB_GETCURSEL, 0, 0);
						GetHNRec->ProtocolFamily =
							i == 0 ? AF_UNSPEC :
							i == 1 ? AF_INET6 : AF_INET;
					}
					else {
						int pos;
						int index;

						GetHNRec->PortType = IdSerial;
						GetHNRec->HostName[0] = 0;
						pos = (int)SendDlgItemMessageA(Dialog, IDC_HOSTCOM, CB_GETCURSEL, 0, 0);
						assert(pos >= 0);
						index = (int)SendDlgItemMessageA(Dialog, IDC_HOSTCOM, CB_GETITEMDATA, pos, 0);
						GetHNRec->ComPort = dlg_data->ComPortInfoPtr[index].port_no;
					}
					EndDialog(Dialog, 1);
					return TRUE;
				}
				case IDCANCEL:
					EndDialog(Dialog, 0);
					return TRUE;

				case IDC_HOSTTCPIP:
					EnableDlgItem(Dialog,IDC_HOSTNAMELABEL,IDC_HOSTTCPPORT);
					EnableDlgItem(Dialog,IDC_HOSTTCPPROTOCOLLABEL,IDC_HOSTTCPPROTOCOL);
					DisableDlgItem(Dialog,IDC_HOSTCOMLABEL,IDC_HOSTCOM);
					return TRUE;

				case IDC_HOSTSERIAL:
					EnableDlgItem(Dialog,IDC_HOSTCOMLABEL,IDC_HOSTCOM);
					DisableDlgItem(Dialog,IDC_HOSTNAMELABEL,IDC_HOSTTCPPORT);
					DisableDlgItem(Dialog,IDC_HOSTTCPPROTOCOLLABEL,IDC_HOSTTCPPROTOCOL);
					break;

				case IDC_HOSTTELNET: {
					WORD i;
					GetRB(Dialog,&i,IDC_HOSTTELNET,IDC_HOSTTELNET);
					if ( i==1 ) {
						SetDlgItemInt(Dialog,IDC_HOSTTCPPORT,GetHNRec->TelPort,FALSE);
					}
					break;
				}

				case IDC_HOSTHELP:
					PostMessage(GetParent(Dialog),WM_USER_DLGHELP2,HlpFileNewConnection,0);
					break;
			}
			break;
		case WM_DESTROY:
			ComPortInfoFree(dlg_data->ComPortInfoPtr, dlg_data->ComPortInfoCount);
			free(dlg_data);
			break;
	}
	return FALSE;
}

static INT_PTR CALLBACK DirDlg(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static const DlgTextInfo TextInfos[] = {
		{ 0, "DLG_DIR_TITLE" },
		{ IDC_DIRCURRENT_LABEL, "DLG_DIR_CURRENT" },
		{ IDC_DIRNEW_LABEL, "DLG_DIR_NEW" },
		{ IDOK, "BTN_OK" },
		{ IDCANCEL, "BTN_CANCEL" },
		{ IDC_DIRHELP, "BTN_HELP" },
	};

	switch (Message) {
		case WM_INITDIALOG: {
			PTTSet ts;
			wchar_t *CurDir;
			RECT R;
			HDC TmpDC;
			SIZE s;
			HWND HDir, HSel, HOk, HCancel, HHelp;
			POINT D, B, S;
			int WX, WY, WW, WH, CW, DW, DH, BW, BH, SW, SH;

			ts = (PTTSet)lParam;
			CurDir = ts->FileDirW;
			SetWindowLongPtr(Dialog, DWLP_USER, lParam);
			SetDlgTextsW(Dialog, TextInfos, _countof(TextInfos), ts->UILanguageFileW);
			SetDlgItemTextW(Dialog, IDC_DIRCURRENT, CurDir);

// adjust dialog size
			// get size of current dir text
			HDir = GetDlgItem(Dialog, IDC_DIRCURRENT);
			GetWindowRect(HDir,&R);
			D.x = R.left;
			D.y = R.top;
			ScreenToClient(Dialog,&D);
			DH = R.bottom-R.top;
			TmpDC = GetDC(Dialog);
			GetTextExtentPoint32W(TmpDC,CurDir,(int)wcslen(CurDir),&s);
			ReleaseDC(Dialog,TmpDC);
			DW = s.cx + s.cx/10;

			// get button size
			HOk = GetDlgItem(Dialog, IDOK);
			HCancel = GetDlgItem(Dialog, IDCANCEL);
			HHelp = GetDlgItem(Dialog, IDC_DIRHELP);
			GetWindowRect(HHelp,&R);
			B.x = R.left;
			B.y = R.top;
			ScreenToClient(Dialog,&B);
			BW = R.right-R.left;
			BH = R.bottom-R.top;

			// calc new dialog size
			GetWindowRect(Dialog,&R);
			WX = R.left;
			WY = R.top;
			WW = R.right-R.left;
			WH = R.bottom-R.top;
			GetClientRect(Dialog,&R);
			CW = R.right-R.left;
			if (D.x+DW < CW) {
				DW = CW-D.x;
			}
			WW = WW + D.x+DW - CW;

			// resize current dir text
			MoveWindow(HDir,D.x,D.y,DW,DH,TRUE);
			// move buttons
			MoveWindow(HOk,(D.x+DW-4*BW)/2,B.y,BW,BH,TRUE);
			MoveWindow(HCancel,(D.x+DW-BW)/2,B.y,BW,BH,TRUE);
			MoveWindow(HHelp,(D.x+DW+2*BW)/2,B.y,BW,BH,TRUE);
			// resize edit box
			HDir = GetDlgItem(Dialog, IDC_DIRNEW);
			GetWindowRect(HDir,&R);
			D.x = R.left;
			D.y = R.top;
			ScreenToClient(Dialog,&D);
			DW = R.right-R.left;
			if (DW<s.cx) {
				DW = s.cx;
			}
			MoveWindow(HDir,D.x,D.y,DW,R.bottom-R.top,TRUE);
			// select dir button
			HSel = GetDlgItem(Dialog, IDC_SELECT_DIR);
			GetWindowRect(HSel, &R);
			SW = R.right-R.left;
			SH = R.bottom-R.top;
			S.x = R.bottom;
			S.y = R.top;
			ScreenToClient(Dialog, &S);
			MoveWindow(HSel, D.x + DW + 8, S.y, SW, SH, TRUE);
			WW = WW + SW;

			// resize dialog
			MoveWindow(Dialog,WX,WY,WW,WH,TRUE);

			CenterWindow(Dialog, GetParent(Dialog));

			return TRUE;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK: {
					PTTSet ts = (PTTSet)GetWindowLongPtr(Dialog,DWLP_USER);
					BOOL OK = FALSE;
					wchar_t *current_dir;
					wchar_t *new_dir;
					hGetCurrentDirectoryW(&current_dir);
					hGetDlgItemTextW(Dialog, IDC_DIRNEW, &new_dir);
					if (wcslen(new_dir) > 0) {
						wchar_t *FileDirExpanded;
						hExpandEnvironmentStringsW(new_dir, &FileDirExpanded);
						if (DoesFolderExistW(FileDirExpanded)) {
							free(ts->FileDirW);
							ts->FileDirW = new_dir;
							OK = TRUE;
						}
						else {
							free(new_dir);
						}
						free(FileDirExpanded);
					}
					SetCurrentDirectoryW(current_dir);
					free(current_dir);
					if (OK) {
						EndDialog(Dialog, 1);
					}
					else {
						static const TTMessageBoxInfoW info = {
							"Tera Term",
							"MSG_TT_ERROR", L"Tera Term: Error",
							"MSG_FIND_DIR_ERROR", L"Cannot find directory",
							MB_ICONEXCLAMATION
						};
						TTMessageBoxW(Dialog, &info, ts->UILanguageFileW);
					}
					return TRUE;
				}

				case IDCANCEL:
					EndDialog(Dialog, 0);
					return TRUE;

				case IDC_SELECT_DIR: {
					PTTSet ts = (PTTSet)GetWindowLongPtr(Dialog,DWLP_USER);
					wchar_t *uimsgW;
					wchar_t *buf;
					wchar_t *FileDirExpanded;
					GetI18nStrWW("Tera Term", "DLG_SELECT_DIR_TITLE", L"Select new directory", ts->UILanguageFileW, &uimsgW);
					hGetDlgItemTextW(Dialog, IDC_DIRNEW, &buf);
					if (buf[0] == 0) {
						free(buf);
						hGetDlgItemTextW(Dialog, IDC_DIRCURRENT, &buf);
					}
					hExpandEnvironmentStringsW(buf, &FileDirExpanded);
					free(buf);
					if (doSelectFolderW(Dialog, FileDirExpanded, uimsgW, &buf)) {
						SetDlgItemTextW(Dialog, IDC_DIRNEW, buf);
						free(buf);
					}
					free(FileDirExpanded);
					free(uimsgW);
					return TRUE;
				}

				case IDC_DIRHELP:
					PostMessage(GetParent(Dialog),WM_USER_DLGHELP2,HlpFileChangeDir,0);
					break;
			}
	}
	return FALSE;
}


//
// static textに書かれたURLをダブルクリックすると、ブラウザが起動するようにする。
// based on sakura editor 1.5.2.1 # CDlgAbout.cpp
// (2005.4.7 yutaka)
//

typedef struct {
	WNDPROC proc;
	BOOL mouseover;
	HFONT font;
	HWND hWnd;
	int timer_done;
} url_subclass_t;

static url_subclass_t author_url_class;

// static textに割り当てるプロシージャ
static LRESULT CALLBACK UrlWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	url_subclass_t *parent = (url_subclass_t *)GetWindowLongPtr( hWnd, GWLP_USERDATA );
	HDC hdc;
	POINT pt;
	RECT rc;

	switch (msg) {
#if 0
	case WM_SETCURSOR:
		{
			// カーソル形状変更
			HCURSOR hc;

			hc = (HCURSOR)LoadImage(NULL,
					MAKEINTRESOURCE(IDC_HAND),
					IMAGE_CURSOR,
					0,
					0,
					LR_DEFAULTSIZE | LR_SHARED);
			if (hc != NULL) {
				SetClassLongPtr(hWnd, GCLP_HCURSOR, (LONG_PTR)hc);
			}
			return (LRESULT)0;
		}
#endif

	// シングルクリックでブラウザが起動するように変更する。(2015.11.16 yutaka)
	//case WM_LBUTTONDBLCLK:
	case WM_LBUTTONDOWN:
	{
			char url[128];

			// get URL
			SendMessage(hWnd, WM_GETTEXT , sizeof(url), (LPARAM)url);
			// kick WWW browser
			ShellExecute(NULL, NULL, url, NULL, NULL,SW_SHOWNORMAL);
		}
		break;

	case WM_MOUSEMOVE:
		{
			BOOL bHilighted;
			pt.x = LOWORD( lParam );
			pt.y = HIWORD( lParam );
			GetClientRect( hWnd, &rc );
			bHilighted = PtInRect( &rc, pt );

			if (parent->mouseover != bHilighted) {
				parent->mouseover = bHilighted;
				InvalidateRect( hWnd, NULL, TRUE );
				if (parent->timer_done == 0) {
					parent->timer_done = 1;
					SetTimer( hWnd, 1, 200, NULL );
				}
			}

		}
		break;

	case WM_TIMER:
		// URLの上にマウスカーソルがあるなら、システムカーソルを変更する。
		if (author_url_class.mouseover) {
			HCURSOR hc;
			//SetCapture(hWnd);

			hc = (HCURSOR)LoadImage(NULL, MAKEINTRESOURCE(IDC_HAND),
			                        IMAGE_CURSOR, 0, 0,
			                        LR_DEFAULTSIZE | LR_SHARED);

			SetSystemCursor(CopyCursor(hc), 32512 /* OCR_NORMAL */);    // 矢印
			SetSystemCursor(CopyCursor(hc), 32513 /* OCR_IBEAM */);     // Iビーム

		} else {
			//ReleaseCapture();
			// マウスカーソルを元に戻す。
			SystemParametersInfo(SPI_SETCURSORS, 0, NULL, 0);

		}

		// カーソルがウィンドウ外にある場合にも WM_MOUSEMOVE を送る
		GetCursorPos( &pt );
		ScreenToClient( hWnd, &pt );
		GetClientRect( hWnd, &rc );
		if( !PtInRect( &rc, pt ) ) {
			SendMessage( hWnd, WM_MOUSEMOVE, 0, MAKELONG( pt.x, pt.y ) );
		}
		break;

	case WM_PAINT:
		{
		// ウィンドウの描画
		PAINTSTRUCT ps;
		HFONT hFont;
		HFONT hOldFont;
		TCHAR szText[512];

		hdc = BeginPaint( hWnd, &ps );

		// 現在のクライアント矩形、テキスト、フォントを取得する
		GetClientRect( hWnd, &rc );
		GetWindowText( hWnd, szText, 512 );
		hFont = (HFONT)SendMessage( hWnd, WM_GETFONT, (WPARAM)0, (LPARAM)0 );

		// テキスト描画
		SetBkMode( hdc, TRANSPARENT );
		SetTextColor( hdc, parent->mouseover ? RGB( 0x84, 0, 0 ): RGB( 0, 0, 0xff ) );
		hOldFont = (HFONT)SelectObject( hdc, (HGDIOBJ)hFont );
		TextOut( hdc, 2, 0, szText, lstrlen( szText ) );
		SelectObject( hdc, (HGDIOBJ)hOldFont );

		// フォーカス枠描画
		if( GetFocus() == hWnd )
			DrawFocusRect( hdc, &rc );

		EndPaint( hWnd, &ps );
		return 0;
		}

	case WM_ERASEBKGND:
		hdc = (HDC)wParam;
		GetClientRect( hWnd, &rc );

		// 背景描画
		if( parent->mouseover ){
			// ハイライト時背景描画
			SetBkColor( hdc, RGB( 0xff, 0xff, 0 ) );
			ExtTextOut( hdc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL );
		}else{
			// 親にWM_CTLCOLORSTATICを送って背景ブラシを取得し、背景描画する
			HBRUSH hbr;
			HBRUSH hbrOld;

			hbr = (HBRUSH)SendMessage( GetParent( hWnd ), WM_CTLCOLORSTATIC, wParam, (LPARAM)hWnd );
			hbrOld = (HBRUSH)SelectObject( hdc, hbr );
			FillRect( hdc, &rc, hbr );
			SelectObject( hdc, hbrOld );
		}
		return (LRESULT)1;

	case WM_DESTROY:
		// 後始末
		SetWindowLongPtr( hWnd, GWLP_WNDPROC, (LONG_PTR)parent->proc );
		if( parent->font != NULL ) {
			DeleteObject( parent->font );
		}

		// マウスカーソルを元に戻す。
		SystemParametersInfo(SPI_SETCURSORS, 0, NULL, 0);
		return (LRESULT)0;
	}

	return CallWindowProc( parent->proc, hWnd, msg, wParam, lParam );
}

// static textにプロシージャを設定し、サブクラス化する。
static void do_subclass_window(HWND hWnd, url_subclass_t *parent)
{
	HFONT hFont;
	LOGFONT lf;

	//SetCapture(hWnd);

	if (!IsWindow(hWnd)) {
		return;
	}

	// 親のプロシージャをサブクラスから参照できるように、ポインタを登録しておく。
	SetWindowLongPtr( hWnd, GWLP_USERDATA, (LONG_PTR)parent );
	// サブクラスのプロシージャを登録する。
	parent->proc = (WNDPROC)SetWindowLongPtr( hWnd, GWLP_WNDPROC, (LONG_PTR)UrlWndProc);

	// 下線を付ける
	hFont = (HFONT)SendMessage( hWnd, WM_GETFONT, (WPARAM)0, (LPARAM)0 );
	GetObject( hFont, sizeof(lf), &lf );
	lf.lfUnderline = TRUE;
	parent->font = hFont = CreateFontIndirect( &lf ); // 不要になったら削除すること
	if (hFont != NULL) {
		SendMessage( hWnd, WM_SETFONT, (WPARAM)hFont, (LPARAM)FALSE );
	}

	parent->hWnd = hWnd;
	parent->timer_done = 0;
}

#if defined(_MSC_VER)
// ビルドしたときに使われたVisual C++のバージョンを取得する(2009.3.3 yutaka)
static void GetCompilerInfo(char *buf, size_t buf_size)
{
	char tmpbuf[128];
	int msc_ver, vs_ver, msc_low_ver;

	strcpy_s(buf, buf_size, "Microsoft Visual C++");
#ifdef _MSC_FULL_VER
	// _MSC_VER  VS Ver.  VS internal Ver.  MSVC++ Ver.
	// 1400      2005     8.0               8.0
	// 1500      2008     9.0               9.0
	// 1600      2010     10.0              10.0
	// 1700      2012     11.0              11.0
	// 1800      2013     12.0              12.0
	// 1900      2015     14.0              14.0
	// 1910      2017     15.0              14.10
	// 1910      2017     15.1              14.10
	// 1910      2017     15.2              14.10
	// 1911      2017     15.3.x            14.11
	// 1911      2017     15.4.x            14.11
	// 1912      2017     15.5.x            14.12
	// 1913      2017     15.6.x            14.13
	// 1914      2017     15.7.x            14.14
	// 1915      2017     15.8.x            14.15
	// 1916      2017     15.9.x            14.16
	// 1920      2019     16.0.x            14.20
	// 1921      2019     16.1.x            14.21
	// 1929      2019     16.11.x           14.29
	// 1930      2022     17.0.x            14.30
	// 1936      2022     17.6.x            14.36
	msc_ver = (_MSC_FULL_VER / 10000000);
	msc_low_ver = (_MSC_FULL_VER / 100000) % 100;
	if (msc_ver < 19) {
		vs_ver = msc_ver - 6;
	}
	else {
		vs_ver = msc_ver - 5;
	}

	_snprintf_s(tmpbuf, sizeof(tmpbuf), _TRUNCATE, " %d.%d",
				vs_ver,
				msc_low_ver);
	strncat_s(buf, buf_size, tmpbuf, _TRUNCATE);
	if (_MSC_FULL_VER % 100000) {
		_snprintf_s(tmpbuf, sizeof(tmpbuf), _TRUNCATE, " build %d",
					_MSC_FULL_VER % 100000);
		strncat_s(buf, buf_size, tmpbuf, _TRUNCATE);
	}
#elif defined(_MSC_VER)
	_snprintf_s(tmpbuf, sizeof(tmpbuf), _TRUNCATE, " %d.%d",
				(_MSC_VER / 100) - 6,
				_MSC_VER % 100);
	strncat_s(buf, buf_size, tmpbuf, _TRUNCATE);
#endif
}

#elif defined(__MINGW32__)
static void GetCompilerInfo(char *buf, size_t buf_size)
{
#if defined(__GNUC__) || defined(__clang__)
	_snprintf_s(buf, buf_size, _TRUNCATE,
				"mingw " __MINGW64_VERSION_STR " "
#if defined(__clang__)
				"clang " __clang_version__
#elif defined(__GNUC__)
				"gcc " __VERSION__
#endif
		);
#else
	strncat_s(buf, buf_size, "mingw", _TRUNCATE);
#endif
}

#else
static void GetCompilerInfo(char *buf, size_t buf_size)
{
	strncpy_s(buf, buf_size, "unknown compiler");
}
#endif

#if defined(WDK_NTDDI_VERSION)
// ビルドしたときに使われた SDK のバージョンを取得する
// URL: https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/
// バージョン番号には
// (1) Visual Studio でプロジェクトのプロパティの "Windows SDK バージョン" に列挙されるバージョン
// (2) インストールされた SDK が「アプリと機能」で表示されるバージョン
// (3) 上記 URL での表示バージョン
// (4) インストール先フォルダ名
// があるが、最後のブロックの数字は同じにならないことが多い。
// e.g. (1) 10.0.18362.0, (2) 10.0.18362.1, (3) 10.0.18362.1, (4) 10.0.18362.0
//      (1) 10.0.22000.0, (2) 10.0.22000.832, (3) 10.0.22000.832, (4) 10.0.22000.0
static void GetSDKInfo(char *buf, size_t buf_size)
{
	if (WDK_NTDDI_VERSION >= 0x0A00000B) {
		strncpy_s(buf, buf_size, "Windows SDK", _TRUNCATE);
		switch (WDK_NTDDI_VERSION) {
			case 0x0A00000B: // NTDDI_WIN10_CO
				             // 10.0.22000.194 も存在するが判定できない
				strncat_s(buf, buf_size, " for Windows 11 (10.0.22000.832)", _TRUNCATE);
				break;
			case 0x0A00000C: // NTDDI_WIN10_NI
				             // 10.0.22621.1 も存在するが判定できない
				             // (2)インストーラ, (3)URL では別扱いの 10.0.22621.1778 も同じ判定になる
				             // AppVeyor で使われているはずのバージョン番号を返す
				strncat_s(buf, buf_size, " for Windows 11 (10.0.22621.755)", _TRUNCATE);
				break;
			default: {
				char str[32];
				sprintf_s(str, sizeof(str), " (NTDDI_VERSION=0x%08X)",
				          WDK_NTDDI_VERSION);
				strncat_s(buf, buf_size, str, _TRUNCATE);
				break;
			}
		}
	}
	else if (WDK_NTDDI_VERSION >= 0x0A000000) {
		strncpy_s(buf, buf_size, "Windows 10 SDK", _TRUNCATE);
		switch (WDK_NTDDI_VERSION) {
			case 0x0A000000: // NTDDI_WIN10, 1507
				strncat_s(buf, buf_size, " (10.0.10240.0)", _TRUNCATE);
				break;
			case 0x0A000001: // NTDDI_WIN10_TH2, 1511
				strncat_s(buf, buf_size, " Version 1511 (10.0.10586.212)", _TRUNCATE);
				break;
			case 0x0A000002: // NTDDI_WIN10_RS1, 1607
				strncat_s(buf, buf_size, " Version 1607 (10.0.14393.795)", _TRUNCATE);
				break;
			case 0x0A000003: // NTDDI_WIN10_RS2, 1703
				strncat_s(buf, buf_size, " Version 1703 (10.0.15063.468)", _TRUNCATE);
				break;
			case 0x0A000004: // NTDDI_WIN10_RS3, 1709
				strncat_s(buf, buf_size, " Version 1709 (10.0.16299.91)", _TRUNCATE);
				break;
			case 0x0A000005: // NTDDI_WIN10_RS4, 1803
				strncat_s(buf, buf_size, " Version 1803 (10.0.17134.12)", _TRUNCATE);
				break;
			case 0x0A000006: // NTDDI_WIN10_RS5, 1809
				strncat_s(buf, buf_size, " Version 1809 (10.0.17763.132)", _TRUNCATE);
				break;
			case 0x0A000007: // NTDDI_WIN10_19H1, 1903
				strncat_s(buf, buf_size, " Version 1903 (10.0.18362.1)", _TRUNCATE);
				break;
			case 0x0A000008: // NTDDI_WIN10_VB, 2004
				strncat_s(buf, buf_size, " Version 2004 (10.0.19041.685)", _TRUNCATE);
				break;
			case 0x0A000009: // NTDDI_WIN10_MN, 2004? cf. _PCW_REGISTRATION_INFORMATION
				strncat_s(buf, buf_size, " Version 2004 (10.0.19645.0)", _TRUNCATE);
				break;
			case 0x0A00000A: // NTDDI_WIN10_FE, 2104
				strncat_s(buf, buf_size, " Version 2104 (10.0.20348.1)", _TRUNCATE);
				break;
			default:
				strncat_s(buf, buf_size, " (unknown)", _TRUNCATE);
				break;
		}
	}
	else {
		strncpy_s(buf, buf_size, "Windows SDK unknown", _TRUNCATE);
	}
}
#else
static void GetSDKInfo(char *buf, size_t buf_size)
{
	strncpy_s(buf, buf_size, "Windows SDK unknown", _TRUNCATE);
}
#endif

// static text のサイズを変更
static void FitControlSize(HWND Dlg, UINT id)
{
	HDC hdc;
	RECT r;
	DWORD dwExt;
	WORD w, h;
	HWND hwnd;
	POINT point;
	wchar_t *text;
	size_t text_len;

	hwnd = GetDlgItem(Dlg, id);
	hdc = GetDC(hwnd);
	SelectObject(hdc, (HFONT)SendMessage(Dlg, WM_GETFONT, 0, 0));
	hGetDlgItemTextW(Dlg, id, &text);
	text_len = wcslen(text);
	dwExt = GetTabbedTextExtentW(hdc, text, (int)text_len, 0, NULL);
	free(text);
	w = LOWORD(dwExt) + 5; // 幅が若干足りないので補正
	h = HIWORD(dwExt);
	GetWindowRect(hwnd, &r);
	point.x = r.left;
	point.y = r.top;
	ScreenToClient(Dlg, &point);
	MoveWindow(hwnd, point.x, point.y, w, h, TRUE);
}

static INT_PTR CALLBACK AboutDlg(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static const DlgTextInfo TextInfos[] = {
		{ 0, "DLG_ABOUT_TITLE" },
	};
	char buf[128];
#if 0
	HDC hdc;
	HWND hwnd;
#endif
	static HICON dlghicon = NULL;

#if defined(EFFECT_ENABLED) || defined(TEXTURE_ENABLED)
	// for animation
	static HDC dlgdc = NULL;
	static int dlgw, dlgh;
	static HBITMAP dlgbmp = NULL, dlgprevbmp = NULL;
	static LPDWORD dlgpixel = NULL;
	static HICON dlghicon = NULL;
	const int icon_x = 15, icon_y = 10, icon_w = 32, icon_h = 32;
	const int ID_EFFECT_TIMER = 1;
	RECT dlgrc = {0};
	BITMAPINFO       bmi;
	BITMAPINFOHEADER bmiHeader;
	int x, y;
#define POS(x,y) ((x) + (y)*dlgw)
	static short *wavemap = NULL;
	static short *wavemap_old = NULL;
	static LPDWORD dlgorgpixel = NULL;
	static int waveflag = 0;
	static int fullcolor = 0;
	int bitspixel;
#endif

	switch (Message) {
		case WM_INITDIALOG:
			// アイコンを動的にセット
			{
#if defined(EFFECT_ENABLED) || defined(TEXTURE_ENABLED)
				int fuLoad = LR_DEFAULTCOLOR;
				HICON hicon;
				if (IsWindowsNT4()) {
					fuLoad = LR_VGACOLOR;
				}
				hicon = LoadImage(hInst, MAKEINTRESOURCE(IDI_TTERM),
				                  IMAGE_ICON, icon_w, icon_h, fuLoad);
				// Picture Control に描画すると、なぜか透過色が透過にならず、黒となってしまうため、
				// WM_PAINT で描画する。
				dlghicon = hicon;
#else
				SetDlgItemIcon(Dialog, IDC_TT_ICON, MAKEINTRESOURCEW(IDI_TTERM), 0, 0);
#endif
			}

			SetDlgTextsW(Dialog, TextInfos, _countof(TextInfos), ts.UILanguageFileW);

			// Tera Term 本体のバージョン
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "Version %d.%d ", TT_VERSION_MAJOR, TT_VERSION_MINOR);
			{
				char *substr = GetVersionSubstr();
				strncat_s(buf, sizeof(buf), substr, _TRUNCATE);
				free(substr);
			}
			SetDlgItemTextA(Dialog, IDC_TT_VERSION, buf);

			// Onigurumaのバージョンを設定する
			// バージョンの取得は onig_version() を呼び出すのが適切だが、そのためだけにライブラリを
			// リンクしたくなかったので、以下のようにした。Onigurumaのバージョンが上がった場合、
			// ビルドエラーとなるかもしれない。
			// (2005.10.8 yutaka)
			// ライブラリをリンクし、正規の手順でバージョンを得ることにした。
			// (2006.7.24 yutaka)
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "Oniguruma %s", onig_version());
			SetDlgItemTextA(Dialog, IDC_ONIGURUMA_LABEL, buf);

			// SFMTのバージョンを設定する
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "SFMT %s", SFMT_VERSION);
			SetDlgItemTextA(Dialog, IDC_SFMT_VERSION, buf);

			// build info
			{
				// コンパイラ、日時、SDK
				char *info;
				char tmpbuf[128];
				char sdk[128];
				GetCompilerInfo(tmpbuf, sizeof(tmpbuf));
				GetSDKInfo(sdk, _countof(sdk));
				asprintf(&info,
						 "Build info:\r\n"
						 "  Compiler: %s\r\n"
						 "  Date and Time: %s %s\r\n"
						 "  SDK: %s",
						 tmpbuf,
						 __DATE__, __TIME__,
						 sdk);

				SetDlgItemTextA(Dialog, IDC_BUILDTOOL, info);
				free(info);
			}

			FitControlSize(Dialog, IDC_AUTHOR_URL);

			// static textをサブクラス化する。ただし、tabstop, notifyプロパティを有効にしておかないと
			// メッセージが拾えない。(2005.4.5 yutaka)
			do_subclass_window(GetDlgItem(Dialog, IDC_AUTHOR_URL), &author_url_class);

#if defined(EFFECT_ENABLED) || defined(TEXTURE_ENABLED)
			/*
			 * ダイアログのビットマップ化を行い、背景にエフェクトをかけられるようにする。
			 * (2011.5.7 yutaka)
			 */
			// ダイアログのサイズ
			GetWindowRect(Dialog, &dlgrc);
			dlgw = dlgrc.right - dlgrc.left;
			dlgh = dlgrc.bottom - dlgrc.top;
			// ビットマップの作成
			dlgdc = CreateCompatibleDC(NULL);
			ZeroMemory(&bmiHeader, sizeof(BITMAPINFOHEADER));
			bmiHeader.biSize      = sizeof(BITMAPINFOHEADER);
			bmiHeader.biWidth     = dlgw;
			bmiHeader.biHeight    = -dlgh;
			bmiHeader.biPlanes    = 1;
			bmiHeader.biBitCount  = 32;
			bmi.bmiHeader = bmiHeader;
			dlgbmp = CreateDIBSection(NULL, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, &dlgpixel, NULL, 0);
			dlgprevbmp = (HBITMAP)SelectObject(dlgdc, dlgbmp);
			// ビットマップの背景色（朝焼けっぽい）を作る。
			for (y = 0 ; y < dlgh ; y++) {
				double dx = (double)(255 - 180) / dlgw;
				double dy = (double)255/dlgh;
				BYTE r, g, b;
				for (x = 0 ; x < dlgw ; x++) {
					r = min((int)(180+dx*x), 255);
					g = min((int)(180+dx*x), 255);
					b = max((int)(255-y*dx), 0);
					// 画素の並びは、下位バイトからB, G, R, Aとなる。
					dlgpixel[POS(x, y)] = b | g << 8 | r << 16;
				}
			}
			// 2D Water effect 用
			wavemap = calloc(sizeof(short), dlgw * dlgh);
			wavemap_old = calloc(sizeof(short), dlgw * dlgh);
			dlgorgpixel = calloc(sizeof(DWORD), dlgw * dlgh);
			memcpy(dlgorgpixel, dlgpixel, dlgw * dlgh * sizeof(DWORD));

			srand((unsigned int)time(NULL));


#ifdef EFFECT_ENABLED
			// エフェクトタイマーの開始
			SetTimer(Dialog, ID_EFFECT_TIMER, 100, NULL);
#endif

			// 画面の色数を調べる。
			hwnd = GetDesktopWindow();
			hdc = GetDC(hwnd);
			bitspixel = GetDeviceCaps(hdc, BITSPIXEL);
			fullcolor = (bitspixel == 32 ? 1 : 0);
			ReleaseDC(hwnd, hdc);
#endif

			CenterWindow(Dialog, GetParent(Dialog));

			return TRUE;

		case WM_COMMAND:
#ifdef EFFECT_ENABLED
			switch (LOWORD(wParam)) {
				int val;
				case IDOK:
					val = 1;
				case IDCANCEL:
					val = 0;
					KillTimer(Dialog, ID_EFFECT_TIMER);

					SelectObject(dlgdc, dlgprevbmp);
					DeleteObject(dlgbmp);
					DeleteDC(dlgdc);
					dlgdc = NULL;
					dlgprevbmp = dlgbmp = NULL;

					free(wavemap);
					free(wavemap_old);
					free(dlgorgpixel);

					EndDialog(Dialog, val);
					return TRUE;
			}
#else
			switch (LOWORD(wParam)) {
				case IDOK:
					EndDialog(Dialog, 1);
					return TRUE;
				case IDCANCEL:
					EndDialog(Dialog, 0);
					return TRUE;
			}
#endif
			break;

#if defined(EFFECT_ENABLED) || defined(TEXTURE_ENABLED)
		// static textの背景を透過させる。
		case WM_CTLCOLORSTATIC:
			SetBkMode((HDC)wParam, TRANSPARENT);
			return (BOOL)GetStockObject( NULL_BRUSH );
			break;

#ifdef EFFECT_ENABLED
		case WM_ERASEBKGND:
			return 0;
#endif

		case WM_PAINT:
			if (dlgdc) {
				PAINTSTRUCT ps;
				hdc = BeginPaint(Dialog, &ps);

				if (fullcolor) {
					BitBlt(hdc,
						ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
						dlgdc,
						ps.rcPaint.left, ps.rcPaint.top,
						SRCCOPY);
				}

				DrawIconEx(hdc, icon_x, icon_y, dlghicon, icon_w, icon_h, 0, 0, DI_NORMAL);

				EndPaint(Dialog, &ps);
			}
			break;

		case WM_MOUSEMOVE:
			{
				int xpos, ypos;
				static int idx = 0;
				short amplitudes[4] = {250, 425, 350, 650};

				xpos = LOWORD(lParam);
				ypos = HIWORD(lParam);

				wavemap[POS(xpos,ypos)] = amplitudes[idx++];
				idx %= 4;
			}
			break;

		case WM_TIMER:
			if (wParam == ID_EFFECT_TIMER)
			{
				int x, y;
				short height, xdiff;
				short *p_new, *p_old;

				if (waveflag == 0) {
					p_new = wavemap;
					p_old = wavemap_old;
				} else {
					p_new = wavemap_old;
					p_old = wavemap;
				}
				waveflag ^= 1;

				// 水面の計算
				// アルゴリズムは下記サイト(2D Water)より。
				// cf. http://freespace.virgin.net/hugo.elias/graphics/x_water.htm
				for (y = 1; y < dlgh - 1 ; y++) {
					for (x = 1; x < dlgw - 1 ; x++) {
						height = (p_new[POS(x,y-1)] +
							      p_new[POS(x-1,y)] +
							      p_new[POS(x+1,y)] +
							      p_new[POS(x,y+1)]) / 2 - p_old[POS(x,y)];
						height -= (height >> 5);
						p_old[POS(x,y)] = height;
					}
				}

				// 水面の描画
				for (y = 1; y < dlgh - 1 ; y++) {
					for (x = 1; x < dlgw - 1 ; x++) {
						xdiff = p_old[POS(x+1,y)] - p_old[POS(x,y)];
						dlgpixel[POS(x,y)] = dlgorgpixel[POS(x + xdiff, y)];
					}
				}

#if 0
				hdc = GetDC(Dialog);
				BitBlt(hdc,
					0, 0, dlgw, dlgh,
					dlgdc,
					0, 0,
					SRCCOPY);
				ReleaseDC(Dialog, hdc);
#endif

				InvalidateRect(Dialog, NULL, FALSE);
			}
			break;
#endif
		case WM_DPICHANGED:
			SendDlgItemMessage(Dialog, IDC_TT_ICON, Message, wParam, lParam);
			break;

		case WM_DESTROY:
			DestroyIcon(dlghicon);
			dlghicon = NULL;
			break;
	}
	return FALSE;
}

static const wchar_t **LangUIList = NULL;
#define LANG_EXT L".lng"

static const wchar_t *get_lang_folder()
{
	return (IsWindowsNTKernel()) ? L"lang_utf16le" : L"lang";
}

// メモリフリー
static void free_lang_ui_list()
{
	if (LangUIList) {
		const wchar_t **p = LangUIList;
		while (*p) {
			free((void *)*p);
			p++;
		}
		free((void *)LangUIList);
		LangUIList = NULL;
	}
}

static int make_sel_lang_ui(const wchar_t *dir)
{
	int    i;
	int    file_num;
	wchar_t *fullpath;
	HANDLE hFind;
	WIN32_FIND_DATAW fd;

	free_lang_ui_list();

	aswprintf(&fullpath, L"%s\\%s\\*%s", dir, get_lang_folder(), LANG_EXT);

	file_num = 0;
	hFind = FindFirstFileW(fullpath, &fd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				file_num++;
			}
		} while(FindNextFileW(hFind, &fd));
		FindClose(hFind);
	}

	file_num++;  // NULL
	LangUIList = calloc(file_num, sizeof(wchar_t *));

	i = 0;
	hFind = FindFirstFileW(fullpath, &fd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				LangUIList[i++] = _wcsdup(fd.cFileName);
			}
		} while(FindNextFileW(hFind, &fd) && i < file_num);
		FindClose(hFind);
	}
	LangUIList[i] = NULL;
	free(fullpath);

	return i;
}

static int get_sel_lang_ui(const wchar_t **list, const wchar_t *selstr)
{
	size_t n = 0;
	const wchar_t **p = list;

	if (selstr == NULL || selstr[0] == '\0') {
		n = 0;  // English
	} else {
		while (*p) {
			if (wcsstr(selstr, *p)) {
				n = p - list;
				break;
			}
			p++;
		}
	}

	return (int)(n + 1);  // 1origin
}

static INT_PTR CALLBACK GenDlg(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static const DlgTextInfo TextInfos[] = {
		{ 0, "DLG_GEN_TITLE" },
		{ IDC_GENPORT_LABEL, "DLG_GEN_PORT" },
		{ IDC_GENLANGLABEL, "DLG_GEN_LANG" },
		{ IDC_GENLANGUI_LABEL, "DLG_GEN_LANG_UI" },
		{ IDOK, "BTN_OK" },
		{ IDCANCEL, "BTN_CANCEL" },
		{ IDC_GENHELP, "BTN_HELP" },
	};
	static int langui_sel = 1, uilist_count = 0;
	PTTSet ts;
	WORD w;

	switch (Message) {
		case WM_INITDIALOG:
			ts = (PTTSet)lParam;
			SetWindowLongPtr(Dialog, DWLP_USER, lParam);

			SetDlgTextsW(Dialog, TextInfos, _countof(TextInfos), ts->UILanguageFileW);

			SendDlgItemMessageA(Dialog, IDC_GENPORT, CB_ADDSTRING,
			                   0, (LPARAM)"TCP/IP");
			for (w=1;w<=ts->MaxComPort;w++) {
				char Temp[8];
				_snprintf_s(Temp, sizeof(Temp), _TRUNCATE, "COM%d", w);
				SendDlgItemMessageA(Dialog, IDC_GENPORT, CB_ADDSTRING,
				                   0, (LPARAM)Temp);
			}
			if (ts->PortType==IdSerial) {
				if (ts->ComPort <= ts->MaxComPort) {
					w = ts->ComPort;
				}
				else {
					w = 1; // COM1
				}
			}
			else {
				w = 0; // TCP/IP
			}
			SendDlgItemMessage(Dialog, IDC_GENPORT, CB_SETCURSEL,w,0);

			if ((ts->MenuFlag & MF_NOLANGUAGE)==0) {
				int sel = 0;
				int i;
				ShowDlgItem(Dialog,IDC_GENLANGLABEL,IDC_GENLANG);
				for (i = 0;; i++) {
					const TLanguageList *lang = GetLanguageList(i);
					if (lang == NULL) {
						break;
					}
					if (ts->Language == lang->language) {
						sel = i;
					}
					SendDlgItemMessageA(Dialog, IDC_GENLANG, CB_ADDSTRING, 0, (LPARAM)lang->str);
					SendDlgItemMessageA(Dialog, IDC_GENLANG, CB_SETITEMDATA, i, (LPARAM)lang->language);
				}
				SendDlgItemMessage(Dialog, IDC_GENLANG, CB_SETCURSEL, sel, 0);
			}

			// 最初に指定されている言語ファイルの番号を覚えておく。
			uilist_count = make_sel_lang_ui(ts->ExeDirW);
			langui_sel = get_sel_lang_ui(LangUIList, ts->UILanguageFileW_ini);
			if (LangUIList[0] != NULL) {
				int i = 0;
				while (LangUIList[i] != 0) {
					SendDlgItemMessageW(Dialog, IDC_GENLANG_UI, CB_ADDSTRING, 0, (LPARAM)LangUIList[i]);
					i++;
				}
				SendDlgItemMessage(Dialog, IDC_GENLANG_UI, CB_SETCURSEL, langui_sel - 1, 0);
			}
			else {
				EnableWindow(GetDlgItem(Dialog, IDC_GENLANG_UI), FALSE);
			}

			CenterWindow(Dialog, GetParent(Dialog));

			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					ts = (PTTSet)GetWindowLongPtr(Dialog,DWLP_USER);
					if (ts!=NULL) {
						w = (WORD)GetCurSel(Dialog, IDC_GENPORT);
						if (w>1) {
							ts->PortType = IdSerial;
							ts->ComPort = w-1;
						}
						else {
							ts->PortType = IdTCPIP;
						}

						if ((ts->MenuFlag & MF_NOLANGUAGE)==0) {
							WORD language;
							w = (WORD)GetCurSel(Dialog, IDC_GENLANG);
							language = (int)SendDlgItemMessageA(Dialog, IDC_GENLANG, CB_GETITEMDATA, w - 1, 0);

							// Language が変更されたとき、
							// KanjiCode/KanjiCodeSend を変更先の Language に存在する値に置き換える
							if (1 <= language && language < IdLangMax && language != ts->Language) {
								WORD KanjiCode = ts->KanjiCode;
								WORD KanjiCodeSend = ts->KanjiCodeSend;
								ts->KanjiCode = KanjiCodeTranslate(language,KanjiCode);
								ts->KanjiCodeSend = KanjiCodeTranslate(language,KanjiCodeSend);

								ts->Language = language;
							}

						}

						// 言語ファイルが変更されていた場合
						w = (WORD)GetCurSel(Dialog, IDC_GENLANG_UI);
						if (1 <= w && w <= uilist_count && w != langui_sel) {
							aswprintf(&ts->UILanguageFileW_ini, L"%s\\%s", get_lang_folder(), LangUIList[w - 1]);
							WideCharToACP_t(ts->UILanguageFileW_ini, ts->UILanguageFile_ini, sizeof(ts->UILanguageFile_ini));

							ts->UILanguageFileW = GetUILanguageFileFullW(ts->ExeDirW, ts->UILanguageFileW_ini);
							WideCharToACP_t(ts->UILanguageFileW, ts->UILanguageFile, sizeof(ts->UILanguageFile));

							// タイトルの更新を行う。(2014.2.23 yutaka)
							PostMessage(GetParent(Dialog),WM_USER_CHANGETITLE,0,0);
						}
					}

					// TTXKanjiMenu は Language を見てメニューを表示するので、変更の可能性がある
					// OK 押下時にメニュー再描画のメッセージを飛ばすようにした。 (2007.7.14 maya)
					// 言語ファイルの変更時にメニューの再描画が必要 (2012.5.5 maya)
					PostMessage(GetParent(Dialog),WM_USER_CHANGEMENU,0,0);

					EndDialog(Dialog, 1);
					return TRUE;

				case IDCANCEL:
					EndDialog(Dialog, 0);
					return TRUE;

				case IDC_GENHELP:
					PostMessage(GetParent(Dialog),WM_USER_DLGHELP2,HlpSetupGeneral,0);
					break;
			}
			break;

		case WM_DESTROY:
			free_lang_ui_list();
			break;
	}
	return FALSE;
}

static INT_PTR CALLBACK WinListDlg(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static const DlgTextInfo TextInfos[] = {
		{ 0, "DLG_WINLIST_TITLE" },
		{ IDC_WINLISTLABEL, "DLG_WINLIST_LABEL" },
		{ IDOK, "DLG_WINLIST_OPEN" },
		{ IDCANCEL, "BTN_CANCEL" },
		{ IDC_WINLISTCLOSE, "DLG_WINLIST_CLOSEWIN" },
		{ IDC_WINLISTHELP, "BTN_HELP" },
	};
	PBOOL Close;
	int n;
	HWND Hw;

	switch (Message) {
		case WM_INITDIALOG:
			Close = (PBOOL)lParam;
			SetWindowLongPtr(Dialog, DWLP_USER, lParam);

			SetDlgTextsW(Dialog, TextInfos, _countof(TextInfos), ts.UILanguageFileW);

			SetWinList(GetParent(Dialog),Dialog,IDC_WINLISTLIST);

			CenterWindow(Dialog, GetParent(Dialog));

			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					n = SendDlgItemMessage(Dialog,IDC_WINLISTLIST,
					LB_GETCURSEL, 0, 0);
					if (n!=CB_ERR) {
						SelectWin(n);
					}
					EndDialog(Dialog, 1);
					return TRUE;

				case IDCANCEL:
					EndDialog(Dialog, 0);
					return TRUE;

				case IDC_WINLISTLIST:
					if (HIWORD(wParam)==LBN_DBLCLK) {
						PostMessage(Dialog,WM_COMMAND,IDOK,0);
					}
					break;

				case IDC_WINLISTCLOSE:
					n = SendDlgItemMessage(Dialog,IDC_WINLISTLIST,
					LB_GETCURSEL, 0, 0);
					if (n==CB_ERR) {
						break;
					}
					Hw = GetNthWin(n);
					if (Hw!=GetParent(Dialog)) {
						if (! IsWindowEnabled(Hw)) {
							MessageBeep(0);
							break;
						}
						SendDlgItemMessage(Dialog,IDC_WINLISTLIST,
						                   LB_DELETESTRING,n,0);
						PostMessage(Hw,WM_SYSCOMMAND,SC_CLOSE,0);
					}
					else {
						Close = (PBOOL)GetWindowLongPtr(Dialog,DWLP_USER);
						if (Close!=NULL) {
							*Close = TRUE;
						}
						EndDialog(Dialog, 1);
						return TRUE;
					}
					break;

				case IDC_WINLISTHELP:
					PostMessage(GetParent(Dialog),WM_USER_DLGHELP2,HlpWindowWindow,0);
					break;
			}
	}
	return FALSE;
}

BOOL WINAPI _SetupTerminal(HWND WndParent, PTTSet ts)
{
	int i;

	switch (ts->Language) {
	case IdJapanese: // Japanese mode
		i = IDD_TERMDLGJ;
		break;
	case IdKorean: // Korean mode //HKS
	case IdUtf8:   // UTF-8 mode
	case IdChinese:
	case IdRussian: // Russian mode
	case IdEnglish:  // English mode
		i = IDD_TERMDLGK;
		break;
	default:
		// 使っていない
		i = IDD_TERMDLG;
	}

	return
		(BOOL)TTDialogBoxParam(hInst,
							   MAKEINTRESOURCE(i),
							   WndParent, TermDlg, (LPARAM)ts);
}

BOOL WINAPI _SetupWin(HWND WndParent, PTTSet ts)
{
	WinDlgWork *work = (WinDlgWork *)calloc(sizeof(*work), 1);
	INT_PTR r;
	work->ts = ts;
	work->SampleFont = ts->SampleFont;
	work->VTFlag = ts->VTFlag;
	r = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_WINDLG), WndParent, WinDlg, (LPARAM)work);
	free(work);
	return (BOOL)r;
}

BOOL WINAPI _SetupKeyboard(HWND WndParent, PTTSet ts)
{
	return
		(BOOL)DialogBoxParam(hInst,
		                     MAKEINTRESOURCE(IDD_KEYBDLG),
		                     WndParent, KeybDlg, (LPARAM)ts);
}

BOOL WINAPI _SetupSerialPort(HWND WndParent, PTTSet ts)
{
	BOOL r;
	SerialDlgData *dlg_data = calloc(sizeof(*dlg_data), 1);
	dlg_data->pts = ts;
	dlg_data->ComPortInfoPtr = ComPortInfoGet(&dlg_data->ComPortInfoCount);
	if (dlg_data->ComPortInfoCount == 0) {
		static const TTMessageBoxInfoW info = {
			"Tera Term",
			"MSG_TT_NOTICE", L"Tera Term: Notice",
			NULL, L"No serial port",
			MB_ICONINFORMATION | MB_OK
		};
		TTMessageBoxW(WndParent, &info, ts->UILanguageFileW);
		return FALSE; // 変更しなかった
	}

	r = (BOOL)DialogBoxParam(hInst,
							 MAKEINTRESOURCE(IDD_SERIALDLG),
							 WndParent, SerialDlg, (LPARAM)dlg_data);

	ComPortInfoFree(dlg_data->ComPortInfoPtr, dlg_data->ComPortInfoCount);
	free(dlg_data);
	return r;
}

BOOL WINAPI _SetupTCPIP(HWND WndParent, PTTSet ts)
{
	return
		(BOOL)DialogBoxParam(hInst,
		                     MAKEINTRESOURCE(IDD_TCPIPDLG),
		                     WndParent, TCPIPDlg, (LPARAM)ts);
}

BOOL WINAPI _GetHostName(HWND WndParent, PGetHNRec GetHNRec)
{
	return
		(BOOL)DialogBoxParam(hInst,
		                     MAKEINTRESOURCE(IDD_HOSTDLG),
		                     WndParent, HostDlg, (LPARAM)GetHNRec);
}

BOOL WINAPI _ChangeDirectory(HWND WndParent, PTTSet ts)
{
	return
		(BOOL)DialogBoxParam(hInst,
		                     MAKEINTRESOURCE(IDD_DIRDLG),
		                     WndParent, DirDlg, (LPARAM)ts);
}

BOOL WINAPI _AboutDialog(HWND WndParent)
{
	return
		(BOOL)DialogBox(hInst,
		                MAKEINTRESOURCE(IDD_ABOUTDLG),
		                WndParent, AboutDlg);
}

static UINT_PTR CALLBACK TFontHook(HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message) {
		case WM_INITDIALOG:
		{
			PTTSet ts;
			wchar_t *uimsg;

			//EnableWindow(GetDlgItem(Dialog, cmb2), FALSE);
			ts = (PTTSet)((CHOOSEFONTA *)lParam)->lCustData;

			GetI18nStrWW("Tera Term", "DLG_CHOOSEFONT_STC6", L"\"Font style\" selection here won't affect actual font appearance.",
						 ts->UILanguageFileW, &uimsg);
			SetDlgItemTextW(Dialog, stc6, uimsg);
			free(uimsg);

			SetFocus(GetDlgItem(Dialog,cmb1));

			CenterWindow(Dialog, GetParent(Dialog));

			break;
		}
#if 0
		case WM_COMMAND:
			if (LOWORD(wParam) == cmb2) {
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					// フォントの変更による(メッセージによる)スタイルの変更では
					// cmb2 からの通知が来ない
					SendMessage(GetDlgItem(Dialog, cmb2), CB_GETCURSEL, 0, 0);
				}
			}
			else if (LOWORD(wParam) == cmb1) {
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					// フォントの変更前に一時保存されたスタイルが
					// ここを抜けたあとに改めてセットされてしまうようだ
					SendMessage(GetDlgItem(Dialog, cmb2), CB_GETCURSEL, 0, 0);
				}
			}
			break;
#endif
	}
	return FALSE;
}

BOOL WINAPI _ChooseFontDlg(HWND WndParent, LPLOGFONTA LogFont, PTTSet ts)
{
	CHOOSEFONTA cf;
	BOOL Ok;

	memset(&cf, 0, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.hwndOwner = WndParent;
	cf.lpLogFont = LogFont;
	cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT |
	           CF_FIXEDPITCHONLY | CF_SHOWHELP | CF_NOVERTFONTS |
	           CF_ENABLEHOOK;
	if (ts->ListHiddenFonts) {
		cf.Flags |= CF_INACTIVEFONTS;
	}
	cf.lpfnHook = (LPCFHOOKPROC)(&TFontHook);
	cf.nFontType = REGULAR_FONTTYPE;
	cf.hInstance = hInst;
	cf.lCustData = (LPARAM)ts;
	Ok = ChooseFontA(&cf);
	return Ok;
}

BOOL WINAPI _SetupGeneral(HWND WndParent, PTTSet ts)
{
	return
		(BOOL)DialogBoxParam(hInst,
		                     MAKEINTRESOURCE(IDD_GENDLG),
		                     WndParent, GenDlg, (LPARAM)ts);
}

BOOL WINAPI _WindowWindow(HWND WndParent, PBOOL Close)
{
	*Close = FALSE;
	return
		(BOOL)DialogBoxParam(hInst,
		                     MAKEINTRESOURCE(IDD_WINLISTDLG),
		                     WndParent,
							 WinListDlg, (LPARAM)Close);
}
