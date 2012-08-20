/*
 * Copyright © 2012 Fabian Schuiki
 */

#include <cmath>
#include <iostream>
#include <fstream>
#include <SFML/OpenGL.hpp>

#include "Application.h"
#include "ElevationDataSlice.h"
#include "Framebuffer.h"
#include "Planet.h"
#include "ShaderProgram.h"
#include "Texture.h"

#define NUM_DS 3
#define NUM_DS_IT 2


static void glaux_dumpError()
{
	GLenum err = glGetError();
	if (err != GL_NO_ERROR)
		std::cerr << "*** error " << (int)err << std::endl;
}

static void glaux_fullQuad()
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, 1); glVertex2f(-1,  1);
	glTexCoord2f(1, 1); glVertex2f( 1,  1);
	glTexCoord2f(1, 0); glVertex2f( 1, -1);
	glTexCoord2f(0, 0); glVertex2f(-1, -1);
	glEnd();
}

static void glVertex_vec3(const vec3 &v)
{
	glVertex3f(v.x, v.y, v.z);
}

static void glNormal_vec3(const vec3 &n)
{
	glNormal3f(n.x, n.y, n.z);
}


int Application::run(int argc, char* argv[])
{
	sf::ContextSettings settings;
	//settings.depthBits = 24;
	
	//Setup the window.
	window.create(sf::VideoMode(1280, 768), "VirtualTerrain", sf::Style::Default, settings);
	window.setVerticalSyncEnabled(true);
	
	//Load shaders.
	ShaderProgram bypassShader("data/bypass.vert", "data/bypass.frag");
	ShaderProgram normalsShader("data/normals.vert", "data/normals.frag");
	ShaderProgram extractHighlightsShader("data/bypass.vert", "data/extract_highlights.frag");
	ShaderProgram blur0Shader("data/bypass.vert", "data/blur_0.frag");
	ShaderProgram blur1Shader("data/bypass.vert", "data/blur_1.frag");
	ShaderProgram toneMappingShader("data/bypass.vert", "data/tone_mapping.frag");
	
	//Start the clock.
	sf::Clock clock;
	
	//Setup OpenGL state.
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	
	//Initialize the scene framebuffer.
	Framebuffer sceneFBO(window.getSize().x, window.getSize().y);
	Texture sceneColor;
	sceneColor.loadImage(window.getSize().x, window.getSize().y, GL_RGBA16F_ARB, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	Texture sceneDepth;
	sceneDepth.loadImage(window.getSize().x, window.getSize().y, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	sceneFBO.attachTexture(GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, sceneColor.texture);
	sceneFBO.attachTexture(GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, sceneDepth.texture);
	sceneFBO.validate();
	
	//Initialize the downsampling framebuffers.
	Framebuffer *downsampleFBO[NUM_DS][2];
	Texture *downsampleColor[NUM_DS][2];
	for (int i = 0; i < NUM_DS; i++) {
		GLint w = window.getSize().x >> (i+1);
		GLint h = window.getSize().y >> (i+1);
		for (int n = 0; n < 2; n++) {
			downsampleColor[i][n] = new Texture;
			downsampleColor[i][n]->loadImage(w, h, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			downsampleFBO[i][n] = new Framebuffer(w, h);
			downsampleFBO[i][n]->attachTexture(GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, downsampleColor[i][n]->texture);
			downsampleFBO[i][n]->validate();
		}
	}
	
	
	//Create the planet structure.
	Planet planet;
	
	//Camera
	float rotation = 0, inclination = M_PI / 4;
	float radius = planet.radius * 5;
	
	//Light
	float light_rotation = 0, light_inclination = M_PI / 4;
	float light_radius = 1e8;
	
	//Load some elevation data.
	int centerx = 3425, centery = 5116;
	//int centerx = 2400, centery = 3000;
	int grid_size = 100;
	float scale = 8 / (float)grid_size;
	float elev_scale = 0.005 * scale;
	short elev_off = 0;
	ElevationDataSlice eds("/tmp/W020N90.DEM");
	eds.reload(0);
	
	std::cout << "elevation at " << centerx << "x" << centery << ": " << eds.sample(centerx, centery) << std::endl;
	
	//Main loop.
	GLUquadric* quadric = gluNewQuadric();
	float aspect = 1280.0 / 768;
	int mouseX, mouseY;
	bool mouseDown = false;
	bool dragLight = false;
	
	enum {
		kSceneMode = 0,
		kBuffersMode,
		kMaxMode
	};
	int displayMode = kSceneMode;
	bool wireframe = false;
	
	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				window.close();
			else if (event.type == sf::Event::Resized) {
				aspect = (float)event.size.width / event.size.height;
			}
			else if (event.type == sf::Event::TextEntered) {
				switch (event.text.unicode) {
					case 'r': {
						normalsShader.reload();
						extractHighlightsShader.reload();
						blur0Shader.reload();
						blur1Shader.reload();
						toneMappingShader.reload();
					} break;
					case 'b': {
						displayMode++;
						if (displayMode >= kMaxMode)
							displayMode = 0;
					} break;
					case '+': eds.reload(eds.resolution + 1); break;
					case '-': eds.reload(eds.resolution - 1); break;
					case 'w': wireframe = !wireframe; break;
				}
			}
			else if (event.type == sf::Event::KeyPressed) {
			}
			else if (event.type == sf::Event::MouseButtonPressed) {
				mouseX = event.mouseButton.x;
				mouseY = event.mouseButton.y;
				mouseDown = true;
				dragLight = event.mouseButton.button == sf::Mouse::Right;
			}
			else if (event.type == sf::Event::MouseButtonReleased) {
				mouseDown = false;
			}
			else if (event.type == sf::Event::MouseWheelMoved) {
				radius = std::max<float>(std::min<float>(radius + event.mouseWheel.delta*planet.radius*0.01, planet.radius * 10), planet.radius * 1.25);
			}
		}
		if (mouseDown) {
			sf::Vector2i v = sf::Mouse::getPosition(window);
			if (dragLight) {
				light_rotation    += (v.x - mouseX) * 0.005;
				light_inclination = std::max<float>(std::min<float>(light_inclination - (v.y - mouseY) * 0.005, M_PI/2*0.99), -M_PI/2*0.99);
			} else {
				rotation    -= (v.x - mouseX) * 0.005;
				inclination = std::max<float>(std::min<float>(inclination + (v.y - mouseY) * 0.005, M_PI/2*0.99), -M_PI/2*0.99);
			}
			mouseX = v.x;
			mouseY = v.y;
		}
		
		//Measure time.
		/*float dt = clock.getElapsedTime().asSeconds();
		clock.restart();*/
		
		//Draw the scene into the FBO.
		sceneFBO.bind();
		glDisable(GL_TEXTURE_2D);
		glClearColor(0.2, 0.2, 0.2, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(45, aspect, 0.1*radius, 10*radius);
		
		
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		vec3 eye(radius * sin(rotation) * cos(inclination),
				 radius * sin(inclination),
				 radius * cos(rotation) * cos(inclination));
		gluLookAt(eye.x,
				  eye.y,
				  eye.z,  0, 0, 0,  0, 1, 0);
		planet.updateEye(eye);
		
		vec3 light(light_radius * sin(light_rotation) * cos(light_inclination),
				   light_radius * sin(light_inclination),
				   light_radius * cos(light_rotation) * cos(light_inclination));
		
		glUseProgram(0);
		glPushMatrix();
		glTranslatef(light.x, light.y, light.z);
		glColor3f(1, 1, 0);
		gluSphere(quadric, 1e6, 4, 8);
		glPopMatrix();
		
		glEnable(GL_TEXTURE_2D);
		normalsShader.use();
		glUniform1i(normalsShader.uniform("tex"), 0);
		glUniform3f(normalsShader.uniform("lightPos"), light.x, light.y, light.z);
		
		//Draw the planet.
		/*for (int y = 0; y < 18; y++) {
			for (int x = 0; x < 36; x++) {
				DecadePatch &p = planet.patches[x][y];
				if (p.angularQuality < 0)
					continue;
				
				glColor3f(1 - p.angularQuality, p.angularQuality, 0);
				p.draw();
			}
		}*/
		planet.draw();
		
		if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		
		
		//Prepare for post effects.
		glColor3f(1, 1, 1);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		glEnable(GL_TEXTURE_2D);
		
		//Extract the overbright areas of the scene.
		sceneColor.bind();
		downsampleFBO[0][1]->bind();
		extractHighlightsShader.use();
		glUniform1i(extractHighlightsShader.uniform("tex"), 0);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 1); glVertex2f(-1,  1);
		glTexCoord2f(1, 1); glVertex2f( 1,  1);
		glTexCoord2f(1, 0); glVertex2f( 1, -1);
		glTexCoord2f(0, 0); glVertex2f(-1, -1);
		glEnd();
		
		//Downsample and bloom the overbright areas.
		blur0Shader.use();
		glUniform1i(blur0Shader.uniform("tex"), 0);
		blur1Shader.use();
		glUniform1i(blur1Shader.uniform("tex"), 0);
		for (int i = 0; i < NUM_DS; i++) {
			for (int n = 0; n < NUM_DS_IT; n++) {
				int p = (n == 0 && i > 0 ? i-1 : i);
				
				blur0Shader.use();
				downsampleFBO[i][0]->bind();
				downsampleColor[p][1]->bind();
				glUniform1f(blur0Shader.uniform("d"), 1.0 / downsampleFBO[p][0]->width);
				glaux_fullQuad();
				
				blur1Shader.use();
				downsampleFBO[i][1]->bind();
				downsampleColor[i][0]->bind();
				glUniform1f(blur1Shader.uniform("d"), 1.0 / downsampleFBO[p][1]->height);
				glaux_fullQuad();
			}
		}
		
		//Additively blend the bloom filter onto the scene.
		sceneFBO.bind();
		glUseProgram(0);
		glEnable(GL_BLEND);
		glDepthMask(0);
		glBlendFunc(GL_ONE, GL_ONE);
		for (int i = 0; i < NUM_DS; i++) {
			downsampleColor[i][1]->bind();
			glaux_fullQuad();
		}
		glDisable(GL_BLEND);
		glDepthMask(1);
		
		
		//Final image.
		glViewport(0, 0, window.getSize().x, window.getSize().y);
		Framebuffer::unbind();
		glUseProgram(0);
		
		switch (displayMode) {
			case kSceneMode: {
				sceneColor.bind();
				toneMappingShader.use();
				glUniform1i(toneMappingShader.uniform("tex"), 0);
				glaux_fullQuad();
				glUseProgram(0);
			} break;
				
			case kBuffersMode: {
				//Draw the FBOs onto the screen.
				glClearColor(0, 0, 0.5, 1);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				
				//-> color
				sceneColor.bind();
				toneMappingShader.use();
				glUniform1f(toneMappingShader.uniform("tex"), 0);
				glBegin(GL_QUADS);
				glTexCoord2f(0, 1); glVertex2f(-1, 1);
				glTexCoord2f(1, 1); glVertex2f( 0, 1);
				glTexCoord2f(1, 0); glVertex2f( 0, 0);
				glTexCoord2f(0, 0); glVertex2f(-1, 0);
				glEnd();
				glUseProgram(0);
				
				//-> depth
				sceneDepth.bind();
				glBegin(GL_QUADS);
				glTexCoord2f(0, 1); glVertex2f(0, 1);
				glTexCoord2f(1, 1); glVertex2f(1, 1);
				glTexCoord2f(1, 0); glVertex2f(1, 0);
				glTexCoord2f(0, 0); glVertex2f(0, 0);
				glEnd();
				
				//-> downsampled
				for (int i = 0; i < NUM_DS; i++) {
					float f = pow(2, -i);
					downsampleColor[i][1]->bind();
					glBegin(GL_QUADS);
					glTexCoord2f(0, 1); glVertex2f(-1,  -1+f);
					glTexCoord2f(1, 1); glVertex2f(f*0.5-1, -1+f);
					glTexCoord2f(1, 0); glVertex2f(f*0.5-1, -1+f*0.5);
					glTexCoord2f(0, 0); glVertex2f(-1,  -1+f*0.5);
					glEnd();
				}
				
				//-> bloom overlay
				downsampleColor[NUM_DS-1][1]->bind();
				glBegin(GL_QUADS);
				glTexCoord2f(0, 1); glVertex2f(0,  0);
				glTexCoord2f(1, 1); glVertex2f(1,  0);
				glTexCoord2f(1, 0); glVertex2f(1, -1);
				glTexCoord2f(0, 0); glVertex2f(0, -1);
				glEnd();
			} break;
				
			default: break;
		}
		
		glaux_dumpError();
		window.display();
	}
	
	return 0;
}