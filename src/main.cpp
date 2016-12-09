#include "main.h"
#include "sdl.h"
#include "GL/glew.h"
#include "glm/common.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include "imgui_impl_sdl_gl3.h"
#include "imgui.h"
#include "tiny_obj_loader.h"

#include "assimp/cimport.h"
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <string>
#include <iostream>
#include <vector>
#include <memory>

using namespace std;

void logError(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, fmt, args);
	va_end(args);
}

void logDebug(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG, fmt, args);
	va_end(args);
}

glm::mat4 getViewMatrix(Camera* cam)
{
	return glm::lookAt(cam->position, cam->position + cam->front, cam->up);
}

void readToEndOfLine(FILE* file)
{
	char buffer[256];
	fgets(buffer, 256, file);
}

void computeNormals(std::vector<Vertex>& verts, std::vector<uint32>& triangles)
{
	size_t triCount = triangles.size() / 3;

	// compute the face normals using the cross product of two verts
	std::vector<glm::vec3> faceNormals(triCount);
	for (size_t i = 0; i < triCount; ++i) {
		uint32 v1 = triangles[i * 3 + 0];
		uint32 v2 = triangles[i * 3 + 1];
		uint32 v3 = triangles[i * 3 + 2];
		glm::vec3 v12 = verts[v2].location - verts[v1].location;
		//glm::vec3 v13 = verts[v3] - verts[v1];
		glm::vec3 v23 = verts[v3].location - verts[v2].location;
		faceNormals[i] = glm::cross(v12, v23);
	}

	// compute the vert normal using the averages of the face normals
	// that the vertex is shared by
	for (size_t iv = 0; iv < verts.size(); ++iv) {
		glm::vec3 sum(0, 0, 0);
		uint32 shared = 0;
		for (size_t it = 0; it < triCount; ++it) {
			uint32 v1 = triangles[it * 3 + 0];
			uint32 v2 = triangles[it * 3 + 1];
			uint32 v3 = triangles[it * 3 + 2];
			if (iv == v1 || iv == v2 || iv == v3) {
				sum += faceNormals[it];
				shared++;
			}
		}
		verts[iv].normal = sum;
		verts[iv].normal /= (shared * 1.0f);
		verts[iv].normal = glm::normalize(verts[iv].normal);
	}
}

void shareVertices(Mesh& mesh, bool shareVerts)
{
	bool vertsAreShared = mesh.verts.size() != mesh.triangles.size();
	if (shareVerts && !vertsAreShared) {
		logDebug("mesh converted to shared vertices");
	}
	else if (!shareVerts && vertsAreShared) {
		std::vector<Vertex> verts(mesh.triangles.size());
		for (size_t i = 0; i < mesh.triangles.size(); ++i) {
			verts[i] = mesh.verts[mesh.triangles[i]];
			mesh.triangles[i] = i;
		}
		mesh.verts = std::move(verts);
		logDebug("mesh converted to unshared vertices");
	}
}

void loadMesh(Mesh& mesh, std::vector<Vertex>& verts, std::vector<uint32>& triangles)
{
	mesh.verts = std::move(verts);
	mesh.triangles = std::move(triangles);
}

void loadObj(std::string objPath, std::vector<Mesh>& meshes) {
	FILE* file = fopen(objPath.c_str(), "r");
	if (!file) { return; }

	std::vector<Vertex> verts;
	std::vector<uint32> triangles;

	uint32 normalCount = 0;

	bool lastLineWasFace = false;
	bool hasUV = false;
	bool hasNorm = false;
	while (!feof(file))
	{
		float x, y, z;
		float nx, ny, nz;
		char ch = fgetc(file);

		switch (ch)
		{
		case 'v':
		{
			if (lastLineWasFace) {
				Mesh mesh;
				loadMesh(mesh, verts, triangles);
				meshes.push_back(std::move(mesh));
				verts.clear();
				triangles.clear();
				lastLineWasFace = false;
				hasUV = false;
			}

			char ch = fgetc(file);
			char buffer[256];
			fgets(&buffer[0], 256, file);

			if (ch == ' ') {
				sscanf(&buffer[0], "%f %f %f", &x, &y, &z);
				verts.push_back(Vertex { glm::vec3(x, y, z) });
			}
			else if (ch == 't') {
				// indicates a texture coord
				hasUV = true;
			}
			else if (ch == 'n') {
				// indicates a normal
				hasNorm = true;
				sscanf(&buffer[0], "%f %f %f", &nx, &ny, &nz);
				Vertex& vert = verts[normalCount++];
				vert.normal = glm::vec3(nx, ny, nz);
			}
			break;
		}
		case 'f':
		{
			char buffer[256];
			fgets(&buffer[0], 256, file);
			int32 v1, v2, v3, v4;
			v1 = v2 = v3 = v4 = 0;
			int count = 0;
			if (hasUV) {
				int32 vt1, vt2, vt3, vt4;
				if (hasNorm) {
					int32 vn1, vn2, vn3, vn4;
					count = sscanf(&buffer[0], "%d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d", 
						&v1, &vt1, &vn1,
						&v2, &vt2, &vn2,
						&v3, &vt3, &vn3,
						&v4, &vt4, &vn4);
				}
				else {
					count = sscanf(&buffer[0], "%d/%d %d/%d %d/%d %d/%d", &v1, &vt1, &v2, &vt2, &v3, &vt3, &v4, &vt4);
				}
			}
			else if (hasNorm) {
				int32 vn1, vn2, vn3, vn4;
				count = sscanf(&buffer[0], "%d//%d %d//%d %d//%d %d//%d",
					&v1, &vn1,
					&v2, &vn2,
					&v3, &vn3,
					&v4, &vn4);
			}
			else {
				sscanf(&buffer[0], "%d %d %d %d", &v1, &v2, &v3, &v4);
			}
			if (v1 < 0) { v1 = v1 + 1 + verts.size(); }
			if (v2 < 0) { v2 = v2 + 1 + verts.size(); }
			if (v3 < 0) { v3 = v3 + 1 + verts.size(); }
			if (v4 < 0) { v4 = v4 + 1 + verts.size(); }
			// make indices 0-based
			triangles.push_back(v1 - 1);
			triangles.push_back(v2 - 1);
			triangles.push_back(v3 - 1);
			if (v4 != 0) {
				logDebug("quad found");
				triangles.push_back(v1 - 1);
				triangles.push_back(v3 - 1);
				triangles.push_back(v4 - 1);
			}
			lastLineWasFace = true;
			break;
		}
		case '\n':
			break;
		default:
			readToEndOfLine(file);
		}
	}

	Mesh mesh;
	loadMesh(mesh, verts, triangles);
	meshes.push_back(std::move(mesh));
}

struct ObjMeshes
{
	std::string objPath;
	std::vector<Mesh> meshes;
};

void loadObjTiny(ObjMeshes* meshes)
{
	tinyobj::attrib_t attrib;
	std::string err;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	tinyobj::LoadObj(&attrib, &shapes, &materials, &err, meshes->objPath.c_str());
	for (size_t i = 0; i < shapes.size(); ++i) {
		Mesh m = {};
		m.name = shapes[i].name;
		for (size_t i = 0; i < attrib.vertices.size(); i += 3) {
			m.verts.push_back(
				Vertex {
					glm::vec3(
						attrib.vertices[i],
						attrib.vertices[i + 1],
						attrib.vertices[i + 2]),
				});
		}
		tinyobj::mesh_t& mesht = shapes[i].mesh;
		for (size_t j = 0; j < mesht.indices.size(); j++) {
			if (mesht.indices[j].normal_index != -1) {
				Vertex& v = m.verts[mesht.indices[j].vertex_index];
				v.normal = glm::vec3(
						attrib.normals[mesht.indices[j].normal_index],
						attrib.normals[mesht.indices[j].normal_index + 1],
						attrib.normals[mesht.indices[j].normal_index + 2]);
			}
			m.triangles.push_back(mesht.indices[j].vertex_index);
		}
		meshes->meshes.push_back(std::move(m));
	}
}

void loadObjAssimp(ObjMeshes* meshes)
{
	const aiScene* scene = aiImportFile(meshes->objPath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
	if (!scene) {
		logError("Failed to load obj file");
		return;
	}
	for (uint32 im = 0; im < scene->mNumMeshes; ++im) {
		Mesh m = {};
		aiMesh* aiMesh = scene->mMeshes[im];
		for (uint32 i = 0; i < aiMesh->mNumVertices; ++i) {
			m.verts.push_back(
				Vertex {
					glm::vec3(
						aiMesh->mVertices[i].x,
						aiMesh->mVertices[i].y,
						aiMesh->mVertices[i].z),
					glm::vec3(
						aiMesh->mNormals[i].x,
						aiMesh->mNormals[i].y,
						aiMesh->mNormals[i].z)
				});
		}
		for (uint32 i = 0; i < aiMesh->mNumFaces; ++i) {
			const aiFace& face = aiMesh->mFaces[i];
			m.triangles.push_back(face.mIndices[0]);
			m.triangles.push_back(face.mIndices[1]);
			m.triangles.push_back(face.mIndices[2]);
		}
		logDebug("created mesh with %d vertices and %d triangles", m.verts.size(), m.triangles.size() / 3);
		meshes->meshes.push_back(std::move(m));
	}
}

int loadObjThread(void* data)
{
	ObjMeshes* meshes = (ObjMeshes*) data;
	//loadObj(meshes->objPath, meshes->meshes);
	loadObjAssimp(meshes);
	return 0;
}

SDL_Thread* loadObjAsync(ObjMeshes& objMeshes)
{
	SDL_Thread* thread = SDL_CreateThread(loadObjThread, "loadObjThead", &objMeshes);
	return thread;
}

void checkSDLError(int line = -1)
{
	std::string error = SDL_GetError();

	if (error != "") {
		if (line != -1) {
			std::cout << "SDL Error(ln " << line << "): ";
		}
		else {
			std::cout << "SDL Error: ";
		}
		std::cout << error << std::endl;
		SDL_ClearError();
	}
}

GLuint loadShader(std::string shaderPath, GLenum shaderType)
{
	SDL_RWops* file = SDL_RWFromFile(shaderPath.c_str(), "r");
	if (!file) {
		logError("Failed to load file: %s", shaderPath.c_str());
		return -1;
	}

	long size = (long)SDL_RWseek(file, 0, SEEK_END);
	char* contents = (char*)malloc(size + 1);
	contents[size] = '\0';
	SDL_RWseek(file, 0, SEEK_SET);
	SDL_RWread(file, contents, size, 1);
	SDL_RWclose(file);

	GLuint shaderId = glCreateShader(shaderType);
	glShaderSource(shaderId, 1, (const char**)&contents, 0);
	glCompileShader(shaderId);
	free(contents);

	int compileErr = 0;
	int infoLogLength = 0;
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileErr);
	glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLogLength);
	if (infoLogLength > 0) {
		std::vector<char> errorMessage(infoLogLength + 1);
		glGetShaderInfoLog(shaderId, infoLogLength, 0, &errorMessage[0]);
		logError("Error compiling shader %s: %s", shaderPath.c_str(), &errorMessage[0]);
		checkSDLError(__LINE__);
	}
	return shaderId;
}

GLuint loadShaders(std::string vertShaderPath, std::string fragShaderPath)
{
	GLuint vertShaderId = loadShader(vertShaderPath, GL_VERTEX_SHADER);
	GLuint fragShaderId = loadShader(fragShaderPath, GL_FRAGMENT_SHADER);
	if (vertShaderId != -1 && fragShaderId != -1) {
		GLuint programId = glCreateProgram();
		glAttachShader(programId, vertShaderId);
		glAttachShader(programId, fragShaderId);
		glLinkProgram(programId);
		GLint linkErr = 0;
		glGetProgramiv(programId, GL_LINK_STATUS, &linkErr);
		glDeleteShader(vertShaderId);
		glDeleteShader(fragShaderId);
		if (linkErr == GL_FALSE) {
			logError("Error linking shader program");
			return -1;
		}
		return programId;
	}
	return -1;
}

int main(int argc, char *argv[])
{
	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		std::cout << "Failed to init SDL" << std::endl;
		return 1;
	}

	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

	int32 windowWidth = 800;
	int32 windowHeight = 600;

    SDL_Window* mainWindow = SDL_CreateWindow(
                "Model Viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
				windowWidth, windowHeight,
				SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
	if (!mainWindow) {
		std::cout << "Failed to create OpenGL window" << std::endl;
		return 1;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GLContext glcontext = SDL_GL_CreateContext(mainWindow);
	if (!glcontext) {
		std::cout << "Faield to created OpenGL context" << std::endl;
		checkSDLError(__LINE__);
		return 1;
	}

	glewExperimental = GL_TRUE;
	glewInit();

	// disable vsync (1 to enable)
	SDL_GL_SetSwapInterval(0);

	// Setup ImGui binding
	imguiInit(mainWindow);
	ImVec4 clear_color = ImColor(114, 144, 154);
	glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);

	GLuint programId = loadShaders("phongvertshader.vert", "phongfragshader.frag");

	std::string filePath;
	if (argc > 1) {
		// assume second arg is the file to load
		filePath = argv[1];
	}

	float32 scaleFactor = 1.0f;
	if (argc > 2) {
		// assume third arg is the scale factor
		scaleFactor = stof(argv[2]);
	}

	bool flatShading = false;

	ObjMeshes objMeshes = { filePath };
	if (!filePath.empty()) {
		logDebug("loading %s", filePath.c_str());
		uint32 start = SDL_GetTicks();
		SDL_Thread* loadThread = loadObjAsync(objMeshes);
		SDL_WaitThread(loadThread, 0);
		logDebug("loaded in %d ms", SDL_GetTicks() - start);
	}

	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	for (int i = 0; i < objMeshes.meshes.size(); ++i) {
		Mesh* mesh = &objMeshes.meshes[i];
		//shareVertices(*mesh, true);
		//computeNormals(mesh->verts, mesh->triangles);
		glGenBuffers(1, &mesh->vbo);
		glGenBuffers(1, &mesh->ebo);

		glBindVertexArray(vao);

		// one vbo contains both vert locations and normals
		glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
		glBufferData(GL_ARRAY_BUFFER, mesh->verts.size() * sizeof(Vertex), &mesh->verts[0], GL_STATIC_DRAW);

		// positions bound to attrib 0
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
		glEnableVertexAttribArray(0);

		// normals bound to attrib 1
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)sizeof(glm::vec3));
		glEnableVertexAttribArray(1);

		// bind the triangle indices
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->triangles.size() * sizeof(uint32), &mesh->triangles[0], GL_STATIC_DRAW);

		glBindVertexArray(0);
	}

	Camera camera = {};
	camera.position = glm::vec3(0, 0, 20);
	camera.front = glm::vec3(0, 0, -1);
	camera.up = glm::vec3(0, 1, 0);
	camera.right = glm::vec3(1, 0, 0);
	camera.zoom = 60.0f;
	camera.translateSensitivity = 0.1f;
	camera.rotateSensitivity = 0.002f;

	float aspect = (float)windowWidth / (float)windowHeight;
	float nearClip = 0.1f;
	float farClip = 1000.0f;

	// 0 = y-up, 1 = z-up
	int32 upVector = 0;
	// 0 = z-forward, 1 = y-forward
	int32 forwardVector = 0;

	glm::mat4 projection = glm::perspective(glm::degrees(camera.zoom), aspect, nearClip, farClip);
	glm::mat4 view = getViewMatrix(&camera);
	glm::mat4 modelScale = glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor));
	glm::mat4 modelRot = glm::mat4(1.0f);
	glm::mat4 model = modelRot * modelScale;
	glm::mat4 mv = view * model;
	glm::mat4 mvp = projection * mv;

	GLuint mId = glGetUniformLocation(programId, "u_M");
	//GLuint vId = glGetUniformLocation(programId, "V");
	//GLuint mvId = glGetUniformLocation(programId, "MV");
	GLuint mvpId = glGetUniformLocation(programId, "u_MVP");
	GLuint lightPosId = glGetUniformLocation(programId, "u_lightPos");
	GLuint lightColorId = glGetUniformLocation(programId, "u_lightColor");
	GLuint objectColorId = glGetUniformLocation(programId, "u_objectColor");

	//glm::vec3 lightPos(0.0, 2.0, 0.0);
	glm::vec3 lightPos(camera.position);
	glm::vec3 lightColor(1.0, 1.0, 1.0);
	glm::vec3 objectColor(1.0f, 0.5f, 0.2f);

	uint32 gameStart = SDL_GetTicks();
	uint32 frameStart = gameStart;
	uint32 frameEnd = gameStart;
	uint32 frameTime = 0;
	uint32 accumulatedFrameTime = 0;
	uint32 framesCounted = 0;
	bool quit = false;
	while (!quit) {
		frameStart = SDL_GetTicks();
		if (framesCounted >= 30) {
			frameTime = accumulatedFrameTime / framesCounted;
			accumulatedFrameTime = 0;
			framesCounted = 0;
		}
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT) {
				quit = true;
			}

			// let imgui process the events first
			imguiProcessEvent(&event);
			if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard) {
				continue;
			}

			if (event.type == SDL_WINDOWEVENT) {
				switch (event.window.event)
				{
					case SDL_WINDOWEVENT_RESIZED:
					{
						windowWidth = event.window.data1;
						windowHeight = event.window.data2;
						aspect = (float) windowWidth / (float) windowHeight;
						projection = glm::perspective(glm::degrees(camera.zoom), aspect, nearClip, farClip);
						view = getViewMatrix(&camera);
						mv = view * model;
						mvp = projection * mv;
						glViewport(0, 0, windowWidth, windowHeight);
						ImGui::GetIO().DisplaySize = ImVec2((float)windowWidth, (float)windowHeight);
						// effectively resets mouse state for the next time it is called
						SDL_GetRelativeMouseState(0, 0);
					} break;
				}
			}
			if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym)
				{
				case SDLK_ESCAPE:
					quit = true;
					break;
				}
			}
			else if (event.type == SDL_MOUSEMOTION) {
				int32 x, y;
				SDL_GetRelativeMouseState(&x, &y);
				if (x != 0 || y != 0) {
					bool lbPressed = (event.motion.state & SDL_BUTTON_LMASK) != 0;
					bool mbPressed = (event.motion.state & SDL_BUTTON_MMASK) != 0;
					bool rbPressed = (event.motion.state & SDL_BUTTON_RMASK) != 0;
					if (lbPressed || mbPressed || rbPressed) {
						if (lbPressed) {
							camera.front = glm::rotate(camera.front, -x * camera.rotateSensitivity, glm::vec3(0, 1, 0));
							camera.right = glm::rotate(camera.right, -x * camera.rotateSensitivity, glm::vec3(0, 1, 0));
							camera.position += camera.front * (-y * camera.translateSensitivity);
						}
						if (rbPressed) {
							camera.position += camera.right * (x * camera.translateSensitivity);
							camera.position += camera.up * (-y * camera.translateSensitivity);
						}
						lightPos = camera.position;
						view = getViewMatrix(&camera);
						mvp = projection * view * model;
					}
				}
			}
		}

		// draw UI before the scene
		{
			imguiNewFrame(mainWindow);
			ImGuiWindowFlags windowFlags =
					ImGuiWindowFlags_NoTitleBar
					| ImGuiWindowFlags_NoResize
					| ImGuiWindowFlags_NoMove
					| ImGuiWindowFlags_NoScrollbar
					| ImGuiWindowFlags_NoSavedSettings
					| ImGuiWindowFlags_NoInputs;
			ImGui::Begin("dummy", 0, ImVec2((float)windowWidth, 20 * (objMeshes.meshes.size() + 1)), 0.0f, windowFlags);
			ImGui::Text("%.3f ms/frame (%.1f fps)", frameTime / 1000.0f, 1 / (frameTime / 1000.0f));
			ImGui::BeginChild("meshes", ImVec2((float) windowWidth, 200), false);
			for (int i = 0; i < objMeshes.meshes.size(); ++i) {
				Mesh* mesh = &objMeshes.meshes[i];
				ImGui::Text("%d vertices, %d triangles", mesh->verts.size(), mesh->triangles.size() / 3);
			}
			ImGui::EndChild();
			ImGui::End();
			ImGui::Begin("Settings");
			ImGui::Text("scale");
			if (ImGui::SliderFloat("##scaleSlider", &scaleFactor, .001f, 100.0f, 0, 10.0)) {
				modelScale = glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor));
				model = modelRot * modelScale;
				mv = view * model;
				mvp = projection * mv;
			}
			ImGui::BeginGroup();
			ImGui::Text("Model Orientation");
			int newUpVector = upVector;
			ImGui::RadioButton("+Y-Up", &newUpVector, 0);
			ImGui::RadioButton("+Z-Up", &newUpVector, 1);
			if (newUpVector != upVector) {
				upVector = newUpVector;
				if (upVector == 0) {
					model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
				}
				if (upVector == 1) {
					model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
				}
				mv = view * model;
				mvp = projection * mv;
			}
			int newForwardVector = forwardVector;
			ImGui::RadioButton("+Y-Forward", &newForwardVector, 0);
			ImGui::RadioButton("+Z-Forward", &newForwardVector, 1);
			if (newForwardVector != forwardVector) {
				forwardVector = newForwardVector;
				if (forwardVector == 0) {
					model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0, 1, 0));
				}
				if (forwardVector == 1) {
					model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0, 1, 0));
				}
				mv = view * model;
				mvp = projection * mv;
			}
			ImGui::EndGroup();
//			if (mesh && ImGui::Checkbox("Flat Shaded", &flatShading)) {
//				shareVertices(*mesh, !flatShading);
//				computeNormals(mesh->verts, mesh->triangles);
//				glBindVertexArray(vao);
//				// one vbo contains both vert locations and normals
//				glBindBuffer(GL_ARRAY_BUFFER, vbo);
//				glBufferData(GL_ARRAY_BUFFER, mesh->verts.size() * sizeof(Vertex), &mesh->verts[0], GL_STATIC_DRAW);
//				// positions bound to attrib 0
//				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
//				glEnableVertexAttribArray(0);
//				// normals bound to attrib 1
//				glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)sizeof(glm::vec3));
//				glEnableVertexAttribArray(1);
//				// bind the triangle indices
//				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
//				glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->triangles.size() * sizeof(uint32), &mesh->triangles[0], GL_STATIC_DRAW);
//				glBindVertexArray(0);
//			}
			ImGui::Text("translate sens.");
			ImGui::SliderFloat("##tsSlider", &camera.translateSensitivity, 0.001f, 1.0f, 0, 1.0);
			ImGui::Text("rotate sens.");
			ImGui::SliderFloat("##rsSlider", &camera.rotateSensitivity, 0.001f, 0.01f, 0, 1.0);
			ImGui::End();
		}


		glClearDepth(1.0);
		// Clear color buffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(programId);
		glBindVertexArray(vao);
		for (int i = 0; i < objMeshes.meshes.size(); ++i) {
			Mesh* mesh = &objMeshes.meshes[i];
			
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)sizeof(glm::vec3));

			glUniformMatrix4fv(mId, 1, GL_FALSE, &model[0][0]);
			glUniformMatrix4fv(mvpId, 1, GL_FALSE, &mvp[0][0]);
			glUniform3f(lightPosId, lightPos.x, lightPos.y, lightPos.z);
			glUniform3f(lightColorId, lightColor.x, lightColor.y, lightColor.z);
			glUniform3f(objectColorId, objectColor.x, objectColor.y, objectColor.z);

			glDrawElements(GL_TRIANGLES, mesh->triangles.size(), GL_UNSIGNED_INT, 0);
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
		glUseProgram(0);

		// Update window with OpenGL rendering
		ImGui::Render();
		SDL_GL_SwapWindow(mainWindow);
		frameEnd = SDL_GetTicks();
		accumulatedFrameTime += frameEnd - frameStart;
		framesCounted++;
	}

	for (int i = 0; i < objMeshes.meshes.size(); ++i) {
		Mesh* mesh = &objMeshes.meshes[i];
		if (mesh->vbo) {
			glDeleteBuffers(1, &mesh->vbo);
		}
		if (mesh->ebo) {
			glDeleteBuffers(1, &mesh->ebo);
		}
	}

	if (vao) {
		glDeleteVertexArrays(1, &vao);
	}

	imguiShutdown();
	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(mainWindow);
	SDL_Quit();

    return 0;
}
