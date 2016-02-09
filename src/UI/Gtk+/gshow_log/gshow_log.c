#include <gtk/gtk.h>

GtkWidget *log_output;

int
main(int argc, char *argv[])
{
   GtkWidget     *scrolled_win,
                 *window;
   GtkTextBuffer *buffer;

   gtk_init(&argc, &argv);

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(window), "Log viewer");
   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
   gtk_widget_set_size_request(window, 250, 150);
   log_output = gtk_text_view_new();
   gtk_text_view_set_editable(GTK_TEXT_VIEW(log_output), FALSE);
   buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_output));


   scrolled_win = gtk_scrolled_window_new(NULL, NULL);
   gtk_container_add(GTK_CONTAINER(scrolled_win), log_output);
   gtk_container_add(GTK_CONTAINER(window), scrolled_win);
   gtk_widget_show_all(window);

   gtk_main();
   return 0;
}
