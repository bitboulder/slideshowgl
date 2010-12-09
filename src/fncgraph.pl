#!/usr/bin/perl

use strict;

my %ignore=(
	"_"=>1,
## debug
	"debug"=>1,"error"=>1,"debug_ex"=>1,"timer"=>1,"sdlthreadcheck"=>1, # debug
## config
	"cfggetint"=>1,"cfggetenum"=>1,"cfggetcol"=>1,"cfggetfloat"=>1,"cfggetstr"=>1, # cfgget
## macors
	"MAX"=>1,"MIN"=>1, # help.h
	"DE_DIR"=>1,"DE_NEG"=>1, # dplev
	"ADDTXT"=>1, # dpl
	"TAN"=>1,"SIN"=>1,"COS"=>1, # pano.c
## img* var
	"imgexifrot"=>1,"imgexifrotf"=>1,"imgexifinfo"=>1, # imgexif
	"imgposopt"=>1,"imgposcur"=>1,"imgposcol"=>1,"imgposmark"=>1, # imgpos
	"imgldrat"=>1,"imgldtex"=>1,"imgldfn"=>1,"imgldpano"=>1, # imgld
	"imgfilefn"=>1,"imgfiletfn"=>1, # imgfile
	"imgpanoenable"=>1, # imgpano
## global var
	"sdlrat"=>1, # sdl
	"dplgetimgi"=>1,"dplgetzoom"=>1,"dplineff"=>1,"dplshowinfo"=>1, # dpl
	"dplinputnum"=>1,"dplhelp"=>1,"dplloop"=>1,"dplstat"=>1, # dpl
	"glsetbar"=>1, # gl
	"imgget"=>1, # img
	"panoactive"=>1, # pano
## interfaces
	"sdldelay"=>1, # sdl
	"actadd"=>1, # act
	"dplevput"=>1,"dplevputx"=>1, # dplev
	"finddatafile"=>1, # main
);

shift if my $onlyglobal=$ARGV[0]eq"-g";
chdir shift if @ARGV;

system "cscope -b *.[ch]";

my %fnc=();
my %todo=("main"=>1,"sdlthread"=>1,"dplthread"=>1,"ldthread"=>1,"actthread"=>1);

while(keys %todo){
	my $fnc=(keys %todo)[0];
	delete $todo{$fnc};

	next if exists $ignore{$fnc};
	my @pos=&cscope("-1".$fnc," +\\*?$fnc *\\(");
	next if !@pos;
	die "function mulitple found: $fnc (".&joinvalues(@pos).")" if @pos>1;
	$fnc{$fnc}=$pos[0];
	$fnc{$fnc}->{"global"}=1 if "main"eq$fnc || &cscope("-0".$fnc,"\\.h .* +\\*?$fnc *\\(");

	foreach my $call (&cscope("-2".$fnc)){
		#print $fnc."-> ".$call->{"file"}." ".$call->{"fnc"}."\n";
		$fnc{$fnc}->{"call"}->{$call->{"fnc"}}=1;
		$todo{$call->{"fnc"}}=1 if !exists $fnc{$call->{"fnc"}};
	}
}

foreach my $fnc (keys %fnc){
	foreach my $call (keys %{$fnc{$fnc}->{"call"}}){
		delete $fnc{$fnc}->{"call"}->{$call},next if !exists $fnc{$call};
		$fnc{$call}->{"called"}->{$fnc}=1;
	}
}

if($onlyglobal){
	foreach my $fnc (keys %fnc){
		next if exists $fnc{$fnc}->{"global"};
		foreach my $call (keys %{$fnc{$fnc}->{"call"}}){
			delete $fnc{$call}->{"called"}->{$fnc};
			foreach my $called (keys %{$fnc{$fnc}->{"called"}}){
				$fnc{$call}->{"called"}->{$called}=1;
			}
		}
		foreach my $called (keys %{$fnc{$fnc}->{"called"}}){
			delete $fnc{$called}->{"call"}->{$fnc};
			foreach my $call (keys %{$fnc{$fnc}->{"call"}}){
				$fnc{$called}->{"call"}->{$call}=1;
			}
		}
		delete $fnc{$fnc};
	}
}

my $dot="digraph fnc {\n";
foreach my $fnc (keys %fnc){
	my $shape = exists $fnc{$fnc}->{"global"} ? "box" : "ellipse";
	$dot.="\t$fnc [shape=\"$shape\",label=\"$fnc\\n".$fnc{$fnc}->{"file"}."\"]\n";
	foreach my $call (keys %{$fnc{$fnc}->{"call"}}){
		my $f1=$fnc{$fnc }->{"file"}; $f1=~s/\..*//;
		my $f2=$fnc{$call}->{"file"}; $f2=~s/\..*//;
		my $style = $f1 eq $f2 ? "solid" : "dotted";
		$dot.="\t$fnc -> $call [style=\"$style\"]\n";
	}
}
$dot.="}\n";

open DOT,"| dot -Tps >fncgraph.ps";
print DOT $dot;
close DOT;

sub cscope {
	(my $arg,my $pat)=@_;
	my @ret=();
	open CS,"cscope -dL -f cscope.out $arg |";
	while(<CS>){
		chomp $_;
		next if ""ne$pat && $_!~/$pat/;
		(my $file,my $fnc,my $line)=split / +/,$_;
		next if $file=~/\//;
		$ret[@ret  ]->{"file"}=$file;
		$ret[@ret-1]->{"fnc" }=$fnc;
		$ret[@ret-1]->{"line"}=$line;
	}
	close CS;
	return @ret;
}

sub joinvalues {
	my @a=@_;
	foreach my $a (@a){ $a=join " ",values %{$a}; }
	return join " ; ",@a;
}
