#include "borg.h"

static struct bilebio * world = 0;

FILE *borg_log = 0;

#define MAX_ONE_IN 100

double logp_one_in[MAX_ONE_IN];
double logp_complement_of_one_in[MAX_ONE_IN];

int borg_move_primitive( struct bilebio *);

double desirability_map[STAGE_HEIGHT][STAGE_WIDTH];

#define JOIN_XY( x, y ) (((y)<<16) | (x))
#define GET_X( xy ) ((xy) & 0xffff)
#define GET_Y( xy ) ( ((xy) & 0xffff0000) >> 16 )

void calculate_distances_to( struct bilebio * ctx, int x, int y, int map[STAGE_HEIGHT][STAGE_WIDTH]) {
    static int q[STAGE_WIDTH*STAGE_HEIGHT];
    int qs = 0;

    for(int i=0;i<STAGE_WIDTH;i++) for(int j=0;j<STAGE_HEIGHT;j++) {
        map[j][i] = -1;
    }

    map[y][x] = 0;

    q[qs++] = JOIN_XY( x, y );

    while( qs > 0 ) {
        x = GET_X( q[0] );
        y = GET_Y( q[0] );
        memmove( &q[0], &q[1], (qs-1) * sizeof q[0] );
        qs--;

        for(int i=-1;i<=1;i++) for(int j=-1;j<=1;j++) if( i || j ) {
            int nx = x + i, ny = y + j;
            if( nx < 0 || ny < 0 || nx >= STAGE_WIDTH || ny >= STAGE_HEIGHT ) continue;
            int type = ctx->stage[ny][nx].type;
            if (type != TILE_FLOOR &&
                type != TILE_REPELLENT &&
                type != TILE_EXIT &&
                type != TILE_NECTAR &&
                type != TILE_PLAYER &&
                type != TILE_VINE &&
                type != TILE_EXIT &&
                type != TILE_FLOWER) continue;
            int d = map[y][x] + 1;
            if( (map[ny][nx] < 0) || (map[ny][nx] > d) ) {
                map[ny][nx] = d;
                q[qs++] = JOIN_XY( nx, ny );
            }
        }
    }
}

void add_desirability_from( struct bilebio * ctx, int x, int y, double base ) {
    int d[STAGE_HEIGHT][STAGE_WIDTH];
    calculate_distances_to( ctx, x, y, d );

    for(int nx=0;nx<STAGE_WIDTH;nx++) for(int ny=0;ny<STAGE_HEIGHT;ny++) {
        if( d[ny][nx] < 0 ) continue;
        double v = base / (double)(1 + d[ny][nx]);
        if( v > desirability_map[ny][nx] ) {
            desirability_map[ny][nx] = v;
        }
    }
}

void calculate_desirability( struct bilebio * ctx ) {
    for(int x=0;x<STAGE_WIDTH;x++) for(int y=0;y<STAGE_HEIGHT;y++) {
        desirability_map[y][x] = 0;
    }
    for(int x=0;x<STAGE_WIDTH;x++) for(int y=0;y<STAGE_HEIGHT;y++) {
        switch( ctx->stage[y][x].type ) {
            case TILE_EXIT:
                add_desirability_from( ctx, x, y, 100.0 );
                break;
        }
    }

    for(int y=0;y<STAGE_HEIGHT;y++) {
        for(int x=0;x<STAGE_WIDTH;x++) {
            int ch = ( ((int)tile_display( ctx->stage[y][x] )) & A_CHARTEXT);
            if( ch == '.' ) {
                double thr = 60.0;
                int cch = '9';
                while( cch != '0' ) {
                    if( desirability_map[y][x] >= thr ) break;
                    thr *= 0.6;
                    cch--;
                }
                ch = cch;
            }
            fprintf( borg_log, "%c",  ch );
        }
        fprintf( borg_log, "\n" );
    }
}

void initialize_borg( struct bilebio * real_world ) {
    world = real_world;

    logp_one_in[0] = 0; // N/A
    for(int i=1;i<MAX_ONE_IN;i++) {
        logp_one_in[i] = log( 1.0 / ((double)i) );
        logp_complement_of_one_in[i] = log( ((double)(i-1)) / ((double)i) );
    }

    borg_log = fopen( "bbborg.log", "a" );
}

void borg_print(const char*s) {
    fprintf( borg_log, "[borg_print] %s\n", s );
}

void quit_borg() {
    fclose( borg_log );
}

void print_cell( struct tile *t ) {
    fprintf( borg_log, "%c", ( ((int)tile_display( *t )) & A_CHARTEXT) );
    if( t->active ) {
        fprintf( borg_log, "!" );
    }
}

double log_survival_from( struct tile *t, int dx, int dy) {
    if( !t->active ) {
        return 0;
    }
    switch( t->type ) {
        case TILE_VINE:
            if( dx*dx <= 1 && dy*dy <= 1 ) {
                return logp_complement_of_one_in[9];
            }
            return 0;
        case TILE_FLOWER:
            if( (dx*dx == 4 && dy*dy == 1) || (dx*dx == 1 && dy*dy == 4) ) {
                return logp_complement_of_one_in[8];
            }
            return 0;
        case TILE_ROOT:
            if( (dx*dx+dy*dy) <= 2 && (dx || dy) ) {
                return logp_one_in[5];
            }
            return 0;
    }
    return 0;
}

double log_survival_at( struct tile (*map)[STAGE_WIDTH], int x, int y ) {
    double log_survival = 0;
    for(int i=-2;i<=2;i++) for(int j=-2;j<=2;j++) if( i || j ) {
        if( (x+i) < 0 || (x+i) >= STAGE_WIDTH ) continue;
        if( (y+j) < 0 || (y+j) >= STAGE_HEIGHT ) continue;
        log_survival += log_survival_from( &map[y+j][x+i], i, j );
    }
    return log_survival;
}

int mc_survival_game( struct bilebio * holodeck ) {
    for(int i=0;i<10;i++) {
        if( simulate_bilebio( holodeck, borg_move_primitive(holodeck) ) == STATUS_DEAD ) return 0;
    }
    return 1;
}

double mc_survival_rate( struct bilebio * ctx, int initial_move ) {
    struct bilebio holodeck;
    int wins = 0, total = 10;
    for(int i=0;i<total;i++) {
        memcpy( &holodeck, ctx, sizeof holodeck );
        simulate_bilebio( &holodeck, initial_move );
        wins += mc_survival_game( &holodeck );
    }
    return wins / (double) total;
}

void borg_move_candidates( struct bilebio *ctx, int *candidates, int *no_candidates ) {
    int keys[3][3] = {
        { 'y', 'k', 'u' },
        { 'h', '.', 'l' },
        { 'b', 'j', 'n' },
    };
    double best_log_survival = -1000;
    *no_candidates = 0;
    for(int i=-1;i<=1;i++) for(int j=-1;j<=1;j++) {
        const int x = ctx->player_x + i, y = ctx->player_y + j; 
        const int key = keys[j+1][i+1];
        struct tile * t = &ctx->stage[y][x];
        if( is_obstructed( ctx, x, y ) && t->type != TILE_EXIT ) continue;

/*      // the "dodger" style avoided stepping on plants
        if( t->type == TILE_VINE || t->type == TILE_FLOWER ) continue;
*/

        double log_survival = log_survival_at( ctx->stage, x, y );
        if( log_survival > best_log_survival ) {
            best_log_survival = log_survival;
            *no_candidates = 0;
        }
        if( log_survival == best_log_survival ) {
            candidates[(*no_candidates)++] = key;
        }
    }
}

int borg_move() {
    const int sz = 16;
    int candidates[sz];
    int no_candidates;
    borg_move_candidates( world, candidates, &no_candidates );
    double wisdoms[sz];

    double best_chance = -1;
    for(int j=0;j<no_candidates;j++) {
        double wisdom = mc_survival_rate( world, candidates[j] );

        wisdoms[j] = wisdom;
        if( wisdom > best_chance ) {
            best_chance = wisdom;
        }
    }

    for(int j=0;j<no_candidates;) {
        fprintf( borg_log, "%c --> %lf: ", candidates[j], wisdoms[j] );
        if( wisdoms[j] < best_chance ) {
            fprintf( borg_log, "discard\n" );
            memmove( &candidates[j], &candidates[j+1], (no_candidates-(j+1)) * sizeof candidates[0] );
            memmove( &wisdoms[j], &wisdoms[j+1], (no_candidates-(j+1)) * sizeof wisdoms[0] );
            no_candidates--;
        } else {
            fprintf( borg_log, "keep\n" );
            j++;
        }
        fflush( borg_log );
    }

    int xs[sz], ys[sz];

    for(int i=0;i<no_candidates;i++) {
        int x = world->player_x, y = world->player_y;
        switch( candidates[i] ) {
            case 'y': --y; --x; break;
            case 'k': --y; break;
            case 'u': --y; ++x; break;
            case 'b': ++y; --x; break;
            case 'j': ++y; break;
            case 'n': ++y; ++x; break;
            case 'h': --x; break;
            case 'l': ++x; break;
        }
        xs[i] = x;
        ys[i] = y;
    }

    calculate_desirability( world );

    double best_desirability = 0;
    int rv;

    for(int i=0;i<no_candidates;i++) {
        double des = desirability_map[ ys[i] ][ xs[i] ];
        fprintf( borg_log, "desirability of %lf [%d,%d] (%c)\n", des, xs[i], ys[i], candidates[i] );
        if( des > best_desirability ) {
            best_desirability = des;
            rv = i;
        }
    }

    return candidates[rv];
}

int borg_move_primitive( struct bilebio * ctx ) {
    int candidates[16];
    int no_candidates;
    borg_move_candidates( ctx, candidates, &no_candidates );

    if( !no_candidates ) {
        return '.';
    }
    int key = candidates[rand() % no_candidates];
    return key;
}
