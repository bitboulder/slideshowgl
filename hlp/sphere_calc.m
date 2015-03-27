
vx=-1.5:0.01:1.5;
vy=(-1.5:0.01:1.5)';

vx=ones(size(vy)(1),1)*vx;
vy=vy*ones(1,size(vx)(2));

for gsy=-90:10:90

	svx=sin(vx*pi*2);
	cvx=cos(vx*pi*2);
	svy=sinh(vy*pi*2);
	cvy=cosh(vy*pi*2);
	sgy=sin(gsy/180*pi);
	cgy=cos(gsy/180*pi);

	f=cgy./(cvy-svy*sgy);

	x3=f.*svx;
	y3=f.*((cvx-cvy)*sgy+svy);
	z3=f.*(cvy-cvx)*cgy-1;

	for pl='xyz'
		fn=sprintf('sc_%s_g%03i.plot',pl,gsy)
		save('-ascii',fn,[pl '3'])
		system(['echo "#-typ image" >>' fn]);
		system(['echo "#-cblabel ' pl '" >>' fn]);
		system(['echo "#-xlabel vx" >>' fn]);
		system(['echo "#-ylabel vy" >>' fn]);
		system([sprintf('echo "#-title gsy=%i" >>',gsy) fn]);
		system(['echo "#-xrange -0.5:300.5" >>' fn]);
		system(['echo "#-yrange -0.5:300.5" >>' fn]);
		system(['echo "#-xtics -1.5:0.1/101+150.5:1.5" >>' fn]);
		system(['echo "#-ytics -1.5:0.1/101+150.5:1.5" >>' fn]);
	end

end
