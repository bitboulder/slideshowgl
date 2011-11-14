uniform sampler2D tex;
void main(){
	vec4 color;
	
	// texture enabled?
	if(gl_SecondaryColor[0]>0.0){
		color = texture2D(tex,gl_TexCoord[0].st);

		// colmod
		if(gl_Color.x!=0.5){
			color.x = pow(color.x,-log2(gl_Color.x));
			color.y = pow(color.y,-log2(gl_Color.x));
			color.z = pow(color.z,-log2(gl_Color.x));
		}
		if(gl_Color.y!=0.5) color = (0.5-color)*log2(1.0-gl_Color.y) + 0.5;
		if(gl_Color.z!=0.5) color = color + (gl_Color.z - 0.5)*2.0;

		color.a = color.a * gl_Color.a;
	}else{
		color = gl_Color;
	}

	// output
	gl_FragColor = color;
}
