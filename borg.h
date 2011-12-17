#ifndef H_BORG
#define H_BORG

#include "bilebio.h"

void initialize_borg( struct bilebio * );
void quit_borg();
int borg_move();
void borg_print(const char*);
void borg_postmortem();

#endif
