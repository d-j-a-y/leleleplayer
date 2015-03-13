#include <stdio.h>
#include "gui.h"

struct song song;
GtkWidget *progressbar;
GtkAdjustment *adjust;
GtkAdjustment *integer;
GtkRange *range;
int bartag;
int continue_count;

bool InitOpenAL(void) {
	ALCdevice *Device = alcOpenDevice(NULL);
	if(!Device) {
		printf("Error while opening the device\n");
		exit(1);
	}
	
	ALCcontext *Context = alcCreateContext(Device, NULL);
	if(!Context) {
		printf("Error while creating context\n");
		exit(1);
	}

	if(!alcMakeContextCurrent(Context)) {
		printf("Error while activating context\n");
		exit(1);
	}
	return 0;
}

void ShutdownOpenAL(void) {
	ALCcontext *Context = alcGetCurrentContext();
	ALCdevice *Device = alcGetContextsDevice(Context);

	alcMakeContextCurrent(NULL);
	alcDestroyContext(Context);
	alcCloseDevice(Device);
}

static void destroy (GtkWidget *window, gpointer data) {
	ShutdownOpenAL();
	gtk_main_quit ();
}

void explore(GDir *dir, char* folder) {
	const gchar *file;
	FILE *liste;

	liste = fopen("list.txt", "a");
	while((file = g_dir_read_name(dir))) {
		if(g_str_has_suffix(file, "flac") || g_str_has_suffix(file, "mp3")) {
			g_fprintf(liste, "%s\n", g_build_path("/", folder, file, NULL));
		}
		else {
		//	printf("%s\n", g_build_path("/", folder, file, NULL));
			explore(g_dir_open(g_build_path("/", folder, file, NULL), 0, NULL), g_build_path("/", folder, file, NULL));
		}
	}
	fclose(liste);
}

static gboolean endless(gpointer pargument) {
	float resnum = 0;
	FILE *file;
	char line[1000];
	int i;
	int count;
	int rand;
	char *pline;
	
	pline = line;

	file = fopen("list.txt", "r");
	
	for(; fgets(line, 1000, file) != NULL; ++count)
		;

	while(resnum != 1) {
		free(current_sample_array);
		rewind(file);
		pline = line;
		for(i = 0; i < g_random_int_range(0, count); ++i)
			fgets(line, 1000, file);

		for(;*pline != '\n';++pline)
			;
		*pline = '\0';
		resnum = analyze(line);
	}

	gtk_label_set_text((GtkLabel*)(((struct arguments*)pargument)->label), line);
	status = 0;
	play(NULL, pargument);
}

/* When a folder is selected, use that as the new location of the other chooser. */
static void folder_changed (GtkFileChooser *chooser1, struct arguments *pargument) {
	float resnum = 0;
	gchar *folder = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser1));
	GDir *dir = g_dir_open (folder, 0, NULL);
	explore(dir, folder);

	FILE *file;
	char line[1000];
	int i;
	int count;
	int rand;
	char *pline;
	
	pline = line;

	file = fopen("list.txt", "r");
	
	for(; fgets(line, 1000, file) != NULL; ++count) 
		;

	while(resnum != 1) {
		free(current_sample_array);
		rewind(file);
		pline = line;
		for(i = 0; i < g_random_int_range(0, count); ++i)
			fgets(line, 1000, file);

		for(;*pline != '\n';++pline)
			;
		*pline = '\0';
	
		resnum = analyze(line);
		gtk_label_set_text((GtkLabel*)pargument->label, line);
	}
}
/* When a file is selected, display the full path in the GtkLabel widget. */

static void file_changed (GtkFileChooser *chooser2, struct arguments *pargument) {
	int resnum = 0;
	gchar *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser2));

	resnum = analyze(filename);
	gtk_adjustment_configure(adjust, 0, 0, duration, 1, 1, 1);
	gtk_adjustment_changed(adjust);

	if(resnum == 2) {
  		gtk_label_set_text ((GtkLabel*)pargument->label, "Can't Conclude");

	}
	else if(resnum == 0) {
  		gtk_label_set_text ((GtkLabel*)pargument->label, "Much loud");

	}
	else if(resnum == 1) {
  		gtk_label_set_text ((GtkLabel*)pargument->label, "Such calm");

	}
}

static progressbarre(gpointer pargument) {
	gtk_adjustment_set_value (adjust, g_timer_elapsed(((struct arguments*)pargument)->elapsed, NULL));
	gtk_adjustment_changed(adjust);
	g_source_remove(bartag);
	bartag = g_timeout_add_seconds(1, progressbarre, pargument);
}

int play(GtkWidget *button, struct arguments *pargument) {
	ALenum format;
	ALuint buffer;
	alGetSourcei(source, AL_SOURCE_STATE, &status);

	if(status == 0 || status == AL_STOPPED) {
		alDeleteBuffers(1, &buffer);
		alSourcei(source, AL_BUFFER, 0);
		alDeleteSources(1, &source);
		g_timer_start(pargument->elapsed);
		alGenBuffers(1, &buffer);
		alGenSources(1, &source);

		format = AL_FORMAT_STEREO16;
		alBufferData(buffer, format, current_sample_array, nSamples * sizeof(ALint), sample_rate);

		alSourcei(source, AL_BUFFER, buffer);
		alGetSourcei(source, AL_SOURCE_STATE, &status);
		alSourcePlay(source); // finally playing the track!
		g_source_remove(pargument->tag);
		if(pargument->endless_check || continue_count-- > 0)
			pargument->tag = g_timeout_add_seconds(duration, endless, pargument);
		bartag = g_timeout_add_seconds(1, progressbarre, pargument);
		return 0;
	}

	if(status == AL_PLAYING) {
		if(pargument->endless_check) {
			g_timer_stop(pargument->elapsed);
			g_source_remove(pargument->tag);
		}
		alSourcePause(source);
		return 0;
	} 
	else {
		alSourcePlay(source);
		if(pargument->endless_check) {
			g_timer_continue(pargument->elapsed);
			pargument->tag = g_timeout_add_seconds(duration - (int)g_timer_elapsed(pargument->elapsed, NULL), endless, pargument->label);
		}
		return 0;
	}
}



static void check_toggled (GtkToggleButton *check, struct arguments *pargument) {
	pargument->endless_check = !(pargument->endless_check);
}

static void check_continue_toggled(GtkToggleButton *check_continue, GtkWidget *spin_int) {
	if(gtk_toggle_button_get_active(check_continue))
		gtk_widget_set_sensitive(spin_int, TRUE);
	else
		gtk_widget_set_sensitive(spin_int, FALSE);
}

static void spin_count(GtkSpinButton *spin_int, gpointer data) {
	printf("SWAG\n");
	continue_count = gtk_spin_button_get_value(spin_int);
}

int main(int argc, char **argv) {
	struct arguments argument;
	struct arguments *pargument = &argument;
	InitOpenAL();
	source = 0;
	pargument->endless_check = 0;
	pargument->elapsed = g_timer_new();
	GtkWidget *window, *button, *chooser1, *chooser2, *vbox, *check, *check_continue, *button1, *button2, *spin_int;
	GtkFileFilter *filter1, *filter2;

	gtk_init(&argc, &argv);
	
	g_remove("list.txt");
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	
	adjust = (GtkAdjustment*)gtk_adjustment_new(0, 0, 100, 1, 1, 1);
	//gtk_range_set_adjustment (range, adjust);

	#if GTK3
	progressbar = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adjust);
	#endif
	progressbar = gtk_hscale_new(adjust);
	gtk_window_set_title(GTK_WINDOW(window), "lelele player");
	pargument->label = gtk_label_new ("");
	gtk_container_set_border_width(GTK_CONTAINER(window), 25);
	gtk_widget_set_size_request(window, 500, 200);

	spin_int = gtk_spin_button_new_with_range(1, 10000, 1);
	button = gtk_button_new_with_mnemonic("_Play/Pause");
	button1 = gtk_button_new_with_mnemonic("test1");
	button2 = gtk_button_new_with_mnemonic("test2");
	check = gtk_check_button_new_with_label ("Endless mode.");
	check_continue = gtk_check_button_new_with_label("Play this number of songs.");


	g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy), NULL);
  	g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(play), pargument);
	g_signal_connect (G_OBJECT(check), "toggled", G_CALLBACK(check_toggled), pargument);
	g_signal_connect(G_OBJECT(check_continue), "toggled", G_CALLBACK(check_continue_toggled), spin_int);
	g_signal_connect(G_OBJECT(spin_int), "value-changed", G_CALLBACK(spin_count), NULL);


  /* Create two buttons, one to select a folder and one to select a file. */

	chooser1 = gtk_file_chooser_button_new ("Chooser a Folder", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	chooser2 = gtk_file_chooser_button_new ("Chooser a Folder", GTK_FILE_CHOOSER_ACTION_OPEN);

  /* Monitor when the selected folder or file are changed. */
	g_signal_connect (G_OBJECT (chooser2), "selection_changed", G_CALLBACK (file_changed), pargument);
	g_signal_connect (G_OBJECT (chooser1), "selection_changed", G_CALLBACK(folder_changed), pargument);
  /* Set both file chooser buttons to the location of the user's home directory. */
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser2), g_get_home_dir());

	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser1), g_get_home_dir());
  /* Provide a filter to show all files and one to shown only 3 types of images. */
	filter1 = gtk_file_filter_new ();
	filter2 = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter1, "Audio Files");
	gtk_file_filter_set_name (filter2, "All Files");
	gtk_file_filter_add_pattern (filter1, "*.mp3");
	gtk_file_filter_add_pattern (filter1, "*.flac");
	gtk_file_filter_add_pattern (filter1, "*.aac");
	gtk_file_filter_add_pattern (filter2, "*");

  /* Add the both filters to the file chooser button that selects files. */
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser2), filter1);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser2), filter2);

	


	gtk_widget_set_sensitive(spin_int, FALSE);
	vbox = gtk_vbox_new (GTK_ORIENTATION_VERTICAL, 5);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), chooser1);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), chooser2);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), button);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), pargument->label);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), check);
	gtk_box_pack_start_defaults (GTK_BOX(vbox), progressbar);
	gtk_box_pack_start_defaults (GTK_BOX(vbox), check_continue);
	gtk_box_pack_start_defaults (GTK_BOX(vbox), spin_int);


#ifdef GTK3
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
 	gtk_box_pack_start (GTK_BOX (vbox), chooser1, TRUE, TRUE, 3);
	gtk_box_pack_start (GTK_BOX (vbox), chooser2, TRUE, TRUE, 3);
	gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 3);
	gtk_box_pack_start (GTK_BOX (vbox), pargument->label, TRUE, TRUE, 3);
	gtk_box_pack_start (GTK_BOX (vbox), check, TRUE, TRUE, 3);
	gtk_box_pack_start (GTK_BOX(vbox), progressbar, TRUE, TRUE, 3);
#endif

	gtk_container_add (GTK_CONTAINER (window), vbox);
	
	gtk_widget_show_all(window);
	gtk_main();
	free(current_sample_array);
	return 0;
}
