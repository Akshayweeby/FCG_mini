#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>

// ---------- track geometry ----------
// The circuit is a rectangular ring of tarmac: outer edge at |x|<=OX, |z|<=OZ,
// infield (grass) at |x|<IX, |z|<IZ. Road centreline is at |x|=10, |z|=8.
const float OX=12.f, OZ=10.f;   // outer tarmac edge
const float IX=8.f,  IZ=6.f;    // inner tarmac edge
const float CAR_R=0.75f;        // how close the car's centre may get to the edge

// ---------- state ----------
// Car heading: forward = (sin(yaw), 0, cos(yaw)). Increasing yaw turns the car LEFT.
struct Car {
    glm::vec3 p{-2.f,0.15f,8.f};
    float yaw=1.5708f;          // facing +X, along the bottom straight
    float speed=0;
    float steer=0;              // front-wheel steering angle (+ = left)
} car;
bool keys[1024]{};
double lastX=0, lastY=0;
bool captured=false;            // mouse is captured (moves orbit the camera)
bool topDown=false;
float camOrbit=0.f;             // camera angle around the car (0 = directly behind)
float camPitch=0.45f;           // camera elevation angle
GLuint prog;
glm::mat4 baseM(1.0f);          // extra transform applied to every cube (the car uses it)

const char* vs=R"( #version 330 core
layout(location=0) in vec3 p; layout(location=1) in vec3 n; layout(location=2) in vec2 uv;
uniform mat4 M,V,P; uniform mat3 NM; out vec3 N;
void main(){N=NM*n; gl_Position=P*V*M*vec4(p,1);})";
const char* fs=R"( #version 330 core
in vec3 N; uniform vec3 color; uniform int unlit; out vec4 o;
void main(){
    float lam = max(dot(normalize(N), normalize(vec3(.35,1.,.25))), 0.);
    float l = unlit==1 ? 1.0 : (.45 + .55*lam);     // simple sun + ambient
    o = vec4(color*l, 1.);
})";

GLuint shader(GLenum type,const char* src){GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);return s;}

// ---------- unit cube mesh (built once, reused for every box) ----------
struct Mesh { GLuint vao=0, vbo=0, ebo=0; };

Mesh buildCubeMesh(){
    // Each face: 4 vertices, each vertex = position(3) + normal(3) + uv(2) -> stride 8 floats.
    std::vector<float> v; std::vector<unsigned> idx;
    const float corner[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
    const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    for(int f=0;f<6;f++){
        int a=f/2, u=(a+1)%3, w=(a+2)%3;
        float sg=(f%2)?1.f:-1.f;
        for(int k=0;k<4;k++){
            float p[3]={0,0,0}, n[3]={0,0,0};
            p[a]=sg; p[u]=corner[k][0]; p[w]=corner[k][1];
            n[a]=sg;
            v.insert(v.end(), {p[0],p[1],p[2], n[0],n[1],n[2], uv[k][0],uv[k][1]});
        }
        unsigned b=f*4;
        idx.insert(idx.end(), {b,b+1,b+2, b+2,b+3,b});
    }
    Mesh m;
    glGenVertexArrays(1,&m.vao); glGenBuffers(1,&m.vbo); glGenBuffers(1,&m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*sizeof(unsigned),idx.data(),GL_STATIC_DRAW);
    const GLsizei stride=8*sizeof(float);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0);                 glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return m;
}

const Mesh& cubeMesh(){ static Mesh m = buildCubeMesh(); return m; }

// Draws a box centred at p with half-extents s, in the space given by baseM.
// unlit = true skips lighting (used for glowing / self-lit parts such as lamps and exhaust).
void cube(glm::vec3 p,glm::vec3 s,glm::vec3 c,bool unlit=false){
    const Mesh& m=cubeMesh();
    glm::mat4 M=baseM*glm::translate(glm::mat4(1),p)*glm::scale(glm::mat4(1),s);
    glUniformMatrix4fv(glGetUniformLocation(prog,"M"),1,GL_FALSE,glm::value_ptr(M));
    glUniformMatrix3fv(glGetUniformLocation(prog,"NM"),1,GL_FALSE,glm::value_ptr(glm::transpose(glm::inverse(glm::mat3(M)))));
    glUniform3fv(glGetUniformLocation(prog,"color"),1,glm::value_ptr(c));
    glUniform1i(glGetUniformLocation(prog,"unlit"),unlit?1:0);
    glBindVertexArray(m.vao);
    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0);
    glBindVertexArray(0);
}

// ---------- environment helpers ----------
void tree(float x,float z,float s=1.f){
    cube({x,.5f*s,z},{.08f*s,.5f*s,.08f*s},{.35f,.22f,.12f});                 // trunk
    cube({x,1.3f*s,z},{.55f*s,.45f*s,.55f*s},{.1f,.36f,.12f});                // lower crown
    cube({x,1.9f*s,z},{.38f*s,.35f*s,.38f*s},{.16f,.46f,.18f});               // upper crown
}

void lamp(float x,float z){
    const glm::vec3 grey{.55f,.56f,.6f};
    float side = x>0 ? 1.f : -1.f;
    cube({x,1.2f,z},{.05f,1.2f,.05f},grey);                                  // pole
    cube({x-.25f*side,2.4f,z},{.3f,.04f,.04f},grey);                         // arm
    cube({x-.5f*side,2.36f,z},{.16f,.05f,.1f},{1.f,.95f,.7f},true);          // glowing head
}

void cloud(float x,float y,float z){
    const glm::vec3 w{.97f,.97f,.99f};
    cube({x,y,z},{2.f,.5f,1.2f},w,true);
    cube({x+1.2f,y+.35f,z},{1.2f,.4f,.9f},w,true);
    cube({x-1.3f,y+.2f,z},{1.f,.35f,.8f},w,true);
}

// Block of flats with a grid of windows on the face that looks at the road.
void building(float x,float z,float hw,float h,float hd,glm::vec3 col,float facing){
    cube({x,h,z},{hw,h,hd},col);
    const glm::vec3 glass{.14f,.2f,.3f}, lit{.95f,.85f,.5f};
    int cols=std::max(1,(int)(hw*2.f/1.2f));
    for(float y=.7f; y<2*h-.4f; y+=1.1f){
        for(int c=0;c<cols;c++){
            float wx=x+((float)c-(cols-1)/2.f)*1.1f;
            bool isLit=((int)(y*10)+c*7)%5==0;   // a few lit windows for variety
            cube({wx,y,z+facing*(hd+.01f)},{.25f,.22f,.02f},isLit?lit:glass,isLit);
        }
    }
    cube({x,2*h+.05f,z},{hw+.08f,.05f,hd+.08f},{.3f,.3f,.32f});              // roof parapet
}

void grandstand(){
    const glm::vec3 steel{.5f,.52f,.56f}, roof{.18f,.18f,.22f};
    cube({0,1.4f,-17.5f},{7.f,1.4f,1.4f},steel);                             // seating block
    for(int r=0;r<3;r++)
        cube({0,1.1f+r*.4f,-16.6f+r*.45f},{6.8f,.12f,.35f},{.8f,.25f,.25f});  // seat rows
    for(float x=-6.5f;x<=6.6f;x+=2.2f)
        cube({x,1.5f,-16.2f},{.08f,1.5f,.08f},roof);                         // roof struts
    cube({0,3.0f,-18.4f},{7.4f,.08f,2.0f},roof);                             // roof
}

void kerbs(){
    // Alternating red and white kerb blocks sitting on the inner and outer tarmac edges.
    const glm::vec3 red{.85f,.1f,.1f}, white{.95f,.95f,.95f};
    for(int i=-10;i<10;i++){
        glm::vec3 col = (i%2) ? red : white;
        float x=i+.5f;
        cube({x,.1f,-6.15f},{.5f,.04f,.12f},col);   cube({x,.1f,-9.85f},{.5f,.04f,.12f},col);
        cube({x,.1f, 6.15f},{.5f,.04f,.12f},col);   cube({x,.1f, 9.85f},{.5f,.04f,.12f},col);
    }
    for(int i=-7;i<7;i++){
        glm::vec3 col = (i%2) ? red : white;
        float z=i+.5f;
        cube({-8.15f,.1f,z},{.12f,.04f,.5f},col);   cube({-11.85f,.1f,z},{.12f,.04f,.5f},col);
        cube({ 8.15f,.1f,z},{.12f,.04f,.5f},col);   cube({ 11.85f,.1f,z},{.12f,.04f,.5f},col);
    }
}

void drawTrack(){
    const glm::vec3 grass{.16f,.35f,.12f}, tar{.08f,.08f,.085f}, paint{.9f,.9f,.7f};
    cube({0,-.4f,0},{22,.2f,18},grass);                                      // ground
    // black tarmac ring: top and bottom straights span the full width, sides fill between
    cube({0,.05f,-8},{12,.05f,2},tar);     cube({0,.05f,8},{12,.05f,2},tar);
    cube({-10,.05f,0},{2,.05f,8},tar);     cube({10,.05f,0},{2,.05f,8},tar);
    // white dashed lane markings down the middle of each straight
    for(int x=-8;x<=8;x+=4){cube({(float)x,.12f,-8},{.1f,.03f,.6f},paint);cube({(float)x,.12f,8},{.1f,.03f,.6f},paint);}
    for(int z=-6;z<=6;z+=4){cube({-10,.12f,(float)z},{.6f,.03f,.1f},paint);cube({10,.12f,(float)z},{.6f,.03f,.1f},paint);}
    kerbs();
    // buildings with windows, outside the track
    for(int x=-8;x<=8;x+=4){
        building((float)x,-12,1.5f,1.2f,1.5f,{.45f,.32f,.2f},+1.f);
        building((float)x, 12,1.5f,1.0f,1.5f,{.5f,.38f,.24f},-1.f);
    }
    // trees around the outside of the loop
    const float trees[][2]={{-18,-3},{-17,5},{18,-5},{17,4},{-15,-17},{15,-16},{-19,15},{19,14},{0,-20},{-6,20},{7,20},{-21,-9},{21,-9}};
    for(size_t i=0;i<sizeof(trees)/sizeof(trees[0]);i++) tree(trees[i][0],trees[i][1], 0.9f+0.15f*(i%3));
    for(float z=-6;z<=6;z+=6){ lamp(-13,z); lamp(13,z); }
    grandstand();
    cloud(-20,13,-30); cloud(12,15,-35); cloud(-5,12,28); cloud(22,14,20); cloud(0,16,-45);
}

// F1-style car built from boxes. Local space: nose points along +Z, which matches
// the heading vector (sin yaw, 0, cos yaw), so the car always drives the way it looks.
void drawCar(){
    const glm::mat4 carM = glm::translate(glm::mat4(1), car.p) * glm::rotate(glm::mat4(1), car.yaw, glm::vec3(0,1,0));
    baseM = carM;
    const glm::vec3 red{.85f,.05f,.06f}, black{.06f,.06f,.07f}, dark{.12f,.12f,.13f},
                    tyre{.08f,.08f,.09f}, white{.95f,.95f,.93f}, silver{.72f,.74f,.78f},
                    helmet{.92f,.92f,.95f}, visor{.05f,.12f,.2f}, exhaust{1.f,.5f,.1f};

    // monocoque, nose and front
    cube({0,.30f,0},       {.26f,.12f,.62f}, red);
    cube({0,.28f,.92f},    {.13f,.08f,.34f}, red);
    cube({0,.22f,1.28f},   {.07f,.05f,.22f}, red);
    cube({0,.24f,1.1f},    {.1f,.03f,.1f},   white);                          // nose stripe

    // sidepods with air inlets and white stripe
    for(float sx : {-1.f, 1.f}){
        cube({sx*.3f,.27f,-.1f},  {.1f,.1f,.42f}, red);
        cube({sx*.3f,.38f,-.1f},  {.1f,.012f,.42f}, white);
        cube({sx*.4f,.28f,.22f},  {.02f,.06f,.12f}, black);                    // inlet
    }

    // cockpit, helmet, halo
    cube({0,.42f,-.12f},   {.2f,.08f,.24f}, black);
    cube({0,.5f,-.12f},    {.11f,.1f,.11f}, helmet);                           // driver helmet
    cube({0,.5f,.0f},      {.1f,.06f,.03f}, visor);                            // visor
    cube({0,.6f,.02f},     {.14f,.015f,.02f}, black);                          // halo top
    for(float sx : {-1.f, 1.f}) cube({sx*.14f,.5f,.02f},{.015f,.1f,.015f}, black); // halo posts

    // engine cover, shark fin airbox, number board
    cube({0,.5f,-.5f},     {.17f,.13f,.27f}, red);
    cube({0,.7f,-.36f},    {.03f,.1f,.12f},  black);
    cube({0,.635f,-.5f},   {.06f,.01f,.06f}, white);

    // mirrors
    for(float sx : {-1.f, 1.f}){
        cube({sx*.2f,.44f,.32f},  {.02f,.02f,.1f}, dark);
        cube({sx*.26f,.46f,.4f},  {.05f,.03f,.03f}, black);
    }

    // front wing: main plane, flaps, endplates
    cube({0,.11f,1.3f},    {.66f,.025f,.13f}, black);
    cube({0,.15f,1.22f},   {.56f,.02f,.06f}, red);
    for(float sx : {-1.f, 1.f}){
        cube({sx*.66f,.14f,1.3f},  {.02f,.12f,.17f}, red);
        cube({sx*.66f,.14f,1.3f},  {.025f,.02f,.18f}, white);
    }

    // rear wing: main plane, flap, endplates
    cube({0,.7f,-1.22f},   {.5f,.03f,.13f}, black);
    cube({0,.64f,-1.2f},   {.5f,.02f,.1f},  red);
    for(float sx : {-1.f, 1.f}) cube({sx*.5f,.6f,-1.2f}, {.02f,.16f,.16f}, red);

    // floor, diffuser, exhaust glow
    cube({0,.1f,-1.0f},    {.3f,.05f,.2f},  black);
    cube({0,.1f,-1.25f},   {.22f,.03f,.05f}, dark);
    cube({0,.42f,-.9f},    {.04f,.03f,.05f}, exhaust, true);

    // wheels and suspension. Front wheels turn with the steering angle.
    const float wh=.2f;
    for(float sx : {-1.f, 1.f}){
        // suspension arms (fixed to the body)
        cube({sx*.3f,.2f,.82f},   {.2f,.02f,.02f}, silver);
        cube({sx*.3f,.2f,-.78f},  {.2f,.02f,.02f}, silver);

        // front wheel: pivots about its own centre by the steering angle
        baseM = carM * glm::translate(glm::mat4(1), {sx*.48f,wh,.82f}) * glm::rotate(glm::mat4(1), car.steer, glm::vec3(0,1,0));
        cube({0,0,0},          {.13f,wh,.2f}, tyre);
        cube({sx*.13f,0,0},    {.02f,.13f,.13f}, silver);
        cube({sx*.15f,0,0},    {.01f,.04f,.04f}, red);

        // rear wheel (wider, larger), fixed
        baseM = carM * glm::translate(glm::mat4(1), {sx*.5f,wh+.02f,-.78f});
        cube({0,0,0},          {.15f,.22f,.22f}, tyre);
        cube({sx*.14f,0,0},    {.02f,.14f,.14f}, silver);
        cube({sx*.16f,0,0},    {.01f,.04f,.04f}, red);
    }
    baseM = glm::mat4(1.0f);
}

// ---------- track collision ----------
// True if the car centre can be at p: on the tarmac ring, keeping CAR_R clear of both edges.
bool onTrack(glm::vec3 p){
    float x=std::abs(p.x), z=std::abs(p.z);
    if(x > OX-CAR_R || z > OZ-CAR_R) return false;        // outside the outer kerb
    if(x < IX+CAR_R && z < IZ+CAR_R) return false;        // inside the infield
    return true;
}

// ---------- input ----------
void setCapture(GLFWwindow* w, bool on){
    captured = on;
    glfwSetInputMode(w, GLFW_CURSOR, on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if(on) glfwGetCursorPos(w,&lastX,&lastY);   // avoid a jump when capture starts
}

void input(float dt){
    // W forward, S backward/brake. Speed drag makes the car coast to a stop.
    float gas   = keys[GLFW_KEY_W] ? 1.f : 0.f;
    float brake = keys[GLFW_KEY_S] ? 1.f : 0.f;
    car.speed += gas*20.f - brake*14.f;
    car.speed *= std::pow(.18f, dt);
    car.speed  = glm::clamp(car.speed, -6.f, 18.f);

    // A = steer left, D = steer right. The front wheels turn smoothly to the target angle.
    float target = ((keys[GLFW_KEY_A] ? 1.f : 0.f) - (keys[GLFW_KEY_D] ? 1.f : 0.f)) * 0.5f;
    car.steer += (target - car.steer) * std::min(1.f, dt*8.f);

    // Bicycle-model turning: yaw rate depends on speed and steering angle; reversing flips it.
    car.yaw += (car.speed/4.f) * std::tan(car.steer) * dt;

    // Move one axis at a time so the car slides along the kerbs instead of getting stuck.
    glm::vec3 fwd(std::sin(car.yaw), 0, std::cos(car.yaw));
    float dist = car.speed * dt;
    bool hit = false;
    glm::vec3 t = car.p; t.x += fwd.x*dist;
    if(onTrack(t)) car.p.x = t.x; else hit = true;
    t = car.p; t.z += fwd.z*dist;
    if(onTrack(t)) car.p.z = t.z; else hit = true;
    if(hit) car.speed *= 0.5f;                       // kerb contact scrubs speed
}

void key(GLFWwindow* w,int k,int,int a,int){
    if(a==GLFW_PRESS){
        if(k==GLFW_KEY_C) topDown=!topDown;
        if(k==GLFW_KEY_R){ car=Car{}; camOrbit=0.f; camPitch=0.45f; }
        if(k==GLFW_KEY_ESCAPE) setCapture(w,false);    // Esc frees the mouse
    }
    if(k>=0 && k<1024) keys[k] = a!=GLFW_RELEASE;
}

// Mouse movement orbits the camera around the car (it does not steer the car).
void mouse(GLFWwindow*,double x,double y){
    if(!captured){ lastX=x; lastY=y; return; }
    camOrbit += (float)(x-lastX) * 0.004f;
    camPitch  = glm::clamp(camPitch + (float)(y-lastY) * 0.003f, 0.05f, 1.3f);
    lastX=x; lastY=y;
}

void button(GLFWwindow* w,int b,int a,int){
    if(b==GLFW_MOUSE_BUTTON_LEFT && a==GLFW_PRESS && !captured) setCapture(w,true);  // click to capture
}

int main(){
    if(!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    GLFWwindow* w=glfwCreateWindow(1280,720,"F1 MINI TRACK - Akshayanagara Lake",0,0);
    if(!w){ glfwTerminate(); return 1; }
    glfwMakeContextCurrent(w);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);
    glfwSetKeyCallback(w,key);
    glfwSetCursorPosCallback(w,mouse);
    glfwSetMouseButtonCallback(w,button);
    if(glfwRawMouseMotionSupported()) glfwSetInputMode(w,GLFW_RAW_MOUSE_MOTION,GLFW_TRUE);

    prog=glCreateProgram();
    glAttachShader(prog,shader(GL_VERTEX_SHADER,vs));
    glAttachShader(prog,shader(GL_FRAGMENT_SHADER,fs));
    glLinkProgram(prog);

    setCapture(w,true);
    double t=glfwGetTime();
    while(!glfwWindowShouldClose(w)){
        double now=glfwGetTime();
        float dt=std::min((float)(now-t), 0.05f);   // clamp so a stall can't launch the car
        t=now;
        if(!glfwGetWindowAttrib(w,GLFW_FOCUSED)) std::fill(std::begin(keys),std::end(keys),false);

        input(dt);

        int W,H;
        glfwGetFramebufferSize(w,&W,&H);
        H=std::max(H,1);
        glViewport(0,0,W,H);
        glClearColor(.45f,.68f,.85f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glUseProgram(prog);

        glm::mat4 V,P;
        if(topDown){
            // Looking straight down: up vector must not be parallel to the view direction.
            V=glm::lookAt(glm::vec3(0,25,0),glm::vec3(0,0,0),glm::vec3(0,0,-1));
            P=glm::ortho(-22.f,22.f,-14.f,14.f,.1f,60.f);
        }else{
            // Chase camera orbiting the car: camOrbit is measured from "directly behind".
            float a = car.yaw + camOrbit;
            glm::vec3 back(-std::sin(a), 0, -std::cos(a));
            const float dist=7.5f;
            glm::vec3 target = car.p + glm::vec3(0,0.6f,0);
            glm::vec3 eye = target + (back*std::cos(camPitch) + glm::vec3(0,std::sin(camPitch),0))*dist;
            V=glm::lookAt(eye,target,glm::vec3(0,1,0));
            P=glm::perspective(glm::radians(55.f),(float)W/H,.1f,100.f);
        }
        glUniformMatrix4fv(glGetUniformLocation(prog,"V"),1,GL_FALSE,glm::value_ptr(V));
        glUniformMatrix4fv(glGetUniformLocation(prog,"P"),1,GL_FALSE,glm::value_ptr(P));

        drawTrack();
        drawCar();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
