#include "bilebio.h"
#include "borg.h"

const char *ability_names[] = {
    "Move",
    "Dash",
    "Plant Hop",
    "Repellent",
    "Attack",
    "Wall Hop",
    "Life",
    "Wall Walk",
    "Spawn Wall",
    "Energy",
};

const struct {
    unsigned long initial;
    unsigned long recurring;
} ability_costs[] = {
    {0, 0},
    {10, 2},
    {30, 2},
    {60, 10},
    {10, 2},
    {30, 1},
    {60, 50},
    {10, 5},
    {30, 5},
    {60, 0},
};

int main(void)
{
    enum status st;
    struct bilebio bb;
    int i, x, y;

    initscr();
    curs_set(0);
    noecho();
    start_color();
    keypad(stdscr, 1);

    srand(time(NULL));

    for (i = 0; i < COLORS; ++i)
        init_pair(i, i, COLOR_BLACK);

    init_bilebio(&bb);

#ifdef RUN_BORG
    initialize_borg( &bb );
#endif

    while ((st = update_bilebio(&bb)) == STATUS_ALIVE)
        ;

    if (st == STATUS_DEAD) {
        /* Draw the stage. */
        for (y = 0; y < STAGE_HEIGHT; ++y)
            for (x = 0; x < STAGE_WIDTH; ++x)
                mvaddch(y, x, tile_display(bb.stage[y][x]));
        set_status(0, RED, "You died on stage %d! You finished the game with a score of %d!\n", bb.stage_level, bb.player_score);
        set_status(1, BLUE, "Press 'Q' to quit.", bb.player_score);
        while ((i = getch()) != 'Q');
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
    static const chtype display[NUM_TILES] = {
        '.', ',', '#', '@', '%', '*', '~', '$', '>'
    };
    static const chtype color[NUM_TILES] = {
        WHITE, MAGENTA, YELLOW | A_BOLD, BLUE,
        GREEN, MAGENTA, GREEN, YELLOW, CYAN
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
    int i;
    bb->stage_level = 1;
    bb->player_score = 0;
    bb->player_dead = 0;
    bb->selected_ability = ABILITY_MOVE;
    bb->player_energy = 0;
    bb->abilities[ABILITY_MOVE] = 1;
    for (i = 1; i < NUM_ABILITIES; ++i)
        bb->abilities[i] = 0;
    set_stage(bb);
}

#define NUM_STAGES 12

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
    num_roots = bb->stage_level * 2 + 1;
    while (num_roots-- > 0) {
        tries = 20;
        while (tries-- > 0) {
            x = RANDINT(STAGE_WIDTH);
            y = RANDINT(STAGE_HEIGHT);
            if (bb->stage[y][x].type == TILE_FLOOR) {
                bb->stage[y][x] = TILE_FRESH_ROOT();
                if (ONEIN(100 / bb->stage_level))
                    bb->stage[y][x].active = 1;
                break;
            }
        }
    }

    bb->num_nectars_placed = 0;
    bb->under_player = make_tile(TILE_FLOOR);
    bb->stage_age = 0;
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
    set_status(0, WHITE, "Stage: %d Score: %d Energy: %d",
                         bb->stage_level,
                         bb->player_score,
                         bb->player_energy);

    if (bb->abilities[bb->selected_ability]) {
        set_status(1, WHITE, "%d. %s (%d to use)",
                   bb->selected_ability,
                   ability_names[bb->selected_ability],
                   ability_costs[bb->selected_ability].recurring);
    }
    else {
        set_status(1, RED, "%d. %s (%d to learn)",
                   bb->selected_ability,
                   ability_names[bb->selected_ability],
                   ability_costs[bb->selected_ability].initial);
    }
    rx = 0; /* Sum. Lol, reuse variables. */
    for (r = 1; r < NUM_ABILITIES; ++r)
        rx += bb->abilities[r];
    set_status(2, BLUE, rx < 3 ? "%d abilities to learn" : "Can't learn any more", 3 - rx);

#ifdef RUN_BORG
    ch = borg_move();
    refresh();
#else
    ch = getch();
#endif

    switch (ch) {
    case 'Q': return 0;
    case 'h': case KEY_LEFT: successful_move = use_ability(bb, -1, 0); break;
    case 'j': case KEY_DOWN: successful_move = use_ability(bb, 0, 1); break;
    case 'k': case KEY_UP: successful_move = use_ability(bb, 0, -1); break;
    case 'l': case KEY_RIGHT: successful_move = use_ability(bb, 1, 0); break;
    case 'y': case KEY_A1: successful_move = use_ability(bb, -1, -1); break;
    case 'u': case KEY_A3: successful_move = use_ability(bb, 1, -1); break;
    case 'b': case KEY_C1: successful_move = use_ability(bb, -1, 1); break;
    case 'n': case KEY_C3: successful_move = use_ability(bb, 1, 1); break;
    case '.': case KEY_B2: successful_move = use_ability(bb, 0, 0); break;
    case '0':
        /* Fall through. */
    case '1':
        /* Fall through. */
    case '2':
        /* Fall through. */
    case '3':
        /* Fall through. */
    case '4':
        /* Fall through. */
    case '5':
        /* Fall through. */
    case '6':
        /* Fall through. */
    case '7':
        /* Fall through. */
    case '8':
        /* Fall through. */
    case '9':
        bb->selected_ability = ch - '0';
        break;
    case ' ':
        rx = 0; /* Sum. Lol, reuse variables. */
        for (r = 1; r < NUM_ABILITIES; ++r)
            rx += bb->abilities[r];
        if (rx < 3 &&
            bb->player_energy >= ability_costs[bb->selected_ability].initial &&
            !bb->abilities[bb->selected_ability]) {
            /* Check prerequisites. */
            if ((bb->selected_ability == 2 ||
                bb->selected_ability == 3 ||
                bb->selected_ability == 5 ||
                bb->selected_ability == 6 ||
                bb->selected_ability == 8 ||
                bb->selected_ability == 9) &&
                bb->abilities[bb->selected_ability - 1]) {

                bb->player_energy -= ability_costs[bb->selected_ability].initial;
                bb->abilities[bb->selected_ability] = 1;
            }
            else if ((bb->selected_ability == 1 ||
                     bb->selected_ability == 4 ||
                     bb->selected_ability == 7)) {
                bb->player_energy -= ability_costs[bb->selected_ability].initial;
                bb->abilities[bb->selected_ability] = 1;
            }
        }
        break;
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
                        if (ONEIN(5)) {
                            tries = 10;
                            do {
                                /* Prefer places close to the player. */
                                rx = bb->player_x + RANDINT(10) - 5;
                                ry = bb->player_y + RANDINT(40) - 20;
                            } while (!try_to_place(bb, 0, &tries, rx, ry, TILE_FRESH_ROOT()));
                        }
                        else {
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
                        }
                        tile->active = 0;
                    }
                    else
                        if (ACTIVE_CHANCE(ROOT_ACTIVE_BASE, bb->stage_level))
                            tile->active = 1;
                    break;
                case TILE_FLOWER:
                    if (tile->active) {
                        if (ONEIN(4)) {
                            r = RANDINT(8);
                            rx = x + knight_pattern[r][0];
                            ry = y + knight_pattern[r][1];
                            try_to_place(bb, 1, NULL, rx, ry, TILE_FRESH_VINE());
                        }
                        else {
                            r = RANDINT(8);
                            rx = x + knight_pattern[r][0];
                            ry = y + knight_pattern[r][1];
                            try_to_place(bb, 1, NULL, rx, ry, TILE_FRESH_FLOWER());
                            /* Only placing another flower uses a growth. */
                            tile->growth--;
                        }

                        tile->active = 0;
                    }
                    else
                        /* Cannot activate when stale. */
                        if (ACTIVE_CHANCE(FLOWER_ACTIVE_BASE, bb->stage_level) && tile->growth > 0)
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
                        if (ACTIVE_CHANCE(VINE_ACTIVE_BASE, bb->stage_level) && tile->growth > 0)
                            tile->active = 1;
                    break;
                default: break;
                }
                age_tile(bb, tile);
            }
        }

        /* Update random map stuff... like nectar! */
        if (ONEIN(160) && bb->num_nectars_placed++ < 10) {
            tries = 10;
            while (tries-- > 0) {
                rx = RANDINT(STAGE_WIDTH);
                ry = RANDINT(STAGE_HEIGHT);
                if (IN_STAGE(rx, ry) &&
                    (bb->stage[y][x].type == TILE_FLOOR ||
                    TILE_IS_PLANT(bb->stage[y][x]))) {
                    bb->stage[y][x] = TILE_FRESH_NECTAR();
                }
            }
        }

        bb->stage_age++;
    }

    if (bb->player_dead)
        return STATUS_DEAD;

    return STATUS_ALIVE;
}

void age_tile(struct bilebio *bb, struct tile *t)
{
    (void)bb;
    if (t->type == TILE_ROOT) {
        t->age++;
        if (t->age >= 200)
            t->dead = 1;
        if (t->age >= 201)
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
    else if (t->type == TILE_NECTAR) {
        t->age++;
        if (((t->age + 1) % 40) == 0)
            t->growth = t->growth / 2;
        if (t->growth <= 1)
            *t = TILE_FRESH_ROOT();
    }
    else if (t->type == TILE_REPELLENT) {
        t->age++;
        if (t->age >= TILE_REPELLENT_LIFESPAN)
            *t = make_tile(TILE_FLOOR);
    }
}

int is_obstructed(struct bilebio *bb, int x, int y)
{
    if (!IN_STAGE(x, y))
        return 1;

    if (bb->stage[y][x].type != TILE_FLOOR &&
        bb->stage[y][x].type != TILE_REPELLENT &&
        bb->stage[y][x].type != TILE_EXIT &&
        bb->stage[y][x].type != TILE_NECTAR &&
        bb->stage[y][x].type != TILE_PLAYER &&
        bb->stage[y][x].type != TILE_VINE &&
        bb->stage[y][x].type != TILE_FLOWER)
        return 1;

    return 0;
}

int move_player(struct bilebio *bb, int x, int y)
{
    if (is_obstructed(bb, x, y))
        return 0; /* Unsuccessful move. (Don't update) */

    if (bb->stage[y][x].type == TILE_NECTAR) {
        /* Increase score. */
        bb->player_score += bb->stage[y][x].growth * 8;
        /* Add energy. */
        if (bb->abilities[ABILITY_ENERGY])
            bb->player_energy += bb->stage[y][x].growth * 3;
        else
            bb->player_energy += bb->stage[y][x].growth;
        bb->stage[y][x] = make_tile(TILE_FLOOR);
    }
    else if (bb->stage[y][x].type == TILE_EXIT) {
        bb->player_score += bb->stage_level * 100;
        bb->stage_level++;

        if (bb->stage_age < 160) {
            bb->player_energy += 8;
            bb->player_score += bb->stage_level * 10;
        }
        else if (bb->stage_age < 320) {
            bb->player_energy += 4;
            bb->player_score += bb->stage_level * 5;
        }

        set_stage(bb);
        return 0; /* Unsuccessful move. (Don't update) */
    }
    else if (bb->stage[y][x].type == TILE_VINE ||
             bb->stage[y][x].type == TILE_FLOWER) {
        /* 50% chance of success. */
        if (ONEIN(2))
            bb->stage[y][x] = make_tile(TILE_FLOOR);
        else
            return 1; /* Don't move but still update. */
    }
    else if (bb->stage[y][x].type == TILE_PLAYER) {
        return 1;
    }

    bb->stage[bb->player_y][bb->player_x] = bb->under_player;
    bb->player_x = x;
    bb->player_y = y;
    bb->under_player = bb->stage[bb->player_y][bb->player_x];
    bb->stage[bb->player_y][bb->player_x] = make_tile(TILE_PLAYER);
    return 1;
}

int use_ability(struct bilebio *bb, int dx, int dy)
{
    int x, y;

    switch (bb->selected_ability) {
    /* ABILITY_MOVE covered by default. */

    case ABILITY_DASH:
        if (bb->abilities[ABILITY_DASH] && bb->player_energy >= ability_costs[ABILITY_DASH].recurring) {
            if (!is_obstructed(bb, bb->player_x + (dx * 1), bb->player_y + (dy * 1)))
            if (!is_obstructed(bb, bb->player_x + (dx * 2), bb->player_y + (dy * 2)))
            if (!is_obstructed(bb, bb->player_x + (dx * 3), bb->player_y + (dy * 3)))
            if (!is_obstructed(bb, bb->player_x + (dx * 4), bb->player_y + (dy * 4))) {
                bb->player_energy -= ability_costs[ABILITY_DASH].recurring;
                return move_player(bb, bb->player_x + (dx * 4), bb->player_y + (dy * 4));
            }
        }
        return move_player(bb, bb->player_x + dx, bb->player_y + dy);

    case ABILITY_PLANT_HOP:
        if (bb->abilities[ABILITY_PLANT_HOP] && bb->player_energy >= ability_costs[ABILITY_PLANT_HOP].recurring) {
            if (IN_STAGE(bb->player_x + dx, bb->player_y + dy) &&
                TILE_IS_PLANT(bb->stage[bb->player_y + dy][bb->player_x + dx]) &&
                !is_obstructed(bb, bb->player_x + (dx * 2), bb->player_y + (dy * 2))) {
                bb->player_energy -= ability_costs[ABILITY_PLANT_HOP].recurring;
                return move_player(bb, bb->player_x + (dx * 2), bb->player_y + (dy * 2));
            }
        }
        return move_player(bb, bb->player_x + dx, bb->player_y + dy);

    case ABILITY_REPELLENT:
        if (bb->abilities[ABILITY_REPELLENT] && bb->player_energy >= ability_costs[ABILITY_REPELLENT].recurring) {
            for (y = -2; y <= 2; ++y) {
                for (x = -2; x <= 2; ++x) {
                    if (bb->stage[bb->player_y + y][bb->player_x + x].type == TILE_FLOOR)
                        bb->stage[bb->player_y + y][bb->player_x + x] = make_tile(TILE_REPELLENT);
                }
            }
            return 1;
        }
        return move_player(bb, bb->player_x + dx, bb->player_y + dy);

    case ABILITY_ATTACK:
        if (bb->abilities[ABILITY_ATTACK] && bb->player_energy >= ability_costs[ABILITY_ATTACK].recurring) {
            if (IN_STAGE(bb->player_x + dx, bb->player_y + dy) &&
                TILE_IS_PLANT(bb->stage[bb->player_y + dy][bb->player_x + dx]) &&
                /* Can't attack roots. */
                bb->stage[bb->player_y + dy][bb->player_x + dx].type != TILE_ROOT) {
                bb->player_energy -= ability_costs[ABILITY_ATTACK].recurring;
                bb->stage[bb->player_y + dy][bb->player_x + dx] = make_tile(TILE_FLOOR);
            }
        }
        return move_player(bb, bb->player_x + dx, bb->player_y + dy);

    case ABILITY_WALL_HOP:
        if (bb->abilities[ABILITY_WALL_HOP] && bb->player_energy >= ability_costs[ABILITY_WALL_HOP].recurring) {
            if (IN_STAGE(bb->player_x + dx, bb->player_y + dy) &&
                bb->stage[bb->player_y + dy][bb->player_x + dx].type == TILE_WALL &&
                !is_obstructed(bb, bb->player_x + (dx * 2), bb->player_y + (dy * 2))) {
                bb->player_energy -= ability_costs[ABILITY_WALL_HOP].recurring;
                return move_player(bb, bb->player_x + (dx * 2), bb->player_y + (dy * 2));
            }
        }
        return move_player(bb, bb->player_x + dx, bb->player_y + dy);

    /* ABILITY_LIFE isn't used. */

    case ABILITY_WALL_WALK:
        if (bb->abilities[ABILITY_WALL_WALK] && bb->player_energy >= ability_costs[ABILITY_WALL_WALK].recurring) {
            if (IN_STAGE(bb->player_x + dx, bb->player_y + dy) &&
                bb->stage[bb->player_y + dy][bb->player_x + dx].type == TILE_WALL) {
                bb->player_energy -= ability_costs[ABILITY_WALL_WALK].recurring;

                bb->stage[bb->player_y][bb->player_x] = bb->under_player;
                bb->player_x += dx;
                bb->player_y += dy;
                bb->under_player = bb->stage[bb->player_y][bb->player_x];
                bb->stage[bb->player_y][bb->player_x] = make_tile(TILE_PLAYER);
                return 1;
            }
            /* Can't let the player just stand in a wall forever. */
            else if (IN_STAGE(bb->player_x + dx, bb->player_y + dy) &&
                     bb->stage[bb->player_y + dy][bb->player_x + dx].type == TILE_PLAYER) {
                return 0;
            }
        }
        /* Can't let the player just stand in a wall forever. */
        else if (bb->player_energy < ability_costs[ABILITY_WALL_WALK].recurring &&
                 IN_STAGE(bb->player_x + dx, bb->player_y + dy) &&
                 bb->stage[bb->player_y + dy][bb->player_x + dx].type == TILE_PLAYER) {
            return 0;
        }
        return move_player(bb, bb->player_x + dx, bb->player_y + dy);

    /* ABILITY_ENERGY isn't used. */

    case ABILITY_SPAWN_WALL:
        if (bb->abilities[ABILITY_SPAWN_WALL] && bb->player_energy >= ability_costs[ABILITY_SPAWN_WALL].recurring) {
            if (IN_STAGE(bb->player_x + dx, bb->player_y + dy) &&
                bb->stage[bb->player_y + dy][bb->player_x + dx].type == TILE_FLOOR) {
                bb->player_energy -= ability_costs[ABILITY_SPAWN_WALL].recurring;

                bb->stage[bb->player_y + dy][bb->player_x + dx] = make_tile(TILE_WALL);
                return 1;
            }
        }
        return move_player(bb, bb->player_x + dx, bb->player_y + dy);

    default:
        return move_player(bb, bb->player_x + dx, bb->player_y + dy);
    }
}

int try_to_place(struct bilebio *bb, int deadly, int *tries, int x, int y, struct tile t)
{
    if (IN_STAGE(x, y)) {
        if (deadly && bb->stage[y][x].type == TILE_PLAYER) {
            if (bb->abilities[ABILITY_LIFE] && bb->player_energy >= ability_costs[ABILITY_LIFE].recurring) {
                bb->player_energy -= ability_costs[ABILITY_LIFE].recurring;
                return 1;
            }
            else {
                bb->stage[y][x] = t;
                bb->player_dead = true;
                return 1; /* Break out. */
            }
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

