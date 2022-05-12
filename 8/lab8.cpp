#include <iostream>
#include <fstream>
using namespace std;
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <time.h>
#include <cstring>
#include <cmath>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
//for text on the screen
#include "fonts.h"
#include <pthread.h>

class Image {
public:
    int width, height, max;
    char *data;
    Image() {}
    Image(const char *fname) {
        bool isPPM = true;
        char str[1200];
        char newfile[200];
        ifstream fin;
        char *p = strstr((char *)fname, ".ppm");
        if (!p) {
            //not a ppm file
            isPPM = false;
            strcpy(newfile, fname);
            newfile[strlen(newfile)-4] = '\0';
            strcat(newfile, ".ppm");
            sprintf(str, "convert %s %s", fname, newfile);
            system(str);
            fin.open(newfile);
        } else {
            fin.open(fname);
        }
        char p6[10];
        fin >> p6;
        fin >> width >> height;
        fin >> max;
        data = new char [width * height * 3];
        fin.read(data, 1);
        fin.read(data, width * height * 3);
        fin.close();
        if (!isPPM)
            unlink(newfile);
    }
} img("/home/stu/ssingh2/4490/8/lab8.ppm"), 
	sprite("/home/stu/ssingh2/4490/8/bees.png");

struct Vector {
	float x,y,z;

};

typedef double Flt;
//a game object
class Bee {
public:
	Flt pos[3]; //vector 
	Flt vel[3]; //vector
	Flt mass;
	float w, h;
	unsigned int color;
	bool alive_or_dead;
	Bee() {
		w = h = 4.0;
		pos[0] = 1.0;
		pos[1] = 200.0;
		vel[0] = 4.0;
		vel[1] = 0.0;
	}
	void set_dimensions(int x, int y) {
        w = (float)x * 0.05;
        h = w;
    }
};

class Global {
public:
	int xres, yres;
	int frameno;
	Flt gravity;
	Bee bees[2];
	//box components
	float pos[2];
	float w;
	float dir;
	int inside;
	unsigned int texid;
	unsigned int spriteid;
	Global();
} g;

class X11_wrapper {
private:
	Display *dpy;
	Window win;
	GLXContext glc;
public:
	~X11_wrapper();
	X11_wrapper();
	void set_title();
	bool getXPending();
	XEvent getXNextEvent();
	void swapBuffers();
	void reshape_window(int width, int height);
	void check_resize(XEvent *e);
	void check_mouse(XEvent *e);
	int check_keys(XEvent *e);
} x11;

//Function prototypes
void init_opengl(void);
void physics(void);
void render(void);

void *spriteThread(void *arg) {
	//-----------------------------------------------------------------------------
	//Setup timers
	//const double OOBILLION = 1.0 / 1e9;
	struct timespec start, end;
	extern double timeDiff(struct timespec *start, struct timespec *end);
	extern void timeCopy(struct timespec *dest, struct timespec *source);
	//-----------------------------------------------------------------------------
	clock_gettime(CLOCK_REALTIME, &start);
	double diff;
	while(1) {
		//if some amount of time has passed, change the frame numeber
		clock_gettime(CLOCK_REALTIME, &end);
		diff = timeDiff(&start, &end);
		if (diff >= 0.05) {
			//enough time has passed
			++g.frameno;
			if (g.frameno > 20)
				g.frameno = 1;
			timeCopy(&start, &end);
		}

	}
	return (void *)0;
}

int main()
{
	//start the thread
	pthread_t th;
	pthread_create(&th, NULL, spriteThread, NULL);

	init_opengl();
	initialize_fonts();
	//main game loop
	int done = 0;
	while (!done) {
		//process events...
		while (x11.getXPending()) {
			XEvent e = x11.getXNextEvent();
			x11.check_resize(&e);
			x11.check_mouse(&e);
			done = x11.check_keys(&e);
		}
		physics();           //move things
		render();            //draw things
		x11.swapBuffers();   //make video memory visible
		usleep(200);         //pause to let X11 work better
	}
	cleanup_fonts();
	return 0;
}

Global::Global()
{
	xres = 400;
	yres = 200;
	//box
	w = 20.0f;
	pos[0] = 0.0f + w;
	pos[1] = yres/2.0f;
	dir = 5.0f;
	inside = 0;
	gravity = 80.0;
	frameno = 1;

}

X11_wrapper::~X11_wrapper()
{
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
}

X11_wrapper::X11_wrapper()
{
	GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	int w = g.xres, h = g.yres;
	dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		cout << "\n\tcannot connect to X server\n" << endl;
		exit(EXIT_FAILURE);
	}
	Window root = DefaultRootWindow(dpy);
	XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
	if (vi == NULL) {
		cout << "\n\tno appropriate visual found\n" << endl;
		exit(EXIT_FAILURE);
	} 
	Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	XSetWindowAttributes swa;
	swa.colormap = cmap;
	swa.event_mask =
		ExposureMask | KeyPressMask | KeyReleaseMask |
		ButtonPress | ButtonReleaseMask |
		PointerMotionMask |
		StructureNotifyMask | SubstructureNotifyMask;
	win = XCreateWindow(dpy, root, 0, 0, w, h, 0, vi->depth,
		InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
	set_title();
	glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
	glXMakeCurrent(dpy, win, glc);
}

void X11_wrapper::set_title()
{
	//Set the window title bar.
	XMapWindow(dpy, win);
	XStoreName(dpy, win, "4490 Lab8");
}

bool X11_wrapper::getXPending()
{
	//See if there are pending events.
	return XPending(dpy);
}

XEvent X11_wrapper::getXNextEvent()
{
	//Get a pending event.
	XEvent e;
	XNextEvent(dpy, &e);
	return e;
}

void X11_wrapper::swapBuffers()
{
	glXSwapBuffers(dpy, win);
}

void X11_wrapper::reshape_window(int width, int height)
{
	//window has been resized.
	g.xres = width;
	g.yres = height;
	//
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glOrtho(0, g.xres, 0, g.yres, -1, 1);
	g.bees[0].set_dimensions(g.xres, g.yres);
}

void X11_wrapper::check_resize(XEvent *e)
{
	//The ConfigureNotify is sent by the
	//server if the window is resized.
	if (e->type != ConfigureNotify)
		return;
	XConfigureEvent xce = e->xconfigure;
	if (xce.width != g.xres || xce.height != g.yres) {
		//Window size did change.
		reshape_window(xce.width, xce.height);
	}
}
//-----------------------------------------------------------------------------

void X11_wrapper::check_mouse(XEvent *e)
{
	static int savex = 0;
	static int savey = 0;

	//Weed out non-mouse events
	if (e->type != ButtonRelease &&
		e->type != ButtonPress &&
		e->type != MotionNotify) {
		//This is not a mouse event that we care about.
		return;
	}
	//
	if (e->type == ButtonRelease) {
		return;
	}
	if (e->type == ButtonPress) {
		if (e->xbutton.button==1) {
			//Left button was pressed.
			int y = g.yres - e->xbutton.y;
			int x = e->xbutton.x;
			if (x >= g.pos[0]-g.w && x <= g.pos[0]+g.w) {
				if (y >= g.pos[1]-g.w && y <= g.pos[1]+g.w) {
					g.inside++;
					printf("hits: %i\n", g.inside);
				}
			}
			return;
		}
		if (e->xbutton.button==3) {
			//Right button was pressed.
			return;
		}
	}
	if (e->type == MotionNotify) {
		//The mouse moved!
		if (savex != e->xbutton.x || savey != e->xbutton.y) {
			savex = e->xbutton.x;
			savey = e->xbutton.y;
			//Code placed here will execute whenever the mouse moves.


		}
	}
}

int X11_wrapper::check_keys(XEvent *e)
{
	if (e->type != KeyPress && e->type != KeyRelease)
		return 0;
	int key = XLookupKeysym(&e->xkey, 0);
	if (e->type == KeyPress) {
		switch (key) {
			case XK_1:
				//Key 1 was pressed
				break;
			case XK_Escape:
				//Escape key was pressed
				return 1;
		}
	}
	return 0;
}


unsigned char *buildAlphaData(Image *img)
{
    //add 4th component to RGB stream...
    int i;
    int a,b,c;
    unsigned char *newdata, *ptr;
    unsigned char *data = (unsigned char *)img->data;
    newdata = (unsigned char *)malloc(img->width * img->height * 4);
    ptr = newdata;
    for (i=0; i<img->width * img->height * 3; i+=3) {
        a = *(data+0);
        b = *(data+1);
        c = *(data+2);
        *(ptr+0) = a;
        *(ptr+1) = b;
        *(ptr+2) = c;
        //-----------------------------------------------
        //get largest color component...
        //*(ptr+3) = (unsigned char)((
        //      (int)*(ptr+0) +
        //      (int)*(ptr+1) +
        //      (int)*(ptr+2)) / 3);
        //d = a;
        //if (b >= a && b >= c) d = b;
        //if (c >= a && c >= b) d = c;
        //*(ptr+3) = d;
        //-----------------------------------------------
        //this code optimizes the commented code above.
        *(ptr+3) = (a!=255&&b!=255&&c!=255);
        //-----------------------------------------------
        ptr += 4;
        data += 3;
    }
    return newdata;
}

void init_opengl(void)
{
	//OpenGL initialization
	glViewport(0, 0, g.xres, g.yres);
	//Initialize matrices
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	//Set 2D mode (no perspective)
	glOrtho(0, g.xres, 0, g.yres, -1, 1);
	//
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	//Set the screen background color
	glClearColor(0.1, 0.1, 0.1, 1.0);
	//allow 2D texture maps
	glEnable(GL_TEXTURE_2D);
	initialize_fonts();
	//background flowers
	glGenTextures(1, &g.texid);
	glBindTexture(GL_TEXTURE_2D, g.texid);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, img.width, img.height, 0,
                        GL_RGB, GL_UNSIGNED_BYTE, img.data);

	//#define CALL_FUNC
	//#ifdef CALL_FUNC
	//unsigned char *data2 = buildAlphaData(&sprite);
	
	//background bee sprite
	//make a new data stream, with four components
	//add an alpha channel 
	//#else
	unsigned char *data2 = new unsigned char [sprite.width * sprite.height * 4];
	for (int i=0; i<sprite.height; i++) {
    	for (int j=0; j<sprite.width; j++) { 
        	int offset  = i*sprite.width*3 + j*3;
        	int offset2 = i*sprite.width*4 + j*4;
        	data2[offset2+0] = sprite.data[offset+0];
        	data2[offset2+1] = sprite.data[offset+1];
        	data2[offset2+2] = sprite.data[offset+2];
        	data2[offset2+3] =
        	((unsigned char)sprite.data[offset+0] != 255 &&
         	(unsigned char)sprite.data[offset+1] != 255 &&
         	(unsigned char)sprite.data[offset+2] != 255);
   		}
	}
	//#endif
	glGenTextures(1, &g.spriteid);
	glBindTexture(GL_TEXTURE_2D, g.spriteid);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sprite.width, sprite.height, 0,
                        				GL_RGBA, GL_UNSIGNED_BYTE, data2);
	delete [] data2;
	g.bees[0].set_dimensions(g.xres, g.yres);
	
}

void physics()
{
	//++g.frameno;
	//if (g.frameno > 20)
		//g.frameno = 1;

	//physics
	g.bees[0].pos[0] += g.bees[0].vel[0];
	g.bees[0].pos[1] += g.bees[0].vel[1];
	//collision detection
	if (g.bees[0].pos[0] >= g.xres) {
		g.bees[0].pos[0] = g.xres;
		g.bees[0].vel[0] = 0.0;
	}
	if (g.bees[0].pos[0] <= 0) {
		g.bees[0].pos[0] = 0;
		g.bees[0].vel[0] = 0.0;
	}
	if (g.bees[0].pos[1] >= g.yres) {
		g.bees[0].pos[1] = g.yres;
		g.bees[0].vel[1] = 0.0;
	}
	if (g.bees[0].pos[1] <= 0) {
		g.bees[0].pos[1] = 0;
		g.bees[0].vel[1] = 0.0;
	}
	//move the bee toward the flower...
	Flt cx = g.xres/2.0;
	Flt cy = g.yres/2.0;
	cx = g.xres * (218.0/300.0);
	cy = g.yres * (86.0/169.0); 
	Flt dx = cx - g.bees[0].pos[0];
	Flt dy = cy - g.bees[0].pos[1];
	Flt dist = (dx*dx + dy*dy);
	if (dist < 0.01)
		dist = 0.01; //clamp
	//change in velocity based on a force (gravity)
	g.bees[0].vel[0] += (dx / dist) * g.gravity;
	g.bees[0].vel[1] += (dy / dist) * g.gravity;
	g.bees[0].vel[0] += ((Flt)rand() / (Flt)RAND_MAX) * 0.5 - 0.25;
	g.bees[0].vel[1] += ((Flt)rand() / (Flt)RAND_MAX) * 0.5 - 0.25; 
}

void render()
{
	glClear(GL_COLOR_BUFFER_BIT);	
	//day time
	//glColor3ub(255, 255, 225);
	//night time
	glColor3ub(80, 80, 160);
	glBindTexture(GL_TEXTURE_2D, g.texid);
	glBegin(GL_QUADS);
    	glTexCoord2f(0,1); glVertex2i(0,      0);
    	glTexCoord2f(0,0); glVertex2i(0,      g.yres);
   		glTexCoord2f(1,0); glVertex2i(g.xres, g.yres);
    	glTexCoord2f(1,1); glVertex2i(g.xres, 0);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);

	Rect r;
	r.bot = g.yres - 20;
	r.left = 10;
	r.center = 0;
	//
	//glClear(GL_COLOR_BUFFER_BIT);
	ggprint8b(&r, 0, 0x00FFFFFF, "Sukh \n WOKRING ON THE COLORS");

	//Draw bee.
	glPushMatrix();
	glColor3ub(255, 255, 255);
	glTranslatef(g.bees[0].pos[0], g.bees[0].pos[1], 0.0f);

	//set alpha test 
	//set an alpha channel
	//https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glAlphaFunc.xml
	glEnable(GL_ALPHA_TEST);

	//transparent if alpha value is greater than 0.0
	glAlphaFunc(GL_GREATER, 0.0f);

	//Set 4-channels of color intensity
	glColor4ub(255,255,255,255);
	//
	glBindTexture(GL_TEXTURE_2D, g.spriteid);
	//make texture coordinates based on a frame number
	//float tx = ((g.frameno % 5) * 0.2;
	//float ty = 1.0 - ((g.frameno / 5) * 0.1999999999);
	float tx1 = 0.0f + (float)((g.frameno-1) % 5) * 0.2f;
	float tx2 = tx1 + 0.2f;
	float ty1 = 0.0f + (float)((g.frameno-1) / 5) * 0.2f;
	float ty2 = ty1 + 0.2;

	if (g.bees[0].vel[0] > 0.0) {
		float tmp = tx1;
		tx1 = tx2;
		tx2 = tmp;
	}

	glBegin(GL_QUADS);
		glTexCoord2f(tx1,  ty2); glVertex2f(-g.bees[0].w, -g.bees[0].h);
		glTexCoord2f(tx1, ty1); glVertex2f(-g.bees[0].w,  g.bees[0].h);
		glTexCoord2f(tx2, ty1); glVertex2f( g.bees[0].w,  g.bees[0].h);
		glTexCoord2f(tx2, ty2); glVertex2f( g.bees[0].w, -g.bees[0].h);
	glEnd();
	//turn off alpha test
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_ALPHA_TEST);
	glPopMatrix();
}






