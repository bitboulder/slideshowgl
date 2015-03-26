%
%> d		(1)   - log with iz=8.44-2*log2(d)  ->  d=2^(4.22-iz/2)
%> r		(0.5) - worst at 0.5
%> gsy	(50)  - worse at pole
%
%py0=0.5-asinh(tan(gsy/180*pi))/pi/2
%pym=0.5-asinh(tan((gsy+d*r)/180*pi))/pi/2
%py1=0.5-asinh(tan((gsy+d)/180*pi))/pi/2
%
%pe=pym-py0-(py1-py0)*r
%ie=pe*2^iz*256
%
%ie=1
%iz=-log2(pe)-8
%
%ph=py1-py0
%ih=ph*2^iz*256
%
%pe = (
%	 -asinh(tan(gsy/180*pi))
%	+asinh(tan((gsy+d*r)/180*pi))
%	+(-asinh(tan((gsy+d)/180*pi))
%	  +asinh(tan(gsy/180*pi)) )*r
%	)/pi/2
%
%%% iz-r %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

d=1;
r=(0:0.01:1)';
gsy=50;

d=d*ones(size(r));
gsy=gsy.*ones(size(r));

py0=0.5-asinh(tan(gsy/180*pi))/pi/2;
pym=0.5-asinh(tan((gsy+d.*r)/180*pi))/pi/2;
py1=0.5-asinh(tan((gsy+d)/180*pi))/pi/2;

pe=pym-py0-(py1-py0).*r;

iz=-log2(pe)-8;

iz=[r iz];
save -ascii "iz-r" iz

%%% iz-d %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

d=2.^(-10:0.5:5)';
r=0.5;
gsy=50;

r=r*ones(size(d));
gsy=gsy.*ones(size(d));

py0=0.5-asinh(tan(gsy/180*pi))/pi/2;
pym=0.5-asinh(tan((gsy+d.*r)/180*pi))/pi/2;
py1=0.5-asinh(tan((gsy+d)/180*pi))/pi/2;

pe=pym-py0-(py1-py0).*r;

iz=-log2(pe)-8;

iz=[d iz 8.44-2*log2(d)];
save -ascii "iz-d" iz
system("echo '#-xlabel d' >>iz-d");
system("echo '#-ylabel iz' >>iz-d");
system("echo '#-xlog' >>iz-d");
system("echo '#-ygrid -xgrid' >>iz-d");
system("echo '#-cn calc' >>iz-d");
system("echo '#-cn est' >>iz-d");

%%% iz-g %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

d=1;
r=0.5;
gsy=(0:2:80)';

d=d.*ones(size(gsy));
r=r*ones(size(gsy));

py0=0.5-asinh(tan(gsy/180*pi))/pi/2;
pym=0.5-asinh(tan((gsy+d.*r)/180*pi))/pi/2;
py1=0.5-asinh(tan((gsy+d)/180*pi))/pi/2;

pe=pym-py0-(py1-py0).*r;

iz=-log2(pe)-8;

iz=[gsy iz];
save -ascii "iz-g" iz

%%% iz-gd %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

d=2.^(-10:0.5:5)';
r=0.5;
gsy=(10:2:80);

d=d*ones(1,size(gsy)(2));
gsy=ones(size(d)(1),1)*gsy;
r=r*ones(size(d));

py0=0.5-asinh(tan(gsy/180*pi))/pi/2;
pym=0.5-asinh(tan((gsy+d.*r)/180*pi))/pi/2;
py1=0.5-asinh(tan((gsy+d)/180*pi))/pi/2;

pe=pym-py0-(py1-py0).*r;

iz=abs(-log2(pe)-8);

iz=iz+2*log2(d);

save -ascii "iz-dg" iz
system("echo '#-typ image' >>iz-dg");
system("echo '#-xlabel gsy' >>iz-dg");
system("echo '#-ylabel log2(d)' >>iz-dg");
system("echo '#-cblabel iz+2*log2(d)' >>iz-dg");
system("echo '#-xtics 10:10/0.5-5:80' >>iz-dg");
system("echo '#-ytics -10:1/2+20:5' >>iz-dg");

%%% d-iz %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

gsy=50;
iz=(0:22)';
d=2.^(4.22-iz/2);

d=[iz d];
save -ascii "d-iz" d
system("echo '#-ylabel d' >>d-iz");
system("echo '#-xlabel iz' >>d-iz");
system("echo '#-ylog' >>d-iz");
system("echo '#-ygrid -xgrid' >>d-iz");
system("echo '#-cn d-est' >>d-iz");

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
