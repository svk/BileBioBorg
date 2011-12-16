#include "borg.h"

static struct bilebio * world = 0;

void initialize_borg( struct bilebio * real_world ) {
    world = real_world;
}

int borg_move() {
    return '.';
}
