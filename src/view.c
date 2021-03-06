/* $Id$ */
static char const _copyright[] =
"Copyright (c) 2007-2012 Pierre Pronchery <khorben@defora.org>";
/* This file is part of DeforaOS Desktop Browser */
static char const _license[] =
"view is free software; you can redistribute it and/or modify it under the\n"
"terms of the GNU General Public License version 3 as published by the Free\n"
"Software Foundation.\n"
"\n"
"view is distributed in the hope that it will be useful, but WITHOUT ANY\n"
"WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS\n"
"FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more\n"
"details.\n"
"\n"
"You should have received a copy of the GNU General Public License along with\n"
"view; if not, see <http://www.gnu.org/licenses/>.\n";
/* TODO:
 * - zoom/dezoom images */



#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <Desktop.h>
#include "../config.h"
#define _(string) gettext(string)
#define N_(string) (string)

#include "common.c"

/* constants */
#define PROGNAME	"view"

#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR	PREFIX "/share"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR	DATADIR "/locale"
#endif


/* view */
/* private */
/* types */
typedef struct _View
{
	char * pathname;

	/* widgets */
	GtkWidget * window;
	GtkWidget * ab_window;
} View;


/* constants */
#ifndef EMBEDDED
static char const * _view_authors[] =
{
	"Pierre Pronchery <khorben@defora.org>",
	NULL
};
#endif


/* prototypes */
static View * _view_new(char const * path);
static View * _view_new_open(void);
static void _view_delete(View * view);

/* useful */
static int _view_error(View * view, char const * message, int ret);

/* callbacks */
#ifdef EMBEDDED
static void _on_close(gpointer data);
#endif
static gboolean _on_closex(gpointer data);
#ifndef EMBEDDED
static void _on_file_edit(gpointer data);
static void _on_file_open_with(gpointer data);
static void _on_file_close(gpointer data);
static void _on_help_contents(gpointer data);
static void _on_help_about(gpointer data);
#else
static void _on_edit(gpointer data);
static void _on_open_with(gpointer data);
#endif


/* constants */
#ifndef EMBEDDED
static DesktopMenu _view_menu_file[] =
{
	{ N_("Open _with..."), G_CALLBACK(_on_file_open_with), NULL, 0, 0 },
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Close"), G_CALLBACK(_on_file_close), GTK_STOCK_CLOSE,
		GDK_CONTROL_MASK, GDK_KEY_W },
	{ NULL, NULL, NULL, 0, 0 }
};

static DesktopMenu _view_menu_file_edit[] =
{
	{ N_("_Edit"), G_CALLBACK(_on_file_edit), GTK_STOCK_EDIT,
		GDK_CONTROL_MASK, GDK_KEY_E },
	{ N_("Open _with..."), G_CALLBACK(_on_file_open_with), NULL, 0, 0 },
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Close"), G_CALLBACK(_on_file_close), GTK_STOCK_CLOSE,
		GDK_CONTROL_MASK, GDK_KEY_W },
	{ NULL, NULL, NULL, 0, 0 }
};

static DesktopMenu _view_menu_help[] =
{
	{ N_("Contents"), G_CALLBACK(_on_help_contents), "help-contents", 0,
		GDK_KEY_F1 },
# if GTK_CHECK_VERSION(2, 6, 0)
	{ N_("_About"), G_CALLBACK(_on_help_about), GTK_STOCK_ABOUT, 0, 0 },
# else
	{ N_("_About"), G_CALLBACK(_on_help_about), NULL, 0, 0 },
# endif
	{ NULL, NULL, NULL, 0, 0 }
};

static DesktopMenubar _view_menubar[] =
{
	{ N_("_File"), _view_menu_file },
	{ N_("_Help"), _view_menu_help },
	{ NULL, NULL }
};

static DesktopMenubar _view_menubar_edit[] =
{
	{ N_("_File"), _view_menu_file_edit },
	{ N_("_Help"), _view_menu_help },
	{ NULL, NULL }
};
#else
static DesktopAccel _view_accel[] =
{
	{ G_CALLBACK(_on_close), GDK_CONTROL_MASK, GDK_KEY_W },
	{ NULL, 0, 0 }
};

static DesktopToolbar _view_toolbar[] =
{
	{ N_("Open with..."), G_CALLBACK(_on_open_with), GTK_STOCK_OPEN,
		GDK_CONTROL_MASK, GDK_O, NULL },
	{ N_("Edit"), G_CALLBACK(_on_edit), GTK_STOCK_EDIT, GDK_CONTROL_MASK,
		GDK_E, NULL },
	{ NULL, NULL, NULL, 0, 0, NULL }
};
#endif /* EMBEDDED */


/* variables */
static Mime * _mime = NULL;
static unsigned int _view_cnt = 0;


/* functions */
/* view_new */
static GtkWidget * _new_image(View * view, char const * path);
static GtkWidget * _new_text(View * view, char const * path);

static View * _view_new(char const * pathname)
{
	View * view;
	struct stat st;
	char const * type;
	char buf[256];
	GtkAccelGroup * group;
	GtkWidget * vbox;
	GtkWidget * widget;

	if((view = malloc(sizeof(*view))) == NULL)
		return NULL; /* FIXME handle error */
	view->window = NULL;
	view->ab_window = NULL;
	_view_cnt++;
	if((view->pathname = strdup(pathname)) == NULL
			|| lstat(pathname, &st) != 0)
	{
		_view_error(view, strerror(errno), 1);
		return NULL;
	}
	if(_mime == NULL)
		_mime = mime_new(NULL);
	if((type = mime_type(_mime, pathname)) == NULL)
	{
		_view_error(view, _("Unknown file type"), 1);
		return NULL;
	}
	group = gtk_accel_group_new();
	view->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_add_accel_group(GTK_WINDOW(view->window), group);
	snprintf(buf, sizeof(buf), "%s%s", _("View - "), pathname);
	gtk_window_set_title(GTK_WINDOW(view->window), buf);
	g_signal_connect_swapped(view->window, "delete-event", G_CALLBACK(
				_on_closex), view);
	vbox = gtk_vbox_new(FALSE, 0);
#ifndef EMBEDDED
	widget = desktop_menubar_create(
			(mime_get_handler(_mime, type, "edit") != NULL)
			? _view_menubar_edit : _view_menubar, view, group);
#else
	desktop_accel_create(_view_accel, view, group);
	widget = desktop_toolbar_create(_view_toolbar, view, group);
	if(mime_get_handler(_mime, type, "edit") == NULL)
		gtk_widget_set_sensitive(GTK_WIDGET(_view_toolbar[1].widget),
				FALSE);
#endif
	gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);
	if(strncmp(type, "image/", 6) == 0)
	{
		if((widget = _new_image(view, pathname)) == NULL)
			return NULL;
	}
	else if(strncmp(type, "text/", 5) == 0)
	{
		widget = _new_text(view, pathname);
		gtk_window_set_default_size(GTK_WINDOW(view->window), 600, 400);
	}
	else
	{
		_view_error(view, _("Unable to view file type"), 1);
		return NULL;
	}
	gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(view->window), vbox);
	gtk_widget_show_all(view->window);
	return view;
}

static GtkWidget * _new_image(View * view, char const * path)
{
	GtkWidget * window;
	GError * error = NULL;
	GdkPixbuf * pixbuf;
	GtkWidget * widget;
	int pw;
	int ph;
	GdkScreen * screen;
	gint monitor;
	GdkRectangle rect;

	window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(window),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	if((pixbuf = gdk_pixbuf_new_from_file_at_size(path, -1, -1, &error))
			== NULL)
	{
		_view_error(view, error->message, 1);
		return NULL;
	}
	widget = gtk_image_new_from_pixbuf(pixbuf);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(window),
			widget);
	pw = gdk_pixbuf_get_width(pixbuf) + 4;
	ph = gdk_pixbuf_get_height(pixbuf) + 4;
	/* get the current monitor size */
	screen = gdk_screen_get_default();
#if GTK_CHECK_VERSION(2, 14, 0)
	gtk_widget_realize(view->window);
	monitor = gdk_screen_get_monitor_at_window(screen,
			gtk_widget_get_window(view->window));
#else
	monitor = 0; /* XXX hard-coded */
#endif
	gdk_screen_get_monitor_geometry(screen, monitor, &rect);
	/* set an upper bound to the size of the window */
	gtk_window_set_default_size(GTK_WINDOW(view->window), min(pw,
				rect.width), min(ph, rect.height));
	return window;
}

static GtkWidget * _new_text(View * view, char const * path)
{
	GtkWidget * widget;
	GtkWidget * text;
	PangoFontDescription * desc;
	FILE * fp;
	GtkTextBuffer * tbuf;
	GtkTextIter iter;
	char buf[BUFSIZ];
	size_t len;

	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	text = gtk_text_view_new();
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
	desc = pango_font_description_new();
	pango_font_description_set_family(desc, "monospace");
	gtk_widget_modify_font(text, desc);
	pango_font_description_free(desc);
	gtk_container_add(GTK_CONTAINER(widget), text);
	if((fp = fopen(path, "r")) == NULL)
	{
		_view_error(view, strerror(errno), 0);
		return widget;
	}
	tbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	while((len = fread(buf, sizeof(char), sizeof(buf), fp)) > 0)
	{
		gtk_text_buffer_get_end_iter(tbuf, &iter);
		gtk_text_buffer_insert(tbuf, &iter, buf, len);
	}
	fclose(fp);
	return widget;
}


/* view_new_open */
static View * _view_new_open(void)
{
	View * ret;
	GtkWidget * dialog;
	char * pathname = NULL;

	dialog = gtk_file_chooser_dialog_new(_("View file..."), NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
			GTK_RESPONSE_ACCEPT, NULL);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		pathname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
					dialog));
	gtk_widget_destroy(dialog);
	if(pathname == NULL)
		return NULL;
	ret = _view_new(pathname);
	free(pathname);
	return ret;
}


/* view_delete */
static void _view_delete(View * view)
{
	free(view->pathname);
	if(view->ab_window != NULL)
		gtk_widget_destroy(view->ab_window);
	if(view->window != NULL)
		gtk_widget_destroy(view->window);
	free(view);
	if(--_view_cnt == 0)
		gtk_main_quit();
}


/* useful */
/* view_error
 * POST	view is deleted if ret != 0 */
static void _error_response(GtkWidget * widget, gint arg, gpointer data);
static int _error_text(char const * message, int ret);

static int _view_error(View * view, char const * message, int ret)
{
	GtkWidget * dialog;

	if(view == NULL && ret != 0) /* XXX */
		return _error_text(message, ret);
	dialog = gtk_message_dialog_new((view != NULL && view->window != NULL)
			? GTK_WINDOW(view->window) : NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE, "%s",
#if GTK_CHECK_VERSION(2, 6, 0)
			_("Error"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
			"%s",
#endif
			message);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Error"));
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(
				_error_response), (ret != 0) ? view : NULL);
	gtk_widget_show(dialog);
	return ret;
}

static void _error_response(GtkWidget * widget, gint arg, gpointer data)
{
	View * view = data;

	if(view != NULL)
		_view_delete(view);
	gtk_widget_destroy(widget);
}

static int _error_text(char const * message, int ret)
{
	fputs(PROGNAME, stderr);
	perror(message);
	return ret;
}


/* callbacks */
#ifdef EMBEDDED
/* on_close */
static void _on_close(gpointer data)
{
	View * view = data;

	_on_closex(view);
}
#endif


/* on_closex */
static gboolean _on_closex(gpointer data)
{
	View * view = data;

	_view_delete(view);
	if(_view_cnt == 0)
		gtk_main_quit();
	return FALSE;
}


#ifndef EMBEDDED
/* on_file_edit */
static void _on_file_edit(gpointer data)
{
	View * view = data;

	if(mime_action(_mime, "edit", view->pathname) != 0)
		_view_error(view, _("Could not edit file"), 0);
}


/* on_file_open_with */
static void _on_file_open_with(gpointer data)
{
	View * view = data;
	GtkWidget * dialog;
	GtkFileFilter * filter;
	char * filename = NULL;
	pid_t pid;

	dialog = gtk_file_chooser_dialog_new(_("Open with..."),
			GTK_WINDOW(view->window),
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
			GTK_RESPONSE_ACCEPT, NULL);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Executable files"));
	gtk_file_filter_add_mime_type(filter, "application/x-executable");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Shell scripts"));
	gtk_file_filter_add_mime_type(filter, "application/x-shellscript");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All files"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
					dialog));
	gtk_widget_destroy(dialog);
	if(filename == NULL)
		return;
	if((pid = fork()) == -1)
		_view_error(view, "fork", 0);
	else if(pid == 0)
	{
		if(close(0) != 0)
			_view_error(NULL, "stdin", 0);
		execlp(filename, filename, view->pathname, NULL);
		_view_error(NULL, filename, 0);
		exit(2);
	}
	g_free(filename);
}


/* on_file_close */
static void _on_file_close(gpointer data)
{
	View * view = data;

	_on_closex(view);
}


/* on_help_contents */
static void _on_help_contents(gpointer data)
{
	desktop_help_contents(PACKAGE, PROGNAME);
}


/* on_help_about */
static gboolean _about_on_closex(gpointer data);

static void _on_help_about(gpointer data)
{
	View * view = data;

	if(view->ab_window != NULL)
	{
		gtk_widget_show(view->ab_window);
		return;
	}
	view->ab_window = desktop_about_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(view->ab_window), GTK_WINDOW(
				view->window));
	desktop_about_dialog_set_authors(view->ab_window, _view_authors);
	desktop_about_dialog_set_copyright(view->ab_window, _copyright);
	desktop_about_dialog_set_logo_icon_name(view->ab_window,
			"system-file-manager");
	desktop_about_dialog_set_license(view->ab_window, _license);
	desktop_about_dialog_set_name(view->ab_window, PACKAGE);
	desktop_about_dialog_set_version(view->ab_window, VERSION);
	g_signal_connect_swapped(G_OBJECT(view->ab_window), "delete-event",
			G_CALLBACK(_about_on_closex), view);
	gtk_widget_show(view->ab_window);
}

static gboolean _about_on_closex(gpointer data)
{
	View * view = data;

	gtk_widget_hide(view->ab_window);
	return TRUE;
}


#else
static void _on_edit(gpointer data)
{
	View * view = data;

	if(mime_action(_mime, "edit", view->pathname) != 0)
		_view_error(view, _("Could not edit file"), 0);
}


static void _on_open_with(gpointer data)
{
	/* FIXME implement */
}
#endif /* EMBEDDED */


/* usage */
static int _usage(void)
{
	fputs(_("Usage: view file...\n"), stderr);
	return 1;
}


/* public */
/* functions */
/* main */
int main(int argc, char * argv[])
{
	int o;
	int i;

	if(setlocale(LC_ALL, "") == NULL)
		_view_error(NULL, "setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "")) != -1)
		switch(o)
		{
			default:
				return _usage();
		}
	if(optind == argc)
		_view_new_open();
	else
		for(i = optind; i < argc; i++)
			_view_new(argv[i]);
	if(_view_cnt)
		gtk_main();
	return 0;
}
