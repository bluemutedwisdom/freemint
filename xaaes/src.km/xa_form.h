/*
 * $Id$
 * 
 * XaAES - XaAES Ain't the AES (c) 1992 - 1998 C.Graham
 *                                 1999 - 2003 H.Robbers
 *                                        2004 F.Naumann & O.Skancke
 *
 * A multitasking AES replacement for FreeMiNT
 *
 * This file is part of XaAES.
 *
 * XaAES is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * XaAES is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with XaAES; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _xa_form_h
#define _xa_form_h

#include "global.h"
#include "xa_types.h"

int do_form_alert(enum locks lock, struct xa_client *client, int default_button, char *alert);
//void center_form(OBJECT *form, int bars);
//void display_toolbar(enum locks lock, struct xa_window *wind, int item);

//void init_form_do(enum locks lock, XA_TREE *wt, OBJECT *form, int item, bool draw);
//void set_form_do(enum locks lock, struct xa_client *client, OBJECT *form, int item);

//void set_button_timer(enum locks lock, struct xa_window *wind);

//SendMessage handle_form_window;

//ClassicClick click_form_do;

//ExitForm
//	XA_form_exit,
//	exit_toolbar,
//	exit_wdial,
//	exit_form_do,
//	exit_form_dial,
//	classic_exit_form_do;

AES_function
	XA_form_center,
	XA_form_dial,
	XA_form_button,
	XA_form_alert,
	XA_form_do,
	XA_form_error,
	XA_form_keybd;

#endif /* _xa_form_h */
