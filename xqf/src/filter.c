/* XQF - Quake server browser and launcher
 * Copyright (C) 1998-2000 Roman Pozlevich <roma@botik.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <sys/types.h>
#include <stdio.h>

#include <gtk/gtk.h>

#include "xqf.h"
#include "dialogs.h"
#include "server.h"
#include "config.h"
#include "filter.h"
#include "flt-player.h"


static int server_filter (struct server *s);
static void server_filter_init (void);


struct filter filters[FILTERS_TOTAL] = {
  {
    "Server",
    "S Filter",
    "SF Cfg",
    server_filter,
    server_filter_init,
    NULL,
    1,
    FILTER_NOT_CHANGED
  },
  { 
    "Player",
    "P Filter",
    "PF Cfg",
    player_filter,
    player_filter_init,
    player_filter_done,
    1,
    FILTER_NOT_CHANGED
  }
};


unsigned char cur_filter = 0;

int	filter_retries;
int	filter_ping;
int 	filter_not_full;
int 	filter_not_empty;
int	filter_no_cheats;
int	filter_no_password;

unsigned filter_current_time = 1;


static  GtkWidget *filter_retries_spinner;
static  GtkWidget *filter_ping_spinner;
static  GtkWidget *filter_not_full_check_button;
static  GtkWidget *filter_not_empty_check_button;
static  GtkWidget *filter_no_cheats_check_button;
static  GtkWidget *filter_no_password_check_button;


void apply_filters (unsigned mask, struct server *s) {
  unsigned flt_time;
  unsigned i;
  int n;

  flt_time = s->flt_last;

  for (n = 0, i = 1; n < FILTERS_TOTAL; n++, i <<= 1) {
    if ((mask & i) == i && (filters[n].last_changed > s->flt_last ||
                                                    (s->flt_mask & i) != i)) {
      if ((*filters[n].func) (s))
	s->filters |= i;
      else 
	s->filters &= ~i;

      if (flt_time < filters[n].last_changed)
	flt_time = filters[n].last_changed;
    }
  }

  s->flt_mask |= mask;
  s->flt_last = flt_time;
}


GSList *build_filtered_list (unsigned mask, GSList *servers) {
  struct server *s;
  GSList *list = NULL;

  while (servers) {
    s = (struct server *) servers->data;
    apply_filters (mask | FILTER_PLAYER_MASK, s);

    if ((s->filters & mask) == mask) {
      list = g_slist_prepend (list, s);
      server_ref (s);
    }

    servers = servers->next;
  }

  list = g_slist_reverse (list);
  return list;
}


static int server_filter (struct server *s) {
  if (s->ping == -1)	/* no information */
    return TRUE;

  if (s->retries < filter_retries && 
      s->ping < filter_ping && 
      (!filter_not_full || s->curplayers != s->maxplayers) && 
      (!filter_not_empty || s->curplayers != 0) &&
      (!filter_no_cheats || (s->flags & SERVER_CHEATS) == 0) &&
      (!filter_no_password || (s->flags & SERVER_PASSWORD) == 0))
    return TRUE;

  return FALSE;
}


static void server_filter_init (void) {
  config_push_prefix ("/" CONFIG_FILE "/Server Filter");

  filter_retries     = config_get_int  ("retries=2");
  filter_ping        = config_get_int  ("ping=1000");
  filter_not_full    = config_get_bool ("not full=false");
  filter_not_empty   = config_get_bool ("not empty=false");
  filter_no_cheats   = config_get_bool ("no cheats=false");
  filter_no_password = config_get_bool ("no password=false");

  config_pop_prefix ();
}


static void server_filter_new_defaults (void) {
  int i;

  config_push_prefix ("/" CONFIG_FILE "/Server Filter");

  i = gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (filter_retries_spinner));
  if (filter_retries != i) {
    config_set_int  ("retries", filter_retries = i);
    filters[FILTER_SERVER].changed = FILTER_CHANGED;
  }

  i = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (filter_ping_spinner));
  if (filter_ping != i) {
    config_set_int  ("ping", filter_ping = i);
    filters[FILTER_SERVER].changed = FILTER_CHANGED;
  }

  i = GTK_TOGGLE_BUTTON (filter_not_full_check_button)->active;
  if (filter_not_full != i) {
    config_set_bool ("not full", filter_not_full = i);
    filters[FILTER_SERVER].changed = FILTER_CHANGED;
  }

  i = GTK_TOGGLE_BUTTON (filter_not_empty_check_button)->active;
  if (filter_not_empty != i) {
    config_set_bool ("not empty", filter_not_empty = i);
    filters[FILTER_SERVER].changed = FILTER_CHANGED;
  }

  i = GTK_TOGGLE_BUTTON (filter_no_cheats_check_button)->active;
  if (filter_no_cheats != i) {
    config_set_bool ("no cheats", filter_no_cheats = i);
    filters[FILTER_SERVER].changed = FILTER_CHANGED;
  }

  i = GTK_TOGGLE_BUTTON (filter_no_password_check_button)->active;
  if (filter_no_password != i) {
    config_set_bool ("no password", filter_no_password = i);
    filters[FILTER_SERVER].changed = FILTER_CHANGED;
  }

  config_pop_prefix ();

  if (filters[FILTER_SERVER].changed == FILTER_CHANGED)
    filters[FILTER_SERVER].last_changed = ++filter_current_time;
}


static void server_filter_page (GtkWidget *notebook) {
  GtkWidget *page_hbox;
  GtkWidget *alignment;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  GtkObject *adj;

  page_hbox = gtk_hbox_new (TRUE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (page_hbox), 8);

  label = gtk_label_new ("Server Filter");
  gtk_widget_show (label);

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page_hbox, label);

  frame = gtk_frame_new ("Server would pass filter if");
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (page_hbox), frame, FALSE, FALSE, 0);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  table = gtk_table_new (6, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_container_set_border_width (GTK_CONTAINER (table), 6);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  /* max ping */

  label = gtk_label_new ("ping is less than");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL,
                                                                         0, 0);
  gtk_widget_show (label);

  adj = gtk_adjustment_new (filter_ping, 0.0, MAX_PING, 100.0, 1000.0, 0.0);

  filter_ping_spinner = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 0, 0);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (filter_ping_spinner), 
                                                            GTK_UPDATE_ALWAYS);
  gtk_widget_set_usize (filter_ping_spinner, 64, -1);
  gtk_table_attach_defaults (GTK_TABLE (table), filter_ping_spinner, 1, 2, 0, 1);
  gtk_widget_show (filter_ping_spinner);

  /* max timeouts */

  label = gtk_label_new ("the number of retires is fewer than");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 
                                                                         0, 0);
  gtk_widget_show (label);

  adj = gtk_adjustment_new (filter_retries, 0.0, MAX_RETRIES, 1.0, 1.0, 0.0);

  filter_retries_spinner = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 0, 0);
  gtk_widget_set_usize (filter_retries_spinner, 64, -1);
  gtk_table_attach_defaults (GTK_TABLE (table), filter_retries_spinner, 
                                                                   1, 2, 1, 2);
  gtk_widget_show (filter_retries_spinner);

  /* not full */

  filter_not_full_check_button = 
                            gtk_check_button_new_with_label ("it is not full");
  gtk_toggle_button_set_active (
            GTK_TOGGLE_BUTTON (filter_not_full_check_button), filter_not_full);
  gtk_table_attach_defaults (GTK_TABLE (table), filter_not_full_check_button, 
                                                                   0, 2, 2, 3);
  gtk_widget_show (filter_not_full_check_button);

  /* not empty */

  filter_not_empty_check_button = 
                           gtk_check_button_new_with_label ("it is not empty");
  gtk_toggle_button_set_active (
          GTK_TOGGLE_BUTTON (filter_not_empty_check_button), filter_not_empty);
  gtk_table_attach_defaults (GTK_TABLE (table), filter_not_empty_check_button, 
                                                                   0, 2, 3, 4);
  gtk_widget_show (filter_not_empty_check_button);

  /* no cheats */

  filter_no_cheats_check_button = 
                    gtk_check_button_new_with_label ("cheats are not allowed");
  gtk_toggle_button_set_active (
          GTK_TOGGLE_BUTTON (filter_no_cheats_check_button), filter_no_cheats);
  gtk_table_attach_defaults (GTK_TABLE (table), filter_no_cheats_check_button, 
                                                                   0, 2, 4, 5);
  gtk_widget_show (filter_no_cheats_check_button);

  /* no password */

  filter_no_password_check_button = 
                      gtk_check_button_new_with_label ("no password required");
  gtk_toggle_button_set_active (
      GTK_TOGGLE_BUTTON (filter_no_password_check_button), filter_no_password);
  gtk_table_attach_defaults (GTK_TABLE (table), filter_no_password_check_button, 
                                                                   0, 2, 5, 6);
  gtk_widget_show (filter_no_password_check_button);

  gtk_widget_show (table);
  gtk_widget_show (alignment);
  gtk_widget_show (frame);
  gtk_widget_show (page_hbox);
}


int filters_cfg_dialog (int page_num) {
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *notebook;
  GtkWidget *button;
  GtkWidget *window;
  int changed = FALSE;
  int i;

#ifdef DEBUG
  const char *flt_status[3] = { "not changed", "changed", "data changed" };
#endif

  window = dialog_create_modal_transient_window ("XQF: Filters", 
                                                            TRUE, TRUE, NULL);
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);
         
  /*
   *  Notebook
   */

  notebook = gtk_notebook_new ();
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
  gtk_notebook_set_tab_hborder (GTK_NOTEBOOK (notebook), 4);
  gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);

  server_filter_page (notebook);

  player_filter_page (notebook);

  gtk_widget_show (notebook);

  /* 
   *  Buttons at the bottom
   */

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Cancel");
  gtk_widget_set_usize (button, 80, -1);
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                   GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT (window));
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("OK");
  gtk_widget_set_usize (button, 80, -1);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                          GTK_SIGNAL_FUNC (server_filter_new_defaults), NULL);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                          GTK_SIGNAL_FUNC (player_filter_new_defaults), NULL);
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                   GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT (window));
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);

  gtk_widget_show (hbox);

  gtk_widget_show (vbox);

  gtk_widget_show (window);

  for (i = 0; i < FILTERS_TOTAL; i++)
    filters[i].changed = FILTER_NOT_CHANGED;

  gtk_notebook_set_page (GTK_NOTEBOOK (notebook), page_num);

  gtk_main ();

  unregister_window (window);

  player_filter_cfg_clean_up ();

#ifdef DEBUG
  for (i = 0; i < FILTERS_TOTAL; i++) {
    fprintf (stderr, "%s Filter (%d): %s\n", filters[i].name, i,
	                                      flt_status[filters[i].changed]);
  }
#endif

  for (i = 0; i < FILTERS_TOTAL; i++) {
    if (filters[i].changed == FILTER_CHANGED) {
      changed = TRUE;
      break;
    }
  }

  return changed;
}


void filters_init (void) {
  int i;

  for (i = 0; i < FILTERS_TOTAL; i++) {
    if (filters[i].filter_init)
      (*filters[i].filter_init) ();
  }
}


void filters_done (void) {
  int i;

  for (i = 0; i < FILTERS_TOTAL; i++) {
    if (filters[i].filter_done)
      (*filters[i].filter_done) ();
  }
}

