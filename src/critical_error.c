/* critical_error.c - see critical_error.h */

#include <dos.h>
#include "critical_error.h"

static int _far critical_error_handler(unsigned deverr, unsigned errcode,
                                        unsigned _far *devhdr)
{
    (void)deverr;
    (void)errcode;
    (void)devhdr;
    return _HARDERR_FAIL;
}

void install_silent_critical_handler(void)
{
    _harderr(critical_error_handler);
}
