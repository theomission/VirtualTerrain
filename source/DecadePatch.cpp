/*
 * Copyright © 2012 Fabian Schuiki
 */

#include <cmath>
#include "DecadePatch.h"
#include "Planet.h"


vec3 DecadePatch::getVertex(float x, float y, float z)
{
	float lon = (this->x + 10*x) / 180 * M_PI;
	float lat = (this->y + 10*y) / 180 * M_PI;
	
	return vec3(z * cos(lat) * sin(lon), z * sin(lat), z * cos(lat) * cos(lon));
}

vec3 DecadePatch::getVertexNormal(float x, float y)
{
	return getVertex(x, y, 1);
}


void DecadePatch::updateBuffers()
{
	struct Vertex {
		float x, y, z;
		float nx, ny, nz;
		float s, t;
	} vertices[11*11];
	
	for (int y = 0; y < 11; y++) {
		for (int x = 0; x < 11; x++) {
			float elev00 = planet->elevation.sample(this->x + x, this->y + y);
			float elev01 = planet->elevation.sample(this->x + x, this->y + y + 1);
			float elev10 = planet->elevation.sample(this->x + x + 1, this->y + y);
			
			struct Vertex &v = vertices[y * 11 + x];
			vec3 vv = getVertex(x*0.1, y*0.1, planet->radius + elev00);
			v.x = vv.x;
			v.y = vv.y;
			v.z = vv.z;
			
			vec3 n;
			n.x = (elev10 - elev00);
			n.y = 1;
			n.z = (elev01 - elev00);
			n.normalize();
			
			vec3 vn = getVertexNormal(x*0.1, y*0.1);
			v.nx = n.y * vn.x + n.x * vn.z - n.z * vn.x * vn.y;
			v.ny = n.y * vn.y + n.z * (vn.x*vn.x + vn.z*vn.z);
			v.nz = n.y * vn.z - n.x * vn.x - n.z * (vn.y * vn.z);
			
			v.s = x*0.1;
			v.t = y*0.1;
		}
	}
	
	vertexBuffer.loadData(8 * 4 * 11*11, vertices, GL_STATIC_DRAW);
	
	
	GLushort indices[4*100];
	for (int y = 0; y < 10; y++) {
		for (int x = 0; x < 10; x++) {
			indices[(y*10+x)*4 + 0] = y*11+x;
			indices[(y*10+x)*4 + 1] = y*11+x+1;
			indices[(y*10+x)*4 + 2] = (y+1)*11+x+1;
			indices[(y*10+x)*4 + 3] = (y+1)*11+x;
		}
	}
	
	indexBuffer.loadData(2 * 4 * 100, indices, GL_STATIC_DRAW);
}

void DecadePatch::draw()
{
	vertexBuffer.bind();
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 8*4, (void*)0);
	glEnableClientState(GL_NORMAL_ARRAY);
	glNormalPointer(GL_FLOAT, 8*4, (void*)12);
	
	indexBuffer.bind();
	glDrawElements(GL_QUADS, 4*100, GL_UNSIGNED_SHORT, (void*)0);
}