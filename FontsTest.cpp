#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <freetype.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <future>
using namespace std;

typedef struct _ft_fontinfo {
    FT_Face    face;
    FT_Library library;
    int32_t     mono;
} ft_fontinfo;

typedef enum _glyph_format_t {
    GLYPH_FMT_ALPHA,
    GLYPH_FMT_MONO,
} glyph_format_t;

typedef struct _glyph_t {
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    uint16_t advance;
    uint8_t  format;
    uint8_t  pitch;
    uint8_t  *data;
    void     *handle;
} glyph_t;

HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
ft_fontinfo   font_info;
long int size = 0;
unsigned char *font_buf = NULL;
mutex m;
const int thp = 16;
int f_size, x, y; string loadfonts; wstring cs;

uint8_t bitmap_mono_get_pixel(const uint8_t* buff, uint32_t w, uint32_t h, uint32_t x, uint32_t y) {
    uint32_t line_length = ((w + 15) >> 4) << 1;
    uint32_t offset = y * line_length + (x >> 3);
    uint32_t offset_bit = 7 - (x % 8);
    const uint8_t* data = buff + offset;
    if (buff == NULL || (x > w && y > h)) return 0;
    return (*data >> offset_bit) & 0x1;
}

static int font_ft_get_glyph(ft_fontinfo *font_info, wchar_t c, float font_size, glyph_t* g) {
    FT_Glyph glyph;
    FT_GlyphSlot glyf;
    FT_Int32 flags = FT_LOAD_DEFAULT | FT_LOAD_RENDER ;
    if (font_info->mono) flags |= FT_LOAD_TARGET_MONO;
    FT_Set_Char_Size(font_info->face, 0, font_size * 64, 0, 96);
    if (!FT_Load_Char(font_info->face, c, flags)) {
        glyf = font_info->face->glyph;
        FT_Get_Glyph(glyf, &glyph);
        g->format = GLYPH_FMT_ALPHA;
        g->h = glyf->bitmap.rows;
        g->w = glyf->bitmap.width;
        g->pitch = glyf->bitmap.pitch;
        g->x = glyf->bitmap_left;
        g->y = -glyf->bitmap_top;
        g->data = glyf->bitmap.buffer;
        g->advance = glyf->metrics.horiAdvance / 64;
        if (g->data != NULL) {
            if (glyf->bitmap.pixel_mode == FT_PIXEL_MODE_MONO) g->format = GLYPH_FMT_MONO;
            g->handle = glyph;
        }
        else FT_Done_Glyph(glyph);
    }
    return g->data != NULL || c == ' ' ? 1 : 0;
}

inline void fontsload(string name) {
    FILE *font_file = fopen(name.c_str(), "rb");
    if (font_file == NULL) { printf("Can not open font file!\n"); getchar();return; }
    fseek(font_file, 0, SEEK_END);
    size = ftell(font_file);
    fseek(font_file, 0, SEEK_SET);
    font_buf = (unsigned char*)calloc(size, sizeof(unsigned char));
    fread(font_buf, size, 1, font_file); fclose(font_file);
    font_info.mono = 1;
    FT_Init_FreeType(&(font_info.library));
    FT_New_Memory_Face(font_info.library, font_buf, size, 0, &(font_info.face));
    FT_Select_Charmap(font_info.face, FT_ENCODING_UNICODE);
}

inline void putspace(short i, short j, HANDLE handle, short x, short y) {
	m.lock();
	SetConsoleCursorPosition(handle, (COORD){(short)(i*2+x), (short)(j+y)});
	printf("  ");
	m.unlock();
}

inline void Showc(float size, wchar_t c, int x, int y) {
	glyph_t g;
    float font_size = size;
    font_ft_get_glyph(&font_info, c, font_size, &g);
    SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY|0);
    if (g.format == GLYPH_FMT_MONO) {
    	thread t[thp];
        for (int j = 0; j < g.h; j+=thp) {
        	if (j+y > 570) break;
        	for (int k = 0; k < thp; k++) if (j+k<g.h) t[k] = thread([](int j, glyph_t g, HANDLE handle, int x, int y) {
        		for (int i = 0; i < g.w; ++i) {
	                uint8_t pixel = bitmap_mono_get_pixel(g.data, g.w, g.h, i, j);
	                if (pixel) putspace(i, j, handle, x, y);
	                else continue;
	            }
			}, j+k, g, handle, x, y);
			for (int k = 0; k < thp; k++) if (j+k<g.h) t[k].join();
        }
    }
    else if (g.format == GLYPH_FMT_ALPHA) {
        for (int j = 0; j < g.h; ++j) {
            for (int i = 0; i < g.w; ++i)
                putchar(" .:ioVM@"[g.data[j*g.w + i] >> 5]);
            putchar('\n');
        }
    }
}

inline void SetSize(int size) {
	CONSOLE_FONT_INFOEX cfi; 
	cfi.cbSize = sizeof cfi, cfi.nFont = 0, cfi.dwFontSize.X = 0, cfi.dwFontSize.Y = size, cfi.FontFamily = FF_DONTCARE, cfi.FontWeight = FW_NORMAL; 
	wcscpy(cfi.FaceName, L"Consolas");
	SetCurrentConsoleFontEx(handle, FALSE, &cfi); 
	CONSOLE_FONT_INFO consoleCurrentFont;
	GetCurrentConsoleFont(handle, FALSE, &consoleCurrentFont); 
}

inline void ToWindow() {
	SetConsoleTitle("Fonts Test");
	SetConsoleOutputCP(65001);
	SMALL_RECT wrt = {0, 0, 37, 3}; SetSize(1);
	SetConsoleWindowInfo(handle, true, &wrt);
	system("mode con cols=2000 lines=570");
	system("color F0");
	DWORD mode;
	GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode);
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), mode);
	SetWindowLongPtrA(GetConsoleWindow(), GWL_STYLE, GetWindowLongPtrA(GetConsoleWindow(), GWL_STYLE)&~WS_SIZEBOX&~WS_MAXIMIZEBOX&~WS_MINIMIZEBOX);
	CONSOLE_CURSOR_INFO cursor;
	GetConsoleCursorInfo(handle, &cursor); cursor.bVisible = false;
	SetConsoleCursorInfo(handle, &cursor);
}

inline void endfonts() {
	FT_Done_FreeType(font_info.library);
    free(font_buf);
}

namespace WinCon {
	glyph_t g;
	inline void PrintString(int &x, int &y, wstring &s, int wid = 20, float F_SIZE = 70) {
		int len = s.size(), yy, wi;
		font_ft_get_glyph(&font_info, L'W', F_SIZE, &g);
		yy = g.h, wi = g.w;
		font_ft_get_glyph(&font_info, L'a', F_SIZE, &g);
		int ly = g.h;
		font_ft_get_glyph(&font_info, L'i', F_SIZE, &g);
		int lyy = g.h;
		for (int i = 0; i < len; i++) {
			if (s[i] == L'`') {
				x -= g.w*4+wid*2; if (x+wi*2 > 2000) x = 20, y += (double)yy*1.5;
			} else if (s[i] == L'~') {
				x = 20, y += (double)yy*1.5;
			} else if (s[i] == L'\''||s[i] == L'\"'||s[i] == L'^') {
				Showc(F_SIZE, s[i], x, y - yy);
		    	x += g.w*2+wid; if (x+wi*2 > 2000) x = 20, y += (double)yy*1.5;
			} else if (s[i] == L' ') {
				x += F_SIZE*0.8+wid; if (x+wi*2 > 2000) x = 20, y += (double)yy*1.5;
			} else if (s[i] == L'j') {
				font_ft_get_glyph(&font_info, s[i], F_SIZE, &g); Showc(F_SIZE, s[i], x, y - lyy);
		    	x += g.w*2+wid; if (x+wi*2 > 2000) x = 20, y += (double)yy*1.5;
			} else if (s[i] == L'g'||s[i] == L'y'||s[i] == 'p' || s[i] == 'q') {
				font_ft_get_glyph(&font_info, s[i], F_SIZE, &g); Showc(F_SIZE, s[i], x, y - ly);
		    	x += g.w*2+wid; if (x+wi*2 > 2000) x = 20, y += (double)yy*1.5;
			} else {
				font_ft_get_glyph(&font_info, s[i], F_SIZE, &g); Showc(F_SIZE, s[i], x, y - g.h);
			    x += g.w*2+wid; if (x+wi*2 > 2000) x = 20, y += (double)yy*1.5;
			}
			if (y > 570) system("color F0"), y = 20+f_size;
		}
		this_thread::sleep_for(chrono::milliseconds(10));
	}
}

int main() {
	ToWindow();
	freopen("FontsTest.inf", "r", stdin);
	SetConsoleTextAttribute(handle, BACKGROUND_INTENSITY|15*16|FOREGROUND_INTENSITY|15), cin >> loadfonts >> f_size;
    fontsload(loadfonts);
    x = 20, y = 20+f_size;
    while(true) {
    	SetConsoleTextAttribute(handle, BACKGROUND_INTENSITY|15*16|FOREGROUND_INTENSITY|15), getline(wcin, cs);
    	WinCon::PrintString(x, y, cs, 5, f_size);
	}
    endfonts();
    return 0;
}
