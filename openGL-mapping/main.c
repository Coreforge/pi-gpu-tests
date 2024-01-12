#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>

#include "arm64-asmtests.h"

#define ALIGN_MEMCPY 0
#define ALIGN_MEMCPY_SIZE 4
#define MEMCPY_64BIT 1

#define READ_TEST 1

#if MEMCPY_64BIT==1
#define MEMCPY_GROUP_SIZE 8
#else
#define MEMCPY_GROUP_SIZE 4
#endif

const char* vtx_Shader = 
"#version 330\n"
"layout (location = 0) in vec3 pos;\n"
"layout (location = 1) in vec2 tex;\n"
"out vec2 texcoords;\n"
"void main(){\n"
"gl_Position = vec4(pos,1.0f);\n"
"texcoords = tex;\n"
"}";

const char* frg_Shader = 
"#version 330\n"
"out vec4 colour;\n"
"in vec2 texcoords;\n"
"uniform sampler2D text;\n"
"void main(){\n"
"vec4 col = texture(text,texcoords);\n"
"colour = vec4(0.1f,col.g,col.b,1.0f);\n"
"}";

void* this_memcpy(void* dst, const void* src, size_t n){
	volatile char* vc_src = (char*)src;
	volatile char* vc_dst = (char*)dst;
    printf("Copying 0x%llx bytes from 0x%llx to 0x%llx\n",n, src, dst);

	// copy byte by byte, hopefully avoiding any alignment issues
	size_t pos = 0;
	/*while(pos < n){
		if((uint64_t)(pos+vc_src) % 4 != 0 || n-pos <= 4){ // not aligned to 4 bytes, or less than 4 bytes left
			*(vc_dst+pos) = *(vc_src+pos);	// copy single byte
			pos++;
		}
		if((uint64_t)(pos+vc_src) % 4 == 0 && n-pos > 4){// aligned and more 4 or more bytes left
			*((volatile uint32_t*)(vc_dst+pos)) = *((volatile uint32_t*)(vc_src+pos));	// copy 4 bytes
			pos += 4;
		}
	}*/
    while(pos < n){
#if ALIGN_MEMCPY == 1
        if((uint64_t)(dst+pos) % 4 != 0 || (uint64_t)(src+pos) % 4 != 0){
            // one of the addresses isn't aligned
            *(vc_dst+pos) = *(vc_src+pos);
            pos++;
        }
        if((uint64_t)(dst+pos) % 4 == 0 && (uint64_t)(src+pos) % 4 == 0 && n-pos >= MEMCPY_GROUP_SIZE)
            // both are aligned
#else
        if(n-pos >= MEMCPY_GROUP_SIZE)
        // we don't care about alignment
#endif
        {
            // copy in large chunks
#if MEMCPY_64BIT==1
            *(uint64_t*)(dst + pos) = *(uint64_t*)(src + pos);
#else       
            *(uint32_t*)(dst + pos) = *(uint32_t*)(src + pos);
#endif
            //*(ptr_dst+pos) = *(ptr_src+pos);
            pos += MEMCPY_GROUP_SIZE;
        }

        if(n-pos < MEMCPY_GROUP_SIZE && n-pos != 0){
            *(vc_dst+pos) = *(vc_src+pos);
            pos++;
        }
    }
	/*for(size_t i = 0; i < n; i++){
		*(vc_dst+i) = *(vc_src+i);
	}*/
	return dst;
}

GLuint compileShader(const char* src,GLuint type){
    GLuint shader;
    GLuint i = glGetError();
    shader = glCreateShader(type);
    i = glGetError();
    glShaderSource(shader,1,&src,NULL);
    i = glGetError();
    glCompileShader(shader);
    i = glGetError();
    int success;
    glGetShaderiv(shader,GL_COMPILE_STATUS,&success);
    i = glGetError();
    printf("Creating shader\n");
    if(!success){
        GLuint len;
        i = glGetError();
        glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&len);
        i = glGetError();
        char* buf = (char*)malloc(len+1);
        glGetShaderInfoLog(shader,len,NULL,buf);
        i = glGetError();
        printf("Shader compiling failed:\n%s\n",buf);
        free(buf);
        return 0;
    }
    return shader;
}

GLuint compileProgram(const char* vtx, const char* frg){
    GLuint vertex = compileShader(vtx,GL_VERTEX_SHADER);
    GLuint fragment = compileShader(frg,GL_FRAGMENT_SHADER);
    if(vertex == 0 || fragment == 0){
        // one of the shaders failed to compile
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program,vertex);
    glAttachShader(program,fragment);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program,GL_LINK_STATUS,&success);
    if(!success){
        GLuint len;
        glGetProgramiv(program,GL_INFO_LOG_LENGTH,&len);
        char* buf = (char*)malloc(len+1);
        glGetProgramInfoLog(program,len,NULL,buf);
        printf("Shader linking failed:\n%s\n",buf);
        free(buf);
        return 0;
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    printf("Linked Program %d\n",program);
    return program;
}

int main(){

    // GLFW setup
    GLFWwindow* window;

    if(!glfwInit()){
        printf("GLFW Init Error\n");
        return -1;
    }
    
    glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_API);
    //glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(1280,720,"Mapping test",NULL,NULL);
    if(!window){
        glfwTerminate();
        printf("Could not create OpenGL Context\n");
        return -1;
    }

    glfwMakeContextCurrent(window);

    if(glewInit() != GLEW_OK){
        printf("GLEW Init Error\n");
        return -1;
    }

    printf("%s\n",glGetString(GL_VERSION));
    printf("%s\n",glGetString(GL_RENDERER));

    // other GL setup

    // VAO setup
    GLuint baseVAO;
    glGenVertexArrays(1,&baseVAO);
    glBindVertexArray(baseVAO);
    

    // just used to create a rectangle over the whole viewport
    float verts[] = {-1.0f,1.0f,0.0f, 0.0f,1.0f, 
                     1.0f,1.0f,0.0f,  1.0f,1.0f,
                     -1.0f,-1.0f,0.0f,0.0f,0.0f,
                     1.0f,-1.0f,0.0f, 1.0f,0.0f,  };
    GLuint VBO;
    GLuint i = glGetError();
    glGenBuffers(1,&VBO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,20,(void*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,20,(void*)12);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    //glBindVertexArray(0);
    i = glGetError();


    GLuint prog = compileProgram(vtx_Shader,frg_Shader);
    if(prog == 0){
        return -1;
    }
    i = glGetError();
    GLuint texBuffer;
    glGenBuffers(1,&texBuffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,texBuffer);
    //glBufferStorage(GL_TEXTURE_BUFFER,1280*720*4,NULL,GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    
    GLuint tex;
    glGenTextures(1,&tex);
    glBindTexture(GL_TEXTURE_2D,tex);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);

    void* tmp = malloc(1280*720*4);
    memset(tmp,128,1280*720*2);
    //glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1280,720,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    i = glGetError();
    printf("err: %d\n",i);
    //glBindTexture(GL_TEXTURE_BUFFER,tex);
    i = glGetError();
    printf("err: %d\n",i);
    glBufferData(GL_PIXEL_UNPACK_BUFFER,1280*720*4,NULL,GL_STATIC_DRAW);
    glBufferStorage(GL_PIXEL_UNPACK_BUFFER,1280*720*4,NULL,GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT);
    //glTexBuffer(GL_PIXEL_UNPACK_BUFFER,GL_RGBA,texBuffer);
    i = glGetError();
    printf("err: %d\n",i);
    void* buf = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER,0,1280*720*4, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);//| GL_MAP_FLUSH_EXPLICIT_BIT);
    i = glGetError();
    printf("err: %d\n",i);


    runAsmTests(buf + 512);
    printf("\n\nSecond run: \n");

    runAsmTests(buf + 256);



    // this is the critical part. This can be done a number of ways to mess different things up
    // offset by 1 to ruin alignment. Don't want to go too easy in the pi
    this_memcpy(buf+1,tmp+1,1280*720*4-1);

    

    glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER,0,1280*720*4);

    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

#if READ_TEST==1
    // read back the buffer into the tmp buffer
    printf("Mapping buffer for reading\n");
    buf = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER,0,1280*720*4, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    i = glGetError();
    glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
    i = glGetError();
    printf("Buffer address: %lx\n", buf);
    this_memcpy(tmp+1,buf+1,1280*720*4-1);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    printf("Read back data, writing it again\n");
    // and now write it back again. If we got here, this worked before, so nothing should go wrong. 
    buf = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER,0,1280*720*4, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    this_memcpy(buf+1,tmp+1,1280*720*4-1);
    glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER,0,1280*720*4);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
#endif

    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1280,720,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);

    free(tmp);

    while(!glfwWindowShouldClose(window)){

        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(baseVAO);
        i = glGetError();
        glUseProgram(prog);
        i = glGetError();
        glActiveTexture(GL_TEXTURE0);
        i = glGetError();
        glBindTexture(GL_TEXTURE_2D,tex);
        i = glGetError();
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        i = glGetError();
        //glBindVertexArray(0);
        // GLFW main loop stuff
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}