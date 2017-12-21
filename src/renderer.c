#include <renderer.h>

#include <algebra.h>
#include <stb_image.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// private definitions

struct model {
	GLuint prog;
	GLuint vao;
	GLuint texture;
	GLenum primitive;
	GLint n_elem;
};

struct camera {
	GLfloat x;
	GLfloat z;
	GLfloat pitch;
	GLfloat yaw;
};

struct _renderer {
	EGLDisplay dpy;
	EGLContext ctx;
	EGLSurface surf;

	GLuint prog[3];
	struct model model[3];
	struct camera camera;
};

const char *GetError(EGLint error_code);
GLuint CreateProgram(renderer *state, const char *name);
struct model MakeSphere(float r, int stacks, int slices);
struct model MakeTerrain(GLfloat l, GLuint N);
struct model MakeAxes(GLfloat l);
struct model MakeCube(GLfloat l);
int DrawModel(renderer *state, struct model *model, GLfloat view[9]);
char *GetShaderSource(const char *src_file);

// public definitions

renderer *renderer_create(struct gbm_device *gbm, struct gbm_surface *surf) {
	renderer *state = malloc(sizeof(renderer));
	state->camera.x = 0.0f;
	state->camera.z = 0.0f;
	state->camera.pitch = 0.0f;
	state->camera.yaw = 3*M_PI/4;

	state->dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, gbm, NULL);
	state->ctx = EGL_NO_CONTEXT;
	if (state->dpy == EGL_NO_DISPLAY) {
		fprintf(stderr, "eglGetPlatformDisplay failed\n");
	}
	printf("\neglGetPlatformDisplay successful\n");

	EGLint major, minor;
	if (eglInitialize(state->dpy, &major, &minor) == EGL_FALSE) {
		fprintf(stderr, "eglInitialize failed\n");
	}
	printf("eglInitialize successful (EGL %i.%i)\n", major, minor);
	
	if (eglBindAPI(EGL_OPENGL_ES_API == EGL_FALSE)) {
		fprintf(stderr, "eglBindAPI failed\n");
	}
	printf("eglBindAPI successful\n");

// 	`size` is the upper value of the possible values of `matching`
	const int size = 1;
	int matching;
	const EGLint attrib_required[] = {
		EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_NATIVE_RENDERABLE, EGL_TRUE,
		EGL_NATIVE_VISUAL_ID, GBM_FORMAT_XRGB8888,
	//	EGL_NATIVE_VISUAL_TYPE, ? 
	EGL_NONE};

	EGLConfig *config = malloc(size*sizeof(EGLConfig));
	eglChooseConfig(state->dpy, attrib_required, config, size, &matching);
	printf("EGLConfig matching: %i (requested: %i)\n", matching, size);
	
	const EGLint attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
	
	state->ctx = eglCreateContext(state->dpy, *config, EGL_NO_CONTEXT, attribs);
	if (state->ctx == EGL_NO_CONTEXT) {
		fprintf(stderr, "eglGetCreateContext failed\n");
	}
	printf("eglCreateContext successful\n");
	state->surf = eglCreatePlatformWindowSurface(state->dpy, *config, surf, NULL);
	if (state->surf == EGL_NO_SURFACE) {
		fprintf(stderr, "eglCreatePlatformWindowSurface failed\n");
	}
	printf("eglCreatePlatformWindowSurface successful\n");

	if (eglMakeCurrent(state->dpy, state->surf, state->surf, state->ctx) == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent failed\n");
	}
	printf("eglMakeCurrent successful (context binding)\n");
	
//	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	state->prog[0] = CreateProgram(state, "basic");
	state->prog[1] = CreateProgram(state, "lightning");
	state->prog[2] = CreateProgram(state, "texture_lightning");

	state->model[0] = MakeAxes(1.0f);
	state->model[0].prog = state->prog[0];
	state->model[1] = MakeTerrain(1.0f, 10);
	state->model[1].prog = state->prog[2];

	glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	return state;
}

void DecreaseAngle(GLfloat *angle)
{
	*angle -= M_PI/128;
	if (*angle < -M_PI)
		*angle += 2*M_PI;
}

void IncreaseAngle(GLfloat *angle)
{
	*angle += M_PI/128;
	if (*angle > M_PI)
		*angle -= 2*M_PI;
}

int renderer_render(renderer* state, input* input_state) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	if (input_get_keystate_down(input_state))
		DecreaseAngle(&state->camera.pitch);
	if (input_get_keystate_up(input_state))
		IncreaseAngle(&state->camera.pitch);
	if (input_get_keystate_left(input_state))
		DecreaseAngle(&state->camera.yaw);
	if (input_get_keystate_right(input_state))
		IncreaseAngle(&state->camera.yaw);

	state->camera.yaw -= 0.001*input_get_dx(input_state);
	state->camera.pitch -= 0.001*input_get_dy(input_state);
	input_reset_dxdy(input_state);

	GLfloat rotx[16], roty[16], trasl[16], view[16], persp[16], projection[16];
	algebra_matrix_rotation_x(rotx, state->camera.pitch);
	algebra_matrix_rotation_y(roty, state->camera.yaw);
	algebra_matrix_traslation(trasl, -state->camera.x, -2.0f,
	-state->camera.z);
	GLfloat temp[16];
	algebra_matrix_multiply(temp, rotx, roty);
	GLfloat temp2[16];
	algebra_matrix_multiply(temp2, trasl, temp);
	algebra_matrix_persp(persp, M_PI/4, 1.7786f, 0.1f, 100.0f);
	algebra_matrix_multiply(projection, persp, temp2);
	
	for (int i=0; i<2; i++)
		DrawModel(state, &state->model[i], projection);
	
	if (eglSwapBuffers(state->dpy, state->surf) == EGL_FALSE) {
		fprintf(stderr, "eglSwapBuffers failed\n");
	}
	return 0;
}

int renderer_destroy(renderer *state) {
	if (eglMakeCurrent(state->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent failed (context unbinding)\n");
	}
	printf("\neglMakeCurrent successful (context unbinding)\n");

	if (eglDestroySurface(state->dpy, state->surf) == EGL_FALSE) {
		fprintf(stderr, "eglDestroySurface failed\n");
	}
	printf("eglDestroySurface successful\n");

	if (eglDestroyContext(state->dpy, state->ctx) == EGL_FALSE) {
		fprintf(stderr, "eglDestroyContext failed\n");
	}
	printf("eglDestroyContext successful\n");

	if (eglTerminate(state->dpy) == EGL_FALSE) {
		fprintf(stderr, "eglTerminate failed\n");
	}
	printf("eglTerminate successful\n");
	
	free(state);
	return 0;
}

GLuint CreateProgram(renderer *state, const char *name)
{
	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	char vert_path[64], frag_path[64];
	sprintf(vert_path, "shaders/%s.vert", name);
	sprintf(frag_path, "shaders/%s.frag", name);
	char *vert_src_handle = GetShaderSource(vert_path);
	char *frag_src_handle = GetShaderSource(frag_path);
	const char *vert_src = vert_src_handle;
	const char *frag_src = frag_src_handle;
	glShaderSource(vert, 1, &vert_src, NULL);
	glShaderSource(frag, 1, &frag_src, NULL);
	glCompileShader(vert);
	GLint success = 0;
	glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE) {
		GLint logSize = 0;
		glGetShaderiv(vert, GL_INFO_LOG_LENGTH, &logSize);
		char *errorLog = malloc(logSize*sizeof(char));
		glGetShaderInfoLog(vert, logSize, &logSize, errorLog);
		printf("%s\n", errorLog);
		free(errorLog);
	}
	glCompileShader(frag);
	glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE) {
		GLint logSize = 0;
		glGetShaderiv(frag, GL_INFO_LOG_LENGTH, &logSize);
		char *errorLog = malloc(logSize*sizeof(char));
		glGetShaderInfoLog(frag, logSize, &logSize, errorLog);
		printf("%s\n", errorLog);
		free(errorLog);
	}
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);
	glDeleteShader(vert);
	glDeleteShader(frag);
	free(vert_src_handle);
	free(frag_src_handle);
	return prog;
}

int DrawModel(renderer *state, struct model *model, GLfloat m[16])
{
	glUseProgram(model->prog);

	GLint matrix = glGetUniformLocation(model->prog, "matrix");
	glUniformMatrix4fv(matrix, 1, GL_TRUE, m);

	if (model->prog == state->prog[0]) {
	} else if (model->prog == state->prog[1]) {
		GLint objectColor = glGetUniformLocation(model->prog, "objectColor");
		glUniform3f(objectColor, 0.7f, 0.0f, 0.0f);
		GLint lightColor = glGetUniformLocation(model->prog, "lightColor");
		glUniform3f(lightColor, 1.0f, 1.0f, 1.0f);
		GLint lightPos = glGetUniformLocation(model->prog, "lightPos");
		glUniform3f(lightPos, 0.2f, 0.8f, 0.5f);
	}
	
	if (model->prog == state->prog[2])
		glBindTexture(GL_TEXTURE_2D, model->texture);

	glBindVertexArray(model->vao);
	glDrawElements(model->primitive, model->n_elem, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	return 0;
}

struct model MakeAxes(GLfloat l)
{
	struct model axes;
	glGenVertexArrays(1, &axes.vao);
	GLuint vbo, ebo;
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(axes.vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	GLfloat buffer[] = {
		0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f,
		+l/2, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f,

		0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, +l/2, 0.0f, 0.0f, 0.5f, 0.0f,

		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f,
		0.0f, 0.0f, +l/2, 0.0f, 0.0f, 0.5f
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(buffer), buffer, GL_STATIC_DRAW);
	GLuint indices[] = {0, 1, 2, 3, 4, 5};
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	axes.n_elem = 6;
	axes.primitive = GL_LINES;
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(GLfloat), 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(GLfloat),
	(void*)(3*sizeof(GLfloat)));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);

	return axes;
}

struct model MakeTerrain(GLfloat l, GLuint N)
{
	struct model terrain;
	glGenVertexArrays(1, &terrain.vao);
	GLuint vbo, ebo;
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(terrain.vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	GLuint size = 8*N*2*(N+1);
	GLfloat *buffer = malloc(size*sizeof(GLfloat));
	for (GLuint i=0; i<N; i++) {
		for (GLuint j=0; j<2*(N+1); j++) {
			GLuint a = 16*(N+1)*i+8*j;
			buffer[a] = j/2*l;
			buffer[a+1] = 0.0f;
			buffer[a+2] = i*l+j%2*l;
			buffer[a+3] = 0.0f;
			buffer[a+4] = 1.0f;
			buffer[a+5] = 0.0f;
			buffer[a+6] = j/2*1.0f;
			buffer[a+7] = j%2*1.0f;
		}
	}
	glBufferData(GL_ARRAY_BUFFER, size*sizeof(GLfloat), buffer, GL_STATIC_DRAW);
	free(buffer);
	GLuint size_i = 6*N*N;
	GLuint *indices = malloc(size_i*sizeof(GLuint));
	for (GLuint i=0; i<6*N*N; i+=3) {
		GLuint a = i/(6*N)*2+i/3;
		indices[i] = a;
		indices[i+1] = a+1;
		indices[i+2] = a+2;
	}
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size_i*sizeof(GLuint), indices, GL_STATIC_DRAW);
	free(indices);
	terrain.n_elem = size_i;
	terrain.primitive = GL_TRIANGLES;
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat), 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat),
	(void*)(3*sizeof(GLfloat)));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(GLfloat),
	(void*)(6*sizeof(GLfloat)));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);

	GLint width, height, nrChannels;
	GLubyte *data = stbi_load("textures/parquet-light.jpg", &width, &height,
	&nrChannels, 0);
	glGenTextures(1, &terrain.texture);
	glBindTexture(GL_TEXTURE_2D, terrain.texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
	GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(data);

	return terrain;
}

struct model MakeCube(GLfloat l)
{
	struct model cube;
	glGenVertexArrays(1, &cube.vao);
	GLuint vbo, ebo;
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(cube.vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	GLfloat buffer[] = {
		-l/2, +l/2, +l/2, -1.0, +0.0, +0.0,
		-l/2, +l/2, -l/2, -1.0, +0.0, +0.0,
		-l/2, -l/2, +l/2, -1.0, +0.0, +0.0,
		-l/2, -l/2, -l/2, -1.0, +0.0, +0.0,

		+l/2, +l/2, +l/2, +1.0, +0.0, +0.0,
		+l/2, +l/2, -l/2, +1.0, +0.0, +0.0,
		+l/2, -l/2, +l/2, +1.0, +0.0, +0.0,
		+l/2, -l/2, -l/2, +1.0, +0.0, +0.0,

		+l/2, -l/2, +l/2, +0.0, -1.0, +0.0,
		+l/2, -l/2, -l/2, +0.0, -1.0, +0.0,
		-l/2, -l/2, +l/2, +0.0, -1.0, +0.0,
		-l/2, -l/2, -l/2, +0.0, -1.0, +0.0,

		+l/2, +l/2, +l/2, +0.0, +1.0, +0.0,
		+l/2, +l/2, -l/2, +0.0, +1.0, +0.0,
		-l/2, +l/2, +l/2, +0.0, +1.0, +0.0,
		-l/2, +l/2, -l/2, +0.0, +1.0, +0.0,

		+l/2, +l/2, -l/2, +0.0, +0.0, -1.0,
		+l/2, -l/2, -l/2, +0.0, +0.0, -1.0,
		-l/2, +l/2, -l/2, +0.0, +0.0, -1.0,
		-l/2, -l/2, -l/2, +0.0, +0.0, -1.0,

		+l/2, +l/2, +l/2, +0.0, +0.0, +1.0,
		+l/2, -l/2, +l/2, +0.0, +0.0, +1.0,
		-l/2, +l/2, +l/2, +0.0, +0.0, +1.0,
		-l/2, -l/2, +l/2, +0.0, +0.0, +1.0,
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(buffer), buffer, GL_STATIC_DRAW);
	GLuint indices[] = {
		0, 1, 2, 1, 2, 3,
		4, 5, 6, 5, 6, 7,
		8, 9, 10, 9, 10, 11,
		12, 13, 14, 13, 14, 15,
		16, 17, 18, 17, 18, 19,
		20, 21, 22, 21, 22, 23
	};
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	cube.n_elem = 36;
	cube.primitive = GL_TRIANGLES;
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(GLfloat), 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(GLfloat),
	(void*)(3*sizeof(GLfloat)));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
	return cube;
}

struct model MakeSphere(float r, int stacks, int slices)
{
	struct model sphere;

	GLfloat *buffer = malloc(3*(2+slices*stacks)*sizeof(GLfloat));
	float *theta = malloc(stacks*sizeof(float));
	float *phi = malloc(slices*sizeof(float));
	for (int i=0; i<stacks; i++) {
		theta[i] = M_PI/(stacks+1)*(i+1);
	}
	for (int i=0; i<slices; i++) {
		phi[i] = 2*M_PI/slices*i;
	}
	buffer[0] = 0.0f, buffer[1] = 0.0f, buffer[2] = r;
	for (int i=0; i<stacks; i++)
		for (int j=0; j<slices; j++) {
			buffer[3*(slices*i+j)+3] = r*sin(theta[i])*cos(phi[j]);
			buffer[3*(slices*i+j)+4] = r*sin(theta[i])*sin(phi[j]);
			buffer[3*(slices*i+j)+5] = r*cos(theta[i]);
		}
	buffer[3+3*slices*stacks] = 0.0f, buffer[4+3*slices*stacks] = 0.0f,
	buffer[5+3*slices*stacks] = -r;
	
	sphere.n_elem = 6*stacks*slices;
	GLuint *indices = malloc(sphere.n_elem*sizeof(GLuint));
	for (int j=0; j<slices; j++) {
		indices[3*j] = 0;
		indices[3*j+1] = j+1;
		indices[3*j+2] = (j+1)%slices+1;
	}
	for (int i=0; i<stacks-1; i++)
		for (int j=0; j<slices; j++) {
			indices[6*(slices*i+j)+3*slices] = 1+slices*i+j;
			indices[6*(slices*i+j)+3*slices+1] = 1+slices*(i+1)+j;
			indices[6*(slices*i+j)+3*slices+2] =
			1+slices*(i+1)+(j+1)%slices;
			indices[6*(slices*i+j)+3*slices+3] = 1+slices*i+j;
			indices[6*(slices*i+j)+3*slices+4] =
			1+slices*(i+1)+(j+1)%slices;
			indices[6*(slices*i+j)+3*slices+5] =
			1+slices*i+(j+1)%slices;
		}
	for (int j=0; j<slices; j++) {
		indices[sphere.n_elem-3*slices+3*j] = stacks*slices+1-slices+j;
		indices[sphere.n_elem-3*slices+3*j+1] = stacks*slices+1;
		indices[sphere.n_elem-3*slices+3*j+2] = stacks*slices+1-slices+(j+1)%slices;
	}
	
	glGenVertexArrays(1, &sphere.vao);
	GLuint vbo, ebo;
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(sphere.vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ARRAY_BUFFER, 3*(2+slices*stacks)*sizeof(GLfloat), buffer, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphere.n_elem*sizeof(GLuint), indices,
	GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), 0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
	
	free(indices);
	free(buffer);
	free(phi);
	free(theta);

	return sphere;
}

char *GetShaderSource(const char *src_file)
{
	char *buffer = 0;
	long lenght;
	FILE *f = fopen(src_file, "rb");
	if (f == NULL) {
		fprintf(stderr, "fopen on %s failed\n", src_file);
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	lenght = ftell(f);
	fseek(f, 0, SEEK_SET);
	buffer = malloc(lenght);
	if (buffer == NULL) {
		fprintf(stderr, "malloc failed\n");
		fclose(f);
		return NULL;
	}

	fread(buffer, 1, lenght, f);
	fclose(f);
	buffer[lenght-1] = '\0';
	return buffer;
}



const char *GetError(EGLint error_code) {
	switch (error_code) {
	case EGL_SUCCESS:             return "EGL_SUCCESS";
	case EGL_NOT_INITIALIZED:     return "EGL_NOT_INITITALIZED";
	case EGL_BAD_ACCESS:          return "EGL_BAD_ACCESS";
	case EGL_BAD_ALLOC:           return "EGL_BAD_ALLOC";
	case EGL_BAD_ATTRIBUTE:       return "EGL_BAD_ATTRIBUTE";
	case EGL_BAD_CONTEXT:         return "EGL_BAD_CONTEXT";
	case EGL_BAD_CONFIG:          return "EGL_BAD_CONFIG";
	case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
	case EGL_BAD_DISPLAY:         return "EGL_BAD_DISPLAY";
	case EGL_BAD_SURFACE:         return "EGL_BAD_SURFACE";
	case EGL_BAD_MATCH:           return "EGL_BAD_MATCH";
	case EGL_BAD_PARAMETER:       return "EGL_BAD_PARAMETER";
	case EGL_BAD_NATIVE_PIXMAP:   return "EGL_BAD_NATIVE_PIXMAP";
	case EGL_BAD_NATIVE_WINDOW:   return "EGL_BAD_NATIVE_WINDOW";
	case EGL_CONTEXT_LOST:        return "EGL_CONTEXT_LOST";
	default:                      return "Unknown error";
	}
}
