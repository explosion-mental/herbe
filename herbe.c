#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xft/Xft.h>
#include <X11/Xresource.h>
#include <X11/Xlib.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#define EXIT_ACTION 0
#define EXIT_FAIL 1
#define EXIT_DISMISS 2

enum corners { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

static Display *dpy;
static int screen;
static Visual *visual;
static Colormap cmap;
static Window window;
static int screen_width;
static int screen_height;
static int exit_code = EXIT_DISMISS;

#include "config.h"

static void
die(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(EXIT_FAIL);
}

static int
maxlen(char *string, XftFont *font, int max_text_width)
{
	int i;
	int eol = strlen(string);
	XGlyphInfo info;
	XftTextExtentsUtf8(dpy, font, (FcChar8 *)string, eol, &info);

	if (info.width > max_text_width) {
		eol = max_text_width / font->max_advance_width;
		info.width = 0;

		while (info.width < max_text_width) {
			eol++;
			XftTextExtentsUtf8(dpy, font, (FcChar8 *)string, eol, &info);
		}
		eol--;
	}

	for (i = 0; i < eol; i++)
		if (string[i] == '\n') {
			string[i] = ' ';
			return ++i;
		}

	if (info.width <= max_text_width)
		return eol;

	int temp = eol;

	while (string[eol] != ' ' && eol)
		--eol;

	return eol == 0 ? temp : ++eol;
}

static void
expire(int sig)
{
	XEvent event;
	event.type = ButtonPress;
	/* SIGUSR1 dismiss, SIGUSR2 action */
	event.xbutton.button = (sig == SIGUSR2) ? action : dismiss;
	XSendEvent(dpy, window, 0, 0, &event);
	XFlush(dpy);
}

static void
read_y_offset(unsigned int **offset, int *id)
{
    int shm_id = shmget(8432, sizeof(unsigned int), IPC_CREAT | 0660);

    if (shm_id == -1)
	    die("shmget failed");

    *offset = (unsigned int *)shmat(shm_id, 0, 0);

    if (*offset == (unsigned int *)-1)
	    die("shmat failed\n");

    *id = shm_id;
}

static void
xrdbloadcolor(XrmDatabase xrdb, const char *name, char *var)
{
	XrmValue value;
	char *type;
	int i;

	if (XrmGetResource(xrdb, name, NULL, &type, &value) == True) { /* exist */
		if (strnlen(value.addr, 8) == 7 && value.addr[0] == '#') { /* is a hex color */
			for (i = 1; i < 7; i++) {
				if ((value.addr[i] < 48)
				|| (value.addr[i] > 57 && value.addr[i] < 65)
				|| (value.addr[i] > 70 && value.addr[i] < 97)
				|| (value.addr[i] > 102))
					break;
			}
			if (i == 7) {
				strncpy(var, value.addr, 7);
				var[7] = '\0';
			}
		}
        }
}

static void
loadxrdb(XrmDatabase xrdb)
{
	xrdbloadcolor(xrdb, "color0", background_color);
	xrdbloadcolor(xrdb, "color2", border_color);
	xrdbloadcolor(xrdb, "color4", font_color);
	XrmDestroyDatabase(xrdb);	/* close the database */
}

static void
free_y_offset(int id)
{
    shmctl(id, IPC_RMID, NULL);
}

static void
setup(void)
{
	struct sigaction act_expire;

	/* init signals */
	act_expire.sa_handler = expire;
	act_expire.sa_flags = SA_RESTART;
	sigemptyset(&act_expire.sa_mask);
	sigaction(SIGALRM, &act_expire, 0);
	sigaction(SIGTERM, &act_expire, 0);
	sigaction(SIGINT, &act_expire, 0);
	sigaction(SIGUSR1, &act_expire, 0);
	sigaction(SIGUSR2, &act_expire, 0);

	/* init display */
	if (!(dpy = XOpenDisplay(NULL)))
		die("Cannot open display");

	screen = DefaultScreen(dpy);
	visual = DefaultVisual(dpy, screen);
	cmap = DefaultColormap(dpy, screen);

	screen_width = DisplayWidth(dpy, screen);
	screen_height = DisplayHeight(dpy, screen);
}

int
main(int argc, char *argv[])
{
	if (argc == 1)
		die("usage: %s message", argv[0]);

	XrmInitialize();
	setup();

	/* init xresources */
	char *res_man = XResourceManagerString(dpy);
	XrmDatabase db = XrmGetStringDatabase(res_man);
	loadxrdb(db);

	XSetWindowAttributes attributes;
	attributes.event_mask = ExposureMask | ButtonPressMask;
	attributes.override_redirect = True;

	XftColor color;

	XftColorAllocName(dpy, visual, cmap, background_color, &color);
	attributes.background_pixel = color.pixel;

	XftColorAllocName(dpy, visual, cmap, border_color, &color);
	attributes.border_pixel = color.pixel;


	int max_text_width = width - 2 * padding;
	char **lines = NULL;
	size_t num_of_lines = 0;

	XftFont *font = XftFontOpenName(dpy, screen, font_pattern);

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-t")) {  /* duration */
			duration = atoi(argv[++i]);
			continue; /* ignore this argument */
		} else if (!strcmp(argv[i], "-v")) {
			puts("herbe-"VERSION);
			exit(0);
		}
		for (unsigned int eol = maxlen(argv[i], font, max_text_width); eol; argv[i] += eol, eol = maxlen(argv[i], font, max_text_width)) {

			if (!(lines = reallocarray(lines, ++num_of_lines, sizeof(char *)))
			|| !(lines[num_of_lines - 1] = reallocarray(NULL, eol + 1, sizeof(char))))
				die("reallocarray failed");

			strncpy(lines[num_of_lines - 1], argv[i], eol);
			lines[num_of_lines - 1][eol] = '\0';
		}
	}

    int y_offset_id;
    unsigned int *y_offset;
    read_y_offset(&y_offset, &y_offset_id);

	unsigned int text_height = font->ascent - font->descent;
	unsigned int height = (num_of_lines - 1) * line_spacing + num_of_lines * text_height + 2 * padding;
	unsigned int x = pos_x;
	unsigned int y = pos_y + *y_offset;

    unsigned int used_y_offset = (*y_offset) += height + padding;

	if (corner == TOP_RIGHT || corner == BOTTOM_RIGHT)
		x = screen_width - width - border_size * 2 - x;

	if (corner == BOTTOM_LEFT || corner == BOTTOM_RIGHT)
		y = screen_height - height - border_size * 2 - y;

	window = XCreateWindow(dpy, RootWindow(dpy, screen), x, y, width, height, border_size, DefaultDepth(dpy, screen),
			CopyFromParent, visual, CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &attributes);

	XftDraw *draw = XftDrawCreate(dpy, window, visual, cmap);
	XftColorAllocName(dpy, visual, cmap, font_color, &color);

	XMapWindow(dpy, window);

	if (duration != 0)
		alarm(duration);

	while (1) {

		XEvent event;
		XNextEvent(dpy, &event);

		if (event.type == Expose) {
			XClearWindow(dpy, window);
			for (size_t i = 0; i < num_of_lines; i++)
				XftDrawStringUtf8(draw, &color, font, padding, line_spacing * i + text_height * (i + 1) + padding,
								  (FcChar8 *)lines[i], strlen(lines[i]));
		} else if (event.type == ButtonPress) {
			if (event.xbutton.button == dismiss)
				break;
			else if (event.xbutton.button == action) {
				exit_code = EXIT_ACTION;
				break;
			}
		}
	}


	for (size_t i = 0; i < num_of_lines; i++)
		free(lines[i]);

    	if (used_y_offset == *y_offset)
		free_y_offset(y_offset_id);
	free(lines);
	XftDrawDestroy(draw);
	XftColorFree(dpy, visual, cmap, &color);
	XftFontClose(dpy, font);
	XCloseDisplay(dpy);

	return exit_code;
}
