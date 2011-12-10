
#include <assert.h>
#include <curses.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLACK   COLOR_PAIR(COLOR_BLACK)
#define BLUE    COLOR_PAIR(COLOR_BLUE)
#define GREEN   COLOR_PAIR(COLOR_GREEN)
#define CYAN    COLOR_PAIR(COLOR_CYAN)
#define RED     COLOR_PAIR(COLOR_RED)
#define MAGENTA COLOR_PAIR(COLOR_MAGENTA)
#define YELLOW  COLOR_PAIR(COLOR_YELLOW)
#define WHITE   COLOR_PAIR(COLOR_WHITE)

/* [0-1) */
#define RAND()      (rand() * 0.99999999 / RAND_MAX)
#define RANDINT(n)  ((int)floor((RAND() * n)))
#define ONEIN(n)    (RANDINT(n) == 0)

enum status {
    STATUS_QUIT,
    STATUS_ALIVE,
    STATUS_DEAD
};

#define STAGE_HEIGHT    20
#define STAGE_WIDTH     80

#define TILE_FLOOR      0
#define TILE_WALL       1
#define TILE_PLAYER     2
#define TILE_ROOT       3
#define TILE_FLOWER     4
#define TILE_VINE       5
#define TILE_NECTAR     6
#define TILE_EXIT       7
#define TILE_TYPES      8

#define TILE_FRESH_ROOT()   make_plant(TILE_ROOT, 1)
#define TILE_FRESH_FLOWER() make_plant(TILE_FLOWER, 2)
#define TILE_FRESH_VINE()   make_plant(TILE_VINE, 2)

#define TILE_IS_PLANT(t)    ((t).type == TILE_ROOT ||   \
                             (t).type == TILE_FLOWER || \
                             (t).type == TILE_ROOT)

struct tile {
    unsigned long type;
    unsigned long growth;
    unsigned long age;
    int active;
    int dead;
};

struct tile make_plant(unsigned long type, unsigned long growth);
struct tile make_tile(unsigned long type);
chtype tile_display(struct tile t);

#define IN_BOUNDS(x, y)     ((x) >= 0 && (x) < STAGE_WIDTH && \
                             (y) >= 0 && (y) < STAGE_HEIGHT)

struct bilebio {
    struct tile stage[STAGE_HEIGHT][STAGE_WIDTH];
    unsigned long stage_level;
    unsigned long player_x, player_y;
    unsigned long player_score;
    int player_dead;
};

void init_bilebio(struct bilebio *bb);
void set_stage(struct bilebio *bb);
enum status update_bilebio(struct bilebio *bb);
void age_tile(struct bilebio *bb, struct tile *t);
int move_player(struct bilebio *bb, int dx, int dy);
int try_to_place(struct bilebio *bb, int deadly, int *tries, unsigned long x, unsigned long y, struct tile t);

void set_status(int row, chtype color, const char *fmt, ...);

/* Level progression. */
unsigned long get_num_roots(unsigned long stage_level);

int main(void)
{
    enum status st;
    struct bilebio bb;
    int i, x, y;

    initscr();
    curs_set(0);
    noecho();
    start_color();

    for (i = 0; i < COLORS; ++i)
        init_pair(i, i, COLOR_BLACK);

    srand(time(NULL));

    init_bilebio(&bb);

    while ((st = update_bilebio(&bb)) == STATUS_ALIVE)
        ;

    if (st == STATUS_DEAD) {
        /* Draw the stage. */
        for (y = 0; y < STAGE_HEIGHT; ++y)
            for (x = 0; x < STAGE_WIDTH; ++x)
                mvaddch(y, x, tile_display(bb.stage[y][x]));
        set_status(0, RED, "You died! You finished the game with a score of %d!\n", bb.player_score);
        set_status(1, BLUE, "Press 'Q' to quit.", bb.player_score);
        while ((i = getch()) != 'Q')
            ;
    }

    echo();
    endwin();

    return 0;
}

struct tile make_plant(unsigned long type, unsigned long growth)
{
    struct tile t;
    t.type = type;
    t.growth = growth;
    t.age = t.active = t.dead = 0;
    return t;
}

struct tile make_tile(unsigned long type)
{
    struct tile t;
    t.type = type;
    t.growth = t.age = t.active = t.dead = 0;
    return t;
}

chtype tile_display(struct tile t)
{
    static const chtype display[TILE_TYPES] = {
        '.', '#', '@', '%', '*', '~', '$', '>'
    };
    static const chtype color[TILE_TYPES] = {
        WHITE, YELLOW | A_BOLD, BLUE, GREEN,
        MAGENTA, GREEN, YELLOW, CYAN
    };

    if (t.active) {
        return display[t.type] | RED;
    }
    else if (t.dead) {
        return display[t.type] | YELLOW;
    }
    return display[t.type] | color[t.type];
}

void init_bilebio(struct bilebio *bb)
{
    bb->stage_level = 1;
    bb->player_score = 0;
    bb->player_dead = 0;
    set_stage(bb);
}

#define NUM_STAGES 8

const struct tile stages[NUM_STAGES][STAGE_HEIGHT][STAGE_WIDTH] = {
#include "stages.inc"
};

void set_stage(struct bilebio *bb)
{
    int x, y;
    int num_roots;
    int tries;

    /* Clear the stage. */
    memset(bb->stage, 0, sizeof(bb->stage));
    /* Select a stage. */
    memcpy(bb->stage, stages[RANDINT(NUM_STAGES)], sizeof(bb->stage));

    /* Find the player. */
    for (y = 0; y < STAGE_HEIGHT; ++y) {
        for (x = 0; x < STAGE_WIDTH; ++x) {
            if (bb->stage[y][x].type == TILE_PLAYER) {
                bb->player_x = x;
                bb->player_y = y;
            }
        }
    }

    /* Populate the stage. */
    num_roots = (int)floor(pow(2, bb->stage_level) / 10.0f) + 1;
    tries = 10;
    while (num_roots-- > 0) {
        while (tries-- > 0) {
            x = RANDINT(STAGE_WIDTH);
            y = RANDINT(STAGE_HEIGHT);
            if (bb->stage[y][x].type == TILE_FLOOR) {
                bb->stage[y][x] = TILE_FRESH_ROOT();
                break;
            }
        }
    }
}

enum status update_bilebio(struct bilebio *bb)
{
    int ch;
    int x, y, rx, ry, r;
    struct tile *tile;
    int tries;
    int successful_move = 0;
    struct tile temp_stage[STAGE_HEIGHT][STAGE_WIDTH];
    int knight_pattern[8][2] = {
        {-2, -1},
        { 2, -1},
        {-2,  1},
        { 2,  1},

        {-1, -2},
        {-1,  2},
        { 1, -2},
        { 1,  2},
    };

    /* Draw the stage. */
    for (y = 0; y < STAGE_HEIGHT; ++y)
        for (x = 0; x < STAGE_WIDTH; ++x)
            mvaddch(y, x, tile_display(bb->stage[y][x]));

    /* Draw the statuses. */
    set_status(0, WHITE, "Stage: %d Score: %d", bb->stage_level, bb->player_score);

    ch = getch();

    switch (ch) {
    case 'Q': return 0;
    case 'h': successful_move = move_player(bb, -1, 0); break;
    case 'j': successful_move = move_player(bb, 0, 1); break;
    case 'k': successful_move = move_player(bb, 0, -1); break;
    case 'l': successful_move = move_player(bb, 1, 0); break;
    case 'y': successful_move = move_player(bb, -1, -1); break;
    case 'u': successful_move = move_player(bb, 1, -1); break;
    case 'b': successful_move = move_player(bb, -1, 1); break;
    case 'n': successful_move = move_player(bb, 1, 1); break;
    case '.': successful_move = 1; break;
    case 'r': set_stage(bb); return 1; break;
    default: break;
    }

    if (successful_move) {
        /* Update the plants. */
        memcpy(temp_stage, bb->stage, sizeof(bb->stage));
        for (y = 0; y < STAGE_HEIGHT; ++y) {
            for (x = 0; x < STAGE_WIDTH; ++x) {
                tile = &bb->stage[y][x];
                /* We check from temp_stage, rather than bb->stage because
                 * bb->stage will change, and we don't want the new guys
                 * growing. */
                switch (temp_stage[y][x].type) {
                case TILE_ROOT:
                    if (tile->active) {
                        if (tile->growth > 0) {

                            try_to_place(bb, 1, NULL, x - 2, y, TILE_FRESH_VINE());
                            try_to_place(bb, 1, NULL, x - 1, y, TILE_FRESH_FLOWER());
                            try_to_place(bb, 1, NULL, x + 1, y, TILE_FRESH_FLOWER());
                            try_to_place(bb, 1, NULL, x + 2, y, TILE_FRESH_VINE());


                            try_to_place(bb, 1, NULL, x, y - 2, TILE_FRESH_VINE());
                            try_to_place(bb, 1, NULL, x, y - 1, TILE_FRESH_FLOWER());
                            try_to_place(bb, 1, NULL, x, y + 1, TILE_FRESH_FLOWER());
                            try_to_place(bb, 1, NULL, x, y + 2, TILE_FRESH_VINE());


                            try_to_place(bb, 1, NULL, x + 1, y + 1, TILE_FRESH_VINE());
                            try_to_place(bb, 1, NULL, x - 1, y - 1, TILE_FRESH_VINE());
                            try_to_place(bb, 1, NULL, x - 1, y + 1, TILE_FRESH_VINE());
                            try_to_place(bb, 1, NULL, x + 1, y - 1, TILE_FRESH_VINE());

                            tile->growth--;
                        }
                        else {
                            tries = 10;
                            do {
                                rx = RANDINT(STAGE_WIDTH);
                                ry = RANDINT(STAGE_HEIGHT);
                            } while (!try_to_place(bb, 0, &tries, rx, ry, TILE_FRESH_ROOT()));
                        }
                        tile->active = 0;
                    }
                    else
                        if (tile->growth && ONEIN(10))
                            tile->active = 1;
                        if (tile->age == 79)
                            tile->active = 1;
                    break;
                case TILE_FLOWER:
                    if (tile->active) {
                        /* Flowers do nothing when stale. */
                        r = RANDINT(8);
                        rx = x + knight_pattern[r][0];
                        ry = y + knight_pattern[r][1];
                        try_to_place(bb, 1, NULL, rx, ry, TILE_FRESH_FLOWER());

                        tile->growth--;
                        tile->active = 0;
                    }
                    else
                        /* Cannot activate when stale. */
                        if (ONEIN(15) && tile->growth > 0)
                            tile->active = 1;
                    break;
                case TILE_VINE:
                    if (tile->active) {
                        rx = x + RANDINT(3) - 1;
                        ry = y + RANDINT(3) - 1;
                        try_to_place(bb, 1, NULL, rx, ry, TILE_FRESH_VINE());
                        tile->growth--;
                        tile->active = 0;
                    }
                    else
                        /* Cannot activate when stale. */
                        if (ONEIN(10) && tile->growth > 0)
                            tile->active = 1;
                    break;
                default: break;
                }
                age_tile(bb, tile);
            }
        }

        /* Update random map stuff... like nectar! */
        if (ONEIN(80)) {
            tries = 10;
            do {
                rx = RANDINT(STAGE_WIDTH);
                ry = RANDINT(STAGE_HEIGHT);
            } while (!try_to_place(bb, 0, &tries, rx, ry, make_tile(TILE_NECTAR)));
        }
    }

    if (bb->player_dead)
        return STATUS_DEAD;

    return STATUS_ALIVE;
}

void age_tile(struct bilebio *bb, struct tile *t)
{
    if (t->type == TILE_ROOT) {
        t->age++;
        if (t->age >= 80)
            t->dead = 1;
        if (t->age >= 81)
            *t = make_tile(TILE_FLOOR);
    }
    else if (t->type == TILE_FLOWER) {
        t->age++;
        if (t->age >= 40)
            t->dead = 1;
        if (t->age >= 41)
            *t = make_tile(TILE_FLOOR);
    }
    else if (t->type == TILE_VINE) {
        t->age++;
        if (t->age >= 40)
            t->dead = 1;
        if (t->age >= 41)
            *t = make_tile(TILE_FLOOR);
    }
}

int move_player(struct bilebio *bb, int dx, int dy)
{
    if (!IN_BOUNDS(bb->player_x + dx, bb->player_y + dy))
        return 0;

    if (bb->stage[bb->player_y + dy][bb->player_x + dx].type != TILE_FLOOR &&
        bb->stage[bb->player_y + dy][bb->player_x + dx].type != TILE_EXIT &&
        bb->stage[bb->player_y + dy][bb->player_x + dx].type != TILE_NECTAR)
        /* Keep this in? */
        /*!bb->stage[bb->player_y + dy][bb->player_x + dx].dead)*/
        return 0;

    if (bb->stage[bb->player_y + dy][bb->player_x + dx].type == TILE_NECTAR)
        bb->player_score *= 1.1;
    if (bb->stage[bb->player_y + dy][bb->player_x + dx].type == TILE_EXIT) {
        bb->player_score += bb->stage_level * 100;
        bb->stage_level++;
        set_stage(bb);
        return 0; /* Skip the update. */
    }

    bb->stage[bb->player_y][bb->player_x] = make_tile(TILE_FLOOR);
    bb->player_x += dx;
    bb->player_y += dy;
    bb->stage[bb->player_y][bb->player_x] = make_tile(TILE_PLAYER);
    return 1;
}

int try_to_place(struct bilebio *bb, int deadly, int *tries, unsigned long x, unsigned long y, struct tile t)
{
    if (IN_BOUNDS(x, y)) {
        if (deadly && bb->stage[y][x].type == TILE_PLAYER) {
            bb->stage[y][x] = t;
            bb->player_dead = true;
            return 1; /* Break out. */
        }
        else if (bb->stage[y][x].type == TILE_FLOOR) {
            bb->stage[y][x] = t;
        }
    }

    if (tries && (*tries)-- > 0)
        /* Failed, but break out. */
        return 1;
    /* Always break out when NULL is passed. */
    else if (!tries)
        return 1;

    /* Still tries to be had! */
    return 0;
}

void set_status(int row, chtype color, const char *fmt, ...)
{
    va_list args;
    char buf[80];

    row += STAGE_HEIGHT;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    move(row, 0);
    clrtoeol();

    attron(color);
    mvprintw(row, 0, "%s", buf);
    attroff(color);
}
