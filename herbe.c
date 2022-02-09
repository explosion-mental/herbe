#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/Xresource.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>


#define EXIT_ACTION 0
#define EXIT_FAIL 1
#define EXIT_DISMISS 2

enum corners { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

static Display *display;
static Window window;
static int exit_code = EXIT_DISMISS;

#include "config.h"

static void die(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(EXIT_FAIL);
}

int get_max_len(char *string, XftFont *font, int max_text_width)
{
	int eol = strlen(string);
	XGlyphInfo info;
	XftTextExtentsUtf8(display, font, (FcChar8 *)string, eol, &info);

	if (info.width > max_text_width)
	{
		eol = max_text_width / font->max_advance_width;
		info.width = 0;

		while (info.width < max_text_width)
		{
			eol++;
			XftTextExtentsUtf8(display, font, (FcChar8 *)string, eol, &info);
		}

		eol--;
	}

	for (int i = 0; i < eol; i++)
		if (string[i] == '\n')
		{
			string[i] = ' ';
			return ++i;
		}

	if (info.width <= max_text_width)
		return eol;

	int temp = eol;

	while (string[eol] != ' ' && eol)
		--eol;

	if (eol == 0)
		return temp;
	else
		return ++eol;
}

void expire(int sig)
{
	XEvent event;
	event.type = ButtonPress;
	event.xbutton.button = (sig == SIGUSR2) ? (ACTION_BUTTON) : (DISMISS_BUTTON);
	XSendEvent(display, window, 0, 0, &event);
	XFlush(display);
}

void read_y_offset(unsigned int **offset, int *id) {
    int shm_id = shmget(8432, sizeof(unsigned int), IPC_CREAT | 0660);
    if (shm_id == -1) die("shmget failed");

    *offset = (unsigned int *)shmat(shm_id, 0, 0);
    if (*offset == (unsigned int *)-1) die("shmat failed\n");
    *id = shm_id;
}

void
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

void
loadxrdb(XrmDatabase xrdb)
{
	if (xrdb != NULL) {
		xrdbloadcolor(xrdb, "color0", background_color);
		xrdbloadcolor(xrdb, "color8", border_color);
		xrdbloadcolor(xrdb, "color2", font_color);
 		XrmDestroyDatabase(xrdb);	/* close the database */
	} else { /* fallback colors */
		strcpy(background_color, "#444444");
		strcpy(border_color, "#bbbbbb");
		strcpy(font_color, "#222222");
	}
}

void free_y_offset(int id) {
    shmctl(id, IPC_RMID, NULL);
}

int main(int argc, char *argv[])
{
	if (argc == 1)
        die("Usage: %s body", argv[0]);

	struct sigaction act_expire, act_ignore;

	act_expire.sa_handler = expire;
	act_expire.sa_flags = SA_RESTART;
	sigemptyset(&act_expire.sa_mask);

	act_ignore.sa_handler = SIG_IGN;
	act_ignore.sa_flags = 0;
	sigemptyset(&act_ignore.sa_mask);

	sigaction(SIGALRM, &act_expire, 0);
	sigaction(SIGTERM, &act_expire, 0);
	sigaction(SIGINT, &act_expire, 0);

	sigaction(SIGUSR1, &act_ignore, 0);
	sigaction(SIGUSR2, &act_ignore, 0);

	if (!(display = XOpenDisplay(0)))
		die("Cannot open display");

	/* init xresources */
	XrmInitialize();
	char *res_man = XResourceManagerString(display);
	XrmDatabase db = XrmGetStringDatabase(res_man);
	loadxrdb(db);

	int screen = DefaultScreen(display);
	Visual *visual = DefaultVisual(display, screen);
	Colormap colormap = DefaultColormap(display, screen);

	int screen_width = DisplayWidth(display, screen);
	int screen_height = DisplayHeight(display, screen);

	XSetWindowAttributes attributes;
	attributes.override_redirect = True;
	XftColor color;
	XftColorAllocName(display, visual, colormap, background_color, &color);
	attributes.background_pixel = color.pixel;
	XftColorAllocName(display, visual, colormap, border_color, &color);
	attributes.border_pixel = color.pixel;

	int num_of_lines = 0;
	int max_text_width = width - 2 * padding;
	int lines_size = 5;
	char **lines = malloc(lines_size * sizeof(char *));
	if (!lines)
		die("malloc failed");

	XftFont *font = XftFontOpenName(display, screen, font_pattern);

	for (int i = 1; i < argc; i++)
	{
		for (unsigned int eol = get_max_len(argv[i], font, max_text_width); eol; argv[i] += eol, num_of_lines++, eol = get_max_len(argv[i], font, max_text_width))
		{
			if (lines_size <= num_of_lines)
			{
				lines = realloc(lines, (lines_size += 5) * sizeof(char *));
				if (!lines)
					die("realloc failed");
			}

			lines[num_of_lines] = malloc((eol + 1) * sizeof(char));
			if (!lines[num_of_lines])
				die("malloc failed");

			strncpy(lines[num_of_lines], argv[i], eol);
			lines[num_of_lines][eol] = '\0';
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

	window = XCreateWindow(display, RootWindow(display, screen), x, y, width, height, border_size, DefaultDepth(display, screen),
						   CopyFromParent, visual, CWOverrideRedirect | CWBackPixel | CWBorderPixel, &attributes);

	XftDraw *draw = XftDrawCreate(display, window, visual, colormap);
	XftColorAllocName(display, visual, colormap, font_color, &color);

	XSelectInput(display, window, ExposureMask | ButtonPress);
	XMapWindow(display, window);

	sigaction(SIGUSR1, &act_expire, 0);
	sigaction(SIGUSR2, &act_expire, 0);

	if (duration != 0)
		alarm(duration);

	for (;;)
	{
		XEvent event;
		XNextEvent(display, &event);

		if (event.type == Expose)
		{
			XClearWindow(display, window);
			for (int i = 0; i < num_of_lines; i++)
				XftDrawStringUtf8(draw, &color, font, padding, line_spacing * i + text_height * (i + 1) + padding,
								  (FcChar8 *)lines[i], strlen(lines[i]));
		}
		else if (event.type == ButtonPress)
		{
			if (event.xbutton.button == DISMISS_BUTTON)
				break;
			else if (event.xbutton.button == ACTION_BUTTON)
			{
				exit_code = EXIT_ACTION;
				break;
			}
		}
	}


	for (int i = 0; i < num_of_lines; i++)
		free(lines[i]);

    if (used_y_offset == *y_offset) free_y_offset(y_offset_id);
	free(lines);
	XftDrawDestroy(draw);
	XftColorFree(display, visual, colormap, &color);
	XftFontClose(display, font);
	XCloseDisplay(display);

	return exit_code;
}
