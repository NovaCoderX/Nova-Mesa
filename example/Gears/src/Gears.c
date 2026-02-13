#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <sys/time.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>


#include <proto/cybergraphics.h>
#include <cybergraphics/cybergraphics.h>


#include <GL/amiga_mesa.h>
#include <GL/glu.h>

#define WIDTH 640
#define HEIGHT 480
#define DEPTH 32


// Hardware screen - Only used when running in fullscreen.
static struct Screen *_hardwareScreen = NULL;

// Hardware window - Used for both fullscreen and window modes.
static struct Window *_hardwareWindow = NULL;

struct Library *CyberGfxBase = NULL;


static AMesaContext *context = NULL;

static UWORD _blankCursor[] = { 0x0000, 0x0000,   // reserved, must be NULL
		0x0000, 0x0000,   // 1 row of image data
		0x0000, 0x0000    // reserved, must be NULL
		};


static ULONG getCyberModeId(int width, int height, int depth) {
    ULONG modeId = INVALID_ID;
    ULONG nextid = NextDisplayInfo(INVALID_ID);
    ULONG pixelFormat = 0;

	while ((nextid != INVALID_ID)) {
		if (IsCyberModeID(nextid)) {
			GLboolean validMode = GL_FALSE;

			pixelFormat = GetCyberIDAttr(CYBRIDATTR_PIXFMT, nextid);
			if (depth == 8) {
				if (pixelFormat == PIXFMT_LUT8) {
					validMode = GL_TRUE;
				}
			} else {
                if (depth == 32) {
    				if ((pixelFormat == PIXFMT_ARGB32) || (pixelFormat == PIXFMT_BGRA32) || (pixelFormat == PIXFMT_RGBA32)) {
    					validMode = GL_TRUE;
    				}
                } else {
    				if ((pixelFormat == PIXFMT_BGR24) || (pixelFormat == PIXFMT_RGB24)) {
    					validMode = GL_TRUE;
    				}
                }
			}

			if (validMode) {
                if (GetCyberIDAttr(CYBRIDATTR_WIDTH, nextid) == width) {
                	if (GetCyberIDAttr(CYBRIDATTR_HEIGHT, nextid) == height) {
                        // Success, exact match.
                        modeId = nextid;
                        break;
                    }
                }
            }
		}

		nextid = NextDisplayInfo(nextid);
	}

    return modeId;
}



// Demo code - START
static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;

static void gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width, GLint teeth, GLfloat tooth_depth) {
	GLint i;
	GLfloat r0, r1, r2;
	GLfloat angle, da;
	GLfloat u, v, len;

	r0 = inner_radius;
	r1 = outer_radius - tooth_depth / 2.0;
	r2 = outer_radius + tooth_depth / 2.0;

	da = 2.0 * M_PI / teeth / 4.0;

	glShadeModel(GL_FLAT);

	glNormal3f(0.0, 0.0, 1.0);

	/* draw front face */
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= teeth; i++) {
		angle = i * 2.0 * M_PI / teeth;
		glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
		if (i < teeth) {
			glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
			glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
		}
	}
	glEnd();

	/* draw front sides of teeth */
	glBegin(GL_QUADS);
	da = 2.0 * M_PI / teeth / 4.0;
	for (i = 0; i < teeth; i++) {
		angle = i * 2.0 * M_PI / teeth;

		glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
	}
	glEnd();

	glNormal3f(0.0, 0.0, -1.0);

	/* draw back face */
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= teeth; i++) {
		angle = i * 2.0 * M_PI / teeth;
		glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
		glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
		if (i < teeth) {
			glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
			glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
		}
	}
	glEnd();

	/* draw back sides of teeth */
	glBegin(GL_QUADS);
	da = 2.0 * M_PI / teeth / 4.0;
	for (i = 0; i < teeth; i++) {
		angle = i * 2.0 * M_PI / teeth;

		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -width * 0.5);
		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
	}
	glEnd();

	/* draw outward faces of teeth */
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i < teeth; i++) {
		angle = i * 2.0 * M_PI / teeth;

		glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
		u = r2 * cos(angle + da) - r1 * cos(angle);
		v = r2 * sin(angle + da) - r1 * sin(angle);
		len = sqrt(u * u + v * v);
		u /= len;
		v /= len;
		glNormal3f(v, -u, 0.0);
		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
		glNormal3f(cos(angle), sin(angle), 0.0);
		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -width * 0.5);
		u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
		v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
		glNormal3f(v, -u, 0.0);
		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -width * 0.5);
		glNormal3f(cos(angle), sin(angle), 0.0);
	}

	glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
	glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);

	glEnd();

	glShadeModel(GL_SMOOTH);

	/* draw inside radius cylinder */
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= teeth; i++) {
		angle = i * 2.0 * M_PI / teeth;
		glNormal3f(-cos(angle), -sin(angle), 0.0);
		glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
		glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
	}
	glEnd();
}

static void draw(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();
	glRotatef(view_rotx, 1.0, 0.0, 0.0);
	glRotatef(view_roty, 0.0, 1.0, 0.0);
	glRotatef(view_rotz, 0.0, 0.0, 1.0);

	glPushMatrix();
	glTranslatef(-3.0, -2.0, 0.0);
	glRotatef(angle, 0.0, 0.0, 1.0);
	glCallList(gear1);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(3.1, -2.0, 0.0);
	glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
	glCallList(gear2);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(-3.1, 4.2, 0.0);
	glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
	glCallList(gear3);
	glPopMatrix();

	glPopMatrix();

	amesa_swap_buffers(context);
}

static void reshape(int width, int height) {
	GLfloat h = (GLfloat) height / (GLfloat) width;

	glViewport(0, 0, (GLint) width, (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0, 0.0, -40.0);
}

static void init(void) {
	static GLfloat pos[4] = { 5.0, 5.0, 10.0, 0.0 };
	static GLfloat red[4] = { 0.8, 0.1, 0.0, 1.0 };
	static GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
	static GLfloat blue[4] = { 0.2, 0.2, 1.0, 1.0 };

	glLightfv(GL_LIGHT0, GL_POSITION, pos);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);

	/* make the gears */
	gear1 = glGenLists(1);
	glNewList(gear1, GL_COMPILE);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
	gear(1.0, 4.0, 1.0, 20, 0.7);
	glEndList();

	gear2 = glGenLists(1);
	glNewList(gear2, GL_COMPILE);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
	gear(0.5, 2.0, 2.0, 10, 0.7);
	glEndList();

	gear3 = glGenLists(1);
	glNewList(gear3, GL_COMPILE);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
	gear(1.3, 2.0, 0.5, 10, 0.7);
	glEndList();

	glEnable(GL_NORMALIZE);

	glClearColor(0, 0, 0, 1);

	reshape(WIDTH, HEIGHT);
}
// Demo code - FINISH


void exitT(void) {
	if (context) {
		amesa_destroy_context(context);
		context = NULL;
	}

	if (_hardwareWindow) {
		ClearPointer(_hardwareWindow);
		CloseWindow(_hardwareWindow);
		_hardwareWindow = NULL;
	}

	if (_hardwareScreen) {
		// We opened the screen and therefore must close it (running in full screen mode).
		CloseScreen(_hardwareScreen);
		_hardwareScreen = NULL;
	}

	if (CyberGfxBase) {
		CloseLibrary(CyberGfxBase);
		CyberGfxBase = NULL;
	}
}

static struct Screen* CreateHardwareScreen(ULONG modeId, int width, int height, int bpp) {
	return OpenScreenTags(NULL, SA_Depth, bpp, SA_DisplayID, modeId, SA_Top, 0, SA_Left, 0, SA_Width, width, SA_Height, height, SA_Type,
			CUSTOMSCREEN, SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Draggable, FALSE, SA_Exclusive, TRUE, SA_AutoScroll, FALSE, TAG_DONE);
}

static struct Window* CreateHardwareWindowFullScreen(int width, int height, struct Screen *sreen) {
	return OpenWindowTags(NULL, WA_Left, 0, WA_Top, 0, WA_Width, width, WA_Height, height, WA_CustomScreen, (ULONG )sreen, WA_Backdrop,
			TRUE, WA_Borderless, TRUE, WA_Activate, TRUE, WA_SimpleRefresh, TRUE, WA_NoCareRefresh, TRUE, WA_ReportMouse, FALSE, WA_RMBTrap,
			TRUE, WA_IDCMP, IDCMP_CLOSEWINDOW|IDCMP_ACTIVEWINDOW|IDCMP_INACTIVEWINDOW, TAG_END);
}

int main(int argc, char **argv) {
    struct timeval t1, t2;
    double elapsedtime;
    static GLint frames = 0;
    ULONG modeId = INVALID_ID;

    // start timer
    gettimeofday(&t1, NULL);

	atexit(exitT);

	CyberGfxBase = OpenLibrary((UBYTE* )"cybergraphics.library", 0);

	modeId = getCyberModeId(WIDTH, HEIGHT, DEPTH);

	_hardwareScreen = CreateHardwareScreen(modeId, WIDTH, HEIGHT, DEPTH);

	_hardwareWindow = CreateHardwareWindowFullScreen(WIDTH, HEIGHT, _hardwareScreen);

	SetPointer(_hardwareWindow, (UWORD* )_blankCursor, 1, 1, 0, 0);

	context = amesa_create_context(_hardwareWindow);

	amesa_make_current(context);

	init();

	while (frames < 120) {
		draw();
		angle += 2.0;
		frames++;
	}

    // stop timer
    gettimeofday(&t2, NULL);

    // compute and print the elapsed time in millisec
    elapsedtime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
    elapsedtime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms

    printf("GL_RENDERER = %s\n", (char *) glGetString(GL_RENDERER));
	printf("GL_VERSION = %s\n", (char *) glGetString(GL_VERSION));
	printf("GL_VENDOR = %s\n", (char *) glGetString(GL_VENDOR));
	//printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));

	// Test
	//printf("depth = %u\n", depth);

	GLfloat seconds = (elapsedtime / 1000.0);
	GLfloat fps = frames / seconds;
    printf("%d frames in %6.3f seconds = %6.3f FPS\n", frames, seconds, fps);

	return EXIT_SUCCESS;
}
