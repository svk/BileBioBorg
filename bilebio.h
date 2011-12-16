#ifndef H_BILEBIO
#define H_BILEBIO

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

#define TILE_FRESH_ROOT()   make_plant(TILE_ROOT, 1)
#define TILE_FRESH_FLOWER() make_plant(TILE_FLOWER, 2)
#define TILE_FRESH_VINE()   make_plant(TILE_VINE, 2)
#define TILE_FRESH_NECTAR() make_plant(TILE_NECTAR, 16)

#define TILE_IS_PLANT(t)    ((t).type == TILE_VINE ||   \
                             (t).type == TILE_FLOWER || \
                             (t).type == TILE_ROOT)

#define ROOT_ACTIVE_BASE        20
#define FLOWER_ACTIVE_BASE      15
#define VINE_ACTIVE_BASE        10
/* Chance = (l+b-1) / (b^2), where b = base chance and l = stage level. */
#define ACTIVE_CHANCE(base, level)   (ONEIN(((base)*(base))/((level)+(base)-1)))

#define TILE_REPELLENT_LIFESPAN 10

enum {
    TILE_FLOOR,
    TILE_REPELLENT,
    TILE_WALL,
    TILE_PLAYER,
    TILE_ROOT,
    TILE_FLOWER,
    TILE_VINE,
    TILE_NECTAR,
    TILE_EXIT,
    NUM_TILES
};

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

enum {
    ABILITY_MOVE,
    ABILITY_DASH,
    ABILITY_PLANT_HOP,
    ABILITY_REPELLENT,
    ABILITY_ATTACK,
    ABILITY_WALL_HOP,
    ABILITY_LIFE,
    ABILITY_WALL_WALK,
    ABILITY_SPAWN_WALL,
    ABILITY_ENERGY,
    NUM_ABILITIES
};


#define STAGE_HEIGHT    20
#define STAGE_WIDTH     80

#define IN_STAGE(x, y)  ((x) >= 0 && (x) < STAGE_WIDTH && \
                         (y) >= 0 && (y) < STAGE_HEIGHT)

struct bilebio {
    struct tile stage[STAGE_HEIGHT][STAGE_WIDTH];
    unsigned long stage_level;
    unsigned long stage_age;
    unsigned long num_nectars_placed;
    int player_x, player_y;
    unsigned long player_score;
    int player_dead;
    unsigned long player_energy;
    int abilities[NUM_ABILITIES];
    unsigned long selected_ability;
    struct tile under_player;
};

void init_bilebio(struct bilebio *bb);
void set_stage(struct bilebio *bb);
enum status update_bilebio(struct bilebio *bb);
void age_tile(struct bilebio *bb, struct tile *t);
int is_obstructed(struct bilebio *bb, int x, int y);
int move_player(struct bilebio *bb, int x, int y);
int use_ability(struct bilebio *bb, int dx, int dy);
int try_to_place(struct bilebio *bb, int deadly, int *tries, int x, int y, struct tile t);

void set_status(int row, chtype color, const char *fmt, ...);

#endif
