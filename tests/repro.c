/*
 * Minimal reproducer for two D3DMetal problems seen through Mesa's d3d12 (OpenGL-on-D3D12) driver.
 *
 *   1. flat varyings + GL's default LAST provoking vertex: the draw produces nothing.
 *   2. glMultiDrawElementsIndirect with GL_PATCHES (tessellation) and 32+ commands: nothing is drawn.
 *
 * Each test clears to blue, draws, and reads back the centre pixel. "BLUE" means nothing was drawn.
 * Build: x86_64-w64-mingw32-gcc -O1 -o repro.exe repro.c -lopengl32 -lgdi32
 * Run it inside a CrossOver/Wine bottle next to Mesa's opengl32.dll + libgallium_wgl.dll (GALLIUM_DRIVER=d3d12).
 */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>

/* Pixel-format calls must go to opengl32 (Mesa); Wine's gdi32 versions check the host driver's formats. */
WINGDIAPI int WINAPI wglChoosePixelFormat(HDC, const PIXELFORMATDESCRIPTOR *);
WINGDIAPI BOOL WINAPI wglSetPixelFormat(HDC, int, const PIXELFORMATDESCRIPTOR *);

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#define GL_STATIC_DRAW 0x88E4
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_TESS_CONTROL_SHADER 0x8E88
#define GL_TESS_EVALUATION_SHADER 0x8E87
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_PATCHES 0x000E
#define GL_PATCH_VERTICES 0x8E72
#define GL_FIRST_VERTEX_CONVENTION 0x8E4D
#define GL_LAST_VERTEX_CONVENTION 0x8E4E

#define GLFUNCS(X) \
	X(GLuint, glCreateShader, (GLenum)) \
	X(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
	X(void, glCompileShader, (GLuint)) \
	X(void, glGetShaderiv, (GLuint, GLenum, GLint *)) \
	X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
	X(GLuint, glCreateProgram, (void)) \
	X(void, glAttachShader, (GLuint, GLuint)) \
	X(void, glLinkProgram, (GLuint)) \
	X(void, glGetProgramiv, (GLuint, GLenum, GLint *)) \
	X(void, glUseProgram, (GLuint)) \
	X(GLint, glGetAttribLocation, (GLuint, const GLchar *)) \
	X(void, glGenVertexArrays, (GLsizei, GLuint *)) \
	X(void, glBindVertexArray, (GLuint)) \
	X(void, glGenBuffers, (GLsizei, GLuint *)) \
	X(void, glBindBuffer, (GLenum, GLuint)) \
	X(void, glBufferData, (GLenum, GLsizeiptr, const void *, GLenum)) \
	X(void, glEnableVertexAttribArray, (GLuint)) \
	X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
	X(void, glVertexAttribDivisor, (GLuint, GLuint)) \
	X(void, glPatchParameteri, (GLenum, GLint)) \
	X(void, glProvokingVertex, (GLenum)) \
	X(void, glMultiDrawElementsIndirect, (GLenum, GLenum, const void *, GLsizei, GLsizei))

#define DECL(r, n, a) typedef r (APIENTRY *PFN_##n) a; static PFN_##n n##_;
GLFUNCS(DECL)
#define LOAD(r, n, a) n##_ = (PFN_##n)wglGetProcAddress(#n); if (!n##_) { printf("missing %s\n", #n); return 1; }

static GLuint program(const char **src, const GLenum *type, int n)
{
	GLuint p = glCreateProgram_(); GLint ok; char log[1024];
	for (int i = 0; i < n; i++) {
		GLuint s = glCreateShader_(type[i]);
		glShaderSource_(s, 1, &src[i], NULL); glCompileShader_(s);
		glGetShaderiv_(s, GL_COMPILE_STATUS, &ok);
		if (!ok) { glGetShaderInfoLog_(s, sizeof log, NULL, log); printf("compile error: %s\n", log); }
		glAttachShader_(p, s);
	}
	glLinkProgram_(p); glGetProgramiv_(p, GL_LINK_STATUS, &ok);
	if (!ok) printf("link error\n");
	glUseProgram_(p);
	return p;
}

static void report(const char *name, int x, int y)
{
	unsigned char px[4] = {0};
	glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
	int blue = px[0] == 0 && px[1] == 0 && px[2] == 255;
	printf("%-58s %3d,%3d,%3d  %s\n", name, px[0], px[1], px[2], blue ? "BLUE (nothing drawn)" : "drawn");
}

/* Test 1: a fullscreen quad whose colour comes from a flat varying. */
static const char *flat_vs[] = {
	/* 0: flat only */
	"#version 430 compatibility\nin vec2 pos; in float shadeIn; flat out float shade;\n"
	"void main(){ shade = shadeIn; gl_Position = vec4(pos, 0.0, 1.0); }\n",
	/* 1: smooth float next to a flat float */
	"#version 430 compatibility\nin vec2 pos; in float shadeIn; out float weight; flat out float shade;\n"
	"void main(){ weight = 1.0; shade = shadeIn; gl_Position = vec4(pos, 0.0, 1.0); }\n",
	/* 2: smooth vec3 + vec4 + float, then a flat float (several smooth values first) */
	"#version 430 compatibility\nin vec2 pos; in float shadeIn; out vec3 uvw; out vec4 tint; out float weight; flat out float shade;\n"
	"void main(){ uvw = vec3(0.5); tint = vec4(1.0); weight = 1.0; shade = shadeIn; gl_Position = vec4(pos, 0.0, 1.0); }\n",
};
static const char *flat_fs[] = {
	"#version 430 compatibility\nflat in float shade; layout(location = 0) out vec4 colour;\n"
	"void main(){ colour = vec4(shade, shade, 0.0, 1.0); }\n",
	"#version 430 compatibility\nin float weight; flat in float shade; layout(location = 0) out vec4 colour;\n"
	"void main(){ colour = vec4(shade * weight, shade, 0.0, 1.0); }\n",
	"#version 430 compatibility\nin vec3 uvw; in vec4 tint; in float weight; flat in float shade; layout(location = 0) out vec4 colour;\n"
	"void main(){ colour = vec4(shade * weight * tint.r, shade + uvw.z - 0.5, 0.0, 1.0); }\n",
};
static int flat_variant;
static void flat_test(const char *name, GLenum provoking)
{
	const char *src[2] = { flat_vs[flat_variant], flat_fs[flat_variant] };
	GLenum type[2] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
	GLuint p = program(src, type, 2);
	glProvokingVertex_(provoking);
	/* x, y, shade per vertex (the same shade on every vertex, so the provoking vertex doesn't change the result) */
	float quad[] = { -1,-1,0.75f, 1,-1,0.75f, 1,1,0.75f, -1,-1,0.75f, 1,1,0.75f, -1,1,0.75f };
	GLuint vao, vbo; glGenVertexArrays_(1, &vao); glBindVertexArray_(vao);
	glGenBuffers_(1, &vbo); glBindBuffer_(GL_ARRAY_BUFFER, vbo); glBufferData_(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
	GLint l = glGetAttribLocation_(p, "pos"); glEnableVertexAttribArray_(l); glVertexAttribPointer_(l, 2, GL_FLOAT, GL_FALSE, 12, NULL);
	GLint ls = glGetAttribLocation_(p, "shadeIn"); glEnableVertexAttribArray_(ls); glVertexAttribPointer_(ls, 1, GL_FLOAT, GL_FALSE, 12, (void *)8);
	glClearColor(0, 0, 1, 1); glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	report(name, 128, 128);
	glProvokingVertex_(GL_LAST_VERTEX_CONVENTION);
}

/* Test 2: one tessellated quad patch drawn through glMultiDrawElementsIndirect with `count` commands.
 * Only the first command draws anything; the rest have zero indices (like culled tiles). */
static void mdi_patch_test(int count)
{
	const char *src[4] = {
		"#version 430 compatibility\nin vec2 pos; out vec2 p;\nvoid main(){ p = pos; }\n",
		"#version 430 compatibility\nlayout(vertices = 4) out; in vec2 p[]; out vec2 q[];\n"
		"void main(){ q[gl_InvocationID] = p[gl_InvocationID];\n"
		" for (int i = 0; i < 4; i++) gl_TessLevelOuter[i] = 2.0; gl_TessLevelInner[0] = gl_TessLevelInner[1] = 2.0; }\n",
		"#version 430 compatibility\nlayout(quads, equal_spacing, ccw) in; in vec2 q[];\n"
		"void main(){ vec2 a = mix(q[0], q[1], gl_TessCoord.x), b = mix(q[3], q[2], gl_TessCoord.x);\n"
		" gl_Position = vec4(mix(a, b, gl_TessCoord.y), 0.0, 1.0); }\n",
		"#version 430 compatibility\nlayout(location = 0) out vec4 colour;\nvoid main(){ colour = vec4(1.0, 1.0, 0.0, 1.0); }\n" };
	GLenum type[4] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_FRAGMENT_SHADER };
	GLuint p = program(src, type, 4);
	float quad[] = { -1,-1, 1,-1, 1,1, -1,1 };
	unsigned idx[4] = { 0, 1, 2, 3 };
	GLuint vao, vbo, ebo, dbo; glGenVertexArrays_(1, &vao); glBindVertexArray_(vao);
	glGenBuffers_(1, &vbo); glBindBuffer_(GL_ARRAY_BUFFER, vbo); glBufferData_(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
	GLint l = glGetAttribLocation_(p, "pos"); glEnableVertexAttribArray_(l); glVertexAttribPointer_(l, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glGenBuffers_(1, &ebo); glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, ebo); glBufferData_(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);
	unsigned *cmd = calloc((size_t)count * 5, sizeof *cmd); /* count, instanceCount, firstIndex, baseVertex, baseInstance */
	cmd[0] = 4; cmd[1] = 1;
	glGenBuffers_(1, &dbo); glBindBuffer_(GL_DRAW_INDIRECT_BUFFER, dbo);
	glBufferData_(GL_DRAW_INDIRECT_BUFFER, (GLsizeiptr)count * 20, cmd, GL_STATIC_DRAW); free(cmd);
	glPatchParameteri_(GL_PATCH_VERTICES, 4);
	glClearColor(0, 0, 1, 1); glClear(GL_COLOR_BUFFER_BIT);
	glMultiDrawElementsIndirect_(GL_PATCHES, GL_UNSIGNED_INT, NULL, count, 0);
	char name[96]; snprintf(name, sizeof name, "tessellated multi-draw indirect, %d commands", count);
	report(name, 128, 128);
}

int main(void)
{
	WNDCLASSA wc = {0};
	wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandle(NULL); wc.lpszClassName = "repro";
	RegisterClassA(&wc);
	HWND w = CreateWindowA("repro", "repro", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);
	HDC dc = GetDC(w);
	PIXELFORMATDESCRIPTOR pfd = { sizeof pfd, 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA, 32, 0,0,0,0,0,0, 8, 0, 0,0,0,0,0, 24, 8, 0, PFD_MAIN_PLANE, 0, 0,0,0 };
	int pf = wglChoosePixelFormat(dc, &pfd);
	if (!pf || !wglSetPixelFormat(dc, pf, &pfd)) { printf("pixel format failed\n"); return 1; }
	HGLRC tmp = wglCreateContext(dc); wglMakeCurrent(dc, tmp);
	typedef HGLRC (WINAPI *PFNCCA)(HDC, HGLRC, const int *);
	PFNCCA cca = (PFNCCA)wglGetProcAddress("wglCreateContextAttribsARB");
	int attrs[] = { 0x2091, 4, 0x2092, 3, 0x9126, 0x2, 0 }; /* GL 4.3 compatibility */
	HGLRC ctx = cca ? cca(dc, NULL, attrs) : NULL;
	if (!ctx) { printf("could not create a GL 4.3 context\n"); return 1; }
	wglMakeCurrent(dc, ctx); wglDeleteContext(tmp);
	GLFUNCS(LOAD)
	printf("GL_RENDERER: %s\nGL_VERSION:  %s\n\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
	glViewport(0, 0, 256, 256);

	const char *variant_names[] = { "flat only", "smooth float + flat float", "several smooth + flat float" };
	for (flat_variant = 0; flat_variant < 3; flat_variant++) {
		char name[96];
		snprintf(name, sizeof name, "%s, FIRST provoking vertex", variant_names[flat_variant]);
		flat_test(name, GL_FIRST_VERTEX_CONVENTION);
		snprintf(name, sizeof name, "%s, LAST provoking vertex (GL default)", variant_names[flat_variant]);
		flat_test(name, GL_LAST_VERTEX_CONVENTION);
	}
	int counts[] = { 1, 16, 17, 31, 32, 256, 4096 };
	for (unsigned i = 0; i < sizeof counts / sizeof counts[0]; i++) mdi_patch_test(counts[i]);
	return 0;
}
