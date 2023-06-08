#include "FT6206.h"

void INIT_FT6206(void) {
	touches = 0;
}

TS_POINT getPoint(uint8_t n) {
	//readData();
	if ((touches == 0) || (n > 1)) {
		TS_POINT_set(0, 0, 0);
	}
	else {
		TS_POINT_set(touchX[n], touchY[n], 1);
	}
	return coor;
}

void TS_POINT_clear(void) {
	coor.x = coor.y = coor.z = 0;
}

void TS_POINT_set(int16_t _x, int16_t _y, int16_t _z) {
	coor.x = _x;
	coor.y = _y;
	coor.z = _z;
}

bool same_coordinate(TS_POINT p1) {
	return ((p1.x == coor.x) && (p1.y == coor.y) && (p1.z == coor.z));
}

bool different_coordiname(TS_POINT p1) {
	return ((p1.x != coor.x) || (p1.y != coor.y) || (p1.z != coor.z));
}