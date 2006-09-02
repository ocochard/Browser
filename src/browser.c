/* browser.c */



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "callbacks.h"
#include "browser.h"


/* macros */
#define min(a, b) ((a) > (b) ? (b) : (a))


/* types */
struct _menu
{
	char * name;
	GtkSignalFunc callback;
	char * stock;
};

struct _menubar
{
	char * name;
	struct _menu * menu;
};


/* constants */
struct _menu _menu_file[] =
{
	{ "_New window", G_CALLBACK(on_file_new_window),
		GTK_STOCK_NEW /* FIXME */ },
	{ "", NULL, NULL },
	{ "_Refresh", G_CALLBACK(on_refresh), GTK_STOCK_REFRESH },
	{ "_Properties", G_CALLBACK(on_properties),
		GTK_STOCK_PROPERTIES },
	{ "", NULL, NULL },
	{ "_Close", G_CALLBACK(on_file_close), GTK_STOCK_CLOSE },
	{ NULL, NULL, NULL }
};

static struct _menu _menu_edit[] =
{
	{ "_Cut", G_CALLBACK(on_edit_cut), GTK_STOCK_CUT },
	{ "Cop_y", G_CALLBACK(on_edit_copy), GTK_STOCK_COPY },
	{ "_Paste", NULL, GTK_STOCK_PASTE },
	{ "", NULL, NULL },
	{ "_Delete", G_CALLBACK(on_edit_delete), GTK_STOCK_DELETE },
	{ "", NULL, NULL },
	{ "_Select all", G_CALLBACK(on_edit_select_all), NULL },
	{ "_Unselect all", G_CALLBACK(on_edit_unselect_all), NULL },
	{ "", NULL, NULL },
	{ "_Preferences", G_CALLBACK(on_edit_preferences),
		GTK_STOCK_PREFERENCES },
	{ NULL, NULL, NULL }
};

static struct _menu _menu_help[] =
{
#if GTK_CHECK_VERSION(2, 6, 0)
	{ "_About", G_CALLBACK(on_help_about), GTK_STOCK_ABOUT },
#else
	{ "_About", G_CALLBACK(on_help_about), NULL },
#endif
	{ NULL, NULL, NULL }
};

static struct _menubar _menubar[] =
{
	{ "_File", _menu_file },
	{ "_Edit", _menu_edit },
	{ "_Help", _menu_help },
	{ NULL, NULL }
};


/* Browser */
static int _new_pixbufs(Browser * browser);
static GtkWidget * _new_menubar(Browser * browser);
static GtkListStore * _create_store(void);
#if GTK_CHECK_VERSION(2, 6, 0)
static void _new_iconview(Browser * browser);
#endif
static void _new_detailview(Browser * browser);
Browser * browser_new(char const * directory)
{
	Browser * browser;
	GtkWidget * vbox;
	GtkWidget * tb_menubar;
	GtkWidget * toolbar;
	GtkWidget * widget;
	GtkToolItem * toolitem;
	GtkToolItem * tb_button;

	if((browser = malloc(sizeof(*browser))) == NULL)
		return NULL;
	if(_new_pixbufs(browser) != 0)
	{
		browser_error(browser, "Error while loading default icons", -1);
		free(browser);
		return NULL;
	}

	/* config */
	/* FIXME */
	memset(&browser->prefs, sizeof(browser->prefs), 0);
	memset(&browser->prefs_tmp, sizeof(browser->prefs_tmp), 0);

	/* mime */
	browser->mime = mime_new();

	/* history */
	browser->history = g_list_append(NULL, strdup(directory == NULL
				? g_get_home_dir() : directory));
	browser->current = browser->history;

	browser->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(browser->window), 640, 480);
	gtk_window_set_title(GTK_WINDOW(browser->window), "File browser");
	g_signal_connect(browser->window, "delete_event", G_CALLBACK(on_closex),
			NULL);
	vbox = gtk_vbox_new(FALSE, 0);
	/* menubar */
	tb_menubar = _new_menubar(browser);
	gtk_box_pack_start(GTK_BOX(vbox), tb_menubar, FALSE, FALSE, 0);
	/* toolbar */
	toolbar = gtk_toolbar_new();
	browser->tb_back = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
	g_signal_connect(browser->tb_back, "clicked", G_CALLBACK(on_back),
			browser);
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_back), FALSE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), browser->tb_back, -1);
	browser->tb_updir = gtk_tool_button_new_from_stock(GTK_STOCK_GO_UP);
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_updir), strcmp(
				browser->current->data, "/") != 0);
	g_signal_connect(browser->tb_updir, "clicked", G_CALLBACK(on_updir),
			browser);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), browser->tb_updir, -1);
	browser->tb_forward = gtk_tool_button_new_from_stock(
			GTK_STOCK_GO_FORWARD);
	g_signal_connect(browser->tb_forward, "clicked", G_CALLBACK(on_forward),
			browser);
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_forward), FALSE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), browser->tb_forward, -1);
	tb_button = gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
	g_signal_connect(tb_button, "clicked", G_CALLBACK(on_refresh), browser);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_button, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(),
			-1);
	tb_button = gtk_tool_button_new_from_stock(GTK_STOCK_HOME);
	g_signal_connect(tb_button, "clicked", G_CALLBACK(on_home), browser);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_button, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(),
			-1);
	tb_button = gtk_tool_button_new_from_stock(GTK_STOCK_PROPERTIES);
	g_signal_connect(G_OBJECT(tb_button), "clicked", G_CALLBACK(
				on_properties), browser);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_button, -1);
#if GTK_CHECK_VERSION(2, 6, 0)
	toolitem = gtk_menu_tool_button_new(NULL, "View as...");
	g_signal_connect(G_OBJECT(toolitem), "clicked", G_CALLBACK(on_view_as),
			browser);
	menu = gtk_menu_new();
	menuitem = gtk_menu_item_new_with_label("Details");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(
				on_view_detail), browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_menu_item_new_with_label("Icons");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(
				on_view_icon), browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	menuitem = gtk_menu_item_new_with_label("List");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(
				on_view_list), browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show_all(menu);
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolitem), menu);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
#endif
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
	/* toolbar */
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
			GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	widget = gtk_label_new(" Location: ");
	toolitem = gtk_tool_item_new();
	gtk_container_add(GTK_CONTAINER(toolitem), widget);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	browser->tb_path = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(browser->tb_path), browser->current->data);
	g_signal_connect(G_OBJECT(browser->tb_path), "activate", G_CALLBACK(
				on_path_activate), browser);
	toolitem = gtk_tool_item_new();
	gtk_tool_item_set_expand(toolitem, TRUE);
	gtk_container_add(GTK_CONTAINER(toolitem), browser->tb_path);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_JUMP_TO);
	g_signal_connect(G_OBJECT(toolitem), "clicked", G_CALLBACK(
				on_path_activate), browser);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
	/* icon view */
	browser->scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(browser->scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), browser->scrolled, TRUE, TRUE, 0);
	/* statusbar */
	browser->statusbar = gtk_statusbar_new();
	browser->statusbar_id = 0;
	gtk_box_pack_start(GTK_BOX(vbox), browser->statusbar, FALSE, FALSE, 0);
	/* store */
	browser->store = _create_store();
#if GTK_CHECK_VERSION(2, 6, 0)
	_new_iconview(browser);
	gtk_icon_view_set_item_width(GTK_ICON_VIEW(browser->iconview), 96);
	gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(browser->iconview),
			BR_COL_PIXBUF_48);
	gtk_icon_view_set_orientation(GTK_ICON_VIEW(browser->iconview),
			GTK_ORIENTATION_VERTICAL);
	gtk_container_add(GTK_CONTAINER(browser->scrolled), browser->iconview);
	gtk_widget_grab_focus(browser->iconview);
	browser->detailview = NULL;
#else
	_new_detailview(browser);
	gtk_container_add(GTK_CONTAINER(browser->scrolled),
			browser->detailview);
	gtk_widget_grab_focus(browser->detailview);
#endif
	browser_refresh(browser);

	/* preferences */
	browser->pr_window = NULL;

	gtk_container_add(GTK_CONTAINER(browser->window), vbox);
	gtk_widget_show_all(browser->window);
	return browser;
}

static int _new_pixbufs(Browser * browser)
{
	browser->theme = gtk_icon_theme_new();
	gtk_icon_theme_set_custom_theme(browser->theme, "gnome");
	browser->pb_file_24 = gtk_icon_theme_load_icon(browser->theme,
			"gnome-fs-regular", 24, 0, NULL);
	browser->pb_folder_24 = gtk_icon_theme_load_icon(browser->theme,
			"gnome-fs-directory", 24, 0, NULL);
#if !GTK_CHECK_VERSION(2, 6, 0)
	return browser->pb_file_24 == NULL || browser->pb_folder_24 == NULL;
#else
	browser->pb_file_48 = gtk_icon_theme_load_icon(browser->theme,
			"gnome-fs-regular", 48, 0, NULL);
	browser->pb_folder_48 = gtk_icon_theme_load_icon(browser->theme,
			"gnome-fs-directory", 48, 0, NULL);
	return browser->pb_file_48 == NULL || browser->pb_folder_48 == NULL
		|| browser->pb_file_48 == NULL || browser->pb_folder_48 == NULL;
#endif
}

static GtkWidget * _new_menubar(Browser * browser)
{
	GtkWidget * tb_menubar;
	GtkWidget * menu;
	GtkWidget * menubar;
	GtkWidget * menuitem;
	unsigned int i;
	unsigned int j;
	struct _menu * p;

	tb_menubar = gtk_menu_bar_new();
	for(i = 0; _menubar[i].name != NULL; i++)
	{
		menubar = gtk_menu_item_new_with_mnemonic(_menubar[i].name);
		menu = gtk_menu_new();
		for(j = 0; _menubar[i].menu[j].name != NULL; j++)
		{
			p = &_menubar[i].menu[j];
			if(p->name[0] == '\0')
				menuitem = gtk_separator_menu_item_new();
			else if(p->stock == NULL)
				menuitem = gtk_menu_item_new_with_mnemonic(
						p->name);
			else
				menuitem = gtk_image_menu_item_new_from_stock(
						p->stock, NULL);
			if(p->callback != NULL)
				g_signal_connect(G_OBJECT(menuitem), "activate",
						G_CALLBACK(p->callback),
						browser);
			else
				gtk_widget_set_sensitive(menuitem, FALSE);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		}
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menubar), menu);
		gtk_menu_bar_append(GTK_MENU_BAR(tb_menubar), menubar);
	}
	return tb_menubar;
}

static int _sort_func(GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b,
		gpointer data)
{
	gboolean is_dir_a, is_dir_b;
	gchar * name_a, * name_b;
	int ret;

	/* FIXME sorts folders before files => optional */
	gtk_tree_model_get(model, a, BR_COL_IS_DIRECTORY, &is_dir_a,
			BR_COL_DISPLAY_NAME, &name_a, -1);
	gtk_tree_model_get(model, b, BR_COL_IS_DIRECTORY, &is_dir_b,
			BR_COL_DISPLAY_NAME, &name_b, -1);
	if(!is_dir_a && is_dir_b)
		ret = 1;
	else if(is_dir_a && !is_dir_b)
		ret = -1;
	else
		ret = g_utf8_collate(name_a, name_b);
	g_free(name_a);
	g_free(name_b);
	return ret;
}

static GtkListStore * _create_store(void)
{
	GtkListStore * store;

#if !GTK_CHECK_VERSION(2, 6, 0)
	store = gtk_list_store_new(BR_NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, 
			GDK_TYPE_PIXBUF, G_TYPE_BOOLEAN, G_TYPE_STRING);
#else
	store = gtk_list_store_new(BR_NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, 
			GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF, G_TYPE_BOOLEAN,
			G_TYPE_STRING);
#endif
	gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(store),
			_sort_func, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING); /* FIXME optional */
	return store;
}

#if GTK_CHECK_VERSION(2, 6, 0)
static void _new_iconview(Browser * browser)
{
	browser->iconview = gtk_icon_view_new_with_model(GTK_TREE_MODEL(
				browser->store));
	gtk_icon_view_set_text_column(GTK_ICON_VIEW(browser->iconview),
			BR_COL_DISPLAY_NAME);
	gtk_icon_view_set_margin(GTK_ICON_VIEW(browser->iconview), 4);
	gtk_icon_view_set_column_spacing(GTK_ICON_VIEW(browser->iconview), 4);
	gtk_icon_view_set_row_spacing(GTK_ICON_VIEW(browser->iconview), 4);
	gtk_icon_view_set_spacing(GTK_ICON_VIEW(browser->iconview), 4);
	gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(browser->iconview),
			GTK_SELECTION_MULTIPLE);
	g_signal_connect(G_OBJECT(browser->iconview), "item-activated",
			G_CALLBACK(on_icon_default), browser);
	g_signal_connect(G_OBJECT(browser->iconview), "button-press-event",
			G_CALLBACK(on_view_popup), browser);
}
#endif

static void _new_detailview(Browser * browser)
{
	GtkTreeSelection * treesel;

	browser->detailview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
				browser->store));
	if((treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(
						browser->detailview))) != NULL)
		gtk_tree_selection_set_mode(treesel, GTK_SELECTION_MULTIPLE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(browser->detailview),
			gtk_tree_view_column_new_with_attributes("",
				gtk_cell_renderer_pixbuf_new(), "pixbuf",
				BR_COL_PIXBUF_24, NULL));
	gtk_tree_view_append_column(GTK_TREE_VIEW(browser->detailview),
			gtk_tree_view_column_new_with_attributes("Filename",
				gtk_cell_renderer_text_new(), "text",
				BR_COL_DISPLAY_NAME, NULL));
	gtk_tree_view_append_column(GTK_TREE_VIEW(browser->detailview),
			gtk_tree_view_column_new_with_attributes("MIME type",
				gtk_cell_renderer_text_new(), "text",
				BR_COL_MIME_TYPE, NULL));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(browser->detailview),
			TRUE);
	g_signal_connect(G_OBJECT(browser->detailview), "row-activated",
			G_CALLBACK(on_detail_default), browser);
	g_signal_connect(G_OBJECT(browser->detailview), "button-press-event",
			G_CALLBACK(on_view_popup), browser);
}


void browser_delete(Browser * browser)
{
	g_list_foreach(browser->history, (GFunc)free, NULL);
	g_list_free(browser->history);
	g_object_unref(browser->store);
	free(browser);
}


/* useful */
int browser_error(Browser * browser, char const * message, int ret)
{
	GtkWidget * dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(browser->window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
	if(ret < 0)
		g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(
					gtk_main_quit), NULL);
	else
		g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(
					gtk_widget_destroy), NULL);
	gtk_widget_show(dialog);
	return ret;
}


static void _store_loop(Browser * browser, char const * name);
void browser_refresh(Browser * browser)
{
	GDir * dir;
	char const * name;
	unsigned int cnt;
	unsigned int hidden_cnt;
	char status[36];

	gtk_list_store_clear(browser->store);
	if((dir = g_dir_open(browser->current->data, 0, NULL)) == NULL)
		return;
	gtk_entry_set_text(GTK_ENTRY(browser->tb_path), browser->current->data);
	for(cnt = 0, hidden_cnt = 0; (name = g_dir_read_name(dir)) != NULL;
			cnt++)
	{
		if(name[0] == '.')
		{
			hidden_cnt++;
			if(!browser->prefs.show_hidden_files)
				continue;
		}
		_store_loop(browser, name);
	}
	if(browser->statusbar_id)
		gtk_statusbar_remove(GTK_STATUSBAR(browser->statusbar),
				gtk_statusbar_get_context_id(
					GTK_STATUSBAR(browser->statusbar), ""),
				browser->statusbar_id);
	snprintf(status, sizeof(status), "%u file%c (%u hidden)", cnt, cnt <= 1
			? '\0' : 's', hidden_cnt);
	browser->statusbar_id = gtk_statusbar_push(GTK_STATUSBAR(
				browser->statusbar),
			gtk_statusbar_get_context_id(GTK_STATUSBAR(
					browser->statusbar), ""), status);
}

static void _store_loop(Browser * browser, char const * name)
{
	GtkTreeIter iter;
	gchar * path;
	gchar * display_name;
	char const * type = NULL;
	gboolean is_dir;
	GdkPixbuf * icon_24;
#if GTK_CHECK_VERSION(2, 6, 0)
	GdkPixbuf * icon_48 = NULL; /* FIXME */
#endif

	path = g_build_filename(browser->current->data, name, NULL);
	is_dir = g_file_test(path, G_FILE_TEST_IS_DIR);
	display_name = g_filename_to_utf8(name, -1, NULL, NULL, NULL);
	gtk_list_store_append(browser->store, &iter);
#if !GTK_CHECK_VERSION(2, 6, 0)
	if(is_dir)
		icon_24 = browser->pb_folder_24;
	else if(!is_dir && browser->mime != NULL && (type = mime_type(
					browser->mime, name)) != NULL)
	{
		if((icon_24 = mime_icons(browser->mime, browser->theme, type,
						NULL)) == NULL)
			icon_24 = browser->pb_file_24;
	}
	else
		icon_24 = browser->pb_file_24;
#else
	if(is_dir)
	{
		icon_24 = browser->pb_folder_24;
		icon_48 = browser->pb_folder_48;
	}
	else if(!is_dir && browser->mime != NULL && (type = mime_type(
					browser->mime, name)) != NULL)
	{
		icon_24 = mime_icons(browser->mime, browser->theme, type,
				&icon_48);
		if(icon_24 == NULL)
			icon_24 = browser->pb_file_24;
		if(icon_48 == NULL)
			icon_48 = browser->pb_file_48;
	}
	else
	{
		icon_24 = browser->pb_file_24;
		icon_48 = browser->pb_file_48;
	}
#endif
	gtk_list_store_set(browser->store, &iter, BR_COL_PATH, path,
			BR_COL_DISPLAY_NAME, display_name,
			BR_COL_IS_DIRECTORY, is_dir,
			BR_COL_PIXBUF_24, icon_24,
#if GTK_CHECK_VERSION(2, 6, 0)
			BR_COL_PIXBUF_48, icon_48,
#endif
			BR_COL_MIME_TYPE, type == NULL ? "" : type,
			-1);
	g_free(path);
	g_free(display_name);
}


void browser_set_location(Browser * browser, char const * path)
{
	if(g_file_test(path, G_FILE_TEST_IS_REGULAR))
		return mime_open(browser->mime, path);
	if(!g_file_test(path, G_FILE_TEST_IS_DIR))
		return;
	if(browser->history == NULL)
	{
		if((browser->history = g_list_alloc()) == NULL)
			return;
		browser->history->data = strdup(path);
		browser->current = browser->history;
	}
	else if(strcmp(browser->current->data, path) != 0)
	{
		g_list_foreach(browser->current->next, (GFunc)free, NULL);
		g_list_free(browser->current->next);
		browser->current->next = NULL;
		browser->history = g_list_append(browser->history,
				strdup(path));
		browser->current = g_list_last(browser->history);
		gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_back),
				TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_forward),
				FALSE);
	}
	gtk_widget_set_sensitive(GTK_WIDGET(browser->tb_updir),
			strcmp(browser->current->data, "/") != 0);
	browser_refresh(browser);
}
