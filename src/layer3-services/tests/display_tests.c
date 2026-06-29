#include "display_tests.h"
#include "../display_client.h"
#include "../display_messages.h"
#include "../displayserver.h"
#include "../nameserver.h"
#include "../clock_client.h"
#include "../clock_messages.h"
#include "../../layer4-ui/ui_elem.h"
#include "../../layer4-ui/ui_app.h"
#include "../../layer1-processes/syscall.h"
#include "../../layer1-processes/rpi.h"
#include "../../layer1-processes/project.h"

#define TEST_PRINT(fmt, ...) uart_printf(CONSOLE, fmt "\r\n", ##__VA_ARGS__)

#define TRAIN_LINES     7
#define TRAIN_SRC_W     80   /* design width of the ASCII art */
#define TRAIN_LINE_MAX  128
#define TRAIN_FRAMES    12

static const UiStyle train_text_style = { .align = UI_CENTER };

/* Neofetch splash train (shell.c cmd_neofetch), plain ASCII. */
static const char *const train_body[TRAIN_LINES - 1] = {
	"                                   \"--.",
	"    ____====\"____--------------_____//______",
	"   //! ||   ==== ==== ==== ==== ====   || !\\\\",
	"  (____||___====_====_====_====_====___||____)",
	".._____________________DB_____________________.",
	"============================================================================",
};

static const char *const train_wheels[4] = {
	"   //( )--( )--( ) +--------+ ( )--( )--( )\\\\",
	"   // )--( )--( ) +--------+ ( )--( )--( \\\\",
	"   //( )--( )--( )+--------+ ( )--( )--( )\\\\",
	"   // ( )--( )--( ) +--------+ ( )--( )--()\\\\",
};

static void train_trim_line(char *line, int max_w)
{
	int len = 0;
	while (line[len])
		len++;
	if (len > max_w)
		line[max_w] = 0;
}

static void train_layout_vertical(int body_h, int *top_pad, int *bottom_pad)
{
	int pad = body_h - TRAIN_LINES;
	if (pad < 0)
		pad = 0;
	*top_pad = pad / 2;
	*bottom_pad = pad - *top_pad;
}

static void train_pad_line(char *out, int out_sz, int pad, const char *src)
{
	int p = 0;
	while (pad-- > 0 && p < out_sz - 1)
		out[p++] = ' ';
	while (*src && p < out_sz - 1)
		out[p++] = *src++;
	out[p] = 0;
}

static void train_build_frame(int frame, char lines[TRAIN_LINES][TRAIN_LINE_MAX],
                              char *status, int status_sz, int inner_w)
{
	int shift = frame % 4;
	int wheel = frame % 4;
	int dots  = (frame % 3) + 1;
	int max_w = inner_w;

	if (max_w > TRAIN_LINE_MAX - 1)
		max_w = TRAIN_LINE_MAX - 1;
	if (max_w < TRAIN_SRC_W + 3)
		shift = shift % 2;

	for (int i = 0; i < 5; i++)
		train_pad_line(lines[i], TRAIN_LINE_MAX, shift, train_body[i]);
	train_pad_line(lines[5], TRAIN_LINE_MAX, shift, train_wheels[wheel]);
	train_pad_line(lines[6], TRAIN_LINE_MAX, 0, train_body[5]);

	for (int i = 0; i < TRAIN_LINES; i++)
		train_trim_line(lines[i], max_w);

	status[0] = 'L'; status[1] = 'o'; status[2] = 'a'; status[3] = 'd';
	status[4] = 'i'; status[5] = 'n'; status[6] = 'g';
	int p = 7;
	for (int i = 0; i < dots && p < status_sz - 1; i++)
		status[p++] = '.';
	status[p] = 0;
}

/* Neofetch-style chugging train; each frame through the display server. */
static void display_loading_train(void)
{
	char line_bufs[TRAIN_LINES][TRAIN_LINE_MAX];
	char status[16];
	UiElem content[TRAIN_LINES + 2];
	int clock = WhoIs(CLOCK_SERVER_NAME);
	int rows, cols, body_h, inner_w, top_pad, bottom_pad;

	display_get_size(&rows, &cols);
	body_h  = UI_APP_BODY_H(rows);
	inner_w = UI_APP_INNER_W(cols);
	train_layout_vertical(body_h, &top_pad, &bottom_pad);

	for (int f = 0; f < TRAIN_FRAMES; f++) {
		int n = 0;
		UiAppShell sh;

		train_build_frame(f, line_bufs, status, (int)sizeof(status), inner_w);

		if (top_pad > 0)
			content[n++] = (UiElem){ .type = UI_SPACER, .h = top_pad };
		for (int i = 0; i < TRAIN_LINES; i++)
			content[n++] = (UiElem){
				.type = UI_TEXT, .text = line_bufs[i], .style = train_text_style,
			};
		if (bottom_pad > 0)
			content[n++] = (UiElem){ .type = UI_SPACER, .h = bottom_pad };

		ui_app_shell_init(&sh, "MyNIX", status, rows, content, n);
		ui_app_shell_render(&sh, f == 0);
		if (clock >= 0)
			Delay(clock, 2);
		else
			Yield();
	}
}

/*
 * Display-server regression — run last before UI (train loading animation).
 * Registry + GET_SIZE + neofetch train via render_tree. No ESC[6n / blocking input.
 */
int run_display_server_tests(void)
{
	int rows, cols;
	int disp_tid;
	int failures = 0;

	uart_printf(CONSOLE, "\r\n");
	uart_printf(CONSOLE, "\033[1;36m[====] %s Display Server Tests:\033[0m\r\n",
		    PROJECT_DISPLAY_NAME);

	disp_tid = WhoIs(DISPLAY_SERVER_NAME);
	if (disp_tid < 0) {
		TEST_PRINT("  \033[1;31m[ FAIL ]\033[0m WhoIs(\"%s\") failed", DISPLAY_SERVER_NAME);
		return -1;
	}
	TEST_PRINT("  \033[1;32m[  OK  ]\033[0m WhoIs DisplayServer TID=%d", disp_tid);

	if (DisplayServerTid() != disp_tid) {
		TEST_PRINT("  \033[1;31m[ FAIL ]\033[0m DisplayServerTid()=%d expected %d",
			   DisplayServerTid(), disp_tid);
		failures++;
	} else {
		TEST_PRINT("  \033[1;32m[  OK  ]\033[0m DisplayServerTid() matches registry");
	}

	display_get_size(&rows, &cols);
	if (rows > 0 && cols > 0)
		TEST_PRINT("  \033[1;32m[  OK  ]\033[0m GET_SIZE: %d cols x %d rows", cols, rows);
	else {
		TEST_PRINT("  \033[1;31m[ FAIL ]\033[0m GET_SIZE: %d x %d", cols, rows);
		failures++;
	}

	display_loading_train();
	TEST_PRINT("  \033[1;32m[  OK  ]\033[0m Loading animation (neofetch train)");

	if (failures > 0) {
		TEST_PRINT("  \033[1;31m[ FAIL ]\033[0m Display server: %d failure(s)", failures);
		uart_printf(CONSOLE, "\r\n");
		return -1;
	}
	TEST_PRINT("  \033[1;32m[  OK  ]\033[0m Display server tests finished");
	uart_printf(CONSOLE, "\r\n");
	return 0;
}
