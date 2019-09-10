/* Project base code - transforms using local matrix functions 
   to be written by students - <2019 merge with tiny loader changes to support multiple objects>
	CPE 471 Cal Poly Z. Wood + S. Sueda
*/

#include <iostream>
#include <glad/glad.h>
#include <cmath>
#include <vector>
#include <stdlib.h>
#include "stb_image.h"
#include "GLSL.h"

#include "Program.h"
#include "Shape.h"
#include "MatrixStack.h"
#include "WindowManager.h"
#include "Texture.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define PI 3.1415f
#define DUMMY 29
#define OFFENSE 0
#define GOAL 1
#define DEFENSE 2
#define GROUND 3
#define SPEED 3.0

#define STAND 0
#define WINDUP 1
#define KICK 2

#define GRAVITY 0.09f;

using namespace std;
using namespace glm;

class Application : public EventCallbacks {

public:
	//camera variables
	typedef struct gamecamera {
		double theta;
		double phi;
		vec3 lookAtPos;
		vec3 eye;
	} GameCamera;

	typedef struct boundingsphere {
		vec3 center;
		float radius;
	} BoundingSphere;

	typedef struct rigidbody {
		vec3 position;
		vec3 velocity;
		vec3 acceleration;
		BoundingSphere bsphere;
	} Rigidbody;

	WindowManager * windowManager = nullptr;

	// Our shader program
	std::shared_ptr<Program> prog;
	std::shared_ptr<Program> texProg;
	std::shared_ptr<Program> skyboxProg;
	std::shared_ptr<Program> diffProg;

	shared_ptr<Shape> grass;
	shared_ptr<Shape> ballPtr;
	shared_ptr<Shape> kicker[DUMMY];
	shared_ptr<Shape> goal;
	shared_ptr<Shape> skybox;

	bool useCel = false;

	vec3 kickerPos = vec3(-2.0, 0.0, 35.0);
	bool wind = true;
	vec3 light = vec3(-40, 60, -30);
    float celebrateAngle = 0.0;

	//ball movement variables
	short animationSetter = STAND;
	bool kickedBall = false;
	float kickRotation = 0.0;
	float kickOffset = 0.4;

	GameCamera camera = { -PI / 2.0, 0.0, vec3(0, 0, 0), vec3(10, 15, 70)};
	BoundingSphere kickerFootBSphere = { vec3(-2.0, 0.0, 35.0), 3.0 };

	Rigidbody ball = { vec3(6.0, 2.0, 30.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), { vec3(6.0, 2.0, 30.0), 1.5f} };
	bool miss = false;

	//texture variables
	shared_ptr<Texture> ballTex;
	shared_ptr<Texture> flareTex0;

	//skybox
	unsigned int skyboxID;

	// Contains vertex information for OpenGL
	GLuint VertexArrayID;

	// Data necessary to give our triangle to OpenGL
	GLuint VertexBufferID;

	//example data that might be useful when trying to compute bounds on multi-shape
	vec3 gMin, gMax;

	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
		vec3 side;
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
		if (key == GLFW_KEY_Z && action == GLFW_PRESS) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		if (key == GLFW_KEY_Z && action == GLFW_RELEASE) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
		// WASD camera movement controls
		if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			camera.eye.x += SPEED * camera.lookAtPos.x;
			camera.eye.z += SPEED * camera.lookAtPos.z;
		}
		if (key == GLFW_KEY_S && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			camera.eye.x -= SPEED * camera.lookAtPos.x;
			camera.eye.z -= SPEED * camera.lookAtPos.z;
		}
		if (key == GLFW_KEY_D && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			side = cross(camera.lookAtPos, vec3(0, 1, 0));
			camera.eye.x += SPEED * side.x;
			camera.eye.z += SPEED * side.z;
		}
		if (key == GLFW_KEY_A && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			side = cross(camera.lookAtPos, vec3(0, 1, 0));
			camera.eye.x -= SPEED * side.x;
			camera.eye.z -= SPEED * side.z;
		}
		if (camera.eye.x > 90)
			camera.eye.x = 90;
		if (camera.eye.z > 90)
			camera.eye.z = 90;
		if (camera.eye.x < -90)
			camera.eye.x = -90;
		if (camera.eye.z < -90)
			camera.eye.z = -90;
		//kick ball
		if (key == GLFW_KEY_SPACE && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			if (animationSetter == KICK)
				animationSetter = STAND;
			else
				animationSetter++;
		} 
		// restart
		if (key == GLFW_KEY_R && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			animationSetter = STAND;
			kickRotation = 0.0f;
			kickedBall = false;
			miss = not miss;
			celebrateAngle = 0.0;
			kickOffset = (miss == true) ? 1.5f : 0.4f;
			kickerPos = vec3(-2.0f, 0.0f, 35.0f);
			ball.position = vec3(6.0f, 2.0f, 30.0f);
			ball.velocity = vec3(0.0f, 0.0f, 0.0f);
			ball.acceleration = vec3(0.0f, 0.0f, 0.0f);
			kickerFootBSphere.center = vec3(-2.0f, 0.0f, 35.0f);
			ball.bsphere.center = ball.position;
		}
	}

	void mouseCallback(GLFWwindow *window, int button, int action, int mods)
	{
		double posX, posY;

		if (action == GLFW_PRESS)
		{
			glfwGetCursorPos(window, &posX, &posY);
			cout << "Pos X " << posX <<  " Pos Y " << posY << endl;
		}
	}

	void scrollCallback(GLFWwindow *window, double in_deltaX, double in_deltaY) {

		camera.phi += 0.02 * in_deltaY;
		camera.theta -= 0.02 * in_deltaX;
	   
	    if (camera.phi > 4.0 * PI / 9.0) 
	    	camera.phi = 4.0 * PI / 9.0;
	    else if (camera.phi < -4.0 * PI / 9.0)
	    	camera.phi = -4.0 * PI / 9.0;
	}

	void resizeCallback(GLFWwindow *window, int width, int height) {
		glViewport(0, 0, width, height);
	}

	void init(const std::string& resourceDirectory) {	
		// checks GLSL version
		GLSL::checkVersion();

		// Set background color.
		glClearColor(0.00f, 0.50f, 0.94f, 1.0f);

		// Enable z-buffer test.
		glEnable(GL_DEPTH_TEST);

		// Initialize the GLSL lighting program.
		prog = make_shared<Program>();
		// set verbose attribute to true
		prog->setVerbose(true);

		// sets the name of the shaders as strings, these are attributes in Program class
		if (useCel == true) {
			light = vec3(-20, 30, 60);
			prog->setShaderNames(resourceDirectory + "/cel_vert.glsl", resourceDirectory + "/cel_frag.glsl");
		}
		else
			prog->setShaderNames(resourceDirectory + "/phong_vert.glsl", resourceDirectory + "/phong_frag.glsl");
		prog->init();
		// adds uniform variables to uniform hash tables
		prog->addUniform("P");
		prog->addUniform("V");
		prog->addUniform("M");
		prog->addAttribute("vertPos");
		prog->addAttribute("vertNor");
		prog->addUniform("MatDif");
		prog->addUniform("MatAmb");
		prog->addUniform("MatSpec");
		prog->addUniform("shine");
		prog->addUniform("lightPos");

		// Initialize the GLSL program.
		diffProg = make_shared<Program>();
		diffProg->setVerbose(true);
		diffProg->setShaderNames(resourceDirectory + "/diffuse_vert.glsl", resourceDirectory + "/diffuse_frag.glsl");
		diffProg->init();
		
		diffProg->addUniform("P");
		diffProg->addUniform("V");
		diffProg->addUniform("M");
		diffProg->addAttribute("vertPos");
		diffProg->addAttribute("vertNor");
		diffProg->addUniform("MatDif");
		diffProg->addUniform("MatAmb");
		diffProg->addUniform("lightPos");

		// Initialize the GLSL texture program.
		texProg = make_shared<Program>();
		// set verbose attribute to true
		texProg->setVerbose(true);
		// sets the name of the shaders as strings, these are attributes in texProgram class
		texProg->setShaderNames(resourceDirectory + "/tex_vert.glsl", resourceDirectory + "/tex_frag.glsl");
		texProg->init();
		// adds uniform variables to uniform hash tables
		texProg->addUniform("P");
		texProg->addUniform("V");
		texProg->addUniform("M");
		texProg->addAttribute("vertPos");
		texProg->addAttribute("vertNor");
		texProg->addAttribute("vertTex");
		texProg->addUniform("Texture");
		texProg->addUniform("lightPos");

		// Initialize the GLSL program for the skybox
		skyboxProg = make_shared<Program>();
		skyboxProg->setVerbose(true);
		skyboxProg->setShaderNames(resourceDirectory + "/skybox_vert.glsl", resourceDirectory + "/skybox_frag.glsl");
		skyboxProg->init();
		skyboxProg->addUniform("P");
		skyboxProg->addUniform("V");
		skyboxProg->addUniform("M");
		skyboxProg->addAttribute("vertPos");
		skyboxProg->addAttribute("vertNor");
	}

	// initializes all texture variables
	void initTex(const std::string& resourceDirectory) {
		ballTex = make_shared<Texture>();
		ballTex->setFilename(resourceDirectory + "/ballTex.jpeg");
		ballTex->init();
		ballTex->setUnit(0);
		ballTex->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
	}

	void initGeom(const std::string& resourceDirectory, char* objNoNormals) {

		// Initialize mesh
		vector<tinyobj::shape_t> TOshapes;
		vector<tinyobj::shape_t> modelVector;
		vector<tinyobj::material_t> objMaterials;
		string errStr;
		int i = 0;

		//load in ground mesh
		bool rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr, (resourceDirectory + "/plane.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			grass = make_shared<Shape>();
			grass->createShape(TOshapes[0]);
			grass->measure();
			grass->init();
		}

		// load in ball mesh
		rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr, (resourceDirectory + "/texSphere.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			ballPtr = make_shared<Shape>();
			ballPtr->createShape(TOshapes[0]);
			ballPtr->measure();
			ballPtr->init();
		}

		// loads in kicker dummy
		rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr, (resourceDirectory + "/dummy.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			while (i < DUMMY) {
				kicker[i] = make_shared<Shape>();
				kicker[i]->createShape(TOshapes[i]);
				kicker[i]->measure();
				kicker[i]->init();
				gMin.x =  kicker[i]->min.x;
				gMin.y = kicker[i]->min.y;
				gMin.z = kicker[i]->min.z;
				
				gMax.x =  kicker[i]->max.x;
				gMax.y = kicker[i]->max.y;
				gMax.z = kicker[i]->max.z;
				i++;
			}
		}

		rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr, (resourceDirectory + "/cylinder.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			goal = make_shared<Shape>();
			goal->createShape(TOshapes[0]);
			goal->measure();
			goal->init();
		}

		// imports the skybox cube program
		rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr, (resourceDirectory + "/cube.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			skybox = make_shared<Shape>();
			skybox->createShape(TOshapes[0]);
			skybox->measure();
			skybox->init();
		}
	}

	void createSky(string dir, vector<string> faces) { 
		//unsigned int textureID;
		glGenTextures(1, &skyboxID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxID);

		int width, height, nrChannels;
		stbi_set_flip_vertically_on_load(false);
		
		for(GLuint i = 0; i < faces.size(); i++) {
			unsigned char *data = stbi_load((dir+faces[i]).c_str(), &width, &height, &nrChannels, 0);
			
			if (data) {
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			} 
			else {
				cout << "failed to load: " << (dir+faces[i]).c_str() << endl;
			}
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		cout << " creating cube map any errors : " << glGetError() << endl;
	}

	void SetMaterial(int i) {
		switch (i) {
			case 0: //shiny blue plastic
				glUniform3f(prog->getUniform("MatAmb"), 0.12, 0.14, 0.3);
				glUniform3f(prog->getUniform("MatDif"), 0.0, 0.16, 0.9);
				glUniform3f(prog->getUniform("MatSpec"), 0.14, 0.2, 0.8);
				glUniform1f(prog->getUniform("shine"), 120.0);
				break;
			case 1: // flat grey
				glUniform3f(prog->getUniform("MatAmb"), 0.13, 0.13, 0.14);
				glUniform3f(prog->getUniform("MatDif"), 0.3, 0.3, 0.4);
				glUniform3f(prog->getUniform("MatSpec"), 0.3, 0.3, 0.4);
				glUniform1f(prog->getUniform("shine"), 20.0);
				break;
			case 2: //Red plastic
				glUniform3f(prog->getUniform("MatAmb"), 0.3294, 0.1235, 0.02745);
				glUniform3f(prog->getUniform("MatDif"), 0.7804, 0.2686, 0.11373);
				glUniform3f(prog->getUniform("MatSpec"), 0.9922, 0.541176, 0.40784);
				glUniform1f(prog->getUniform("shine"), 27.9);
				break;
			case 3: //emerald green
				glUniform3f(prog->getUniform("MatAmb"), 0.0715f, 0.2745f, 0.0715f);
				glUniform3f(prog->getUniform("MatDif"), 0.07568f, 0.11424f, 0.07568f);
				glUniform3f(prog->getUniform("MatSpec"), 0.133f, 0.127811f, 0.133f);
				glUniform1f(prog->getUniform("shine"), 1.0);
				break;
		}
	}

	// computes the centroid of the box
	glm::vec3 getCentroid(shared_ptr<Shape> s) {
		float x = ((s->min.x + s->max.x) / 2.0) * 0.1;
		float y = ((s->min.y + s->max.y) / 2.0) * 0.1;
		float z = ((s->min.z + s->max.z) / 2.0) * 0.1;
		return vec3(x, y, z);
	}

	void getKickerDisplacement(vec3 *movement, bool *wind) {
		// wind up for kick
		if (wind && movement->x >= -8.3) {
			movement->x += -0.07;
			movement->z += 0.07;
		}

		// moves back after kick
		if (*wind == false && movement->x >= 3) {
			*wind = true;
			movement->x += -0.07;
			movement->z += 0.07;
		}

		// run forward to kick
		if (movement->x <= -8.3 || *wind == false) {
			*wind = false;
			movement->x -= -0.3;
			movement->z -= 0.3;
		}
	}

	float getKneeAngle(float PHI) {
		int direction = 1;

		if (PHI >= 0.5)
			direction = -1;
		else if (PHI < -3)
			direction = 1;
		PHI += (direction == 1) ? 0.3 : -0.2;
		return PHI;
	}

	void setModel(std::shared_ptr<Program> prog, std::shared_ptr<MatrixStack>M) {
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, value_ptr(M->topMatrix()));
    }

    void drawSkybox(shared_ptr<MatrixStack> P, shared_ptr<MatrixStack> V) {
    	auto M = make_shared<MatrixStack>();
    	M->pushMatrix();
    		M->translate(vec3(0, 30, 0));
    		M->scale(vec3(200, 200, 200));

   		//to draw the sky box bind the right shader
		skyboxProg->bind();
		//set the projection matrix - can use the same one
		glUniformMatrix4fv(skyboxProg->getUniform("P"), 1, GL_FALSE,
 		value_ptr(P->topMatrix()));
		
		//set the depth function to always draw the box!
		glDepthFunc(GL_LEQUAL);

		//set up view matrix to include your view transforms 
		glUniformMatrix4fv(skyboxProg->getUniform("V"), 1,
		GL_FALSE,value_ptr(V->topMatrix()));

		//set and send model transforms - likely want a bigger cube
		glUniformMatrix4fv(skyboxProg->getUniform("M"), 1, GL_FALSE, value_ptr(M->topMatrix()));

		//bind the cube map texture
		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxID);

		//draw the actual cube
		skybox->draw(skyboxProg);

		//set the depth test back to normal!
		glDepthFunc(GL_LESS);

		//unbind the shader for the skybox
		skyboxProg->unbind(); 
		M->popMatrix();
    }

    //Creates and draws a hierarchical model of the dummy obj mesh file
    void drawPlayer(std::shared_ptr<Shape> *player, 
    	glm::vec3 trans, float rotateOrientation) {
    	
    	auto Model = make_shared<MatrixStack>();

    	// draw pelvis
    	Model->pushMatrix();
  			// translate to proper position on the field
    		Model->translate(trans);
    		// rotate to face correct direction
    		Model->rotate(rotateOrientation, vec3(0, 1, 0));
    		// rotate model upright
    		Model->rotate(-PI / 2.0, vec3(1, 0, 0));

    		// render left legs
    		Model->pushMatrix();
    			Model->translate(getCentroid(player[0]));
    			Model->rotate(-0.6, vec3(0, 1, 0));
    			Model->translate(vec3(-1, -1, -1) * getCentroid(player[0]));
    			
    			// transform knee
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[2]));
    				Model->rotate(1, vec3(0, 1, 0));
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[2]));

    				// transform ankle
    				Model->pushMatrix();
    					// transform to correct position
    					Model->translate(getCentroid(player[4]));
    					// rotate ankel
    					Model->rotate(-0.4, vec3(0, 1, 0));
    					// transform to center
	    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[4]));
	    				Model->scale(vec3(0.1, 0.1, 0.1));
		    			// send to GPU
		    			setModel(prog, Model);
		    			// draw thigh
		    			player[4]->draw(prog);
		    			player[5]->draw(prog);
    				Model->popMatrix();

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw thigh
	    			player[2]->draw(prog);
	    			player[3]->draw(prog);
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[0]->draw(prog);
    			player[1]->draw(prog);
    		Model->popMatrix();

    		// render right leg
    		Model->pushMatrix();
    			Model->translate(getCentroid(player[25]));
    			Model->rotate(-0.6, vec3(0, 1, 0));
    			Model->translate(vec3(-1, -1, -1) * getCentroid(player[25]));
    			
    			// transform knee
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[16]));
    				Model->rotate(1, vec3(0, 1, 0));
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[16]));

    				// transform ankle
    				Model->pushMatrix();
    					// transform to correct position
    					Model->translate(getCentroid(player[20]));
    					// rotate ankel
    					Model->rotate(-0.4, vec3(0, 1, 0));
    					// transform to center
	    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[20]));
	    				Model->scale(vec3(0.1, 0.1, 0.1));
		    			// send to GPU
		    			setModel(prog, Model);
		    			// draw thigh
		    			player[20]->draw(prog);
		    			player[26]->draw(prog);
    				Model->popMatrix();

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw thigh
	    			player[16]->draw(prog);
	    			player[19]->draw(prog);
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[25]->draw(prog);
    			player[14]->draw(prog);
    		Model->popMatrix();

    		// render left shoulder
    		Model->pushMatrix();
    			Model->translate(getCentroid(player[11]));
    			Model->rotate(-PI / 2.0, vec3(1, 0, 0));
    			Model->rotate(-0.6, vec3(0, 0, 1));
    			Model->translate(vec3(-1, -1, -1) * getCentroid(player[11]));
    			
    			// transform left elbow
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[9]));
    				Model->rotate(-PI / 6.0, vec3(0, 0, 1));
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[9]));

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw shoulder
	    			player[9]->draw(prog);
	    			player[8]->draw(prog);
	    			player[7]->draw(prog);
	    			player[6]->draw(prog);
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[11]->draw(prog);
    			player[10]->draw(prog);
    		Model->popMatrix();

    		// render right shoulder
    		Model->pushMatrix();
    			Model->translate(getCentroid(player[15]));
    			Model->rotate(PI / 2.0, vec3(1, 0, 0));
    			Model->rotate(0.6, vec3(0, 0, 1));
    			Model->translate(vec3(-1, -1, -1) * getCentroid(player[15]));
    			
    			// transform right elbow
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[18]));
    				Model->rotate(PI / 6.0, vec3(0, 0, 1));
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[18]));

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw shoulder
	    			player[18]->draw(prog);
	    			player[27]->draw(prog);
	    			player[28]->draw(prog);
	    			player[22]->draw(prog);
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[15]->draw(prog);
    			player[12]->draw(prog);
    		Model->popMatrix();

    		// send torso to GPU
    		Model->scale(vec3(0.1, 0.1, 0.1));
    		setModel(prog, Model); 
    		// draw neck
    		player[13]->draw(prog);
    		// draw head
    		player[17]->draw(prog);
    		// draw chest
    		player[21]->draw(prog);
    		// draw abs
    		player[23]->draw(prog);
    		//draw pelvis
    		player[24]->draw(prog);
		Model->popMatrix();
    }

    void drawKickerWindUp(std::shared_ptr<Shape> *player, 
    	glm::vec3 trans, float rotateOrientation, vec3 *movement) {

    	auto Model = make_shared<MatrixStack>();
    	float PHI_thigh = 0.4 * sin(5.0 * glfwGetTime());
    	float PHI_knee = getKneeAngle(PHI_thigh);
    	getKickerDisplacement(movement, &wind);

    	// draw pelvis
    	Model->pushMatrix();
  			// translate to proper position on the field
    		Model->translate(trans + *movement);
    		// rotate to face correct direction
    		Model->rotate(rotateOrientation, vec3(0, 1, 0));
    		// rotate model upright
    		Model->rotate(-PI / 2.0, vec3(1, 0, 0));

    		// render left legs
    		Model->pushMatrix();
    			Model->translate(getCentroid(player[0]));
    			Model->rotate(PHI_thigh, vec3(0, 1, 0));
    			Model->translate(vec3(-1, -1, -1) * getCentroid(player[0]));
    			
    			// transform left knee
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[2]));
    				Model->rotate(PHI_knee, vec3(0, 1, 0));
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[2]));

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw thigh
	    			player[2]->draw(prog);
	    			player[3]->draw(prog);
	    			player[4]->draw(prog);
		    		player[5]->draw(prog);
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[0]->draw(prog);
    			player[1]->draw(prog);
    		Model->popMatrix();

    		// render right leg
    		Model->pushMatrix();
    			Model->translate(getCentroid(player[25]));
    			Model->rotate(-1 * PHI_thigh, vec3(0, 1, 0));
    			Model->translate(vec3(-1, -1, -1) * getCentroid(player[25]));
    			
    			// transform right knee
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[16]));
    				Model->rotate(PI / 6.0 - PHI_knee, vec3(0, 1, 0));
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[16]));

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw thigh
	    			player[16]->draw(prog);
	    			player[19]->draw(prog);
	    			player[20]->draw(prog);
		    		player[26]->draw(prog);
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[25]->draw(prog);
    			player[14]->draw(prog);
    		Model->popMatrix();

    		// render left shoulder
    		Model->pushMatrix();
    			Model->translate(getCentroid(player[11]));
    			Model->rotate(-PI / 2.0, vec3(1, 0, 0));
    			Model->rotate(-1.0 * PHI_thigh, vec3(0, 0, 1));
    			Model->translate(vec3(-1, -1, -1) * getCentroid(player[11]));
    			
    			// transform left elbow
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[9]));
    				Model->rotate(-PI / 6.0, vec3(0, 0, 1));
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[9]));

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw shoulder
	    			player[9]->draw(prog);
	    			player[8]->draw(prog);
	    			player[7]->draw(prog);
	    			player[6]->draw(prog);
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[11]->draw(prog);
    			player[10]->draw(prog);
    		Model->popMatrix();

    		// render right shoulder
    		Model->pushMatrix();
    			Model->translate(getCentroid(player[15]));
    			Model->rotate(PI / 2.0, vec3(1, 0, 0));
    			Model->rotate(-1.0 * PHI_thigh, vec3(0, 0, 1));
    			Model->translate(vec3(-1, -1, -1) * getCentroid(player[15]));
    			
    			// transform right elbow
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[18]));
    				Model->rotate(PI / 6.0, vec3(0, 0, 1));
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[18]));

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw shoulder
	    			player[18]->draw(prog);
	    			player[27]->draw(prog);
	    			player[28]->draw(prog);
	    			player[22]->draw(prog);
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[15]->draw(prog);
    			player[12]->draw(prog);
    		Model->popMatrix();

    		// send torso to GPU
    		Model->scale(vec3(0.1, 0.1, 0.1));
    		setModel(prog, Model); 
    		// draw neck
    		player[13]->draw(prog);
    		// draw head
    		player[17]->draw(prog);
    		// draw chest
    		player[21]->draw(prog);
    		// draw abs
    		player[23]->draw(prog);
    		//draw pelvis
    		player[24]->draw(prog);
		Model->popMatrix();
    }

    void drawKick(std::shared_ptr<Program> prog, std::shared_ptr<Shape> *player, 
    	glm::vec3 trans, float rotateOrientation) {

    	auto Model = make_shared<MatrixStack>();
    	if (kickRotation < 0.48) {
    		kickRotation += 0.02;
    	}
    	float PHI_thigh = 1.5 * sin(7.0 * kickRotation) - 6.0;

    	// draw pelvis
    	Model->pushMatrix();
  			// translate to proper position on the field
    		Model->translate(trans);
    		// rotate to face correct direction
    		Model->rotate(rotateOrientation, vec3(0, 1, 0));
    		// rotate model upright
    		Model->rotate(-PI / 2.0, vec3(1, 0, 0));

    		// render left legs
    		Model->pushMatrix();
   
    			// transform left knee
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[2]));
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[2]));

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw thigh
	    			player[2]->draw(prog);
	    			player[4]->draw(prog);
	    			player[3]->draw(prog);
		    		player[5]->draw(prog);
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[0]->draw(prog);
    			player[1]->draw(prog);
    		Model->popMatrix();

    		// render right leg
    		Model->pushMatrix();
    			Model->translate(getCentroid(player[25]));
    			Model->rotate(-1 * PHI_thigh, vec3(0, 1, 0));
    			Model->translate(vec3(-1, -1, -1) * getCentroid(player[25]));
    			
    			// transform right knee
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[16]));
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[16]));

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw thigh
	    			player[16]->draw(prog);
	    			player[19]->draw(prog);
	    			player[20]->draw(prog);
		    		player[26]->draw(prog);

		    		vec4 tmpVec = Model->topMatrix() * vec4(kickerFootBSphere.center.x, 
		    			kickerFootBSphere.center.y, kickerFootBSphere.center.z, 1.0);
		    		kickerFootBSphere.center.x = tmpVec.x;
		    		kickerFootBSphere.center.y = tmpVec.y;
		    		kickerFootBSphere.center.z = tmpVec.z;
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[25]->draw(prog);
    			player[14]->draw(prog);
    		Model->popMatrix();

    		// render left shoulder
    		Model->pushMatrix();
    			Model->translate(getCentroid(player[11]));
    			Model->rotate(-PI / 2.0, vec3(1, 0, 0));
    			if (miss == false && ball.position.y == 1.0) {
    				Model->rotate(celebrateAngle, vec3(1, 0, 0));
    			}
    			else {
    				Model->rotate(-1.0 * PHI_thigh, vec3(0, 0, 1));
    			}
    			Model->translate(vec3(-1, -1, -1) * getCentroid(player[11]));
    			
    			// transform left elbow
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[9]));
    				if (miss == false && ball.position.y == 1.0) {
    					Model->rotate(0.0, vec3(1, 0, 0));
    				}
    				else {
    					Model->rotate(-PI / 6.0, vec3(0, 0, 1));
    				}
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[9]));

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw shoulder
	    			player[9]->draw(prog);
	    			player[8]->draw(prog);
	    			player[7]->draw(prog);
	    			player[6]->draw(prog);
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[11]->draw(prog);
    			player[10]->draw(prog);
    		Model->popMatrix();

    		// render right shoulder
    		Model->pushMatrix();
    			Model->translate(getCentroid(player[15]));
    			Model->rotate(PI / 2.0, vec3(1, 0, 0));
    			if (miss == false && ball.position.y == 1.0) {
    				Model->rotate(-1.0 * celebrateAngle, vec3(1, 0, 0));
    			}
    			else {
    				Model->rotate(-1.0 * PHI_thigh, vec3(0, 0, 1));
    			}
    			Model->translate(vec3(-1, -1, -1) * getCentroid(player[15]));
    			
    			// transform right elbow
    			Model->pushMatrix();
    				Model->translate(getCentroid(player[18]));
    				if (miss == false && ball.position.y == 1.0) {
    					Model->rotate(0.0, vec3(1, 0, 0));
    				}
    				else {
    					Model->rotate(PI / 6.0, vec3(0, 0, 1));
    				}
    				Model->translate(vec3(-1, -1, -1) * getCentroid(player[18]));

    				Model->scale(vec3(0.1, 0.1, 0.1));
	    			// send to GPU
	    			setModel(prog, Model);
	    			// draw shoulder
	    			player[18]->draw(prog);
	    			player[27]->draw(prog);
	    			player[28]->draw(prog);
	    			player[22]->draw(prog);
	    		Model->popMatrix();
    			
    			Model->scale(vec3(0.1, 0.1, 0.1));
    			// send to GPU
    			setModel(prog, Model);
    			// draw thigh
    			player[15]->draw(prog);
    			player[12]->draw(prog);
    		Model->popMatrix();

    		// send torso to GPU
    		Model->scale(vec3(0.1, 0.1, 0.1));
    		setModel(prog, Model); 
    		// draw neck
    		player[13]->draw(prog);
    		// draw head
    		player[17]->draw(prog);
    		// draw chest
    		player[21]->draw(prog);
    		// draw abs
    		player[23]->draw(prog);
    		//draw pelvis
    		player[24]->draw(prog);
		Model->popMatrix();

		if (celebrateAngle < PI - 0.2)
			celebrateAngle += 0.07;
	}

    void drawField(shared_ptr<MatrixStack> P, shared_ptr<MatrixStack> V) {

    	auto M = make_shared<MatrixStack>();

    	diffProg->bind();
    	glUniform3f(diffProg->getUniform("lightPos"), light.x, light.y, light.z);
    	glUniform3f(diffProg->getUniform("MatAmb"), 0.0715f, 0.2745f, 0.0715f);
		glUniform3f(diffProg->getUniform("MatDif"), 0.07568f, 0.11424f, 0.07568f);
		glUniformMatrix4fv(diffProg->getUniform("P"), 1, GL_FALSE, value_ptr(P->topMatrix()));
		glUniformMatrix4fv(diffProg->getUniform("V"), 1, GL_FALSE, value_ptr(V->topMatrix()));
    	
		M->pushMatrix();
				M->scale(vec3(200.0f, 1.0f, 200.0f));
				setModel(diffProg, M);
				grass->draw(diffProg);
		M->popMatrix();
		diffProg->unbind();
    }

    void drawGoal(shared_ptr<MatrixStack> P, shared_ptr<MatrixStack> V) {

    	auto M = make_shared<MatrixStack>();

		SetMaterial(GOAL);

		// top post
		M->pushMatrix();
			M->translate(vec3(0.0f, 32.0f, -44.0f));

			// right post
			M->pushMatrix();
				M->translate(vec3(29.0f, -17.0f, 0.0f));
				M->scale(vec3(0.6f, 10.0f, 0.6f));
				setModel(prog, M);
				goal->draw(prog);
			M->popMatrix();

			// left post
			M->pushMatrix();
				M->translate(vec3(-27.0f, -17.0f, 0.0f));
				M->scale(vec3(0.6f, 10.0f, 0.6f));
				setModel(prog, M);
				goal->draw(prog);
			M->popMatrix();


			M->rotate(PI / 2.0, vec3(0, 0, 1));
			M->scale(vec3(0.6f, 17.0f, 0.6f));
			setModel(prog, M);
			goal->draw(prog);
		M->popMatrix();
    }

    void drawSoccerBall(vec3 pos, shared_ptr<MatrixStack> P, shared_ptr<MatrixStack> V) {

    	if (glm::distance(ball.bsphere.center, kickerFootBSphere.center) < ball.bsphere.radius + kickerFootBSphere.radius) {
    		kickedBall = true;
			ball.acceleration = vec3(kickOffset, 2.0, -3.0);
		}

    	auto M = make_shared<MatrixStack>();
    	
		texProg->bind();
    	glUniformMatrix4fv(texProg->getUniform("P"), 1, GL_FALSE, value_ptr(P->topMatrix()));
		glUniformMatrix4fv(texProg->getUniform("V"), 1, GL_FALSE, value_ptr(V->topMatrix()));
		glUniform3f(texProg->getUniform("lightPos"), light.x, light.y, light.z);

    	// render grass texture
    	ballTex->bind(texProg->getUniform("Texture"));
		// ball transformations
		M->pushMatrix();
			// soccer ball physically based transforms
			if (kickedBall) {
				ball.acceleration.y -= GRAVITY;
				ball.acceleration.y = ball.acceleration.y - GRAVITY;
				ball.velocity = ball.acceleration;
				ball.position += ball.velocity;
				ball.bsphere.center = ball.position;

				// check if ball missed
				if (-29.0 * ball.position.x + 32 * ball.position.y - 44.0 <= 0.0 && ball.position.x > 28.0) {
					ball.acceleration.x = -0.6 * ball.acceleration.x;
					ball.acceleration.z = -0.5*ball.acceleration.z;
				}

				if (ball.position.y < 1.0) {
					ball.position.y = 1.0;
					kickedBall = false;
				}
			}

			M->translate(ball.position);
			setModel(texProg, M);
			ballPtr->draw(texProg);
		M->popMatrix();
		texProg->unbind();
    }

	void render() {

		// Create the matrix stacks - please leave these alone for now
		auto Projection = make_shared<MatrixStack>();
		auto View = make_shared<MatrixStack>();
		auto Model = make_shared<MatrixStack>();

		// Get current frame buffer size.
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		glViewport(0, 0, width, height);

		// Clear framebuffer.
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Use the local matrices for lab 4
		float aspect = width/(float)height;
		// Apply perspective projection.
		Projection->pushMatrix();
		Projection->perspective(45.0f, aspect, 0.01f, 300.0f);

		// View is global translation along negative z for now
		camera.lookAtPos.x = cos(camera.phi) * cos(camera.theta);
		camera.lookAtPos.y = sin(camera.phi);
		camera.lookAtPos.z = cos(camera.phi) * cos((PI / 2.0) - camera.theta);
		View->pushMatrix();
			View->lookAt(camera.eye, camera.eye + camera.lookAtPos, vec3(0, 1, 0));

		// draws the skybox
		drawSkybox(Projection, View);

		// draw soccer ball
		drawSoccerBall(ball.position, Projection, View);
		drawField(Projection, View);

		prog->bind();
		glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
		glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, value_ptr(View->topMatrix()));
		glUniform3f(prog->getUniform("lightPos"), light.x, light.y, light.z);

		drawGoal(Projection, View);

		// render kicker
		SetMaterial(OFFENSE);
		if (animationSetter == STAND) {
			drawPlayer(kicker, kickerPos, PI / 3.0);
		} else if (animationSetter == WINDUP) {
			drawKickerWindUp(kicker, vec3(0.0, 0.0, 0.0), PI / 3.0, &kickerPos);
		} else {
			drawKick(prog, kicker, kickerPos, PI / 3.0);
		}

		// render goalie and defensemen
		SetMaterial(DEFENSE);
		drawPlayer(kicker, glm::vec3(0.0, 0.0, -35.0), 3.0 * PI / 2.0);
		drawPlayer(kicker, glm::vec3(-30.0, 0.0, 0.0), 3.0 * PI / 2.0);
		drawPlayer(kicker, glm::vec3(20.0, 0.0, -10.0), 3.0 * PI / 2.0);

		prog->unbind();
		
		// Pop matrix stacks
		Projection->popMatrix();
		View->popMatrix();
	}
};

int main(int argc, char *argv[])
{
	vector<std::string> faces {
		"hills_lf.tga",
		"hills_rt.tga",
		"hills_up.tga",
		"hills_dn.tga",
		"hills_ft.tga",
		"hills_bk.tga"
	};

	// Where the resources are loaded from
	std::string resourceDir = "../resources";

	// Creates a new Application object
	Application *application = new Application();

	// Your main will always include a similar set up to establish your window
	// and GL context, etc.
	WindowManager *windowManager = new WindowManager();
	windowManager->init(640, 480);
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;

	// This is the code that will likely change program to program as you
	// may need to initialize or set up different data and state
	// Command line args
	if (argc == 2) {
		if (*argv[1] == '1') {
			application->useCel = true;
		}
	}
	else if (argc == 3) {
		if (*argv[1] == '1') {
			application->useCel = true;
		}
		resourceDir = argv[2];
	}

	application->init(resourceDir);
	application->initGeom(resourceDir, argv[1]);
	application->initTex(resourceDir);
	application->createSky(resourceDir + "/skybox/", faces);
	
	// Loop until the user closes the window.
	while (! glfwWindowShouldClose(windowManager->getHandle()))
	{
		// Render scene.
		application->render();

		// Swap front and back buffers.
		glfwSwapBuffers(windowManager->getHandle());
		// Poll for and process events.
		glfwPollEvents();
	}

	// Quit program.
	windowManager->shutdown();
	return 0;
}