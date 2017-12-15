#include <renderer.h>

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

struct _renderer {
	EGLDisplay dpy;
	EGLContext ctx;
	EGLSurface surf;
};

const char *get_error(EGLint error_code) {
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

const EGLint attrib_required[] = {
	EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
	EGL_NATIVE_RENDERABLE, EGL_TRUE,
	EGL_NATIVE_VISUAL_ID, GBM_FORMAT_XRGB8888,
//	EGL_NATIVE_VISUAL_TYPE, ? 
EGL_NONE};

struct model {
	GLuint vao;
	GLint n_elem;
} sphere, triangle;

struct model make_sphere(float r, int stacks, int slices)
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

GLuint prog;

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

void RotateX(GLfloat theta, GLfloat *matrix)
{
	matrix[0] = 1.0f, matrix[1] = 0.0f, matrix[2] = 0.0f;
	matrix[3] = 0.0f, matrix[4] = cos(theta), matrix[5] = -sin(theta);
	matrix[6] = 0.0f, matrix[7] = sin(theta), matrix[8] = cos(theta);
}

renderer *renderer_create(struct gbm_device *gbm, struct gbm_surface *surf) {
	renderer *state = malloc(sizeof(renderer));

	state->dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, gbm, NULL);
	state->ctx = EGL_NO_CONTEXT;
	if (state->dpy == EGL_NO_DISPLAY) {
		fprintf(stderr, "eglGetPlatformDisplay failed\n");
	}
	printf("\nEGL\neglGetPlatformDisplay successful\n");

	EGLint major, minor;
	if (eglInitialize(state->dpy, &major, &minor) == EGL_FALSE) {
		fprintf(stderr, "eglInitialize failed\n");
	}
	printf("eglInitialize successful (EGL %i.%i)\n", major, minor);
	
	if (eglBindAPI(EGL_OPENGL_ES_API == EGL_FALSE)) {
		fprintf(stderr, "eglBindAPI failed\n");
	}
	printf("eglBindAPI successful\n");

	// `size` is the upper value of the possible values of `matching`
	const int size = 1;
	int matching;
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
	printf("eglMakeCurrent successful\n");

/*	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	char *vert_src_handle = GetShaderSource("shaders/simple.vert");
	char *frag_src_handle = GetShaderSource("shaders/simple.frag");
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
	prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);
	glDeleteShader(vert);
	glDeleteShader(frag);
	free(vert_src_handle);
	free(frag_src_handle);
	glUseProgram(prog);

	
	sphere = make_sphere(0.5, 50, 50);
	
	glGenVertexArrays(1, &triangle.vao);
	GLuint vbo, ebo;
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(triangle.vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	GLfloat buffer[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, -0.5f, 0.5f,
	0.0f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f};
	glBufferData(GL_ARRAY_BUFFER, sizeof(buffer), buffer, GL_STATIC_DRAW);
	GLuint indices[] = {0, 1, 2, 2, 1, 3, 0, 1, 4, 0, 2 ,4, 2, 3, 4, 1, 3,
	4};
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	triangle.n_elem = 18;
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), 0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);*/

	glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	return state;
}

int renderer_render(renderer* state, int secs) {
	glClear(GL_COLOR_BUFFER_BIT);

/*	glUseProgram(prog);
	GLfloat rot[9];
	RotateX(M_PI/8*secs, rot);
	glUniformMatrix3fv(1, 1, GL_TRUE, rot);
	glBindVertexArray(sphere.vao);
	glDrawElements(GL_TRIANGLES, sphere.n_elem, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);*/
	
	if (eglSwapBuffers(state->dpy, state->surf) == EGL_FALSE) {
		fprintf(stderr, "eglSwapBuffers failed\n");
	}

	return 0;
}

int renderer_destroy(renderer *state) {
	if (eglMakeCurrent(state->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent failed\n");
	}
	printf("eglMakeCurrent successful\n");

	printf("surf: %i\n", eglDestroySurface(state->dpy, state->surf));

	if (eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent failed\n");
	}
	printf("eglMakeCurrent successful\n");

	printf("ctx: %i\n", eglDestroyContext(state->dpy, state->ctx));
	printf("dpy: %i\n", eglTerminate(state->dpy));

	free(state);
	return 0;
}
