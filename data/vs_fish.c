const float pi = 3.14159265358979323846;

void main(){
	gl_FrontColor = gl_Color;
	gl_FrontSecondaryColor = gl_SecondaryColor;
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;

	// apply modelview matrix
	vec4 v = gl_ModelViewMatrix * gl_Vertex;

	// angle with n=(0,0,-1): acos(dot(n,v.xyz)/|n|/|v.xyz|
	float w = acos(-v.z/length(v.xyz));

	// get f and rat from projection matrix
	float rat = gl_ProjectionMatrix[0][0];
	float f   = gl_ProjectionMatrix[1][1];
	float fm  = gl_ProjectionMatrix[2][2];

	// fisheye
	float r = 0.0;
	if(fm==-1.0) r = f*tan(w);         // gnomonisch
	if(fm== 0.0) r = 2.0*f*tan(w/2.0); // winkeltreu
	if(fm== 1.0) r = f*w;              // äquidistant
	if(fm== 2.0) r = 2.0*f*sin(w/2.0); // flächentreu
	if(fm== 3.0) r = f*sin(w);         // orthographisch

	// final position = scale v.xy with r and rat
	float s = r/length(v.xy);
	gl_Position.x = v.x * s / rat;
	gl_Position.y = v.y * s;
	gl_Position.z = w/pi;
	gl_Position.w = 1.0;
}
