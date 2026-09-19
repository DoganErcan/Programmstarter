#define STARTER_TEST
#include "../programmstarter.c"
#include <glib/gstdio.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { char* fail=g_strdup_printf("FAIL %u: %s",__LINE__,#x);g_file_set_contents("Testergebnis.txt",fail,-1,NULL);g_free(fail);return 1; } } while(0)
static void pump(unsigned ms)
{
    ULONGLONG until=GetTickCount64()+ms;
    do { while(g_main_context_iteration(NULL,FALSE)){} Sleep(1); } while(GetTickCount64()<until);
}
int WINAPI wWinMain(HINSTANCE a,HINSTANCE b,PWSTR c,int d)
{
    (void)a;(void)b;(void)c;(void)d; char* error=NULL;
    CHECK(prepare_paths());
    /* Freie Nummern, Lücken und automatische Dateikopien prüfen. */
    HANDLE first=NULL,second=NULL,third=NULL,reused=NULL;
    CHECK(reserve_instance(&first,&error)==0);
    CHECK(reserve_instance(&second,&error)==1);
    CHECK(reserve_instance(&third,&error)==2);
    CloseHandle(second);
    CHECK(reserve_instance(&reused,&error)==1);
    CloseHandle(reused);CloseHandle(third);
    char* numbered=g_build_filename(base_dir,"programme1.txt",NULL);
    g_unlink(numbered);
    CHECK(choose_instance_list(&error));
    CHECK(!strcmp(config_path,numbered) && g_file_test(numbered,G_FILE_TEST_IS_REGULAR));
    char *template_text=NULL,*copy_text=NULL;gsize template_size=0,copy_size=0;
    char* template_path=g_build_filename(base_dir,"programme.txt",NULL);
    CHECK(g_file_get_contents(template_path,&template_text,&template_size,NULL));
    CHECK(g_file_get_contents(numbered,&copy_text,&copy_size,NULL));
    CHECK(template_size==copy_size && !memcmp(template_text,copy_text,copy_size));
    g_free(template_text);g_free(copy_text);
    CloseHandle(instance_slot);instance_slot=NULL;
    CHECK(g_file_set_contents(numbered,"# Meine eigene Liste",-1,NULL));
    CHECK(choose_instance_list(&error));
    CHECK(g_file_get_contents(numbered,&copy_text,NULL,NULL));
    CHECK(!strcmp(copy_text,"# Meine eigene Liste"));g_free(copy_text);
    CloseHandle(instance_slot);instance_slot=NULL;CloseHandle(first);
    g_unlink(numbered);g_free(numbered);
    g_free(config_path);config_path=template_path;
    const char* valid="\xEF\xBB\xBF# Kommentar\r\n[Werkzeuge]\r\nMein Editor | \"C:\\Pfad mit Leerzeichen\\ä.exe\"\r\n\n";
    GPtrArray* parsed=parse_list(valid,strlen(valid),&error);
    CHECK(parsed && parsed->len==2 && !error);
    Entry* item=g_ptr_array_index(parsed,1);
    CHECK(!strcmp(item->name,"Mein Editor") && !strcmp(item->path,"C:\\Pfad mit Leerzeichen\\ä.exe"));
    g_ptr_array_unref(parsed);
    const char* bad[]={"Kein Trenner","Name | "," | C:\\a.exe","[ ]","a|b|c","\xFF"};
    for(unsigned i=0;i<G_N_ELEMENTS(bad);++i){parsed=parse_list(bad[i],strlen(bad[i]),&error);CHECK(!parsed && error);g_clear_pointer(&error,g_free);}
    const char nul[]={'a',0,'b'};CHECK(!parse_list(nul,sizeof nul,&error));g_clear_pointer(&error,g_free);
    char* absolute=resolve_path("%WINDIR%\\System32\\notepad.exe",&error);
    CHECK(absolute && g_file_test(absolute,G_FILE_TEST_IS_REGULAR));g_free(absolute);
    CHECK(!start_path("absichtlich-nicht-vorhanden.exe",&error) && error);g_clear_pointer(&error,g_free);
    GtkApplication* app=gtk_application_new("de.dogan.programmstarter.tests",G_APPLICATION_NON_UNIQUE);
    CHECK(g_application_register(G_APPLICATION(app),NULL,NULL));activate(app,NULL);pump(150);
    CHECK(entries && entries->len==5 && gtk_widget_get_visible(window));
    /* Invalid reload must keep the existing buttons and their owned strings. */
    char* original=config_path;config_path=g_build_filename(base_dir,"invalid-test.txt",NULL);
    CHECK(g_file_set_contents(config_path,"ungueltige Zeile",-1,NULL));
    GPtrArray* before=entries;reload_clicked(NULL,NULL);CHECK(entries==before);
    g_unlink(config_path);g_free(config_path);config_path=original;reload_clicked(NULL,NULL);
    char* probe=resolve_path("..\\Test ä mit Leerzeichen\\Programmstarter.exe",&error);
    char* probe_dir=g_path_get_dirname(probe),*marker=g_build_filename(probe_dir,"gestartet.txt",NULL);
    g_unlink(marker);CHECK(start_path(probe,&error));
    for(unsigned i=0;i<100 && !g_file_test(marker,G_FILE_TEST_IS_REGULAR);++i)pump(50);
    CHECK(g_file_test(marker,G_FILE_TEST_IS_REGULAR));
    CHECK(gtk_widget_get_visible(window));
    g_free(probe);g_free(probe_dir);g_free(marker);
    pump(150);
    GdkPaintable* paintable=gtk_widget_paintable_new(window);GtkSnapshot* snap=gtk_snapshot_new();
    gdk_paintable_snapshot(paintable,GDK_SNAPSHOT(snap),gtk_widget_get_width(window),gtk_widget_get_height(window));
    GskRenderNode* node=gtk_snapshot_free_to_node(snap);CHECK(node);
    GdkTexture* texture=gsk_renderer_render_texture(gtk_native_get_renderer(GTK_NATIVE(window)),node,NULL);
    CHECK(texture && gdk_texture_save_to_png(texture,"Vorschau.png"));
    g_object_unref(texture);gsk_render_node_unref(node);g_object_unref(paintable);
    gtk_window_destroy(GTK_WINDOW(window));g_object_unref(app);g_ptr_array_unref(entries);
    g_free(base_dir);g_free(config_path);
    char* report=g_strdup_printf("PASS: %u Pruefungen. Parser, Unicode, Pfade, Reload, echter Programmstart, Fenster bleibt offen, Screenshot.\n",checks);
    g_file_set_contents("Testergebnis.txt",report,-1,NULL);g_free(report);return 0;
}
