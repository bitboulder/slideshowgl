uniform sampler2D tex;
void main(){
	vec4 color;
	
	// texture enabled?
	if(gl_SecondaryColor[0]>0.0){
		color = texture2D(tex,gl_TexCoord[0].st);
		if(gl_SecondaryColor[1]>0.0){
			color.x=(color.x-gl_Color.x)/gl_Color.y;
			color.y=1-color.x;
		}else{
			if(gl_Color.x!=0.5) color.xyz = pow(color.xyz,-log2(gl_Color.x));
			if(gl_Color.y!=0.5) color.xyz = (0.5-color.xyz)*log2(1.0-gl_Color.y) + 0.5;
			if(gl_Color.z!=0.5) color.xyz = color.xyz + (gl_Color.z - 0.5)*2.0;
		}
		color.a = color.a * gl_Color.a;
	}else{
		color = gl_Color;
	}

	// output
	gl_FragColor = color;
}
