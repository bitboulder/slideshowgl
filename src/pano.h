#ifndef _PANO_H
#define _PANO_H

struct ipano {
	char enable;
	float gw;
	float gh;
	float gyoff;
	float rotinit;
};

void panocheck();
void panorender();
void panorun();

#endif
