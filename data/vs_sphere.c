const float pi = 3.14159265358979323846;

uniform vec2 arg;

void main(){
	gl_FrontColor = gl_Color;
	gl_FrontSecondaryColor = gl_SecondaryColor;
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;

	// apply modelview matrix
	vec4 v = gl_ModelViewMatrix * gl_Vertex;
	v.x+=arg.x;
	v.y+=arg.y;

	if(v.y==0){
		v.z=v.x=0;
		v.y=-1;
	}else if(v.y==1){
		v.z=v.x=0;
		v.y=1;
	}else{
		float p=(v.x-0.5)*pi*2;
		float t=pi/2-atan(sinh((0.5-v.y)*2*pi));
		float r=v.z;
		v.z=-r*sin(t)*cos(p);
		v.y=-r*cos(t);
		v.x=r*sin(t)*sin(p);
	}

	gl_Position = gl_ProjectionMatrix * v;
}

