#ifndef MAIN_H
#define MAIN_H

#define _CRT_SECURE_NO_WARNINGS 1

#include "glm/vec3.hpp"
#include "GL/glew.h"

#include <vector>

typedef uint32_t uint32;
typedef int32_t int32;
typedef float_t float32;
typedef GLuint glid;

/**
 * @brief A triangle defined by indices to an external vertex list
 * and texture coords to an external tex coord list.
 */
struct Tri
{
	int vertIndices[3];
	int texCoordIndices[3];
};

struct Vertex
{
	glm::vec3 location;
	glm::vec3 normal;
};

/**
 * A complete object made up of vertices conncected by faces
 */
struct Mesh
{
	std::vector<Vertex> verts;
	std::vector<uint32> triangles;
	std::string name;
	glid vbo;
	glid ebo;

	bool empty() { return verts.empty(); }
};

struct Camera
{
	glm::vec3 position;
	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;

	float32 yrot;
	float32 xrot;

	float32 zoom;
	float32 translateSensitivity;
	float32 rotateSensitivity;
};

#endif // MAIN_H
