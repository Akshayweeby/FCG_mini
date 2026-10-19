#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <vector>
#include <iostream>
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION

struct Car { glm::vec3 p{0,0.55f,7}; float yaw=0, speed=0, wheel=0; } car;
bool keys[1024]{}; double lastX=0,lastY=0; bool drag=false; float orbitYaw=0.8f, orbitPitch=0.55f, zoom=18;
GLuint prog, mapTex, carTex; const float PI=3.1415926f;
const char* vs=R"( #version 330 core
layout(location=0) in vec3 p; layout(location=1) in vec2 uv; uniform mat4 M,V,P; out vec2 t; void main(){t=uv;gl_Position=P*V*M*vec4(p,1);})";
const char* fs=R"( #version 330 core
in vec2 t; uniform sampler2D tex; uniform vec3 color; uniform int textured; out vec4 o; void main(){o=textured==1?texture(tex,t):vec4(color,1);})";
GLuint shader(GLenum type,const char* src){GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);return s;}
GLuint loadTex(const char* f){int w,h,n; unsigned char* d=stbi_load(f,&w,&h,&n,4); if(!d)return 0; GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,d);stbi_image_free(d);return t;}
void cube(glm::vec3 p,glm::vec3 s,glm::vec3 c,GLuint tex=0){float v[]={-1,-1,-1,0,0,1,-1,-1,1,0,0,1,1,-1,1,1,0,1,1,1,-1,1,1,1,-1,-1,1,0,0,1,1,-1,1,1,0,1,1,1,1,1,1,1,-1,1,-1,0,1,1}; unsigned i[]={0,1,2,2,3,0,4,5,6,6,7,4,0,4,7,7,3,0,1,5,6,6,2,1,3,2,6,6,7,3,0,1,5,5,4,0}; GLuint vao,b,e;glGenVertexArrays(1,&vao);glGenBuffers(1,&b);glGenBuffers(1,&e);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,b);glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,e);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(i),i,GL_STATIC_DRAW);glVertexAttribPointer(0,3,GL_FLOAT,0,5*sizeof(float),0);glEnableVertexAttribArray(0);glVertexAttribPointer(1,2,GL_FLOAT,0,5*sizeof(float),(void*)(3*sizeof(float)));glEnableVertexAttribArray(1);glm::mat4 m=glm::translate(glm::mat4(1),p)*glm::scale(glm::mat4(1),s);glUniformMatrix4fv(glGetUniformLocation(prog,"M"),1,0,glm::value_ptr(m));glUniform3fv(glGetUniformLocation(prog,"color"),1,glm::value_ptr(c));glUniform1i(glGetUniformLocation(prog,"textured"),tex?1:0);if(tex){glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex);glUniform1i(glGetUniformLocation(prog,"tex"),0);}glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0);glDeleteBuffers(1,&b);glDeleteBuffers(1,&e);glDeleteVertexArrays(1,&vao);}
void drawTrack(){ // traced lake loop: north bend, east side, south connector, west return
 cube({0,-.4f,0},{22,.2f,18},{.16f,.35f,.12f}); cube({0,-.05f,1},{8,.12f,5},{.04f,.18f,.28f},mapTex);
 cube({0,.02f,-8},{22,.08f,2},{.12f,.12f,.12f}); cube({0,.02f,8},{22,.08f,2},{.12f,.12f,.12f}); cube({-10,.02f,0},{2,.08f,8},{.12f,.12f,.12f}); cube({10,.02f,0},{2,.08f,8},{.12f,.12f,.12f});
 for(int x=-8;x<=8;x+=4){cube({(float)x,.12f,-8},{.1f,.03f,.6f},{.9f,.9f,.7f});cube({(float)x,.12f,8},{.1f,.03f,.6f},{.9f,.9f,.7f});}
 for(int z=-6;z<=6;z+=4){cube({-10,.12f,(float)z},{.6f,.03f,.1f},{.9f,.9f,.7f});cube({10,.12f,(float)z},{.6f,.03f,.1f},{.9f,.9f,.7f});}
 for(int x=-8;x<=8;x+=4){cube({(float)x,1.2f,-12},{1.5f,1.2f,1.5f},{.45f,.32f,.2f});cube({(float)x,1.0f,12},{1.5f,1.0f,1.5f},{.5f,.38f,.24f});}
}
void drawCar(){glm::mat4 m=glm::translate(glm::mat4(1),car.p)*glm::rotate(glm::mat4(1),car.yaw,glm::vec3(0,1,0));glUniformMatrix4fv(glGetUniformLocation(prog,"M"),1,0,glm::value_ptr(m));cube({0,0,0},{1.5f,.35f,.7f},{.65f,.03f,.04f},carTex);cube({.25f,.45f,0},{.65f,.25f,.5f},{.04f,.04f,.05f});cube({-1.35f,.15f,0},{.3f,.08f,1.0f},{.8f,.8f,.8f});cube({1.35f,.15f,0},{.3f,.08f,1.0f},{.8f,.8f,.8f});}
void input(GLFWwindow* w,float dt){float a=keys[GLFW_KEY_W]||keys[GLFW_KEY_UP], b=keys[GLFW_KEY_S]||keys[GLFW_KEY_DOWN];car.speed+=(a?8.f:0)-(b?11.f:0);car.speed*=std::pow(.18f,dt);car.speed=glm::clamp(car.speed,-5.f,18.f);float steer=(keys[GLFW_KEY_A]||keys[GLFW_KEY_LEFT])-(keys[GLFW_KEY_D]||keys[GLFW_KEY_RIGHT]);car.yaw+=steer*dt*(1.8f-std::min(std::abs(car.speed)/20.f,1.f));car.p+=glm::vec3(std::sin(car.yaw),0,std::cos(car.yaw))*car.speed*dt;car.p.x=glm::clamp(car.p.x,-9.f,9.f);car.p.z=glm::clamp(car.p.z,-7.f,7.f);}
void key(GLFWwindow*,int k,int, int a,int){if(k>=0&&k<1024)keys[k]=a!=GLFW_RELEASE;}
void mouse(GLFWwindow*,double x,double y){if(!drag){lastX=x;lastY=y;return;}orbitYaw+=(x-lastX)*.008f;orbitPitch=glm::clamp(orbitPitch+(float)(y-lastY)*.008f,.15f,1.35f);lastX=x;lastY=y;}
void button(GLFWwindow*,int b,int a,int){if(b==GLFW_MOUSE_BUTTON_LEFT)drag=a==GLFW_PRESS;}
int main(){if(!glfwInit())return 1;glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);GLFWwindow*w=glfwCreateWindow(1280,720,"Akshayanagara Lake F1 Simulator",0,0);glfwMakeContextCurrent(w);gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);glEnable(GL_DEPTH_TEST);glfwSetKeyCallback(w,key);glfwSetCursorPosCallback(w,mouse);glfwSetMouseButtonCallback(w,button);prog=glCreateProgram();glAttachShader(prog,shader(GL_VERTEX_SHADER,vs));glAttachShader(prog,shader(GL_FRAGMENT_SHADER,fs));glLinkProgram(prog);mapTex=loadTex("lake_map_reference.png");carTex=loadTex("car_reference.png");double t=glfwGetTime();while(!glfwWindowShouldClose(w)){float dt=(float)(glfwGetTime()-t);t=glfwGetTime();input(w,dt);int W,H;glfwGetFramebufferSize(w,&W,&H);glViewport(0,0,W,H);glClearColor(.45f,.68f,.85f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glUseProgram(prog);glm::vec3 eye=car.p+glm::vec3(std::sin(orbitYaw)*std::cos(orbitPitch),std::sin(orbitPitch),std::cos(orbitYaw)*std::cos(orbitPitch))*zoom;glm::mat4 V=glm::lookAt(eye,car.p,glm::vec3(0,1,0)),P=glm::perspective(glm::radians(55.f),(float)W/H,.1f,100.f);glUniformMatrix4fv(glGetUniformLocation(prog,"V"),1,0,glm::value_ptr(V));glUniformMatrix4fv(glGetUniformLocation(prog,"P"),1,0,glm::value_ptr(P));drawTrack();drawCar();glfwSwapBuffers(w);glfwPollEvents();}glfwTerminate();}
