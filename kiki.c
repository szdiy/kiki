/*
 * kiki.c
 *
 *  Created on: 2011-09-19
 *  Author: Anders Ma (xuejiao.ma@gmail.com)
 */
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib-object.h>
#include <glib.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#include "kiki.h"
#include "mx.h"
#include "serial.h"
#include "hacker.h"

#define DEF_LOG_FOLDER "log"
#define MAX_HACKER_BACKLOG 10
#define ADD_ONE_WITH_WRAP_AROUND(value, top) \
	do { if ((value) >= ((top) - 1)) (value) = 0; \
		else (value)++; } while (0)

#define NICKNAME "Nick Name\n昵称"
#define PROFESSION "Profession\n职业"
#define WHY "Why coming here?\n为什么来这里？"
#define STARTTIME "Date/Time\n刷卡时间"

enum
{
  COL_TIME = 0,
  COL_PROF,
  COL_NAME,
  COL_WHY,
  NUM_COLS
} ;

typedef enum
{
  COLD_LOG = 0,
  WARM_LOG
} log_t;


/*
 * static variables
 */

/* System */
static int debug_enable = 0;
/* GTK+ */
static GtkBuilder *theXml;
static GtkWidget  *theHackerList;
static GtkListStore *theHackerListStore;
static GtkTreeIter theHackerListIter;
/* communication */
static mx_t mx;
static serial_t serial;
static g_update_hacker_list = 0;
static hacker_list_t szdiy;
/* rfid processing ring buffer */
static hacker_t hacker_backlog[MAX_HACKER_BACKLOG];
static int hacker_process_index = 0;
static int hacker_present_index = 0;
static int hacker_number = 0;

/* Global variables */
int sighup_flag = 0;            /* 1 => signal has happened, needs attention */
int sigchld_flag = 0; 

/* Static function */
static void reload_configuration();
static void init_log(log_t log);
static gint rfid_id_callback (packet_t *p, void *arg);


void
DBG_PRINTF(const char *msg, ...)
{
        va_list fmtargs;
        char buf[200];

        if (!debug_enable)
                return;
        va_start(fmtargs, msg);
        vsnprintf(buf, sizeof(buf)-1, msg, fmtargs);
        va_end(fmtargs);
        printf("%s",buf);
}

char* strip(char *buffer){
	int x;
	int y;
	int z;

	if (buffer == NULL || buffer[0] == '\x0')
		return buffer;

	/* strip end of string */
	y = (int)strlen(buffer);
	for (x = y - 1; x >= 0; x--) {
		if (buffer[x] == ' ' || buffer[x] == '\n' || buffer[x] == '\r' || buffer[x] == '\t' || buffer[x] == 13)
			buffer[x]='\x0';
		else
			break;
	}

	/* strip beginning of string (by shifting) */
	y = (int)strlen(buffer);
	for (x = 0; x < y; x++) {
		if (buffer[x] == ' ' || buffer[x] == '\n' || buffer[x] == '\r' || buffer[x] == '\t' || buffer[x] == 13)
			continue;
		else
			break;
	}
	if (x > 0) {
		for (z = x; z < y; z++)
			buffer[z-x] = buffer[z];
		buffer[y - x] = '\x0';
	}

	return buffer;
}


static void
enable_debug()
{
        debug_enable = 1;
}

void sigchld(int dummy)
{
	sigchld_flag = 1;
}

void sighup(int dummy)
{
	sighup_flag = 1;
}

void init_signals(void)
{
	struct sigaction sa;
	
	sa.sa_handler = sigchld;	
	sigaction(SIGCHLD, &sa, NULL);
	sa.sa_handler = sighup;
	sigaction(SIGHUP, &sa, NULL);
}

static void usage ()
{
	fprintf(stderr, "kiki [option]\n");
	fprintf(stderr, "\t -d      serial device (eg: /dev/ttyUSB0)\n");
	fprintf(stderr, "\t -s      serial speed (eg: 57600)\n");
	fprintf(stderr, "\t -h      this usage info\n");
	fprintf(stderr, "\t -v      output debug information\n");
}

/*
 * destroy (program quits)
 */
void destroy (GtkWidget *widget, gpointer data)
{
	serial_close(&serial);
	mx_destroy(&mx);
	hackers_destroy(&szdiy);
	gtk_main_quit ();
}

static update_hacker_number(int n)
{
	GtkLabel *label;
	char buf[10];

	snprintf(buf, sizeof(buf), "%d", n);
	label = (GtkLabel*) GTK_WIDGET (gtk_builder_get_object (theXml, "labelHackerNumber"));
	gtk_label_set_text (label, buf); 
}

static gboolean idle_update_hacker_list(gpointer data)
{
	pid_t pid;
	int status;
	char *argv[10];
	time_t timep;
	struct tm *p;
	char tbuf[50], cmd[300], rfid[50], name[50];
	int i;
	char cwd[200];
	time(&timep);
	p = localtime(&timep);


	packet_t pkt;
	pkt.raw.rfid_id[0] = 0;
	pkt.raw.rfid_id[1] = 0;
	pkt.raw.rfid_id[2] = 0;
	pkt.raw.rfid_id[3] = 0;

	/* check if needing hackers reloading */
	if (sighup_flag) {
		sighup_flag = 0;
		reload_configuration();
	}

	if (sigchld_flag) {
		waitpid(-1, &status, 0);
	}

	sprintf(tbuf, "%4d-%2d-%2d/%2d:%2d:%2d", (1900+p->tm_year), 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
	/* special treatment for space 0->" 0"->"00" */	
	for (i = 0; i < strlen(tbuf); i++) {
		if (tbuf[i] == ' ') {
			tbuf[i] = '0';
		}
	}
	if (hacker_present_index != hacker_process_index) {
		/* Append a row and fill in some data */
		gtk_list_store_append (theHackerListStore, &theHackerListIter);
		gtk_list_store_set (theHackerListStore, &theHackerListIter,
					COL_TIME, tbuf,
					COL_NAME, hacker_backlog[hacker_process_index].name,
					COL_PROF, hacker_backlog[hacker_process_index].prof,
					COL_WHY, hacker_backlog[hacker_process_index].why,
					-1);
		/* invoke log.sh shell script */
		getcwd(cwd, sizeof(cwd));
		snprintf(cmd, sizeof(cmd), "%s/log.sh", cwd); 
		snprintf(rfid, sizeof(name), "%s", hacker_backlog[hacker_process_index].rfid);
		snprintf(name, sizeof(name), "%s", hacker_backlog[hacker_process_index].name);
		pid = fork();
		if (pid == 0) {
			argv[0] = strdup("/bin/bash");	
			argv[1] = strdup(cmd);	
			argv[2] = strdup(rfid);	
			argv[3] = strdup(name);	
			argv[4] = strdup(tbuf);	
			argv[5] = NULL;
			execve("/bin/bash", argv, NULL);
			_exit(1);
		}

		ADD_ONE_WITH_WRAP_AROUND(hacker_process_index, MAX_HACKER_BACKLOG);
	}

	return TRUE;
}

static void reload_configuration()
{
	mx_rx_unregister(&mx, PTYPE_RFID_ID, rfid_id_callback);
	hackers_reload(&szdiy);
	init_log(WARM_LOG);
	mx_rx_register(&mx, PTYPE_RFID_ID, rfid_id_callback, NULL);
}

static void init_log(log_t log) 
{
	int i;
	FILE* fp;
	char buf[200];
	char filename[50], full_filename[100];
	char *token;
	hacker_t *hacker;

	time_t timep;
	struct tm *pt;

	time(&timep);
	pt = localtime(&timep);

	/* handling login_time */
	snprintf(filename, sizeof(filename), "%4d-%2d-%2d", (1900+pt->tm_year), 1 + pt->tm_mon, pt->tm_mday);
	snprintf(full_filename, sizeof(full_filename), "%s/%s", DEF_LOG_FOLDER, filename);
	/* special treatment for space 0->" 0"->"00" */	
	for (i = 0; i < strlen(full_filename); i++) {
		if (full_filename[i] == ' ') {
			full_filename[i] = '0';
		}
	}

	fp = fopen(full_filename, "r");
	if (!fp) {
		//fprintf(stderr, "fopen %s error\n", full_filename);
		return;
	}
	while (fgets(buf, sizeof(buf), fp)) {
		// rfid		
		token = strtok(buf, "%");
		if (token) {
			hacker = search_hacker(&szdiy, token);
			if (hacker) {
				hacker->login++;
				// name
				token = strtok(NULL, "%");
				// date/time
				token = strtok(NULL, "%");
				if (token) {
					assert(hacker->login_time == NULL);
					hacker->login_time = strdup(token);
				}
			}
		}
		if (hacker && log == COLD_LOG) {
			/* Append a row and fill in some data */
			gtk_list_store_append (theHackerListStore, &theHackerListIter);
			gtk_list_store_set (theHackerListStore, &theHackerListIter,
						COL_TIME, hacker->login_time,
						COL_NAME, hacker->name,
						COL_PROF, hacker->prof,
						COL_WHY, hacker->why,
						-1);
			update_hacker_number(++hacker_number);
		}
	}
	fclose(fp);
}

static GtkTreeModel* create_and_fill_model (void)
{
	theHackerListStore = gtk_list_store_new (NUM_COLS, G_TYPE_STRING, 
				G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	return GTK_TREE_MODEL (theHackerListStore);
}

static GtkWidget* create_hacker_mvc (void)
{
	GtkTreeViewColumn   *col;
	GtkCellRenderer     *renderer;
	GtkTreeModel        *model;
	GtkWidget           *view;

	view = GTK_WIDGET (gtk_builder_get_object (theXml, "treeview1"));

	/* --- Column #1 --- */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
								-1,      
								STARTTIME,  
								renderer,
								"text", COL_TIME,
								NULL);

	/* --- Column #2 --- */
	renderer = gtk_cell_renderer_text_new ();
  	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
								-1,      
								PROFESSION,  
								renderer,
								"text", COL_PROF,
								NULL);
	/* --- Column #3 --- */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
								-1,      
								NICKNAME,  
								renderer,
								"text", COL_NAME,
								NULL);
	/* --- Column #4 --- */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
								-1,      
								WHY,  
								renderer,
								"text", COL_WHY,
								NULL);

	theHackerListStore = gtk_list_store_new (NUM_COLS, G_TYPE_STRING, 
				G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	model = GTK_TREE_MODEL (theHackerListStore);
	gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);
	g_object_unref (model); /* destroy model automatically with view */

	return view;
}


static gboolean ping_rfid (gpointer data)
{
	packet_t pkt;
	char cmd[5] = {0xaa, 0xbb, 0x02, 0x20, 0x22};

	//update_hacker_number(++hacker_number);
	pkt.type = PTYPE_RFID_CMD;
	memcpy(pkt.raw.rfid_cmd, cmd, 5);
	mx_tx_packet(&mx, &pkt);

	return TRUE;
}

static gint rfid_id_callback (packet_t *p, void *arg)
{
	int i;
	static int counter = 0;
	hacker_t* hacker;
	char rfid[20], tbuf[50];

	time_t timep;
	struct tm *pt;

	time(&timep);
	pt = localtime(&timep);

	g_printf("[rfid %d ] ", counter++);
	for (i = 0; i < 5; i++) {
		g_printf("%x,", p->raw.rfid_id[i]);
	}
	g_printf("\n");

	snprintf(rfid, sizeof(rfid), "%2x%2x%2x%2x%2x", p->raw.rfid_id[0], p->raw.rfid_id[1], 
			p->raw.rfid_id[2], p->raw.rfid_id[3], p->raw.rfid_id[4]);
	/* special treatment for space 0->" 0"->"00" */	
	for (i = 0; i < 10; i++) {
		if (rfid[i] == ' ') {
			rfid[i] = '0';
		}
	}

	hacker = search_hacker(&szdiy, rfid);
	if (hacker && !hacker->login++) {
		/* handling login_time */
		sprintf(tbuf, "%4d-%2d-%2d/%2d:%2d:%2d", (1900+pt->tm_year), 1 + pt->tm_mon, 
				pt->tm_mday, pt->tm_hour, pt->tm_min, pt->tm_sec);
		/* special treatment for space 0->" 0"->"00" */	
		for (i = 0; i < strlen(tbuf); i++) {
			if (tbuf[i] == ' ') {
				tbuf[i] = '0';
			}
		}
		if (!hacker->login_time) { /* double check */
			hacker->login_time = strdup(tbuf);
		}
		hacker_backlog[hacker_present_index].rfid = hacker->rfid;
		hacker_backlog[hacker_present_index].name = hacker->name;
		hacker_backlog[hacker_present_index].prof = hacker->prof;
		hacker_backlog[hacker_present_index].why = hacker->why;
		hacker_backlog[hacker_present_index].login_time = hacker->login_time;
		update_hacker_number(++hacker_number);
		ADD_ONE_WITH_WRAP_AROUND(hacker_present_index, MAX_HACKER_BACKLOG);
	}
}

void show_warning(gpointer window, const gchar *msg)
{
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new(window,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_WARNING,
		GTK_BUTTONS_OK,
		"%s",
		msg);
	gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


/*
 * show new dialog
 */
void on_new (GtkWidget* widget, gpointer data)
{
	GtkDialog *new;

	new = GTK_DIALOG (gtk_builder_get_object (theXml, "newdialog"));
	gtk_dialog_run(new);
}

void on_newDialogClose(GtkWidget* widget, gpointer data)
{
	GtkDialog *new;
		
	new = GTK_DIALOG (gtk_builder_get_object (theXml, "newdialog"));
	gtk_widget_hide(GTK_WIDGET(new));

}

/*
 * show about dialog
 */
void on_about (GtkWidget* widget, gpointer data)
{
	GtkDialog *about;

	about = GTK_DIALOG (gtk_builder_get_object (theXml, "aboutdialog"));
	gtk_dialog_run(about);
	gtk_widget_hide(GTK_WIDGET(about));
}

void on_newDialogButtonNew_clicked(GtkWidget* widget, gpointer data)
{
	hacker_t* hacker;

	GtkEntry  *rfid;
	GtkEntry *nickname;
	GtkEntry *prof;
	GtkEntry *why;
	GtkEntry *email;
	GtkEntry *mac;
	
	rfid = GTK_ENTRY (gtk_builder_get_object (theXml, "newDialogEntryRFID"));
	nickname = GTK_ENTRY (gtk_builder_get_object (theXml, "newDialogEntryNickname"));
	prof = GTK_ENTRY (gtk_builder_get_object (theXml, "newDialogEntryProf"));
	why = GTK_ENTRY (gtk_builder_get_object (theXml, "newDialogEntryWhy"));
	email = GTK_ENTRY (gtk_builder_get_object (theXml, "newDialogEntryEmail"));
	mac = GTK_ENTRY (gtk_builder_get_object (theXml, "newDialogEntryMAC"));

	if (strlen(gtk_entry_get_text (nickname)) == 0 ) {
		show_warning((GtkWidget*)gtk_builder_get_object (theXml, "newdialog"),
				"Nickname is empty!");
		return;
	}
	if (strchr((char*)gtk_entry_get_text (nickname), ' ')) {
		show_warning((GtkWidget*)gtk_builder_get_object (theXml, "newdialog"),
				"Nickname can't contain space character!");
		return;
	}

	hacker = search_hacker(&szdiy, (char*)gtk_entry_get_text (rfid));

	if (hacker != NULL) {
		free(hacker->name);	
		hacker->name = strdup(gtk_entry_get_text (nickname));
		free(hacker->prof);	
		hacker->prof = strdup(gtk_entry_get_text (prof));
		free(hacker->why);	
		hacker->why = strdup(gtk_entry_get_text (why));
		free(hacker->email);	
		hacker->email = strdup(gtk_entry_get_text (email));
		free(hacker->mac);	
		hacker->mac = strdup(gtk_entry_get_text (mac));		
	} else {
		hacker =  new_hacker(&szdiy);
		hacker->rfid = strdup((char*)(gtk_entry_get_text (rfid)));
		hacker->name = strdup((char*)(gtk_entry_get_text (nickname)));
		hacker->prof = strdup(gtk_entry_get_text (prof));
		hacker->why = strdup(gtk_entry_get_text (why));
		hacker->email = strdup(gtk_entry_get_text (email));
		hacker->mac = strdup(gtk_entry_get_text (mac));
	}
	hackers_save(&szdiy);
	reload_configuration();
}

void on_newDialogButtonSign_clicked(GtkWidget* widget, gpointer data)
{
	hacker_t* hacker;
	GtkEntry  *rfid;

	int i;
	char tbuf[50];	
	time_t timep;
	struct tm *pt;
	
	// to be changed to "newDialogEntryNickRFID", while RFID cards are available
	rfid = GTK_ENTRY (gtk_builder_get_object (theXml, "newDialogEntryRFID"));

	if (strlen(gtk_entry_get_text (rfid)) == 0 ) {
		show_warning((GtkWidget*)gtk_builder_get_object (theXml, "newdialog"),
				"Nickname is empty!");
		return;
	}
	if (strchr((char*)gtk_entry_get_text (rfid), ' ')) {
		show_warning((GtkWidget*)gtk_builder_get_object (theXml, "newdialog"),
				"Nickname can't contain space character!");
		return;
	}

	hacker = search_hacker(&szdiy, (char*)gtk_entry_get_text (rfid));

	if (hacker != NULL && !hacker->login++) {
		time(&timep);
		pt = localtime(&timep);

		/* handling login_time */
		sprintf(tbuf, "%4d-%2d-%2d/%2d:%2d:%2d", (1900+pt->tm_year), 1 + pt->tm_mon, 
				pt->tm_mday, pt->tm_hour, pt->tm_min, pt->tm_sec);
		/* special treatment for space 0->" 0"->"00" */	
		for (i = 0; i < strlen(tbuf); i++) {
			if (tbuf[i] == ' ') {
				tbuf[i] = '0';
			}
		}
		if (!hacker->login_time) { /* double check */
			hacker->login_time = strdup(tbuf);
		}
		hacker_backlog[hacker_present_index].rfid = hacker->rfid;
		hacker_backlog[hacker_present_index].name = hacker->name;
		hacker_backlog[hacker_present_index].prof = hacker->prof;
		hacker_backlog[hacker_present_index].why = hacker->why;
		hacker_backlog[hacker_present_index].login_time = hacker->login_time;
		update_hacker_number(++hacker_number);
		ADD_ONE_WITH_WRAP_AROUND(hacker_present_index, MAX_HACKER_BACKLOG);
	}
}

/*
 * main
 */
int main (int argc, char *argv[])
{
	
	GtkWidget *mainWindow;
	GtkWidget *copterDrawingArea;

	extern int optind;
	extern int optopt;
	extern int opterr;
	extern int optreset;

	char *optstr="d:s:h:v";
	char *sdev = NULL;
	int sspeed = -1;
	int opt = 0;


	/*
	 * Init argument control
	 */
	opt = getopt( argc, argv, optstr);
	while( opt != -1 ) {
		switch( opt ) {
		case 'd':
			sdev = optarg;
			break;
		case 's':
			sspeed = atoi(optarg);
			break;
		case 'h':
			usage();
			return 0;
		case 'v':
			enable_debug();
			break;
		default:
			break;
		}
		opt = getopt(argc, argv, optstr);
	}

	if (!g_thread_supported ()){ g_thread_init (NULL); }
	gdk_threads_init ();
	gdk_threads_enter ();

	/*
	 * Init GTK+ and GtkGLExt.
	 */
	gtk_init (&argc, &argv);

	/*
	 * Load the GTK interface.
	 */
	theXml = gtk_builder_new ();
	gtk_builder_add_from_file (theXml, "kiki.glade", NULL);
	if (theXml == NULL)
	{
		g_critical ("Failed to load an initialise the GTK file.\n");
		return -1;
	}

	/*
	 * Get the top-level window reference from loaded Glade file.
	 */
	mainWindow = GTK_WIDGET (gtk_builder_get_object (theXml, "windowMain"));

	// Set unassigned widgets to get handled automatically by the window manager.
	gtk_container_set_reallocate_redraws (GTK_CONTAINER (mainWindow), TRUE);

	/*
	 * Get the window manager to connect any assigned signals in the loaded Glade file to our coded functions.
	 */
	gtk_builder_connect_signals (theXml, NULL);

	/*
	 * Init hackers information
	 */
	hackers_init(&szdiy);
	theHackerList = create_hacker_mvc();
	init_log(COLD_LOG);

	/*
	 * Init serial & mx
	 */
	mx_init(&mx, serial_tx_data, (void*)&serial);
	mx_rx_register(&mx, PTYPE_RFID_ID, rfid_id_callback, NULL);
	serial_init(&serial, mx_rx_data, &mx);
	if (!sdev)
		sdev = "/dev/ttyUSB0";
	if (sspeed == -1)
		sspeed = 19200;
	serial_open(&serial, sdev, sspeed);

	init_signals();

	/*
	 * Show main window.
	 */
	gtk_widget_show (mainWindow);

	/*
	 * Init timer
	 */
	g_timeout_add (1000, ping_rfid, NULL);
	g_idle_add ((GtkFunction)idle_update_hacker_list, NULL);

	// Run the window manager loop.
	gtk_main ();

	gdk_threads_leave ();

	return 0;
}
