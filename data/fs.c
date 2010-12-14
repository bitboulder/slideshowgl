uniform sampler2D tex;
void main(){
	vec4 color = texture2D(tex,gl_TexCoord[0].st);

	// colmod
	if(gl_Color.x!=0.5) color = pow(color,-log2(gl_Color.x));
	if(gl_Color.y!=0.5) color = (0.5-color)*log2(1-gl_Color.y) + 0.5;
	if(gl_Color.z!=0.5) color = color + (gl_Color.z - 0.5)*2;

	// blend
	gl_FragColor.a = color.a*gl_Color.a + gl_FragColor.a*(1-gl_Color.a);
	gl_FragColor.rgb = color.rgb;
}
