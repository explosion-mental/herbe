static char background_color[] = "#3e3e3e";
static char border_color[] = "#ececec";
static char font_color[] = "#ececec";
static char font_pattern[] = "Monofur Nerd Font:size=20:antialias=true:autohint=true";
static const unsigned line_spacing = 5;
static const unsigned int padding = 15;

static const unsigned int width = 450;
static const unsigned int border_size = 2;
static const unsigned int pos_x = 30;
static const unsigned int pos_y = 60;

enum corners corner = TOP_RIGHT;

/* overwritten by -t flag, if 0 the window won't close */
static unsigned int duration = 5; /* in seconds */

/* mouse bindings */
static const KeySym dismiss = Button1;
static const KeySym action = Button2;
