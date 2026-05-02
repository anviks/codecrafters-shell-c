#ifndef REDIRECT_H
#define REDIRECT_H

#include "types.h"

void apply_redirects(Command* cmd);
void restore_redirects(Command* cmd);

#endif
