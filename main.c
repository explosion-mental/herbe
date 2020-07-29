#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

int main(int argc, char *argv[])
{
	Display *display = XOpenDisplay(NULL);
	XEvent event;

	if (display == NULL)
	{
		fprintf(stderr, "Cannot open display\n");
		exit(EXIT_FAILURE);
	}

	int screen = DefaultScreen(display);

	int window_width = DisplayWidth(display, screen);
	int window_height = DisplayHeight(display, screen);

	XftColor color;

	Window root = RootWindow(display, screen);
	XSetWindowAttributes attributes;
	attributes.override_redirect = True;
	XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen), background_color, &color);
	attributes.background_pixel = color.pixel;
	XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen), border_color, &color);
	attributes.border_pixel = color.pixel;

	XftFont *font = XftFontOpenName(display, screen, font_style);

	unsigned short x = pos_x;
	unsigned short y = pos_y;
	int height = font->ascent - font->descent + text_padding * 2;
	switch (corner) {
		case down_right:
			y = window_height - height - border_size * 2 - pos_y;
		case top_right:
			x = window_width - width - border_size * 2 - pos_x;
			break;
		case down_left:
			y = window_height - height - border_size * 2 - pos_y;
	}

	Window window = XCreateWindow(
		display, root, x,
		y, width, height, border_size,
		DefaultDepth(display, screen), CopyFromParent,
		DefaultVisual(display, screen),
		CWOverrideRedirect | CWBackPixel | CWBorderPixel, &attributes);

	XftDraw *draw = XftDrawCreate(display, window, DefaultVisual(display, screen), DefaultColormap(display, screen));
	XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen), font_color, &color);

	XMapWindow(display, window);

	XftDrawString8(draw, &color, font, text_padding, height - text_padding, (XftChar8 *)argv[1], strlen(argv[1]));

	// TODO free xftcolor

	XNextEvent(display, &event);

	sleep(duration);

	XCloseDisplay(display);
	return 0;
}