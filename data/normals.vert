uniform vec3 lightPos;
varying vec3 V, N, L;

void main()
{
	//eye-space normal
	N = normalize(gl_NormalMatrix * gl_Normal);
	
	//eye-space light vector
	V = vec3(gl_ModelViewMatrix * gl_Vertex);
	L = normalize(gl_NormalMatrix * (lightPos - gl_Vertex.xyz));
	
	//bypass
	gl_FrontColor = gl_Color;
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_TexCoord[1].xyz = gl_NormalMatrix * (gl_Color.rgb*2.0-1.0);
}