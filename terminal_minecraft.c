
// good luck deciphering this mess

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <X11/Xlib.h>
#include "X11/keysym.h"


#define X_PIXELS 500
#define Y_PIXELS 180
// in radians:
#define VIEW_WIDTH 1
#define VIEW_HEIGHT 0.7

#define BLOCKS_X 20
#define BLOCKS_Y 20
#define BLOCKS_Z 10

#define EYE_HEIGHT 1.5

#define START_X 5
#define START_Y 5
#define START_Z 4+EYE_HEIGHT

#define BLOCK_BORDER_SIZE 0.03
#define BLOCK_INFILL_CHAR '@'
#define BLOCK_BORDER_CHAR '.'


typedef struct Vector{
	float x;
	float y;
	float z;
} vect;

// used for 2 angles as viewing direction
// first angles measuered from middle down, second is zero when looking in X direction
typedef struct Vector2{
	float psi;
	float phi;
} vect2;

typedef struct Vector_Vector2 {
	vect pos;
	vect2 view;
} player_pos_view;



int key_is_pressed(KeySym ks) {
    Display *dpy = XOpenDisplay(":0");
    char keys_return[32];
    XQueryKeymap(dpy, keys_return);
    KeyCode kc2 = XKeysymToKeycode(dpy, ks);
    int isPressed = !!(keys_return[kc2 >> 3] & (1 << (kc2 & 7)));
    XCloseDisplay(dpy);
    return isPressed;
}



vect vect_add(vect v1, vect v2) {
	vect res;
	res.x = v1.x + v2.x;
	res.y = v1.y + v2.y;
	res.z = v1.z + v2.z;
	return res;
}

vect vect_scale(float  s, vect v) {
	vect res = {s*v.x, s*v.y, s*v.z};
	return res;
}

void vect_normalize(vect *v) {
	float len = sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
	v->x /= len;
	v->y /= len;
	v->z /= len;
}

vect vect_sub(vect v1, vect v2) {
	vect v3 = vect_scale(-1, v2);
	return vect_add(v1, v3);
}

void vect_print(vect v) {
	printf("x=%f, y=%f, z=%f\n", v.x, v.y, v.z);
}

player_pos_view init_posview(void) {
	player_pos_view posview;
	posview.pos.x = START_X;
	posview.pos.y = START_Y;
	posview.pos.z = START_Z;
	posview.view.psi = 0;
	posview.view.phi = 0;
	return posview;
}
char ** init_picture(void) {
	char **picture = malloc(sizeof(char *) * Y_PIXELS);
	for (int i = 0; i < Y_PIXELS; ++i) {
		picture[i] = malloc(sizeof(char) * X_PIXELS);
	}
	return picture;
}
char *** init_blocks(void) {
	char ***blocks = malloc(sizeof(char **) * BLOCKS_Z);
	for (int i = 0; i < BLOCKS_Z; ++i) {
		blocks[i] = malloc(sizeof(char *) * BLOCKS_Y);
		for (int j = 0; j < BLOCKS_X; ++j) {
			blocks[i][j] = malloc(sizeof(char) * BLOCKS_X);
			for (int k = 0; k < BLOCKS_X; ++k) {
				blocks[i][j][k] = ' ';
			}
		}
	}
	return blocks;
}


vect angles_to_vect(vect2 angles) {
	vect res;
	res.x = cos(angles.psi) * cos(angles.phi);
	res.y = cos(angles.psi) * sin(angles.phi);
	res.z = sin(angles.psi);
	return res;
}

vect **init_directions(vect2 view) {
	view.psi -= VIEW_HEIGHT/2.0;
	vect screen_down = angles_to_vect(view);
	view.psi += VIEW_HEIGHT;
	vect screen_up = angles_to_vect(view);
	view.psi -= VIEW_HEIGHT/2.0;
	view.phi -= VIEW_WIDTH/2.0;
	vect screen_left = angles_to_vect(view);
	view.phi += VIEW_WIDTH;
	vect screen_right = angles_to_vect(view);
	view.phi -= VIEW_WIDTH/2.0;

	vect screen_mid_vert = vect_scale(0.5, vect_add(screen_up, screen_down));
	vect screen_mid_hor = vect_scale(0.5, vect_add(screen_left, screen_right));

	vect mid_to_left = vect_sub(screen_left, screen_mid_hor);
	vect mid_to_up = vect_sub(screen_up, screen_mid_vert);

	vect **dir = malloc(sizeof(vect *) * Y_PIXELS);
	for (int i = 0; i < Y_PIXELS; ++i) {
		dir[i] = malloc(sizeof(vect) * X_PIXELS);
	}

	// populate dir array
	for (int y_pix = 0; y_pix < Y_PIXELS; ++y_pix) {
		for (int x_pix = 0; x_pix < X_PIXELS; ++x_pix) {
			vect tmp = vect_add(vect_add(screen_mid_hor, mid_to_left), mid_to_up);
			tmp = vect_sub(tmp, vect_scale(((float)x_pix/(X_PIXELS-1))*2, mid_to_left));
			tmp = vect_sub(tmp, vect_scale(((float)y_pix/(Y_PIXELS-1))*2, mid_to_up));
			vect_normalize(&tmp);
			dir[y_pix][x_pix] = tmp;
		}
	}
	return dir;
}


int ray_outside(vect pos) {
	if (pos.x  >= BLOCKS_X) {
		return 1;
	}
	if (pos.y  >= BLOCKS_Y) {
		return 1;
	}
	if (pos.z  >= BLOCKS_Z) {
		return 1;
	}
	if (pos.x < 0) {
		return 1;
	}
	if (pos.y < 0) {
		return 1;
	}
	if (pos.z < 0) {
		return 1;
	}
	return 0;
}

float min(float a, float b) {
	if (a < b) {
		return a;
	}
	else {
		return b;
	}
}


int on_block_border(vect pos) {
	int cnt = 0;
	if (fabsf(pos.x - roundf(pos.x)) < BLOCK_BORDER_SIZE) {
		cnt++;
	}
	if (fabsf(pos.z - roundf(pos.z)) < BLOCK_BORDER_SIZE) {
		cnt++;
	}
	if (fabsf(pos.y - roundf(pos.y)) < BLOCK_BORDER_SIZE) {
		cnt++;
	}
	if (cnt >= 2) {
		return 1;
	}
	return 0;
}


char raytrace(vect pos, vect dir, char ***blocks) {
	// returns char of first block being hit that is not NULL-character
	float eps = 0.01;
	while (!ray_outside(pos)) {
		char c = blocks[(int)pos.z][(int)pos.y][(int)pos.x];
		if (c != ' ') {
			if (on_block_border(pos)) {
				return BLOCK_BORDER_CHAR;
			}
			else {
				return c;
			}
		}
		float dist = 2;
		if (dir.x > eps) {
			dist = min(dist, ((int)(pos.x+1) - pos.x)/dir.x);
		}
		else if (dir.x < -eps) {
			dist = min(dist, ((int)pos.x - pos.x)/dir.x);
		}
		if (dir.y > eps) {
			dist = min(dist, ((int)(pos.y+1) - pos.y)/dir.y);
		}
		else if (dir.y < -eps) {
			dist = min(dist, ((int)pos.y - pos.y)/dir.y);
		}
		if (dir.z > eps) {
			dist = min(dist, ((int)(pos.z+1) - pos.z)/dir.z);
		}
		else if (dir.z < -eps) {
			dist = min(dist, ((int)pos.z - pos.z)/dir.z);
		}
		pos = vect_add(pos, vect_scale(dist+eps, dir));
	}
	return ' ';
}


void get_picture(char **picture, player_pos_view posview, char ***blocks) {
	// get directions for rays
	vect **directions = init_directions(posview.view);
	for (int y = 0; y < Y_PIXELS; ++y) {
		for (int x = 0; x < X_PIXELS; ++x) {
			picture[y][x] = raytrace(posview.pos, directions[y][x], blocks);
		}
	}
}


void draw_ascii(char **picture) {
	printf("\033[0;0H");	// jump to position 0 0 to overwrite current picture
	for (int i = 0; i < Y_PIXELS; ++i) {
		for (int j = 0; j < X_PIXELS; ++j) {
			printf("%c", picture[i][j]);
		}
		printf("\n");
	}	
}


void update_posview(player_pos_view *posview, char*** blocks) {
	float move_eps = 0.15;
	float tilt_eps = 0.03;
	int x = (int) posview->pos.x, y = (int) posview->pos.y;
	int z = (int) (posview->pos.z - EYE_HEIGHT + 0.01);
	if (blocks[z][y][x] != ' ') {
		posview->pos.z += 1;
	}
	z = (int) (posview->pos.z - EYE_HEIGHT - 0.01) ;
	if (blocks[z][y][x] == ' ') {
		posview->pos.z -= 1;
	}
	if (key_is_pressed(XK_W)) {
		posview->view.psi += tilt_eps;
	}
	if (key_is_pressed(XK_S)) {
		posview->view.psi -= tilt_eps;
	}
	if (key_is_pressed(XK_A)) {
		posview->view.phi -= tilt_eps;
	}
	if (key_is_pressed(XK_D)) {
		posview->view.phi += tilt_eps;
	}
	vect direction = angles_to_vect(posview->view);
	if (key_is_pressed(XK_Up)) {
		posview->pos.x += move_eps*direction.x;
		posview->pos.y += move_eps*direction.y;
	}
	if (key_is_pressed(XK_Down)) {
		posview->pos.x -= move_eps*direction.x;
		posview->pos.y -= move_eps*direction.y;
	}
	if (key_is_pressed(XK_Left)) {
		posview->pos.x += move_eps*direction.y;
		posview->pos.y -= move_eps*direction.x;
	}
	if (key_is_pressed(XK_Right)) {
		posview->pos.x -= move_eps*direction.y;
		posview->pos.y += move_eps*direction.x;
	}
}


vect get_current_block(player_pos_view posview, char ***blocks) {
	// exactly like raytrace for middle ray, but returns position of block that was hit
	float eps = 0.01;
	vect pos = posview.pos;
	vect dir = angles_to_vect(posview.view);
	while (!ray_outside(pos)) {
		char c = blocks[(int)pos.z][(int)pos.y][(int)pos.x];
		if (c != ' ') {
			return pos;
		}
		float dist = 2;
		if (dir.x > eps) {
			dist = min(dist, ((int)(pos.x+1) - pos.x)/dir.x);
		}
		else if (dir.x < -eps) {
			dist = min(dist, ((int)pos.x - pos.x)/dir.x);
		}
		if (dir.y > eps) {
			dist = min(dist, ((int)(pos.y+1) - pos.y)/dir.y);
		}
		else if (dir.y < -eps) {
			dist = min(dist, ((int)pos.y - pos.y)/dir.y);
		}
		if (dir.z > eps) {
			dist = min(dist, ((int)(pos.z+1) - pos.z)/dir.z);
		}
		else if (dir.z < -eps) {
			dist = min(dist, ((int)pos.z - pos.z)/dir.z);
		}
		pos = vect_add(pos, vect_scale(dist+eps, dir));
	}
	return pos;
}


void place_block(vect pos, char ***blocks, char block) {
	int x = (int) pos.x, y = (int) pos.y, z = (int) pos.z;
	// find out on which face to place new block: place it on face to which pos is closest
	// faces 0-5: x+, x-, y+, y-, z+, z-

	float dists[6];
	dists[0] = fabsf(x+1-pos.x);
	dists[1] = fabsf(pos.x-x);
	dists[2] = fabsf(y+1-pos.y);
	dists[3] = fabsf(pos.y-y);
	dists[4] = fabsf(z+1-pos.z);
	dists[5] = fabsf(pos.z-z);
	int min = 0;
	float mindist = dists[0];
	for (int i = 1; i < 6; ++i) {
		if (dists[i] < mindist) {
			mindist = dists[i];
			min = i;
		}
	}
	switch (min) {
		case 0:
			blocks[z][y][x+1] = block;
			break;
		case 1:
			blocks[z][y][x-1] = block;
			break;
		case 2:
			blocks[z][y+1][x] = block;
			break;
		case 3:
			blocks[z][y-1][x] = block;
			break;
		case 4:
			blocks[z+1][y][x] = block;
			break;
		case 5:
			blocks[z-1][y][x] = block;
			break;  
	}

}



int main(void) {
	char **picture = init_picture();
	char ***blocks = init_blocks();
	player_pos_view posview = init_posview();


	for (int x = 0; x < BLOCKS_X; ++x) {
		for (int y = 0; y < BLOCKS_Y; ++y) {
			for (int z = 0; z < 4; ++z) {
				blocks[z][y][x] = BLOCK_INFILL_CHAR;				
			}
		}
	}

	// main loop:
	int i = 0;
	int place_block_allowed = 1;
	while (i >= 0) {
		// updates position and view based on keyboard inputs
		update_posview(&posview, blocks);
		
		// get block currently pointed at (i.e. block in the middle of the view), mark it visibly
		vect current_block = get_current_block(posview, blocks);
		int have_current_block = !ray_outside(current_block);
		int current_block_x = (int) current_block.x;
		int current_block_y = (int) current_block.y;
		int current_block_z = (int) current_block.z;
		char current_block_c;
		int removed_block = 0;
		if (have_current_block) {
			current_block_c = blocks[current_block_z][current_block_y][current_block_x];
			blocks[current_block_z][current_block_y][current_block_x] = 'o';
			if (place_block_allowed && key_is_pressed(XK_space)) {
				if (key_is_pressed(XK_Shift_L)) {
					blocks[current_block_z][current_block_y][current_block_x] = ' ';
					removed_block = 1;
				}
				else {
					place_block(current_block, blocks, BLOCK_INFILL_CHAR);
				}
				place_block_allowed = 0;
			}
			else if (!key_is_pressed(XK_space)) {
				place_block_allowed = 1;
			}
		}
		else if (!key_is_pressed(XK_space)){
			place_block_allowed = 1;
		}
		// creates picture
		get_picture(picture, posview, blocks);
		// reset current_block char
		if (have_current_block && !removed_block) {
			blocks[current_block_z][current_block_y][current_block_x] = current_block_c;
		}

		// outputs picture
		draw_ascii(picture);

	}
}



