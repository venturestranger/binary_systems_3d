#include <GL/glut.h>
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_glut.h"
#include "../imgui/imgui_impl_opengl2.h"
#include <GL/freeglut.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <cmath>
#include <chrono>
#include <exception>

using namespace std;
using namespace chrono;

/* Constants */
const int FPS = 60;
const int FRAME_TIME = 1000 / FPS;
const float PI = 3.1416;
const float INF = 1e10;
const float G = 6.67e-11;
const float c = 299792458;
const float EPS = 1e-10;
const float SCALE_TIME = 86400;
const int ORBIT_SEGS = 100;
const int ORBIT_WIDTH = 4;

/* GLUT callback handlers */
static void resize (ImGuiIO& io)
{
    glViewport(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)io.DisplaySize.x/(GLfloat)io.DisplaySize.y, 1, 1500.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* Global variables */
ofstream file_handler;
bool to_print = false;
char to_file[256] = "dump.txt";
float scale_grid = 1.5;
float scale_body = 5e8;
float dt = 1;
float e, W, old_e, old_a;
float tg = 0, t = 0, T = 0, dT = 0, da = 0;
float old_T = 0;
float w = 1;
float m1_ = 1.441, m2_ = 1.387, a_ = 1.95, e_ = 0.617, dist_ = 100, mts_ = 1;
bool motion_control = true;

float adequate (float x) {
	// Caution! Application's not responding may occur here if 
	// (tg) has been decreasing during very long time.
	// It depends also on maximum (dt) value, so be careful!
	
	for (int i = 20; i > 5; i--)
		while (x > 8 * pow(10, i))
			x -= PI * 2 * pow(10, i);
	
	for (int i = 20; i > 5; i--)
		while (x < -8 * pow(10, i))
			x += PI * 2 * pow(10, i);

	return x;
}

// Update grid destortion
void tgUpdate () {
	tg += 2 * PI / (T / 2) * dt;
	tg = adequate(tg);
	tg -= 2 * PI * int(tg / PI / 2);
}

// Update global time
void tUpdate () {
    t += dt;
}

// Change period
void TUpdate () {
    old_T = T;
    T = 2 * PI / w;
}

class Spector {
private:
    float step, delt_a, delt_b;

	// Position
    float px, py, pz; 
	// Direction
    float dx, dy, dz;
	// Inclination: alpha and beta angles
    float an_a, an_b;

public:
    Spector (
            float px_ = 0, float py_ = 0, float pz_ = 0,
            float step_ = 1, float delt_a_ = 0.1, float delt_b_ = 0.1,
            float dx_ = 0, float dy_ = 0, float dz_ = 0,
            float an_a_ = 0, float an_b_ = 0
            ) {
                px = px_, py = py_, pz = pz_;
                dx = dx_, dy = dy_, dz = dz_;
                an_a = an_a_, an_b = an_b_;
                step = step_;
                delt_a = delt_a_, delt_b = delt_b_;
            }

	// Rotate a camera in space
    void rotate () {
        dx = sin(an_a);
        dz = cos(an_a);
        dy = sin(an_b);
    }

	// Reset a camera in space
    void set () {
        rotate();
        gluLookAt(px, py, pz,
                  px + dx, py + dy, pz + dz,
                  0, 1, 0);
    }

	// Move forward or backward
    void movFor (int swtch) {
        px += sin(an_a) * (swtch == 1 ? cos(an_b) : 1) * step * swtch;
        pz += cos(an_a) * (swtch == 1 ? cos(an_b) : 1) * step * swtch;
        py += (swtch == 1 ? sin(an_b) : 0) * step * swtch;

        if (px > INF) px = INF;
        if (py > INF) py = INF;
        if (pz > INF) pz = INF;

        if (px < -INF) px = -INF;
        if (py < -INF) py = -INF;
        if (pz < -INF) pz = -INF;
    }

	// Move left or right
    void movAside (int swtch) {
        px += sin(an_a + PI/2 * swtch) * step;
        pz += cos(an_a + PI/2 * swtch) * step;

        if (px > INF) px = INF;
        if (pz > INF) pz = INF;

        if (px < -INF) px = -INF;
        if (pz < -INF) pz = -INF;
    }

	// Move up or down
    void movUpDown (int swtch) {
        py += step * swtch;
        if (py > INF) py = INF;
        if (py < -INF) py = -INF;
    }

	// Change alpha angle inclination
    void angleA (float delta) {
        an_a += delta;
        if (an_a > PI * 2) an_a -= PI * 2;
        else if (an_a < -PI * 2) an_a += PI * 2;
    }

	// Change beta angle inclination
    void angleB (float delta) {
        an_b += delta;
        if (an_b > PI/2) an_b = PI/2;
        else if (an_b < -PI/2) an_b = -PI/2;
    }
};

Spector cam = Spector(0, 0, 0, 1, 0.013, 0.013);

class Body {
public:
    float ax, ay;
    ImVec4 color;
    int slices = 16;
    int stacks = 16;
    float a, phi, old_phi;
    float x, y, z;
    float m, r;
    bool to_draw = true;

    Body (
        float phi_ = PI, float a_ = 1,
        float m_ = 1, float r_ = 1,
        float x_ = 0, float y_ = 0, float z_ = 0,
        float cr_ = 1, float cg_ = 0, float cb_ = 0,
        float ax_ = 0, float ay_ = 0
          ) {
                old_phi = 0;
                phi = phi_, a = a_;
                x = x_, y = y_, z = z_;
                m = m_, r = r_;
                color = ImVec4(cr_, cg_, cb_, 1);
                ax = ax_, ay = ay_;
            }

	// Reset an object in space 
    void set () {
        if (to_draw == false) return;

        glColor3d(color.x, color.y, color.z);
        glPushMatrix();
            glTranslated(x, y, z);
            glRotated(60,1,0,0);
            glutSolidSphere(r, slices, stacks);
        glPopMatrix();
    }
};
Body one = Body(PI, 0, 1, 3, -10, 0, -10, 1, 1, 1);
Body two = Body(-PI, 0, 1, 3, 10, 0, 10, 0.15, 0.8, 0.7);

/* Environment variables update */
void envUpdate (float m1, float m2) {
	float remnant = (1 + 73 / 24 * pow(old_e, 2) + 37 / 96 * pow(old_e, 4));

	W = 32 / 5 * pow(G, 4) * pow(m1, 2) * pow(m2, 2) * (m1 + m2) / 
		(pow(c, 5) * pow(old_a, 5) * pow(1 - pow(old_e, 2), 3.5)) * 
		remnant;
	
	e += dt * (-304 * pow(G, 3) * m1 * m2 * (m1 + m2) * old_e) /
            (15 * pow(c, 5) * pow(old_a, 4) * pow((1 - pow(old_e, 2)), 2.5)) *
            (1 + 121 / 304 * pow(old_e, 2));
			
    one.a += dt * (-64 * pow(G, 3) * m1 * m2 * (m1 + m2)) /
            (5 * pow(c, 5) * pow(old_a, 3) * pow((1 - pow(old_e, 2)), 3.5)) *
            remnant;

	dT = 96 / 5 * pow((2 * PI), 2.666) * m1 / pow(c, 5) * m2 / pow((m1 + m2), 0.333) * 
			pow(G, 1.666) * 3.16 * 1e13 / pow(old_T, 1.666) / pow((1 - pow(old_e, 2)), 3.5) * 
			remnant;

	da = (64 * pow(G, 3) * m1 * m2 * (m1 + m2)) / 
		(5 * pow(c, 5) * pow(old_a, 3) * pow((1 - pow(old_e, 2)), 3.5)) * 
		remnant * 3.16 * 1e7;
	
    two.a = one.m * one.a / two.m;
    old_a = one.a;
    old_e = e;

	// Dump data
	if (to_print == true and to_file != "") {
		if (file_handler.is_open() == false) 
			file_handler.open("dumps/" + string(to_file));

		file_handler << "A " << (one.a + two.a > 0 ? one.a + two.a : 0) << endl;
		file_handler << "E " << (e > 0 ? e : 0) << endl;
		file_handler << "W " << (W > 0 ? W : 0) << endl;
		file_handler << "T " << (T > 0 ? T : 0) << endl;
		file_handler << "Time " << t << endl;
	} else file_handler.close();
}

/* Initialize the environment */
void envInit () {
    t = 0, tg = 0;

    e = e_;
    one.phi = PI;
    two.phi = -PI;
    one.m = m1_ * 2e30;
    two.m = m2_ * 2e30;
    one.a = (m1_ != 0 and m2_ != 0 ? a_ * 1e9 : 0);
	
	two.a = one.m * one.a / two.m;

    old_a = one.a;
    old_e = e_;
    scale_body = (max(one.a, two.a) * 2) / dist_;

    one.r = (one.m / ((one.m + two.m) / 2) - 1) * 3 + 3.5;
    two.r = (two.m / ((one.m + two.m) / 2) - 1) * 3 + 3.5;
}

/* Move objects after precalculations */
void move () {
    w = sqrt(G * (one.m + two.m) / pow(one.a, 3));

	// Move the first object
    one.phi = one.phi - w * dt * pow((1 + old_e * cos(one.phi)), 2) / pow((1 - pow(old_e, 2)), 1.5);
	one.phi = adequate(one.phi);
    one.phi -= 2 * PI * int(one.phi / PI / 2);

    float shift = one.a * (1 - pow(old_e, 2)) / (1 + old_e * cos(one.phi));

    one.x = 0 + shift * cos(one.phi) / scale_body;
    one.z = 0 - shift * sin(one.phi) / scale_body;

	// Move the second object
    two.phi = two.phi + w * dt * pow((1 + old_e * cos(two.phi)), 2) / pow((1 - pow(old_e, 2)), 1.5);
	two.phi = adequate(two.phi);
	two.phi -= 2 * PI * int(two.phi / PI / 2);
	
    shift = two.a * (1 - pow(old_e, 2)) / (1 + old_e * cos(two.phi));

    two.x = 0 - shift * cos(two.phi) / scale_body;
    two.z = 0 - shift * sin(two.phi) / scale_body;

    envUpdate(one.m, two.m);
}

class Grid {
public:
    float x, z, y;
    float dx, dz;
    ImVec4 color;
    float segs;
    bool to_draw = true;

    Grid (
          float x_ = -100, float y_ = 0, float z_ = -100,
          float dx_ = 100, float dz_ = 100,
          float segs_ = 70,
          float cr_ = 1, float cg_ = 1, float cb_ = 1
          ) {
                color = ImVec4(cr_, cg_, cb_, 1);
                x = x_, y = y_, z = z_;
                dx = dx_, dz = dz_;
                segs = segs_;
            }

	// Render grid  
    void set () {
        if (to_draw == false) return;

        glColor3d(color.x, color.y, color.z);

        float step = (dx - x) / segs;

        for (float xl = x; xl < dx; xl += step)
            for (float zl = z; zl < dz; zl += step) {
                glBegin(GL_LINES);
                    glVertex3f(xl, scale_grid * sin(tg + sqrt(pow(xl,2) + pow(zl + step,2))), zl + step);
                    glVertex3f(xl, scale_grid * sin(tg + sqrt(pow(xl,2) + pow(zl,2))), zl);
                glEnd();

                glBegin(GL_LINES);
                    glVertex3f(xl, scale_grid * sin(tg + sqrt(pow(xl,2) + pow(zl,2))), zl);
                    glVertex3f(xl + step, scale_grid * sin(tg + sqrt(pow(xl + step,2) + pow(zl,2))), zl);
                glEnd();

                glBegin(GL_LINES);
                    glVertex3f(xl + step, scale_grid * sin(tg + sqrt(pow(xl + step,2) + pow(zl,2))), zl);
                    glVertex3f(xl + step, scale_grid * sin(tg + sqrt(pow(xl + step,2) + pow(zl + step,2))), zl + step);
                glEnd();

                glBegin(GL_LINES);
                    glVertex3f(xl + step, scale_grid * sin(tg + sqrt(pow(xl + step,2) + pow(zl + step,2))), zl + step);
                    glVertex3f(xl, scale_grid * sin(tg + sqrt(pow(xl,2) + pow(zl + step,2))), zl + step);
                glEnd();
            }
    }
} grd = Grid();

class Orbit {
public:
    Body *binding;

    Orbit (Body *binding_ = nullptr) {
        binding = binding_;
    }

	// Render an orbit
    void set (int side) {
        glLineWidth(ORBIT_WIDTH);
        glColor3d(binding->color.x, binding->color.y, binding->color.z);

        glBegin(GL_LINE_STRIP);
        float x, y;
        float   x_radius = binding->a / scale_body,
                y_radius = binding->a / scale_body * sqrt(1 - pow(e, 2));

        for(float angle = 0.0f; angle <= (2 * PI) + 0.1; angle += (2 * PI / ORBIT_SEGS)) {
            x = cos(angle) * x_radius + binding->a / scale_body * e * side;
            y = sin(angle) * y_radius;
            glVertex3f(x, 0, y);
        }
        glEnd();
    }
};
Orbit orb_one = Orbit(&one);
Orbit orb_two = Orbit(&two);

/* Controller interfaces */
static void keyboardDown (ImGuiIO& io)
{
    if (io.KeysDown['a'] == true)
        cam.movAside(1);
    if (io.KeysDown['d'] == true)
        cam.movAside(-1);
    if (io.KeysDown['w'] == true)
        cam.movFor(1);
    if (io.KeysDown['s'] == true)
        cam.movFor(-1);
    if (io.KeysDown[32] == true) 
        cam.movUpDown(1);
    if (io.KeysDown['z'] == true)
        cam.movUpDown(-1);

    glutPostRedisplay();
}

static void mouseDown (ImGuiIO& io)
{
    if (io.MouseDown[1] == true) motion_control = true;
    else motion_control = false;
}

static void mouseMotion (ImGuiIO& io) {
    float mx = io.MousePos.x, my = io.MousePos.y;
    if (motion_control == true
            and my < io.DisplaySize.y
            and my > 0
            and mx < io.DisplaySize.x
            and mx > 0
        ) {
        cam.angleB(-asin((my - io.DisplaySize.y / 2) / (io.DisplaySize.y / 2)) * 0.1);
        cam.angleA(-asin((mx - io.DisplaySize.x / 2) / (io.DisplaySize.x / 2)) * 0.1);
        motion_control = false;
    }
}

/* IMGUI display */
void imguiDisplay (ImGuiIO& io)
{
    float width = io.DisplaySize.x / 3;

	// First tile window
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::Begin("Environment");
	ImGui::SetWindowSize(ImVec2((float)width, (float)550));

	ImGui::Text("");
	ImGui::Text("Grid polygon size:");
	ImGui::SliderFloat("units", &grd.segs, 20.0f, 100.0f);
	ImGui::Text("Grid scale:");
	ImGui::SliderFloat("times", &scale_grid, 0.6f, 3.0f);
	ImGui::Checkbox("Grid is visible:", &grd.to_draw);

	ImGui::Text("");
	ImGui::Text("Time step:");
	ImGui::InputFloat("days", &mts_);
	if (mts_ > 1e14) mts_ = 1e14;
	if (mts_ < -1e14) mts_ = -1e14;

	ImGui::Text("");
	ImGui::Text("First object:");
	ImGui::ColorEdit3("color", (float*)&one.color);
	ImGui::Checkbox("First object is visible:", &one.to_draw);

	ImGui::Text("");
	ImGui::Text("Second object:");
	ImGui::ColorEdit3("color ", (float*)&two.color);
	ImGui::Checkbox("Second object is visible:", &two.to_draw);

	ImGui::Text("");
	ImGui::Text("File to save:");
	ImGui::InputText("", to_file, 256);
	ImGui::Checkbox("To save", &to_print);
	ImGui::Text("");

	ImGui::Text("Application average %.1f FPS", ImGui::GetIO().Framerate);
	if (dt * ImGui::GetIO().Framerate / 86400 >= 1)
		ImGui::Text("Step: %.0f days per computed second", dt / 86400 * ImGui::GetIO().Framerate); 
	else if (dt * ImGui::GetIO().Framerate / 3600 >= 1)
		ImGui::Text("Step: %.0f hours per computed second", dt / 3600 * ImGui::GetIO().Framerate); 
	else
		ImGui::Text("Step: %.0f minutes per computed second", dt / 60 * ImGui::GetIO().Framerate);	

	ImGui::Text("");
	ImGui::Text("Copyright 2022-2023 Bogdan Yakupov\nAll rights reserved");	
	ImGui::End();

	// Second tile window
	ImGui::SetNextWindowPos(ImVec2(width - 1, 0));
	ImGui::Begin("Log out");
	ImGui::SetWindowSize(ImVec2((float)width, (float)155));

	if (old_a / 1000000000 >= 1)
		ImGui::Text("Semi-major axis: %.2f mln km", old_a / 1000000000);
	else
		ImGui::Text("Semi-major axis: %.0f km", old_a / 1000);
	
	ImGui::Text("Eccentricity: %.3f", old_e);

	ImGui::Text("Time: %.0f years", t / 31556952); 
	
	if (T / 86400 >= 1)
		ImGui::Text("Period: %.2f days", T / 86400);
	else if (T / 3600 >= 1)
		ImGui::Text("Period: %.2f hours", T / 3600);
	else
		ImGui::Text("Period: %.2f minutes", T / 60);

	float pw = 1;
	while (W / pw > 1e3) pw *= 1e3;
	if (pw != 1) 
		ImGui::Text("Power: %.1fe%.0f W", W / pw, log10(pw));
	else
		ImGui::Text("Power: %.3f W", W);
	
	ImGui::Text("Period decreasing rate: %.2f mcs per year", dT);
	ImGui::Text("Orbit decreasing rate: %.2f m per year", da);

	ImGui::End();

	// Third tile window
	ImGui::SetNextWindowPos(ImVec2(width*2 - 1, 0));
	ImGui::Begin("Settings");
	ImGui::SetWindowSize(ImVec2((float)width, (float)260));

	ImGui::Text("Mass of the first body:");
	ImGui::InputFloat("solar masses", &m1_);

	ImGui::Text("Mass of the second body:");
	ImGui::InputFloat("solar masses  ", &m2_);

	ImGui::Text("Semi-major axis:");
	ImGui::InputFloat("mln km", &a_);

	ImGui::Text("Eccentricity:");
	ImGui::SliderFloat("units ", &e_, 0.0f, 0.93f);

	ImGui::Text("Area:");
	ImGui::SliderFloat("units  ", &dist_, 1.0f, 110.0f);

	if (ImGui::Button("Reset"))
			envInit();

	ImGui::End();
}

/* Make all the variables null */
void makeNull () {
    old_a = 0.0f;
    old_e = 0.0f;
    T = 0.0f;
	one.x = one.z = 0;
	one.m = two.m = 0;
	two.x = two.z = 0;

	file_handler.close();
}

/* Display call */
static void display (void)
{
    auto initial_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGLUT_NewFrame();

	/*
	HKL english;
	english = LoadKeyboardLayout("00000409", 0);
	ActivateKeyboardLayout(english, KLF_REORDER);
	*/
	
    ImGuiIO& io = ImGui::GetIO();
    imguiDisplay(io);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glLineWidth(1);

    ImGui::Render();
    keyboardDown(io);
    mouseDown(io);
    mouseMotion(io);
    resize(io);

	dt = mts_ / ImGui::GetIO().Framerate * SCALE_TIME;
    if (old_a > 0) {
        tUpdate();
        move();
		TUpdate();
		tgUpdate();
    } else makeNull();

    cam.set();
    one.set();
    two.set();
    grd.set();
    if (one.a > 0) orb_one.set(-1);
    if (one.a > 0) orb_two.set(1);

    auto final_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    while (final_time - initial_time < FRAME_TIME)
        final_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    glutSwapBuffers();
}

/* GLUT environment */
const GLfloat light_ambient[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 0.0f, 0.0f, 0.0f, 0.0f };

const GLfloat mat_ambient[] = { 0.7f, 0.7f, 0.7f, 1.0f };
const GLfloat mat_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

static void idle (void)
{
    glutPostRedisplay();
}

/* Program entry point */
int main (int argc, char *argv[])
{
	
    // GLUT initialization
	glutInit(&argc, argv);
	glutInitWindowSize(1600, 860);
	glutInitWindowPosition(10,10);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutCreateWindow("Two Body System");

	glutDisplayFunc(display);
	glutIdleFunc(idle);

	glClearColor(0,0,0,0);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);

	glLightfv(GL_LIGHT0, GL_AMBIENT,  light_ambient);
	glMaterialfv(GL_FRONT, GL_AMBIENT,   mat_ambient);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGLUT_Init();
    ImGui_ImplGLUT_InstallFuncs();
    ImGui_ImplOpenGL2_Init();

    glutMainLoop();

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGLUT_Shutdown();
    ImGui::DestroyContext();

    return EXIT_SUCCESS;
}
