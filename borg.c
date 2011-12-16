#include "borg.h"

static struct bilebio * world = 0;

FILE *borg_log = 0;

#define MAX_ONE_IN 100

double logp_one_in[MAX_ONE_IN];
double logp_complement_of_one_in[MAX_ONE_IN];

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
    fprintf( borg_log, "calculating LS at (%d,%d)\n", x, y );
    for(int j=-2;j<=2;j++) {
        for(int i=-2;i<=2;i++) {
            if( (x+i) < 0 || (x+i) >= STAGE_WIDTH ) continue;
            if( (y+j) < 0 || (y+j) >= STAGE_HEIGHT ) continue;
            if( i == 0 && j == 0 ) {
                fprintf( borg_log, "X" );
            } else {
                print_cell( &map[y+j][x+i] );
            }
        }
        fprintf( borg_log, "\n" );
    }
    fprintf( borg_log, "\n" );
    for(int j=-2;j<=2;j++) {
        for(int i=-2;i<=2;i++) {
            if( (x+i) < 0 || (x+i) >= STAGE_WIDTH ) continue;
            if( (y+j) < 0 || (y+j) >= STAGE_HEIGHT ) continue;
            fprintf( borg_log, "[%d,%d] ", i, j );
            fprintf( borg_log, "%lf ", log_survival_from( &map[y+j][x+i], i, j ) );
        }
        fprintf( borg_log, "\n" );
    }
    fprintf( borg_log, "\n" );
    for(int i=-2;i<=2;i++) for(int j=-2;j<=2;j++) if( i || j ) {
        if( (x+i) < 0 || (x+i) >= STAGE_WIDTH ) continue;
        if( (y+j) < 0 || (y+j) >= STAGE_HEIGHT ) continue;
        log_survival += log_survival_from( &map[y+j][x+i], i, j );
    }
    return log_survival;
}

int borg_move() {
    int keys[3][3] = {
        { 'y', 'k', 'u' },
        { 'h', '.', 'l' },
        { 'b', 'j', 'n' },
    };
    double best_log_survival = -1000;
    int candidates[16];
    int no_candidates = 0;
    for(int i=-1;i<=1;i++) for(int j=-1;j<=1;j++) {
        const int x = world->player_x + i, y = world->player_y + j; 
        const int key = keys[j+1][i+1];
        struct tile * t = &world->stage[y][x];
        if( is_obstructed( world, x, y ) ) continue;
        if( t->type != 0 || (i == 0 && j == 0) ) continue;

        double log_survival = log_survival_at( world->stage, x, y );
        if( log_survival > best_log_survival ) {
            best_log_survival = log_survival;
            no_candidates = 0;
        }
        if( log_survival == best_log_survival ) {
            candidates[no_candidates++] = key;
        }
    }
    if( !no_candidates ) {
        return '.';
    }
    fprintf( borg_log, "%d candidates, %lf risk: ", no_candidates, best_log_survival );
    for(int i=0;i<no_candidates;i++) {
        fprintf( borg_log, "%c ", candidates[i] );
    }
    int key = candidates[rand() % no_candidates];
    fprintf( borg_log, " --> %c\n", key );
    fflush( borg_log );
//    getch();
    return key;
}
