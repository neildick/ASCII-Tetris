#include "tetris.h"
#include "randoms.h"
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>

#define ASSERT(x)                                                              \
    do {                                                                       \
        if (!(x)) {                                                            \
            printf("assert failed! %s %d\n", __FUNCTION__, __LINE__);          \
            while (1)                                                          \
                ;                                                              \
        }                                                                      \
    } while (0)

/* Some cursor functions */
// static inline void move_cursor_up(int n) { printf("\033[%dA", n); }
// static inline void move_cursor_down(int n) { printf("\033[%dB", n); }
// static inline void move_cursor_left(int n) { printf("\033[%dC", n); }
// static inline void move_cursor_right(int n) { printf("\033[%dD", n); }

// move the cursor to line L column C
static inline void
move_cursor(int L, int C)
{
    printf("\033[%d;%dH", L, C);
}

static inline void
clear_screen(void)
{
    puts("\033[2J");
}

#define COLOR_STR "\033[%dm"
#define END_FMT "\033[0m"

struct tetris_level
{
    int score;
    int nsec;
};

struct tetris_block blocks[] = {
    { { "##", "##" }, 2, 2 },       { { " X ", "XXX" }, 3, 2 },
    { { "@@@@" }, 4, 1 },           { { "OO", "O ", "O " }, 2, 3 },
    { { "&&", " &", " &" }, 2, 3 }, { { "ZZ ", " ZZ" }, 3, 2 }
};

struct tetris_level levels[] = { { 0, 1200000 },    { 1500, 900000 },
                                 { 8000, 700000 },  { 20000, 500000 },
                                 { 40000, 400000 }, { 75000, 300000 },
                                 { 100000, 200000 } };

#define TETRIS_PIECES (sizeof(blocks) / sizeof(struct tetris_block))
#define TETRIS_LEVELS (sizeof(levels) / sizeof(struct tetris_level))

struct termios save;

void
tetris_cleanup_io()
{
    tcsetattr(fileno(stdin), TCSANOW, &save);
}

void
tetris_signal_quit(int s)
{
    tetris_cleanup_io();
}

void
tetris_set_ioconfig()
{
    struct termios custom;
    int fd = fileno(stdin);
    tcgetattr(fd, &save);
    custom = save;
    custom.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(fd, TCSANOW, &custom);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

void
tetris_init(struct tetris* t, int w, int h)
{
    int x, y;
    t->level = 1;
    t->score = 0;
    t->gameover = 0;
    t->w = w;
    t->h = h;
    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++)
            t->game[x][y] = (tile){ ' ', NONE };
    }
}

void
tetris_print(struct tetris* t)
{
    int x, y;
    move_cursor(0, 0);

    printf("[LEVEL: %d | SCORE: %d]\n", t->level, t->score);

    for (y = 0; y < t->h; y++) {
        printf("!");
        for (x = 0; x < t->w; x++) {
            if (x >= t->x && y >= t->y && x < (t->x + t->current.w) &&
                y < (t->y + t->current.h) &&
                t->current.data[y - t->y][x - t->x] != ' ')
                printf(COLOR_STR "[]" END_FMT, t->current.color);
            else {
                if (t->game[x][y].val != ' ') {
                    ASSERT(t->game[x][y].color != NONE);
                    printf(COLOR_STR "[]" END_FMT, t->game[x][y].color);
                } else {
                    printf("  ");
                }
            }
        }
        printf("!\n");
    }
    for (x = 0; x < 2 * t->w + 2; x++)
        printf("~");
    printf("\n");
}

int
tetris_hittest(struct tetris* t)
{
    int x, y, X, Y;
    struct tetris_block b = t->current;
    for (x = 0; x < b.w; x++)
        for (y = 0; y < b.h; y++) {
            X = t->x + x;
            Y = t->y + y;
            if (X < 0 || X >= t->w)
                return 1;
            if (b.data[y][x] != ' ') {
                if ((Y >= t->h) || (X >= 0 && X < t->w && Y >= 0 &&
                                    t->game[X][Y].val != ' ')) {
                    return 1;
                }
            }
        }
    return 0;
}

void
tetris_new_block(struct tetris* t)
{
    t->current = blocks[RANDOM()];
    t->x = (t->w / 2) - (t->current.w / 2);
    t->y = 0;
    t->current.color = RED + RANDOM();
    ASSERT(t->current.color < NONE);
    if (tetris_hittest(t)) {
        t->gameover = 1;
    }
}

void
tetris_print_block(struct tetris* t)
{
    int x, y;
    struct tetris_block b = t->current;
    for (x = 0; x < b.w; x++)
        for (y = 0; y < b.h; y++) {
            if (b.data[y][x] != ' ')
                t->game[t->x + x][t->y + y] = (tile){ b.data[y][x], b.color };
        }
}

void
tetris_rotate(struct tetris* t)
{
    struct tetris_block b = t->current;
    struct tetris_block s = b;
    int x, y;
    b.w = s.h;
    b.h = s.w;
    for (x = 0; x < s.w; x++)
        for (y = 0; y < s.h; y++) {
            b.data[x][y] = s.data[s.h - y - 1][x];
        }
    x = t->x;
    y = t->y;
    t->x -= (b.w - s.w) / 2;
    t->y -= (b.h - s.h) / 2;
    t->current = b;
    if (tetris_hittest(t)) {
        t->current = s;
        t->x = x;
        t->y = y;
    }
}

void
tetris_gravity(struct tetris* t)
{
    t->y++;
    if (tetris_hittest(t)) {
        t->y--;
        tetris_print_block(t);
        tetris_new_block(t);
    }
}

void
tetris_fall(struct tetris* t, int l)
{
    int x, y;
    for (y = l; y > 0; y--) {
        for (x = 0; x < t->w; x++)
            t->game[x][y] = t->game[x][y - 1];
    }
    for (x = 0; x < t->w; x++)
        t->game[x][0].val = ' ';
}

void
tetris_check_lines(struct tetris* t)
{
    int x, y, l;
    int p = 100;
    for (y = t->h - 1; y >= 0; y--) {
        l = 1;
        for (x = 0; x < t->w && l; x++) {
            if (t->game[x][y].val == ' ') {
                l = 0;
            }
        }
        if (l) {
            t->score += p;
            p *= 2;
            tetris_fall(t, y);
            y++;
        }
    }
}

int
tetris_level(struct tetris* t)
{
    int i;
    for (i = 0; i < TETRIS_LEVELS; i++) {
        if (t->score >= levels[i].score) {
            t->level = i + 1;
        } else
            break;
    }
    return levels[t->level - 1].nsec;
}

void
tetris_run(int w, int h)
{
    struct tetris t;
    char cmd;
    int count = 0;
    bool moved = false;
    tetris_init(&t, w, h);

    tetris_new_block(&t);
    clear_screen();

    while (!t.gameover) {
        __WFI();

        if (count % 50 == 0) {
            if (moved) {
                tetris_print(&t);
                moved = false;
            }
        }
        if (count % 350 == 0) {
            tetris_gravity(&t);
            tetris_check_lines(&t);
        }
        if (SEGGER_RTT_HasData()) {
            const size_t BUF_SIZE = 512;
            char buf[BUF_SIZE];
            size_t n = SEGGER_RTT_Read(0, &buf, BUF_SIZE);
            for (int i = 0; i < n; i++) {

                switch (buf[i]) {
                    case 'a':
                        moved = true;
                        t.x--;
                        if (tetris_hittest(&t))
                            t.x++;
                        break;
                    case 'd':
                        moved = true;
                        t.x++;
                        if (tetris_hittest(&t))
                            t.x--;
                        break;
                    case 's':
                        moved = true;
                        tetris_gravity(&t);
                        break;
                    case 'w':
                        moved = true;
                        tetris_rotate(&t);
                        break;
                }
            }
        }
    }

    tetris_print(&t);
    printf("*** GAME OVER ***\n");
}
