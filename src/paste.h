/******************************************************************************
 *                                                                            *
 *                                "paste.h"                                   *
 *                                                                            *
 ******************************************************************************/

#ifndef PASTE_H
#define PASTE_H

void paste_init(void);
void paste_clean(void);
int paste_stop(void);
void paste_start_text(void);
void paste_start_boot(void);
void paste_frame(u8*);

#endif /* PASTE_H */
