#include "ui_elem.h"

const UiStyle ui_style_cpu_meter = { .fg = 32, .flags = UI_BOLD };

int ui_utoa(unsigned v, char *out, int out_sz)
{
	if (out_sz <= 0)
		return 0;
	char t[12];
	int n = 0;
	if (v == 0)
		t[n++] = '0';
	while (v) {
		t[n++] = (char)('0' + v % 10);
		v /= 10;
	}
	int p = 0;
	for (int i = 0; i < n && p < out_sz - 1; i++)
		out[p++] = t[n - 1 - i];
	out[p] = 0;
	return p;
}

void ui_format_pct(unsigned tenths, char *out, int out_sz)
{
	if (out_sz <= 0)
		return;
	if (tenths > UI_HBAR_TENTHS_MAX)
		tenths = UI_HBAR_TENTHS_MAX;
	int p = 0;
	p += ui_utoa(tenths / 10, out + p, out_sz - p);
	if (p < out_sz - 1)
		out[p++] = '.';
	if (p < out_sz - 1)
		p += ui_utoa(tenths % 10, out + p, out_sz - p);
	if (p < out_sz - 1)
		out[p++] = '%';
	if (p < out_sz)
		out[p] = 0;
}

void ui_style_cpu_load(unsigned tenths, UiStyle *out)
{
	if (!out)
		return;
	if (tenths > UI_HBAR_TENTHS_MAX)
		tenths = UI_HBAR_TENTHS_MAX;
	out->bg    = 0;
	out->align = UI_LEFT;
	out->flags = UI_BOLD;
	if (tenths >= UI_CPU_LOAD_RED)
		out->fg = 31;   /* red */
	else if (tenths >= UI_CPU_LOAD_YELLOW)
		out->fg = 33;   /* yellow */
	else
		out->fg = 32;   /* green */
}

void ui_hbar_init(UiElem *e, const char *label, const char *right,
                  int value, int max, const UiStyle *style)
{
	if (!e)
		return;
	e->type  = UI_HBAR;
	e->label = label;
	e->right = right;
	e->value = value;
	e->max   = max;
	e->style = style ? *style : ui_style_cpu_meter;
}

void ui_hbar_init_pct(UiElem *e, char *right_buf, int right_sz,
                      const char *label, unsigned tenths, const UiStyle *style)
{
	ui_format_pct(tenths, right_buf, right_sz);
	ui_hbar_init(e, label, right_buf, (int)tenths, UI_HBAR_TENTHS_MAX, style);
}
