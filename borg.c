#include "borg.h"

static struct bilebio * world = 0;

FILE *borg_log = 0;

#define MAX_ONE_IN 100

double logp_one_in[MAX_ONE_IN];
double logp_complement_of_one_in[MAX_ONE_IN];

int borg_move_primitive( struct bilebio *);

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
        if( is_obstructed( ctx, x, y ) ) continue;
        if( t->type != 0 || (i == 0 && j == 0) ) continue;

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

    int rv = candidates[ rand() % no_candidates ];
    return rv;
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
