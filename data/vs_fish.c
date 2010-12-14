void main(){
	gl_FrontColor = gl_Color;
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;

	// apply modelview matrix
	vec4 v = gl_ModelViewMatrix * gl_Vertex;

	// angle with n=(0,0,-1): acos(dot(n,v.xyz)/|n|/|v.xyz|
	float w = acos(-v.z/length(v.xyz));

	// get f and rat from projection matrix
	float rat = gl_ProjectionMatrix[0][0];
	float f = gl_ProjectionMatrix[1][1];

	// fisheye
	float r = 0;
	if(gl_ProjectionMatrix[2][2]==-1) r = f*tan(w);     // gnomonisch
	if(gl_ProjectionMatrix[2][2]== 0) r = 2*f*tan(w/2); // winkeltreu
	if(gl_ProjectionMatrix[2][2]== 1) r = f*w;          // äquidistant
	if(gl_ProjectionMatrix[2][2]== 2) r = 2*f*sin(w/2); // flächentreu
	if(gl_ProjectionMatrix[2][2]== 3) r = f*sin(w);     // orthographisch

	// final position = scale v.xy with r and rat
	float s = r/length(v.xy);
	gl_Position.x = v.x * s / rat;
	gl_Position.y = v.y * s;
	gl_Position.z = w/180;
	gl_Position.w = 1;
}
