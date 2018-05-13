#ifndef _NDS_STATUS_H_
#define _NDS_STATUS_H_

void nds_status_init();
void nds_status_putstr(char *str);
int nds_status_get_bottom();
void nds_status_update(int fldidx, genericptr_t ptr, int chg, int percent, int color, unsigned long *colormasks);

#endif
