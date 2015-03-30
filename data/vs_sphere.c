const float pi = 3.14159265358979323846;

uniform vec3 arg; /* psy,sgy,cgy */

void main(){
	gl_FrontColor = gl_Color;
	gl_FrontSecondaryColor = gl_SecondaryColor;
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;

	// apply modelview matrix
	vec4 v = gl_ModelViewMatrix * gl_Vertex;

	float py=v.y+arg.x;
	float sgy=arg.y;
	float cgy=arg.z;
	if(py<=0){
		v.x=0;
		v.y=-cgy;
		v.z=-sgy;
	}else if(py>=1){
		v.x=0;
		v.y=cgy;
		v.z=sgy;
	}else{
		v.xy=v.xy*2*pi;
		float svx=sin(v.x);
		float cvx=cos(v.x);
		float svy=sinh(v.y);
		float cvy=cosh(v.y);
		float f=cgy/(cvy-svy*sgy);

		v.x=f*svx;
		v.y=f*((cvx-cvy)*sgy+svy);
		v.z=f*(cvy-cvx)*cgy-1;
	}

	gl_Position = gl_ProjectionMatrix * v;
}

