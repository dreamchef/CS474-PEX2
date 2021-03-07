#include "draw.h"
#include <iostream>

/////////////////
// Function   : draw()
// Parameters : int width		number of columns
//              int height		number of rows
//              HDC &img		windows image buffer
//				Scene scene		information about the scene (see scene.h)
//
/////////////////
void draw(
	int width,
	int height,
	HDC& img,
	Scene scene)
{

	// SCENE-WIDE VARIABLES ====================

	// Depth buffer
	float z, bufWatcher;
	buf2d depthBuffer;
	depthBuffer.init(width, height, -1000);

	// Camera variables/constants
	vec4f camPos = scene.cameraLocation;
	vec4f camLook = scene.cameraLookAt;
	vec4f camUp = scene.cameraUp;
	float FOV = scene.cameraFOV;
	float camNear = 0.01;
	float camFar = 100;

	// FACE INSTANCE VARIABLES =================

	// Local array of face objects.
	// face.at(i) references ith vertex
	std::vector<vec4f> face;
	face.resize(3);
	float y;

	// VERTEX INSTANCE VARIABLES ===============

	// Vertex color
	float r, g, b;

	// SCANLINE VARIABLES =============

	float x0, x1;
	float DeltaX0, DeltaX1;
	
	// LOOP THROUGH FACES (SHAPES) ===========================================
	for (int i = 0; i < scene.model.face.size(); i++) {

		// Read color from first vertex in face
		r = scene.model.faceColor.at((scene.model.face.at(i).at(0))).x * 255;
		g = scene.model.faceColor.at((scene.model.face.at(i).at(0))).y * 255;
		b = scene.model.faceColor.at((scene.model.face.at(i).at(0))).z * 255;

		// TODO: Read other vertex colors for interpolation in scanline stage

		// Copy faces from scene argument into local variable
		face.at(0) = scene.model.vertex.at(scene.model.face.at(i).at(0));
		face.at(1) = scene.model.vertex.at(scene.model.face.at(i).at(1));
		face.at(2) = scene.model.vertex.at(scene.model.face.at(i).at(2));

		// World to camera transformation

		vec4f n = camPos - camLook;
		
		vec4f u;
		u.x = (camUp.x * n.y) - (camUp.y * n.x);
		u.y = (camUp.y * n.z) - (camUp.z * n.y);
		u.z = (camUp.z * n.x) - (camUp.x * n.z);
		float uNorm = sqrt(pow(u.x, 2) + pow(u.y, 2) + pow(u.z, 2));
		u.x /= uNorm;
		u.y /= uNorm;
		u.z /= uNorm;
		
		vec4f v;
		v.x = (n.x * u.y) - (n.y * u.x);
		v.y = (n.y * u.z) - (n.z * u.y);
		v.z = (n.z * u.x) - (n.x * u.z);
		float vNorm = sqrt(pow(v.x, 2) + pow(v.y, 2) + pow(v.z, 2));
		v.x /= vNorm;
		v.y /= vNorm;
		v.z /= vNorm;
		
		/*
		For each vertex in the face...

				[ ux uy yz 0]	[ 1 0 0 -camPosx ]
		Tcw =	[ vx vy vz 0] * [ 0 1 0 -camPosy ]
				[ nz ny nz 0]	[ 0 0 1 -camPosz ]
				[ 0  0  0  1]	[ 0 0 0 1		 ]
		*/
		for(int j=0; j<3; j++) {
			
			// Translation matrix
			face.at(j).x = face.at(j).x - camPos.x;
			face.at(j).y = face.at(j).y - camPos.y;
			face.at(j).z = face.at(j).z - camPos.z;

			// Rotation matrix
			face.at(j).x = face.at(j).x * u.x + face.at(j).y * u.y + face.at(j).z * u.z;
			face.at(j).y = face.at(j).x * v.x + face.at(j).y * v.y + face.at(j).z * v.z;
			face.at(j).z = face.at(j).x * n.x + face.at(j).y * n.y + face.at(j).z * n.z;
		}

		// Perspective Projection transformation
		
		for (int j = 0; j < 3; j++) { // For each vertex in face

			face.at(j).x = face.at(j).x / tan(FOV * PI / 360);
			face.at(j).y = face.at(j).y / tan(FOV * PI / 360);
			face.at(j).z = face.at(j).z * ((camFar + camNear)/(camFar - camNear)) + face.at(j).w * ((2 * camFar * camNear) / (camFar - camNear));
			face.at(j).w = -face.at(j).z;
		}
	
		// Viewport transformation
		for (int j = 0; j < 3; j++) { // For each vertex in face

			// Scale
			face.at(j).x = face.at(j).x * width * 0.5;
			face.at(j).y = face.at(j).y * height * 0.5;

			//Offset
			face.at(j).x = face.at(j).x + face.at(j).w * width * 0.5;
			face.at(j).y = face.at(j).y + face.at(j).w * height * 0.5;
		}

		// Device transformation
		for (int j = 0; j < 3; j++) { // For each vertex in face

			// Scale
			face.at(j).x = face.at(j).x / 10;
			face.at(j).y = face.at(j).y / 10;
		}

		// Sort vertices by y-position
		auto compareVertices = [](vec4f a, vec4f b) { // std sort comparison function (y-pos)
			return (a.y < b.y);
		};
		std::sort(face.begin(), face.end(), compareVertices);

		// Set both starting scanline bounds, y and z at vertex with lowest device y-position
		x0 = face.at(0).x;
		x1 = face.at(0).x;
		y = face.at(0).y;
		z = face.at(0).z;

		// Set delta-0 for longest side
		DeltaX0 = (face.at(0).x - face.at(2).x) / (face.at(0).y - face.at(2).y);

		// Set delta-1 for upper half
		DeltaX1 = (face.at(0).x - face.at(1).x) / (face.at(0).y - face.at(1).y);

		//max(x1+DeltaX1,x0+DeltaX0) < max(face.at(0).x, face.at(1).x) && min(x1 + DeltaX1, x0 + DeltaX0) > min(face.at(0).x, face.at(1).x)

		// Draw upper triangle
		while (y <= face.at(1).y) { // while above middle vertex

			for (int x = min(x0, x1); x < max(x0, x1); x++) { // iterate across scanline

				if (x < width && y < height) {
					if (z >= depthBuffer[x][(int)y]) {
						SetPixelV(img, x, (int)y, RGB(r, g, b)); //TODO: interpolate exact color (use lighting info)
						//bufWatcher = depthBuffer[x][(int)y];
						depthBuffer[x][(int)y] = z;
					}
				}
			}
			// adjust deltas
			x0 += DeltaX0; 
			x1 += DeltaX1;

			y++; // move to next scanline
		}
		
		x1 = face.at(1).x;
		
		// Set delta-1 for lower half
		DeltaX1 = (face.at(1).x - face.at(2).x) / (face.at(1).y - face.at(2).y);

		//max(x1 + DeltaX1, x0 + DeltaX0) < max(face.at(0).x, face.at(1).x) && min(x1 + DeltaX1, x0 + DeltaX0) > min(face.at(0).x, face.at(1).x)	

		// Draw lower triangle
		while (y <= face.at(2).y) {
			for (int x = min(x0, x1); x < max(x0, x1); x++) {
				
				if (x < width && y < height) {
					if (z >= depthBuffer[x][(int)y]) {
						SetPixelV(img, x, (int)y, RGB(r, g, b));
						//bufWatcher = depthBuffer[x][(int)y];
						depthBuffer[x][(int)y] = z;
					}
				}
			}
			x0 += DeltaX0;
			x1 += DeltaX1;
			y++;
		}
	}
}