#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glkos.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alut.h>

extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);

typedef struct {
    float x, y, z;
} Vec3;

Vec3 vec3_init(float x, float y, float z) {
    Vec3 ret;
    ret.x = x;
    ret.y = y;
    ret.z = z;
    return ret;
}

Vec3 vec3_add(const Vec3* lhs, const Vec3* rhs) {
    Vec3 ret;
    ret.x = lhs->x + rhs->x;
    ret.y = lhs->y + rhs->y;
    ret.z = lhs->z + rhs->z;
    return ret;
}

Vec3 vec3_sub(const Vec3* lhs, const Vec3* rhs) {
    Vec3 ret;
    ret.x = lhs->x - rhs->x;
    ret.y = lhs->y - rhs->y;
    ret.z = lhs->z - rhs->z;
    return ret;
}

Vec3 vec3_scale(const Vec3* lhs, float x) {
    Vec3 ret;
    ret.x = lhs->x * x;
    ret.y = lhs->y * x;
    ret.z = lhs->z * x;
    return ret;
}

Vec3 vec3_cross(const Vec3* lhs, const Vec3* rhs) {
    Vec3 ret;
    ret.x = (lhs->y * rhs->z) - (lhs->z * rhs->y);
    ret.y = (lhs->z * rhs->x) - (lhs->x * rhs->z);
    ret.z = (lhs->x * rhs->y) - (lhs->y * rhs->x);
    return ret;
}

float vec3_length(const Vec3* v) {
    return sqrtf((v->x * v->x) + (v->y * v->y) + (v->z * v->z));
}

Vec3 vec3_normalize(const Vec3* v) {
    Vec3 ret;
    float l = 1.0f / vec3_length(v);
    ret.x = v->x * l;
    ret.y = v->y * l;
    ret.z = v->z * l;
    return ret;
}

void render_helicopter(Vec3 position, Vec3 direction) {
    int i = 0;
    Vec3 up = vec3_init(0, 1, 0);
    Vec3 r = vec3_cross(&direction, &up);
    Vec3 two_dir = vec3_scale(&direction, 2.0f);

    Vec3 front = vec3_add(&position, &direction);
    Vec3 back = vec3_sub(&position, &two_dir);
    Vec3 top = vec3_add(&position, &up);
    Vec3 bottom = vec3_sub(&position, &up);
    Vec3 right = vec3_add(&position, &r);
    Vec3 left = vec3_sub(&position, &r);

    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_TRIANGLES);
        glVertex3fv((float*) &front);
        glVertex3fv((float*) &left);
        glVertex3fv((float*) &top);

        glVertex3fv((float*) &front);
        glVertex3fv((float*) &bottom);
        glVertex3fv((float*) &left);

        glVertex3fv((float*) &front);
        glVertex3fv((float*) &top);
        glVertex3fv((float*) &right);

        glVertex3fv((float*) &front);
        glVertex3fv((float*) &right);
        glVertex3fv((float*) &bottom);

        glVertex3fv((float*) &top);
        glVertex3fv((float*) &right);
        glVertex3fv((float*) &back);

        glVertex3fv((float*) &right);
        glVertex3fv((float*) &bottom);
        glVertex3fv((float*) &back);

        glVertex3fv((float*) &top);
        glVertex3fv((float*) &back);
        glVertex3fv((float*) &left);

        glVertex3fv((float*) &left);
        glVertex3fv((float*) &back);
        glVertex3fv((float*) &bottom);
    glEnd();


    const float R = 2.0f;
    const float TWOPI = 3.14159f * 2.0f;

    glColor3f(0.7, 0.7, 0.7);
    glBegin(GL_TRIANGLE_FAN);
    for(i = 0; i < 20; ++i) {
        float x = R * sin(i * TWOPI / 18);
        float z = R * cos(i * TWOPI / 18);
        glVertex3f(position.x + x, top.y + 0.5, position.z + z);
    }
    glEnd();
}

ALCdevice* device = NULL;
ALCcontext* context = NULL;
ALuint source = 0;
ALuint buffer = 0;

/* A general OpenGL initialization function.  Sets all of the initial parameters. */
void InitGL(int Width, int Height)	        // We call this right after our OpenGL window is created.
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);		// This Will Clear The Background Color To Black
    glClearDepth(1.0);				// Enables Clearing Of The Depth Buffer
    glDepthFunc(GL_LEQUAL);				// The Type Of Depth Test To Do
    glEnable(GL_DEPTH_TEST);			// Enables Depth Testing
    glShadeModel(GL_SMOOTH);			// Enables Smooth Color Shading

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();				// Reset The Projection Matrix

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);	// Calculate The Aspect Ratio Of The Window

    glMatrixMode(GL_MODELVIEW);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);

    const ALCchar* defaultDeviceName = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    device = alcOpenDevice(defaultDeviceName);
    assert(device);

    context = alcCreateContext(device, NULL);
    assert(context);
    alcMakeContextCurrent(context);

    ALvoid *data;
	ALsizei size, freq;
	ALenum format;

    alGetError();

    alutInit(0, NULL);
    alutLoadWAVFile((ALbyte*) "/rd/test.wav", &format, &data, &size, &freq);
    assert(alutGetError() == ALUT_ERROR_NO_ERROR);

    alGenSources((ALuint)1, &source);
    alSourcei(source, AL_LOOPING, AL_TRUE);

    alGenBuffers(1, &buffer);
    alBufferData(buffer, format, data, size, freq);
    alSourcei(source, AL_BUFFER, buffer);
    alSourcePlay(source);

    ALfloat listenerOri[] = { 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };
    alListenerfv(AL_ORIENTATION, listenerOri);

    alSourcef(source, AL_PITCH, 1);
	alSourcef(source, AL_GAIN, 1);
}

/* The function called when our window is resized (which shouldn't happen, because we're fullscreen) */
void ReSizeGLScene(int Width, int Height)
{
    if (Height == 0)				// Prevent A Divide By Zero If The Window Is Too Small
        Height = 1;

    glViewport(0, 0, Width, Height);		// Reset The Current Viewport And Perspective Transformation

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(45.0f,(GLfloat)Width/(GLfloat)Height,0.1f,100.0f);
    glMatrixMode(GL_MODELVIEW);
}


/* The main drawing function. */
void DrawGLScene()
{
    static float t = 0.0f;
    const float r = 10.0f;

    t += 0.01f;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);		// Clear The Screen And The Depth Buffer
    glLoadIdentity();				// Reset The View

    Vec3 pos = vec3_init(sin(t) * r, 0, cos(t) * r);
    Vec3 target = vec3_init(sin(t + 0.1) * r, 0, cos(t + 0.1) * r);

    Vec3 dir = vec3_sub(&target, &pos);
    vec3_normalize(&dir);

    alSource3f(source, AL_POSITION, pos.x, pos.y, pos.z);
	alSource3f(source, AL_VELOCITY, dir.x, dir.y, dir.z);

    render_helicopter(pos, dir);

    glKosSwapBuffers();
}

int main(int argc, char **argv)
{
    glKosInit();

    InitGL(640, 480);
    ReSizeGLScene(640, 480);

    while(1) {
        DrawGLScene();
    }

    return 0;
}
