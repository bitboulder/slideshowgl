uniform sampler2D tex;
const float a=0.5;
void main(){
	vec4 color;
	float ap=0.5*(1.0+a);
	float am=0.5*(1.0-a);

	// texture enabled?
	if(gl_SecondaryColor[0]>0.0){
		color = texture2D(tex,gl_TexCoord[0].st);

		// colmod
		if(gl_Color.x!=0.5) color.xyz = pow(color.xyz,-log2(gl_Color.x));
		if(gl_Color.y!=0.5) color.xyz = (0.5-color.xyz)*log2(1.0-gl_Color.y) + 0.5;
		if(gl_Color.z!=0.5) color.xyz = color.xyz + (gl_Color.z - 0.5)*2.0;

		color.a = color.a * gl_Color.a;
	}else{
		color = gl_Color;
	}

	// output
	gl_FragColor = color;
	if(color.x>=color.y && color.y>=color.z){
		gl_FragColor.x=ap*color.x+am*color.z;
		gl_FragColor.z=am*color.x+ap*color.z;
		gl_FragColor.y=am*color.x+am*color.z+a*color.y;
	}else if(color.x>=color.z && color.z>=color.y){
		gl_FragColor.x=ap*color.x+am*color.y;
		gl_FragColor.y=am*color.x+ap*color.y;
		gl_FragColor.z=am*color.x+am*color.y+a*color.z;
	}else if(color.y>=color.x && color.x>=color.z){
		gl_FragColor.y=ap*color.y+am*color.z;
		gl_FragColor.z=am*color.y+ap*color.z;
		gl_FragColor.x=am*color.y+am*color.z+a*color.x;
	}else if(color.y>=color.z && color.z>=color.x){
		gl_FragColor.y=ap*color.y+am*color.x;
		gl_FragColor.x=am*color.y+ap*color.x;
		gl_FragColor.z=am*color.y+am*color.x+a*color.z;
	}else if(color.z>=color.x && color.x>=color.y){
		gl_FragColor.z=ap*color.z+am*color.y;
		gl_FragColor.y=am*color.z+ap*color.y;
		gl_FragColor.x=am*color.z+am*color.y+a*color.x;
	}else if(color.z>=color.y && color.y>=color.x){
		gl_FragColor.z=ap*color.z+am*color.x;
		gl_FragColor.x=am*color.z+ap*color.x;
		gl_FragColor.y=am*color.z+am*color.x+a*color.y;
	}
}
