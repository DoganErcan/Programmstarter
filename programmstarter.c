// ==============================================================================================
// Programmstarter – C17 / GTK 4 / Windows, ohne Konsolenfenster. 
// ==============================================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <gtk/gtk.h>
#include <wchar.h>
#include <string.h>

typedef struct { char* name; char* path; gboolean group; } Entry;
static GtkWidget* window, * list, * status;
static char* base_dir, * config_path;
static HANDLE instance_slot;
static GPtrArray* entries;

// ==============================================================================================
// entry_free = Gibt den Speicher eines Listeneintrags einschließlich Name und Pfad frei.
// ==============================================================================================
static void entry_free ( gpointer data )
{
	Entry* item = data;
	g_free ( item->name );
	g_free ( item->path );
	g_free ( item );
}

// ==============================================================================================
// UTF-8 mit optionaler BOM. Bei fehlerhaften Zeilen bleibt die bisherige
// Liste erhalten. Pfade dürfen Leerzeichen, aber kein Trennzeichen | enthalten.
// ==============================================================================================
// parse_list = Liest Gruppen und Programme aus dem Text und prüft das Dateiformat.
// ==============================================================================================
static GPtrArray* parse_list ( const char* data, gsize size, char** error )
{
	if ( size > 1024 * 1024 || memchr ( data, 0, size ) || !g_utf8_validate ( data, ( gssize ) size, NULL ) )
	{
		*error = g_strdup ( "Die Liste muss UTF-8-Text sein (maximal 1 MiB, ohne Nullbytes)." );
		return NULL;
	}

	char* copy = g_strndup ( data, size );
	char* start = g_str_has_prefix ( copy, "\xEF\xBB\xBF" ) ? copy + 3 : copy;
	char** lines = g_strsplit ( start, "\n", -1 );
	GPtrArray* result = g_ptr_array_new_with_free_func ( entry_free );

	for ( unsigned n = 0; lines [ n ]; ++n )
	{
		char* line = g_strstrip ( lines [ n ] );

		if ( !*line || *line == '#' || *line == ';' ) continue;
		Entry* item = g_new0 ( Entry, 1 );
		gsize len = strlen ( line );

		if ( *line == '[' && len >= 3 && line [ len - 1 ] == ']' )
		{
			line [ len - 1 ] = 0;
			item->name = g_strdup ( g_strstrip ( line + 1 ) ); item->group = TRUE;
		} else
		{
			char* divider = strchr ( line, '|' );

			if ( divider && !strchr ( divider + 1, '|' ) )
			{
				*divider = 0;
				item->name = g_strdup ( g_strstrip ( line ) );
				char* path = g_strstrip ( divider + 1 );
				gsize length = strlen ( path );
				if ( length >= 2 && path [ 0 ] == '"' && path [ length - 1 ] == '"' )
				{
					path [ length - 1 ] = 0;
					++path;
				}
				item->path = g_strdup ( path );
			}
		}

		if ( !item->name || !*item->name || ( !item->group && ( !item->path || !*item->path ) ) )
		{
			*error = g_strdup_printf ( "Zeile %u: Erwartet wird [Gruppe] oder Programmname | Pfad.", n + 1 );
			entry_free ( item );
			g_ptr_array_unref ( result ); result = NULL;
			break;
		}

		g_ptr_array_add ( result, item );
	}

	g_strfreev ( lines );
	g_free ( copy );
	return result;
}

// ==============================================================================================
// resolve_path = Ersetzt Windows-Variablen und wandelt relative Pfade in absolute Pfade um.
// ==============================================================================================
static char* resolve_path ( const char* text, char** error )
{
	gunichar2* wide = g_utf8_to_utf16 ( text, -1, NULL, NULL, NULL );

	if ( !wide ) { *error = g_strdup ( "Ungültiger Pfad." ); return NULL; }

	DWORD required = ExpandEnvironmentStringsW ( ( LPCWSTR ) wide, NULL, 0 );

	if ( !required || required > 32768 )
	{
		g_free ( wide );
		*error = g_strdup ( "Der Pfad kann nicht aufgelöst werden." );
		return NULL;
	}

	wchar_t* expanded = g_new ( wchar_t, required );
	DWORD count = ExpandEnvironmentStringsW ( ( LPCWSTR ) wide, expanded, required );
	g_free ( wide );

	if ( !count || count > required )
	{
		g_free ( expanded );
		*error = g_strdup ( "Der Pfad kann nicht aufgelöst werden." );
		return NULL;
	}

	char* utf8 = g_utf16_to_utf8 ( ( gunichar2* ) expanded, -1, NULL, NULL, NULL );
	g_free ( expanded );
	char* absolute = g_canonicalize_filename ( utf8, base_dir ); g_free ( utf8 );
	return absolute;
}

// ==============================================================================================
// ShellExecuteEx startet unabhängig, ohne auf das Programmende zu warten.
// Das Arbeitsverzeichnis ist der Ordner des gestarteten Programms.
// ==============================================================================================
// start_path = Startet die angegebene Datei über Windows; das Starterfenster bleibt offen.
// ==============================================================================================
static gboolean start_path ( const char* text, char** error )
{
	char* path = resolve_path ( text, error );

	if ( !path ) return FALSE;
	if ( !g_file_test ( path, G_FILE_TEST_IS_REGULAR ) )
	{
		*error = g_strdup_printf ( "Datei nicht gefunden: %s", path );
		g_free ( path );
		return FALSE;
	}

	char* directory = g_path_get_dirname ( path );
	gunichar2* wide = g_utf8_to_utf16 ( path, -1, NULL, NULL, NULL );
	gunichar2* cwd = g_utf8_to_utf16 ( directory, -1, NULL, NULL, NULL );
	SHELLEXECUTEINFOW info = { 0 };
	info.cbSize = sizeof info;
	info.fMask = SEE_MASK_FLAG_NO_UI;
	info.lpVerb = L"open"; info.lpFile = ( LPCWSTR ) wide;
	info.lpDirectory = ( LPCWSTR ) cwd;
	info.nShow = SW_SHOWNORMAL;
	gboolean ok = ShellExecuteExW ( &info ) != FALSE;

	if ( !ok )
	{
		DWORD code = GetLastError ( );
		wchar_t message [ 512 ] = { 0 };

		FormatMessageW ( FORMAT_MESSAGE_FROM_SYSTEM |
						 FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, 0, message, G_N_ELEMENTS ( message ), NULL );

		char* reason = g_utf16_to_utf8 ( ( gunichar2* ) message, -1, NULL, NULL, NULL );
		*error = g_strdup_printf ( "Start fehlgeschlagen (%lu): %s\n%s", code, reason ? reason : "Windows-Fehler", path );
		g_free ( reason );
	}

	g_free ( wide );
	g_free ( cwd );
	g_free ( directory );
	g_free ( path );
	return ok;
}

// ==============================================================================================
// launch_clicked = Reagiert auf einen Programm-Button und zeigt das Startergebnis an.
// ==============================================================================================
static void launch_clicked ( GtkButton* button, gpointer data )
{
	( void ) button; Entry* item = data; char* error = NULL;
	if ( start_path ( item->path, &error ) )
	{
		char* message = g_strdup_printf ( "Gestartet: %s", item->name );
		gtk_label_set_text ( GTK_LABEL ( status ), message );
		g_free ( message );
	} else { gtk_label_set_text ( GTK_LABEL ( status ), error ); g_free ( error ); }
}

// ==============================================================================================
// reload_clicked = Lädt die Textdatei neu und baut daraus Gruppen und Programm-Buttons auf.
// ==============================================================================================
static void reload_clicked ( GtkButton* button, gpointer unused )
{
	( void ) button;
	( void ) unused;

	char* data = NULL; gsize size = 0;
	GError* io_error = NULL;

	if ( !g_file_get_contents ( config_path, &data, &size, &io_error ) )
	{
		char* message = g_strdup_printf ( "Liste nicht lesbar: %s\n%s", config_path, io_error->message );
		gtk_label_set_text ( GTK_LABEL ( status ), message );
		g_free ( message );
		g_clear_error ( &io_error );
		return;
	}

	char* error = NULL; GPtrArray* next = parse_list ( data, size, &error );
	g_free ( data );

	if ( !next ) { gtk_label_set_text ( GTK_LABEL ( status ), error ); g_free ( error ); return; }

	GtkWidget* child;

	while ( ( child = gtk_widget_get_first_child ( list ) ) != NULL ) gtk_box_remove ( GTK_BOX ( list ), child );
	if ( entries ) g_ptr_array_unref ( entries );
	entries = next;
	unsigned count = 0;

	for ( guint i = 0; i < entries->len;
		  ++i )
	{
		Entry* item = g_ptr_array_index ( entries, i );
		if ( item->group )
		{
			GtkWidget* title = gtk_label_new ( item->name );
			gtk_label_set_xalign ( GTK_LABEL ( title ), 0 );
			gtk_widget_add_css_class ( title, "heading" ); // Schriftgröße Gruppe 
			gtk_widget_set_margin_top ( title, i ? 16 : 4 );
			gtk_box_append ( GTK_BOX ( list ), title );
			gtk_box_append ( GTK_BOX ( list ), gtk_separator_new ( GTK_ORIENTATION_HORIZONTAL ) );
		} else
		{
			GtkWidget* launch = gtk_button_new ( );
			GtkWidget* row = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 10 );
			gtk_box_append ( GTK_BOX ( row ), gtk_image_new_from_icon_name ( "media-playback-start-symbolic" ) );
			GtkWidget* label = gtk_label_new ( item->name );
			gtk_label_set_xalign ( GTK_LABEL ( label ), 0 );
			gtk_label_set_ellipsize ( GTK_LABEL ( label ), PANGO_ELLIPSIZE_END );
			gtk_widget_set_hexpand ( label, TRUE ); gtk_box_append ( GTK_BOX ( row ), label );
			gtk_button_set_child ( GTK_BUTTON ( launch ), row );
			gtk_widget_set_size_request ( launch, -1, 40 );
			gtk_widget_set_tooltip_text ( launch, item->path );
			g_signal_connect ( launch, "clicked", G_CALLBACK ( launch_clicked ), item );
			gtk_box_append ( GTK_BOX ( list ), launch );
			++count;
		}
	}
	if ( !count ) gtk_box_append ( GTK_BOX ( list ), gtk_label_new ( "Noch keine Programme eingetragen." ) );

	char* message = g_strdup_printf ( "%u Programme · Fenster bleibt beim Start geöffnet", count );
	gtk_label_set_text ( GTK_LABEL ( status ), message );
	g_free ( message );
}

// ==============================================================================================
// edit_clicked = Öffnet die Programmliste im zugeordneten Texteditor.
// ==============================================================================================
static void edit_clicked ( GtkButton* button, gpointer unused )
{
	( void ) button; ( void ) unused; char* error = NULL; if ( !start_path ( config_path, &error ) )
	{
		gtk_label_set_text ( GTK_LABEL ( status ), error );
		g_free ( error );
	} else gtk_label_set_text ( GTK_LABEL ( status ), "Liste speichern, anschließend „Neu laden“ anklicken." );
}

// ==============================================================================================
// activate = Baut beim Aktivieren der GTK-Anwendung das Hauptfenster auf.
// ==============================================================================================
static void activate ( GtkApplication* app, gpointer unused )
{
	( void ) unused;
	window = gtk_application_window_new ( app );

	char* list_name = g_path_get_basename ( config_path );
	char* window_title = g_strdup_printf ( "Starter — %s", list_name );

	gtk_window_set_title ( GTK_WINDOW ( window ), window_title );
	g_free ( window_title );
	g_free ( list_name );

	gtk_window_set_default_size ( GTK_WINDOW ( window ), 440, 800 );
	GtkWidget* box = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 12 );

	gtk_widget_set_margin_start ( box, 6 );
	gtk_widget_set_margin_end ( box, 6 );
	gtk_widget_set_margin_top ( box, 6 );
	gtk_widget_set_margin_bottom ( box, 6 );
	gtk_window_set_child ( GTK_WINDOW ( window ), box );

	GtkWidget* title = gtk_label_new ( "Meine Programme" );
	gtk_widget_add_css_class ( title, "title-4" );  // Schriftgröße Programmüberschrift
	gtk_label_set_xalign ( GTK_LABEL ( title ), 0 );
	gtk_box_append ( GTK_BOX ( box ), title );

	GtkWidget* toolbar = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 8 );
	GtkWidget* edit = gtk_button_new_with_label ( "Liste bearbeiten" );
	GtkWidget* reload = gtk_button_new_with_label ( "Neu laden" );
	g_signal_connect ( edit, "clicked", G_CALLBACK ( edit_clicked ), NULL );
	g_signal_connect ( reload, "clicked", G_CALLBACK ( reload_clicked ), NULL );
	gtk_box_append ( GTK_BOX ( toolbar ), edit );
	gtk_box_append ( GTK_BOX ( toolbar ), reload );
	gtk_box_append ( GTK_BOX ( box ), toolbar );

	GtkWidget* scroll = gtk_scrolled_window_new ( );
	gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( scroll ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_vexpand ( scroll, TRUE );
	list = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 6 );
	gtk_scrolled_window_set_child ( GTK_SCROLLED_WINDOW ( scroll ), list );
	gtk_box_append ( GTK_BOX ( box ), scroll );
	status = gtk_label_new ( "" );
	gtk_label_set_wrap ( GTK_LABEL ( status ), TRUE );
	gtk_label_set_xalign ( GTK_LABEL ( status ), 0 );
	gtk_widget_set_tooltip_text ( status, config_path );
	gtk_box_append ( GTK_BOX ( box ), status );
	reload_clicked ( NULL, NULL );
	gtk_window_present ( GTK_WINDOW ( window ) );
}

// ==============================================================================================
// prepare_paths = Ermittelt den EXE-Ordner sowie die Pfade zur Programmliste und zu GTK-Schemas.
// ==============================================================================================
static gboolean prepare_paths ( void )
{
	wchar_t executable [ 32768 ];
	DWORD length = GetModuleFileNameW ( NULL, executable, G_N_ELEMENTS ( executable ) );

	if ( !length || length >= G_N_ELEMENTS ( executable ) ) return FALSE;

	char* path = g_utf16_to_utf8 ( ( gunichar2* ) executable, -1, NULL, NULL, NULL );
	base_dir = g_path_get_dirname ( path );
	g_free ( path );
	config_path = g_build_filename ( base_dir, "programme.txt", NULL );
	char* schemas = g_build_filename ( base_dir, "share", "glib-2.0", "schemas", NULL );
	g_setenv ( "GSETTINGS_SCHEMA_DIR", schemas, TRUE );
	g_free ( schemas );
	return TRUE;
}

// ==============================================================================================
// reserve_instance = Reserviert die kleinste freie Instanznummer für diesen Programmordner.
// ==============================================================================================
static int reserve_instance ( HANDLE* slot, char** error )
{
	char* folded = g_utf8_casefold ( base_dir, -1 );
	char* hash = g_compute_checksum_for_string ( G_CHECKSUM_SHA256, folded, -1 );
	g_free ( folded );
	for ( unsigned number = 0; number < 256; ++number )
	{
		char* name = g_strdup_printf ( "Local\\DoganProgrammstarter_%s_%u", hash, number );
		gunichar2* wide = g_utf8_to_utf16 ( name, -1, NULL, NULL, NULL );

		/* Der Handle hält den Platz belegt; Windows gibt ihn auch nach einem Absturz frei. */
		HANDLE handle = CreateMutexW ( NULL, FALSE, ( LPCWSTR ) wide );
		DWORD code = GetLastError ( );
		g_free ( wide );
		g_free ( name );
		if ( !handle )
		{
			*error = g_strdup_printf ( "Instanznummer konnte nicht reserviert werden (Windows-Fehler %lu).", code );
			g_free ( hash ); return -1;
		}
		if ( code != ERROR_ALREADY_EXISTS ) { *slot = handle; g_free ( hash ); return ( int ) number; }
		CloseHandle ( handle );
	}
	g_free ( hash );
	*error = g_strdup ( "Es sind bereits 256 Instanzen geöffnet." );
	return -1;
}

// ==============================================================================================
// choose_instance_list = Wählt programme.txt, programme1.txt usw.; legt fehlende Kopien an.
// ==============================================================================================
static gboolean choose_instance_list ( char** error )
{
	int number = reserve_instance ( &instance_slot, error );

	if ( number < 0 ) return FALSE;

	char* name = number ? g_strdup_printf ( "programme%d.txt", number ) : g_strdup ( "programme.txt" );
	char* selected = g_build_filename ( base_dir, name, NULL );

	g_free ( name );

	if ( number && !g_file_test ( selected, G_FILE_TEST_IS_REGULAR ) )
	{
		char* template_path = g_build_filename ( base_dir, "programme.txt", NULL );
		gunichar2* source = g_utf8_to_utf16 ( template_path, -1, NULL, NULL, NULL );
		gunichar2* target = g_utf8_to_utf16 ( selected, -1, NULL, NULL, NULL );

		/* TRUE: Eine vorhandene persönliche Liste niemals überschreiben. */
		gboolean copied = CopyFileW ( ( LPCWSTR ) source, ( LPCWSTR ) target, TRUE ) != FALSE;
		DWORD code = GetLastError ( );
		g_free ( source );
		g_free ( target );
		g_free ( template_path );
		if ( !copied && !( code == ERROR_FILE_EXISTS && g_file_test ( selected, G_FILE_TEST_IS_REGULAR ) ) )
		{
			*error = g_strdup_printf ( "Liste konnte nicht angelegt werden (Windows-Fehler %lu):\n%s\n"
									   "Bitte programme.txt und die Schreibrechte im Programmordner prüfen.", code, selected );
			g_free ( selected );
			CloseHandle ( instance_slot );
			instance_slot = NULL;
			return FALSE;
		}
	}

	g_free ( config_path ); config_path = selected;
	return TRUE;
}

#ifndef STARTER_TEST
// ==============================================================================================
// wWinMain = Startfunktion der Windows-Anwendung; startet GTK und dessen Ereignisschleife.
// ==============================================================================================
int WINAPI wWinMain ( HINSTANCE instance, HINSTANCE previous, PWSTR command, int show )
{
	( void ) instance; ( void ) previous;
	( void ) command; ( void ) show;

	if ( !prepare_paths ( ) ) { MessageBoxW ( NULL, L"Programmpfad nicht ermittelbar.", L"Programmstarter", MB_ICONERROR ); return 1; }

	char* error = NULL;

	if ( !choose_instance_list ( &error ) )
	{
		gunichar2* message = g_utf8_to_utf16 ( error, -1, NULL, NULL, NULL );
		MessageBoxW ( NULL, ( LPCWSTR ) message, L"Programmstarter", MB_ICONERROR );
		g_free ( message );
		g_free ( error );
		g_free ( base_dir );
		g_free ( config_path );
		return 1;
	}

	GtkApplication* app = gtk_application_new ( "de.dogan.programmstarter", G_APPLICATION_NON_UNIQUE );
	g_signal_connect ( app, "activate", G_CALLBACK ( activate ), NULL );
	int result = g_application_run ( G_APPLICATION ( app ), 0, NULL );
	g_object_unref ( app );

	if ( entries ) g_ptr_array_unref ( entries );
	g_free ( base_dir );
	g_free ( config_path );
	CloseHandle ( instance_slot );
	return result;
}
#endif
// ==============================================================================================
